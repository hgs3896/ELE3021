// Semapores

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "semapore.h"

void
xem_init(xem_t *semapore, int cap)
{
  initlock(&semapore->lk, "semapore");
  semapore->lock_caps = cap;
  semapore->name = "semapore";
  semapore->pid = 0;
}

void
xem_wait(xem_t *semapore)
{
  acquire(&semapore->lk);
  while(semapore->lock_caps <= 0) {
    sleep(semapore, &semapore->lk);
  }
  --semapore->lock_caps;
  semapore->pid = myproc()->pid;
  release(&semapore->lk);
}

void
xem_unlock(xem_t *semapore)
{
  acquire(&semapore->lk);
  ++semapore->lock_caps;
  semapore->pid = 0;
  wakeup(semapore);
  release(&semapore->lk);
}

int
sys_xem_init(void)
{
  xem_t *semapore;
  if(argptr(0, (char**)&semapore, sizeof(semapore)) < 0) {
    return -1;
  }
  xem_init(semapore, 1);
  return 0;
}

int
sys_xem_wait(void)
{
  xem_t *semapore;
  if(argptr(0, (char**)&semapore, sizeof(semapore)) < 0) {
    return -1;
  }
  xem_wait(semapore);
  return 0;
}

int
sys_xem_unlock(void)
{
  xem_t *semapore;
  if(argptr(0, (char**)&semapore, sizeof(semapore)) < 0) {
    return -1;
  }
  xem_unlock(semapore);
  return 0;
}