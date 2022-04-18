#ifndef _H_STRIDE_SCHEDULER_
#define _H_STRIDE_SCHEDULER_
#include "fraction.h"
#include "param.h"

#define STRIDE_MAX_TICKS 5
#define STRIDE_PROC_LEVEL 100
#define STRIDE_MLFQ_SHARE 22
#define STRIDE_MAX_SHARE 102

typedef struct StrideItem StrideItem;
typedef struct StrideHeap StrideHeap;
typedef struct StrideQueue StrideQueue;
struct StrideItem {
  frac pass;
  uint isMLFQ : 1;
  int share : 31;
  void *proc;
};

struct StrideHeap {
  uint size;
  StrideItem items[1 + NPROC];
};

struct StrideQueue {
  int all_share;
  frac max_pass;
  StrideHeap q;
};

extern StrideQueue stride_queue;
extern uint stride_ticks;

void stride_item_init(StrideItem *item, uint share, void *proc, uint isMLFQ);
int stride_item_copy(StrideItem *dest, const StrideItem *src);
StrideItem *stride_find_item(const struct proc *p);

int is_stride(struct proc *p);
void stride_init(void);
int stride_push(const StrideItem *item);
int stride_pop(void);
StrideItem *stride_top(void);
int stride_can_change_share(int old_share, int new_share);
int stride_adjust(StrideItem *item, int new_share);
void stride_scheduler(struct cpu *c);
void stride_print(void);
int stride_has_to_yield(struct proc *p);
#endif