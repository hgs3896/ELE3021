#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "lwp.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct spinlock lwplock;
static uint used[(MAX_LWPS + 7) / 8];
static struct lwp lwps[MAX_LWPS];

extern void trapret(void);

static void thread_init(void);
static void switchlwpkm(struct lwp *lwp);
static struct lwp ** find_empty_lwp(void);
static void wakeup1(void *chan);

// Wrapper for thread_create
int
sys_thread_create(void)
{
  thread_t *thread;
  void *(*start_routine)(void *);
  void *arg;

  if(argint(0, (int *)&thread) < 0)
    return -1;
  if(argptr(1, (char **)&start_routine, 4) < 0)
    return -1;
  if(argptr(2, (char **)&arg, 4) < 0)
    return -1;
  return thread_create(thread, start_routine, arg);
}

// Wrapper for thread_exit
int
sys_thread_exit(void)
{
  void *ret_val;
  if(argptr(0, (char **)&ret_val, 4) < 0)
    return -1;
  thread_exit(ret_val);
  return 0; // Not reachable
}

// Wrapper for thread_join
int
sys_thread_join(void)
{
  thread_t thread;
  void **ret_val;
  if(argint(0, &thread) < 0)
    return -1;
  if(argptr(1, (char **)&ret_val, 4) < 0)
    return -1;
  return thread_join(thread, ret_val);
}

int
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
  struct proc *curproc = myproc();
  struct lwp *lwp;
  uint sp;

  // lwp 할당
  lwp = alloclwp();
  if(lwp == 0)
    return -1;

  // lwp 구조체 채우기

  // Allocate kernel stack.
  if((lwp->kstack = kalloc()) == 0) {
    dealloclwp(lwp);
    return -1;
  }
  // Initialize the kernel stack pointer
  sp = (uint)lwp->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *lwp->tf;
  lwp->tf = (struct trapframe *)sp;
  *lwp->tf = *mylwp(curproc)->tf;

  sp -= sizeof(uint);
  *(uint *)sp = (uint)trapret;

  // Leave room for context
  sp -= sizeof *lwp->context;
  lwp->context = (struct context *)sp;
  memset(lwp->context, 0, sizeof(struct context));
  lwp->context->eip = (uint)thread_init;
  lwp->context->ebp = (uint)lwp->kstack + KSTACKSIZE;

  // Set the lwp runnable
  lwp->state = LWP_RUNNABLE;

  struct lwp **p_lwp = find_empty_lwp();
  if(!p_lwp) {
    kfree(lwp->kstack);
    dealloclwp(lwp);
    return -1;
  }

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  pde_t *pgdir = curproc->pgdir;
  uint stack_sz = 2 * PGSIZE;
  uint stack_base = stack_base_lwp(p_lwp);
  if((allocuvm(pgdir, stack_base - stack_sz, stack_base)) == 0)
    goto bad;
  clearpteu(pgdir, (char *)(stack_base - stack_sz));
  lwp->stack_sz = stack_base - stack_sz;

  sp = stack_base;

  uint ustack[] = {
      (uint)0xfffffff0,
      (uint)arg,
  };

  sp -= sizeof ustack;
  if(copyout(pgdir, sp, ustack, sizeof ustack) < 0)
    goto bad;

  curproc->pgdir = pgdir;
  // Update the page table
  switchuvm(curproc);

  if(*(uint *)sp != 0xfffffff0)
    panic("Fake Address is bad");
  if((void *)*(uint *)(sp + 4) != arg)
    panic("Argument is bad");

  lwp->tf->ebp = stack_base;
  lwp->tf->esp = sp;
  lwp->tf->eip = (uint)start_routine;
  *thread = lwp->tid = curproc->lwp_cnt++;
  lwp->ptid = mylwp(curproc)->tid;

  // 현재 프로세스에 lwp추가
  acquire(&ptable.lock);
  *p_lwp = lwp;
  release(&ptable.lock);

  return 0;
bad:
  panic("Bad LWP");
  return -1;
}

int
thread_join(thread_t thread, void **ret_val)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;) {
    uint found = 0;
    for(struct lwp **p_lwp = curproc->lwps; p_lwp < &curproc->lwps[NLWPS];
        ++p_lwp) {
      if((*p_lwp) && (*p_lwp)->tid == thread) {
        found = 1;
        if((*p_lwp)->state != LWP_ZOMBIE)
          continue;

        uint stack_sz = 2 * PGSIZE;
        uint stack_base = stack_base_lwp(p_lwp);

        kprintf_debug("thread_join %d\n", (*p_lwp)->tid);

        *ret_val = (*p_lwp)->ret_val;

        // 유저 스택 메모리 해제
        if((deallocuvm(curproc->pgdir, stack_base, stack_base - stack_sz)) ==
           0) {
          panic("not avail to dealloc");
        }

        // 커널 스택 메모리 해제
        kfree((*p_lwp)->kstack);

        // 커널 스택 메모리 초기화
        (*p_lwp)->kstack = 0;

        // lwp 해제
        dealloclwp(*p_lwp);

        // lwp 슬롯 초기화
        *p_lwp = 0;

        release(&ptable.lock);
        return 0;
      }
    }

    // No point waiting if we don't have any children.
    if(!found || curproc->killed) {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    int my_tid = mylwp(curproc)->tid;
    thread_sleep((void *)(my_tid), &ptable.lock); // DOC: wait-sleep
  }

  return 0;
}

void
thread_exit(void *ret_val)
{
  struct proc *curproc = myproc();
  struct lwp *lwp = mylwp(curproc);

  // Save the return value
  lwp->ret_val = ret_val;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1((void *)lwp->ptid);

  // Wakeup child lwps

  // Jump into the scheduler, never to return.
  lwp->state = LWP_ZOMBIE;

  kprintf_debug("thread_exit %d\n", lwp->tid);
  swtchlwp1(get_runnable_lwp(curproc));
  panic("zombie thread exit");
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
thread_sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  struct lwp *lwp = mylwp(p);

  if(p == 0)
    panic("sleep");

  if(lwp == 0)
    panic("sleep without running lwp");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock) { // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  lwp->chan = chan;
  lwp->state = LWP_SLEEPING;

  struct lwp **p_next_lwp = get_runnable_lwp(p);
  if(p_next_lwp != 0)
    swtchlwp1(p_next_lwp);
  else {
    p->state = SLEEPING;
    sched();
  }

  // Tidy up.
  lwp->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock) { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

struct lwp *
alloclwp(void)
{
  acquire(&lwplock);
  for(uint i = 0; i < MAX_LWPS / 8; ++i) {
    if(used[i] == 0xff)
      continue;
    for(uint j = 0; j < 8; ++j) {
      if((used[i] & (1 << j)) == 0) {
        used[i] |= 1 << j;
        release(&lwplock);
        lwps[(i << 3) | j].state = LWP_EMBRYO;
        return &lwps[(i << 3) | j];
      }
    }
  }
  release(&lwplock);
  return 0;
}

void
dealloclwp(struct lwp *unused)
{
  if(!(unused >= lwps && unused < lwps + MAX_LWPS))
    panic("not allocated lwp");
  uint idx = unused - lwps;
  acquire(&lwplock);
  used[idx >> 3] &= ~(1 << (idx & 7)); // used[idx / 8] &= ~(1 << (idx % 8));
  memset(unused, 0, sizeof *unused);
  release(&lwplock);
}

void
swtchlwp(struct lwp **p_lwp)
{
  acquire(&ptable.lock);
  swtchlwp1(p_lwp);
  release(&ptable.lock);
}

void
swtchlwp1(struct lwp **p_lwp)
{
  struct proc *curproc = myproc();
  struct lwp *curlwp = mylwp(curproc);

  if(curlwp == *p_lwp)
    return;

  curproc->lwp_idx = p_lwp - curproc->lwps;
  if(curproc->lwp_idx >= NLWPS || curproc->lwp_idx < 0)
    panic("Bad lwp idx");

  switchlwpkm(*p_lwp);

  int intena = mycpu()->intena;
  swtch(&curlwp->context, (*p_lwp)->context);
  mycpu()->intena = intena;
}

struct lwp **
get_runnable_lwp(struct proc *p)
{
  struct lwp *curlwp = mylwp(p);

  int lwp_idx = (p->lwp_idx + 1) % NLWPS;
  while(lwp_idx != p->lwp_idx) {
    if(p->lwps[lwp_idx] != 0 && p->lwps[lwp_idx] != curlwp &&
       p->lwps[lwp_idx]->state == LWP_RUNNABLE)
      break;
    lwp_idx = (lwp_idx + 1) % NLWPS;
  }
  if(p->lwps[lwp_idx]->kstack == 0)
    panic("empty kernel stack");
  if(p->lwp_idx == lwp_idx)
    return 0;
  return &p->lwps[lwp_idx];
}

void
print_lwps(const struct proc *p)
{
  if(p == 0)
    panic("No process");

  static char *states[] = {
      [LWP_UNUSED] "unused",   [LWP_EMBRYO] "embryo",  [LWP_SLEEPING] "sleep ",
      [LWP_RUNNABLE] "runble", [LWP_RUNNING] "run   ", [LWP_ZOMBIE] "zombie"};

  for(int i = 0; i < p->lwp_cnt; ++i) {
    if(p->lwps[i] == 0)
      continue;
    cprintf("[%d] %s\n", i, states[p->lwps[i]->state]);
  }
}

// Switch TSS to correspond to light-weight process p.
static void
switchlwpkm(struct lwp *lwp)
{
  if(lwp == 0)
    panic("switchlwpkm: no main lwp");
  if(lwp->kstack == 0)
    panic("switchlwpkm: no kstack");

  pushcli();
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)lwp->kstack + KSTACKSIZE;
  popcli();
}

/*
 * struct lwp** find_empty_lwp(void)
 * returns a position to an empty lwp slot of the current process
 */
static struct lwp **
find_empty_lwp(void)
{
  struct proc *p = myproc();
  struct lwp **p_lwp = p->lwps;
  while(p_lwp != p->lwps + NLWPS) {
    if(*p_lwp == 0)
      return p_lwp;
    ++p_lwp;
  }
  return 0;
}

static void
wakeup1(void *chan)
{
  struct proc *p = myproc();
  struct lwp **p_lwp;
  for(p_lwp = p->lwps; p_lwp < &p->lwps[NLWPS]; ++p_lwp) {
    if((*p_lwp) && (*p_lwp)->state == LWP_SLEEPING && (*p_lwp)->chan == chan) {
      p->state = RUNNABLE;
      (*p_lwp)->state = LWP_RUNNABLE;
    }
  }
}

static void
thread_init(void)
{
  kprintf_debug("Thread init\n");
  release(&ptable.lock);
}

// static void
// thread_cleanup(void)
// {
//   acquire(&ptable.lock);

//   struct lwp* lwp = mylwp(myproc());

//   lwp->state = ZOMBIE;
//   // wakeup1((void*)t->tid);

//   sched();
//   panic("thread_cleanup: unreachable statements");
// }

// uint
// get_number_of_lwps(struct proc *p)
// {
//   uint cnt = 0;
//   for(struct lwp **p_lwp = p->lwps; p_lwp < &p->lwps[NLWPS]; ++p_lwp)
//     cnt++;
//   return cnt;
// }