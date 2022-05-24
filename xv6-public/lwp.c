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
static int all_lwp(struct lwp** lwps, enum lwpstate s);

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
  if(argint(0, (int*)&thread) < 0)
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

  // 1. Allocate a LWP control block
  lwp = alloclwp();
  if(lwp == 0)
    return -1;

  // Allocate kernel stack.
  if((lwp->kstack = kalloc()) == 0) {
    dealloclwp(lwp);
    return -1;
  }

  // Fill the kernel stack

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
  lwp->stack_sz = stack_sz;

  sp = stack_base;

  uint ustack[] = {
    (uint)0xfffffff0,
    (uint)arg,
  };

  // Copy the contents of temporary user stack into the page table used in the current process
  sp -= sizeof ustack;
  if(copyout(pgdir, sp, ustack, sizeof ustack) < 0)
    goto bad;

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

        // Copy the return value
        *ret_val = (*p_lwp)->ret_val;

        // 유저 스택 메모리 해제
        if((deallocuvm(curproc->pgdir, stack_base, stack_base - stack_sz)) == 0) {
          panic("cannot dealloc user stacks");
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
        kprintf_info("thread join pid = %d, tid = %d\n", curproc->pid, thread);
        return 0;
      }
    }

    // No point waiting if we don't have any children.
    if(!found || curproc->killed) {
      kprintf_info("Fail to join\n");
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep((void *)(mylwp(curproc)->tid), &ptable.lock); // DOC: wait-sleep
  }

  kprintf_error("bad lwp\n");
  return 0;
}

void
thread_exit(void *ret_val)
{
  struct proc *curproc = myproc();
  struct lwp **p_lwp, *curlwp = mylwp(curproc);

  // Save the return value
  curlwp->ret_val = ret_val;

  acquire(&ptable.lock);

  while(1){
    // Parent lwp might be sleeping in thread_join().
    wakeup1((void *)curlwp->ptid);

    // Pass abandoned children to main lwp.
    for(p_lwp = curproc->lwps; p_lwp < &curproc->lwps[NLWPS]; p_lwp++){
      if((*p_lwp) && (*p_lwp)->ptid == curlwp->tid){
        (*p_lwp)->ptid = 0;
        if((*p_lwp)->state == LWP_ZOMBIE)
          wakeup1(0);
      }
    }

    // If every lwp is ZOMBIE, then exit the whole process
    if(all_lwp(curproc->lwps, LWP_ZOMBIE)){
      exit();
    }

    // Jump into the scheduler, never to return.
    curlwp->state = LWP_ZOMBIE;

    kprintf_info("thread_exit at pid = %d, tid = %d\n", curproc->pid, curlwp->tid);

    struct lwp **p_next_lwp = get_runnable_lwp(curproc);
    if(p_next_lwp != 0 && p_next_lwp != mylwp1(curproc)) {
      // If another lwp is runnable, switch to another lwp.
      swtchlwp1(p_next_lwp);
    } else {
      // Otherwise, switch to the kernel scheduler.
      curproc->state = SLEEPING;
      sched();
    }
  }
  panic("zombie thread exit"); // Never returns
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
  if(curproc->lwp_idx >= NLWPS || curproc->lwp_idx < 0){
    panic("Bad lwp idx");
  }

  switchlwpkm(*p_lwp);

  kprintf_trace("[before] pid %d switch stack from %d(%d) to %d(%d)\n", curproc->pid, curlwp->tid, curlwp->state, (*p_lwp)->tid, (*p_lwp)->state);
  (*p_lwp)->state = LWP_RUNNING;
  kprintf_trace("[ after] pid %d switch stack from %d(%d) to %d(%d)\n", curproc->pid, curlwp->tid, curlwp->state, (*p_lwp)->tid, (*p_lwp)->state);

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
  if(p->lwps[lwp_idx]->state != LWP_RUNNABLE)
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

  for(int i = 0; i < NLWPS; ++i) {
    if(p->lwps[i] == 0)
      continue;
    if(i == p->lwp_idx)
      cprintf("*");
    cprintf("[%d] %s 0x%x\n", i, states[p->lwps[i]->state], p->lwps[i]->chan);
  }
}

int
copy_lwp(struct lwp *dst, const struct lwp *src)
{
  dst->tid = src->tid;
  dst->ptid = src->ptid;
  dst->stack_sz = src->stack_sz;
  dst->state = src->state;
  *dst->tf = *src->tf;
  dst->chan = src->chan;
  dst->ret_val = src->ret_val;
  return 0;
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

/*
 * this wakeup1 only wakes up the lwps up the current process
 */
static void
wakeup1(void *chan)
{
  struct proc *p = myproc();
  struct lwp **p_lwp;
  for(p_lwp = p->lwps; p_lwp < &p->lwps[NLWPS]; ++p_lwp) {
    if((*p_lwp) && (*p_lwp)->state == LWP_SLEEPING && (*p_lwp)->chan == chan) {
      if(p->state == SLEEPING)
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

static int
all_lwp(struct lwp** lwps, enum lwpstate s)
{
  for(struct lwp** p_lwps = lwps; p_lwps < &lwps[NLWPS]; ++p_lwps){
    if(*p_lwps && (*p_lwps)->state != s){
      return 0;
    }
  }
  return 1;
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