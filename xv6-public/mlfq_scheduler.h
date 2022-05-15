#ifndef _H_MLFQ_SCHEDULER_
#define _H_MLFQ_SCHEDULER_
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define NMLFQ 3
#define DEBUG_BOOSTING 0
static const uint MLFQ_BOOSTING_TICKS = 200;
static const uint MLFQ_MAX_TICKS[NMLFQ] = {5, 10, 20};
static const uint MLFQ_TIME_ALLOTS[NMLFQ] = {20, 40, 1 << 31};

typedef struct proc_queue mlfq_queue_t;
extern uint mlfq_ticks;

int is_mlfq(struct proc *p);
int mlfq_push(struct proc *item);
int mlfq_pop(struct proc **item, int lev);
int mlfq_empty(int lev);
void mlfq_scheduler(struct cpu *c);
void mlfq_print(void);
int mlfq_has_to_yield(struct proc *p);
#endif