#include "types.h"
#include "user.h"
#include "uio.h"
#include "rwlock.h"

struct ts_guard {
  struct rwlock_t lock;
  int fd;
};

thread_safe_guard *
thread_safe_guard_init(int fd)
{
  thread_safe_guard *g;

  if((g = malloc(sizeof(struct ts_guard))) == 0) {
    return 0;
  }

  g->fd = fd;
  rwlock_init(&g->lock);
  return g;
}

int
thread_safe_pread(thread_safe_guard *file_guard, void *addr, int n, int off)
{
  int result;
  rwlock_acquire_readlock(&file_guard->lock);
  result = pread(file_guard->fd, addr, n, off);
  rwlock_release_readlock(&file_guard->lock);
  return result;
}

int
thread_safe_pwrite(thread_safe_guard *file_guard, void *addr, int n, int off)
{
  int result;
  rwlock_acquire_writelock(&file_guard->lock);
  result = pwrite(file_guard->fd, addr, n, off);
  rwlock_release_writelock(&file_guard->lock);
  return result;
}

void
thread_safe_guard_destroy(thread_safe_guard *file_guard)
{
  if(!file_guard)
    return;
  free(file_guard);
}