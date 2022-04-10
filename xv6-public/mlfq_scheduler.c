#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "mlfq_scheduler.h"

#define GET_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BEGIN(mlfq)   (((mlfq)->f + 1) % (NPROC+1))
#define NEXT(iter)    (((iter) + 1) % (NPROC+1))
#define PREV(iter)    (((iter) + NPROC) % (NPROC+1))
#define END(mlfq)     (((mlfq)->r + 1) % (NPROC+1))

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct proc_queue {
  int f, r;
  struct proc *items[NPROC + 1];
};

uint mlfq_ticks;
int mlfq_ticks_left;
mlfq_queue_t mlfq[NMLFQ];

static inline int
queue_size(const mlfq_queue_t *q)
{
  return q->r >= q->f ? q->r - q->f : (NPROC + 1) + q->r - q->f;
}

static inline int
queue_is_full(const mlfq_queue_t *q)
{
  return END(q) == q->f;
}

static int
queue_push_item(mlfq_queue_t *q, struct proc *item)
{
  if(queue_is_full(q))
    return -1;

  q->r = NEXT(q->r);
  q->items[q->r] = item;
  return 0;
}

static int
queue_pop_item(mlfq_queue_t *q)
{
  if(queue_size(q) <= 0)
    return -1;

  q->f = NEXT(q->f);
  q->items[q->f] = 0;
  return 0;
}

static inline struct proc *
queue_front(mlfq_queue_t *q)
{
  return q->items[BEGIN(q)];
}

static inline struct proc *
queue_rear(mlfq_queue_t *q)
{
  return q->items[q->r];
}

int
mlfq_push(struct proc *item)
{
  if(item->lev < 0 && item->lev >= NMLFQ)
    return -1;
  if(queue_rear(&mlfq[item->lev]) == item)
    return -1;
  return queue_push_item(&mlfq[item->lev], item);
}

int
mlfq_pop(struct proc **item, int lev)
{
  if(lev < 0 && lev >= NMLFQ)
    return -1;
  *item = queue_front(&mlfq[lev]);
  return queue_pop_item(&mlfq[lev]);
}

int
mlfq_empty(int lev)
{
  return queue_size(&mlfq[lev]) == 0;
}

int
mlfq_remove(struct proc* p)
{
  int lev = p->lev, idx;
  for(idx = BEGIN(&mlfq[lev]);
      idx != END(&mlfq[lev]);
      idx = NEXT(idx)) {
    if(mlfq[lev].items[idx] == p){
      for(;idx!=mlfq[lev].f;idx = PREV(idx)){
        mlfq[lev].items[idx] = mlfq[lev].items[PREV(idx)];
      }
      mlfq[lev].items[mlfq[lev].f] = 0;
      mlfq[lev].f = NEXT(mlfq[lev].f);
      return 0;
    }
  }
  return -1;
}

void
mlfq_boost_priority(void)
{
  int lev;
  struct proc *p;
  for(lev = 1; lev < NMLFQ; lev++) {
    while(queue_size(&mlfq[lev]) > 0) {
      p = queue_front(&mlfq[lev]);
      queue_pop_item(&mlfq[lev]);
      p->lev = 0;
      p->cticks = 0;
      queue_push_item(&mlfq[0], p);
    }
  }
}

inline int
mlfq_has_to_yield(void)
{
  int left;
  struct proc* p = myproc();
  switchkvm();
  mlfq_ticks += 1;   // Increase the MLFQ tick count
  p->cticks += 1;    // Increase the process's tick count
  mlfq_ticks_left--; // Decrease the time quantom for this queue

  if(mlfq_ticks % MLFQ_BOOSTING_TICKS == 0) {
    // mlfq_print();
    cprintf("Priority Boosting Occurs\n");
    mlfq_boost_priority();
    // mlfq_print();
  }
  left = mlfq_ticks_left;
  switchuvm(p);
  // cprintf("%d\n", left);
  return left <= 0;
}

void
mlfq_print(void)
{
  int lev, idx;
  static const char* state2str[] = {
    [UNUSED]    "unused",
    [EMBRYO]    "embryo",
    [SLEEPING]  "sleep ",
    [RUNNABLE]  "runble",
    [RUNNING]   "run   ",
    [ZOMBIE]    "zombie"
  };
  cprintf("MLFQ Queue Info\n");
  for(lev = 0; lev < NMLFQ; lev++) {
    cprintf("Level %d (%d)\n", lev, queue_size(&mlfq[lev]));
    cprintf("f=%d, r=%d\n", mlfq[lev].f, mlfq[lev].r);
    for(idx = BEGIN(&mlfq[lev]);
        idx != END(&mlfq[lev]);
        idx = NEXT(idx))
      cprintf("[%d] %s %s\n", idx, state2str[mlfq[lev].items[idx]->state], mlfq[lev].items[idx]->name);
    cprintf("\n");
  }
}

int
mlfq_is_in_mlfq(struct proc *p)
{
  int idx = mlfq[p->lev].f;
  while(1) {
    idx = (idx + 1) % (NPROC + 1);
    if(mlfq[p->lev].items[idx] == p)
      return 1;
    if(idx == mlfq[p->lev].r)
      break;
  };
  return 0;
}

void
mlfq_scheduler(struct cpu *c)
{
  int lev, idx;
  struct proc *p;

  for(lev = 0; lev < NMLFQ; lev++) {
    for(idx = BEGIN(&mlfq[lev]); idx != END(&mlfq[lev]); idx = NEXT(idx)){
      if(mlfq[lev].items[idx]->state == RUNNABLE){
        goto mlfq_found;
      }
    }
    continue;
    
  mlfq_found:  
    // Get a process from MLFQ
    mlfq_pop(&p, lev);
    mlfq_ticks_left = MLFQ_MAX_TICKS[p->lev];

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    if(p->cticks >= MLFQ_TIME_ALLOTS[p->lev]) {
      p->cticks = 0;                           // Reset the tick counts
      p->lev = GET_MIN(p->lev + 1, NMLFQ - 1); // Lower the priority level
    }

    if(p->state == RUNNABLE || p->state == SLEEPING || p->state == RUNNING){
      mlfq_push(p); // Push the process into MLFQ
    }

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

    break;
  }
}