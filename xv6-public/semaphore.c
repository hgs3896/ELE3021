// Semaphores

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "semaphore.h"

void
xem_init(xem_t *semaphore, int cap)
{
  initlock(&semaphore->lk, "semaphore");
  semaphore->lock_caps = cap;
  semaphore->name = "semaphore";
  semaphore->pid = 0;
}

void
xem_wait(xem_t *semaphore)
{
  acquire(&semaphore->lk);
  while(semaphore->lock_caps <= 0) {
    sleep(semaphore, &semaphore->lk);
  }
  --semaphore->lock_caps;
  semaphore->pid = myproc()->pid;
  release(&semaphore->lk);
}

void
xem_unlock(xem_t *semaphore)
{
  acquire(&semaphore->lk);
  ++semaphore->lock_caps;
  semaphore->pid = 0;
  wakeup(semaphore);
  release(&semaphore->lk);
}

int
sys_xem_init(void)
{
  xem_t *semaphore;
  if(argptr(0, (char**)&semaphore, sizeof(semaphore)) < 0) {
    return -1;
  }
  xem_init(semaphore, 1);
  return 0;
}

int
sys_xem_wait(void)
{
  xem_t *semaphore;
  if(argptr(0, (char**)&semaphore, sizeof(semaphore)) < 0) {
    return -1;
  }
  xem_wait(semaphore);
  return 0;
}

int
sys_xem_unlock(void)
{
  xem_t *semaphore;
  if(argptr(0, (char**)&semaphore, sizeof(semaphore)) < 0) {
    return -1;
  }
  xem_unlock(semaphore);
  return 0;
}