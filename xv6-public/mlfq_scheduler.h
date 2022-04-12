#include "types.h"

#define NMLFQ 3
static const uint MLFQ_BOOSTING_TICKS = 100;
static const uint MLFQ_MAX_TICKS[NMLFQ] = {1, 2, 4};
static const uint MLFQ_TIME_ALLOTS[NMLFQ] = {5, 10, 1 << 31};

typedef struct proc_queue mlfq_queue_t;
extern uint mlfq_ticks;
extern int mlfq_ticks_left;

int mlfq_push(struct proc *item);
int mlfq_pop(struct proc **item, int lev);
int mlfq_empty(int lev);
void mlfq_boost_priority(void);
int mlfq_has_to_yield(void);
void mlfq_scheduler(struct cpu *c);
void mlfq_print(void);
int mlfq_is_in_mlfq(struct proc *p);
int mlfq_remove(struct proc *p);