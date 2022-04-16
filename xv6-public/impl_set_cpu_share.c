#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "schedulers.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int
set_cpu_share(int share)
{
  int ret = 0;
  StrideItem stride_item;
  struct proc *p = myproc();
  acquire(&ptable.lock);
  if(is_mlfq(p)) {
    // Initialize the stride item
    p->lev = STRIDE_PROC_LEVEL;
    p->cticks = 0;
    stride_item_init(&stride_item, share, p, 0);

    // Push it to the Stride queue
    if((ret = stride_push(&stride_item))) {
      panic("The process cannot be pushed into StrideQ");
    }

    // Release the ptable lock for yield
    release(&ptable.lock);
    // Yield the cpu for rescheduling
    yield();

    return ret;
  } else if(is_stride(p)) {
    // Find the stride item and adjust the share value
    ret = stride_adjust(stride_find_item(p), share);
  } else {
    ret = -1;
  }
  release(&ptable.lock);
  return ret;
}

int
sys_set_cpu_share(void)
{
  int share = 0;
  if(argint(0, &share) < 0)
    return -1;
  if(share > 80 || share <= 0)
    return -1;
  return set_cpu_share(share);
}