#include "lwp.h"

int
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
  return 0;
}

int
thread_join(thread_t thread, void **ret_val)
{
  return 0;
}

void
thread_exit(void *ret_val)
{
  return;
}