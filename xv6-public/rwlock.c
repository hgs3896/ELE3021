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

void
rwlock_init(rwlock_t *rwlock)
{
}

void
rwlock_acquire_readlock(rwlock_t *rwlock)
{
}

void
rwlock_acquire_writelock(rwlock_t *rwlock)
{
}

void
rwlock_release_readlock(rwlock_t *rwlock)
{
}

void
rwlock_release_writelock(rwlock_t *rwlock)
{
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