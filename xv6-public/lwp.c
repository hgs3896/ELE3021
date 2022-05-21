#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "lwp.h"

extern void trapret(void);
extern void forkret(void);

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct spinlock lwplock;
static uint used[NCPU * NLWPS / 8];
static struct lwp lwps[NCPU * NLWPS];

struct lwp*
alloclwp(void)
{
  acquire(&lwplock);
  for(uint i = 0; i < NCPU * NLWPS / 8; ++i){
    if(used[i] == 0xff)
      continue;
    for(uint j = 0;j < 8;++j){
      if((used[i] & (1 << j)) == 0){
        used[i] |= 1 << j;
        release(&lwplock);
        return &lwps[(i << 3) | j];
      }
    }
  }
  release(&lwplock);
  return 0;
}

void
dealloclwp(struct lwp* unused)
{
  if(!(unused >= lwps && unused < lwps + NCPU * NLWPS))
    panic("not allocated lwp");
  uint idx = unused - lwps;
  acquire(&lwplock);
  used[idx >> 3] &= ~(1 << (idx & 7));
  release(&lwplock);
  memset(unused, 0, sizeof *unused);
}

static
struct lwp**
find_empty_lwp(void)
{
  struct proc* p = myproc();
  struct lwp** p_lwp = p->lwps;
  while (p_lwp != p->lwps + NLWPS)
  {
    if(*p_lwp == 0)
      return p_lwp;
    ++p_lwp;
  }
  return 0;
}

static void
thread_cleanup(void)
{
  acquire(&ptable.lock);

  struct lwp* lwp = mylwp(myproc());

  lwp->state = ZOMBIE;
  // wakeup1((void*)t->tid);

  sched();
  panic("thread_cleanup: unreachable statements");
}

int
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
  struct lwp* lwp;
  uint sp;

  // lwp 할당
  lwp = alloclwp();
  if(lwp == 0)
    return -1;

  // lwp 구조체 채우기

  // Allocate kernel stack.
  if((lwp->kstack = kalloc()) == 0){
    dealloclwp(lwp);
    return -1;
  }
  // Initialize the kernel stack pointer
  sp = (uint)lwp->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *lwp->tf;
  lwp->tf = (struct trapframe*)sp;
  *lwp->tf = *mylwp(myproc())->tf;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  // Leave room for context
  sp -= sizeof *lwp->context;
  lwp->context = (struct context*)sp;
  memset(lwp->context, 0, sizeof *lwp->context);
  lwp->context->eip = (uint)forkret;

  // Set the lwp runnable
  lwp->state = LWP_RUNNABLE;

  struct lwp** p_lwp = find_empty_lwp();
  if(!p_lwp){
    kfree(lwp->kstack);
    dealloclwp(lwp);
    return -1;
  }

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  pde_t* pgdir = myproc()->pgdir;
  uint stack_sz = 2 * PGSIZE;
  uint stack_base = USERTOP - NPAGESPERLWP * (p_lwp - myproc()->lwps) * PGSIZE;
  if((allocuvm(pgdir, stack_base - stack_sz, stack_base)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(stack_base - stack_sz));
  sp = stack_base;

  uint ustack[] = {
    (uint)thread_cleanup,
    (uint)arg,
  };

  sp -= sizeof ustack;
  if(copyout(pgdir, sp, ustack, sizeof ustack) < 0)
    goto bad;
  
  lwp->tf->esp = sp;
  lwp->tf->eip = (uint)start_routine;

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
  return 0;
}

void
thread_exit(void *ret_val)
{
  return;
}

// Wrapper for thread_create
int
sys_thread_create(void)
{
  thread_t *thread;
  void *(*start_routine)(void *);
  void *arg;

  if(argint(0, (int*)&thread) < 0)
    return -1;
  if(argptr(1, (char**)&start_routine, 4) < 0)
    return -1;
  if(argptr(2, (char**)&arg, 4) < 0)
    return -1;
  return thread_create(thread, start_routine, arg);
}

// Wrapper for thread_exit
int
sys_thread_exit(void)
{
  void *ret_val;
  if(argptr(0, (char**)&ret_val, 4) < 0)
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
  if(argptr(1, (char**)&ret_val, 4) < 0)
    return -1;
  return thread_join(thread, ret_val);
}