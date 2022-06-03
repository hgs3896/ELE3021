// Readers-writer Lock

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "rwlock.h"

extern void xem_init(xem_t *semaphore, int cap);
extern void xem_wait(xem_t *semaphore);
extern void xem_unlock(xem_t *semaphore);

void
rwlock_init(rwlock_t *rwlock)
{
  rwlock->num_readers = 0;
  rwlock->num_writers = 0;
  initlock(&rwlock->lk, "rwlock");
  xem_init(&rwlock->write, 1);
}

void
rwlock_acquire_readlock(rwlock_t *rwlock)
{
  int num_readers;

  acquire(&rwlock->lk);
  while(rwlock->num_writers > 0)
    sleep(rwlock, &rwlock->lk);
  num_readers = ++rwlock->num_readers;
  if(num_readers == 1)
    xem_wait(&rwlock->write);
  release(&rwlock->lk);
}

void
rwlock_acquire_writelock(rwlock_t *rwlock)
{
  acquire(&rwlock->lk);
  rwlock->num_writers++;
  release(&rwlock->lk);

  xem_wait(&rwlock->write);
}

void
rwlock_release_readlock(rwlock_t *rwlock)
{
  int num_readers;

  acquire(&rwlock->lk);
  num_readers = --rwlock->num_readers;
  if(num_readers == 0)
    xem_unlock(&rwlock->write);
  release(&rwlock->lk);
}

void
rwlock_release_writelock(rwlock_t *rwlock)
{
  xem_unlock(&rwlock->write);

  acquire(&rwlock->lk);
  rwlock->num_writers--;
  release(&rwlock->lk);

  wakeup(rwlock);
}

int
sys_rwlock_init(void)
{
  rwlock_t *rwlock;
  if(argptr(0, (char **)&rwlock, sizeof(rwlock)) < 0 || rwlock == 0) {
    return -1;
  }
  rwlock_init(rwlock);
  return 0;
}

int
sys_rwlock_acquire_readlock(void)
{
  rwlock_t *rwlock;
  if(argptr(0, (char **)&rwlock, sizeof(rwlock)) < 0 || rwlock == 0) {
    return -1;
  }
  rwlock_acquire_readlock(rwlock);
  return 0;
}

int
sys_rwlock_acquire_writelock(void)
{
  rwlock_t *rwlock;
  if(argptr(0, (char **)&rwlock, sizeof(rwlock)) < 0 || rwlock == 0) {
    return -1;
  }
  rwlock_acquire_writelock(rwlock);
  return 0;
}

int
sys_rwlock_release_readlock(void)
{
  rwlock_t *rwlock;
  if(argptr(0, (char **)&rwlock, sizeof(rwlock)) < 0 || rwlock == 0) {
    return -1;
  }
  rwlock_release_readlock(rwlock);
  return 0;
}

int
sys_rwlock_release_writelock(void)
{
  rwlock_t *rwlock;
  if(argptr(0, (char **)&rwlock, sizeof(rwlock)) < 0 || rwlock == 0) {
    return -1;
  }
  rwlock_release_writelock(rwlock);
  return 0;
}