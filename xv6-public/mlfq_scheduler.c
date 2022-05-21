#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "schedulers.h"

#define GET_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BEGIN(mlfq) (((mlfq)->f + 1) % (NPROC + 1))
#define NEXT(iter) (((iter) + 1) % (NPROC + 1))
#define PREV(iter) (((iter) + NPROC) % (NPROC + 1))
#define END(mlfq) (((mlfq)->r + 1) % (NPROC + 1))

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct proc_queue {
  int f, r;
  struct proc *items[NPROC + 1];
};

struct spinlock mlfqlock;
uint mlfq_ticks;
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

static void
mlfq_boost_priority(void)
{
  int lev;
  struct proc *p;
  for(int idx = BEGIN(&mlfq[0]); idx != END(&mlfq[0]); idx = NEXT(idx))
    mlfq[0].items[idx]->cticks = 0;
  for(lev = NMLFQ - 1; lev > 0; lev--) {
    while(queue_size(&mlfq[lev]) > 0) {
      p = queue_front(&mlfq[lev]);
      queue_pop_item(&mlfq[lev]);
      p->lev = 0;
      p->cticks = 0;
      queue_push_item(&mlfq[0], p);
    }
  }
}

static int
mlfq_need_boosting(void)
{
  int mlfq_has_to_boost = 0;
  acquire(&mlfqlock); // Priority Boosting
  mlfq_has_to_boost = mlfq_ticks >= MLFQ_BOOSTING_TICKS;
  mlfq_ticks %= MLFQ_BOOSTING_TICKS;
  release(&mlfqlock);
  return mlfq_has_to_boost;
}

int
mlfq_has_to_yield(struct proc *p)
{
  static int mlfq_ticks_elapsed;

  const uint MLFQ_MAX_TICK_OF_CURRENT_PROC = MLFQ_MAX_TICKS[getlev()];

  if(mlfq_ticks_elapsed >= MLFQ_MAX_TICK_OF_CURRENT_PROC) {
    mlfq_ticks_elapsed = 0;
  }
  mlfq_ticks_elapsed++; // Increase the time quantom for this queue

  acquire(&mlfqlock);
  mlfq_ticks += 1; // Increase the MLFQ tick count
  release(&mlfqlock);

  acquire(&ptable.lock);
  p->cticks += 1; // Increase the process's tick count
  if(mlfq_ticks_elapsed >= MLFQ_MAX_TICK_OF_CURRENT_PROC)
    p->yield_by = 2;
  release(&ptable.lock);

  return mlfq_ticks_elapsed >= MLFQ_MAX_TICK_OF_CURRENT_PROC;
}

void
mlfq_print(void)
{
  int lev, idx;
  static const char *state2str[] = {
      [UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
      [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
  cprintf("MLFQ Queue Info\n");
  for(lev = 0; lev < NMLFQ; lev++) {
    cprintf("Level %d (%d)\n", lev, queue_size(&mlfq[lev]));
    cprintf("f=%d, r=%d\n", mlfq[lev].f, mlfq[lev].r);
    for(idx = BEGIN(&mlfq[lev]); idx != END(&mlfq[lev]); idx = NEXT(idx))
      cprintf("[%d] %s %s\n", idx, state2str[mlfq[lev].items[idx]->state],
              mlfq[lev].items[idx]->name);
    cprintf("\n");
  }
}

int
is_mlfq(struct proc *p)
{
  return p && p->lev >= 0 && p->lev < NMLFQ;
}

void
mlfq_scheduler(struct cpu *c)
{
  int lev, idx, begin, end;
  struct proc *p;

  while(1) {
    for(lev = 0; lev < NMLFQ; lev++) {
      begin = BEGIN(&mlfq[lev]), end = END(&mlfq[lev]);
      for(idx = begin; idx != end; idx = NEXT(idx)) {
        mlfq_pop(&p, lev);
        if(p->state == RUNNABLE) {
          goto mlfq_found;
        }
        mlfq_push(p);
      }
    }
    return; // No runnable items found

  mlfq_found:
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), mylwp(p)->context);
    switchkvm();

    if(is_mlfq(p)) {
      if(p->cticks >=
         MLFQ_TIME_ALLOTS[p->lev]) { // Check if it has consumed its time allots
        p->cticks = 0;               // Reset the tick counts
        p->lev = GET_MIN(p->lev + 1, NMLFQ - 1); // Lower the priority level
      }

      if(p->state != ZOMBIE) {
        // Push a non-zombie process into MLFQ
        mlfq_push(p);
      }

      if(mlfq_need_boosting()) {
#if DEBUG_BOOSTING
        cprintf("Priority Boosting Occurs\n");
#endif
        mlfq_boost_priority();
      }
    }

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

    if(p->yield_by == 1) { // yield by stride
      p->yield_by = 0;
      return;
    }
    p->yield_by = 0;
  }
}