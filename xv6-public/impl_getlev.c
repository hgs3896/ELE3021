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

// Simple system call
int
getlev(void)
{
  struct proc *p = myproc();
  acquire(&ptable.lock);
  uint lev = p->lev;
  release(&ptable.lock);
  if(0 <= lev && lev < NMLFQ)
    return lev;
  return -1;
}

// Wrapper for getlev
int
sys_getlev(void)
{
  return getlev();
}