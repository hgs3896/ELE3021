#include "types.h"
#include "stat.h"
#include "user.h"

xem_t sem;

void *
test_without_sem(void *arg)
{
  int id = (int)arg;
  for(int i = 0; i < 10; ++i)
    printf(1, "%d", id);
  thread_exit(0);
  return 0;
}

void *
test_with_sem(void *arg)
{
  int id = (int)arg;
  xem_wait(&sem);
  for(int i = 0; i < 10; ++i)
    printf(1, "%d", id);
  xem_unlock(&sem);
  thread_exit(0);
  return 0;
}

int
main(int argc, char *argv[])
{
  void *ret;
  const int N = 10;
  thread_t t[N];

  printf(1, "1. Test without any synchronization\n");
  for(int i = 0; i < N; ++i) {
    if(thread_create(&t[i], test_without_sem, (void*)(i)) < 0) {
      printf(1, "panic at thread create\n");
      exit();
    }
  }
  for(int i = 0; i < N; ++i) {
    if(thread_join(t[i], &ret) < 0) {
      printf(1, "panic at thread join\n");
      exit();
    }
  }
  printf(1, "\nIts sequence could be mixed\n");

  printf(1, "2. Test with synchronization of a binary semapore\n");
  xem_init(&sem);
  xem_wait(&sem);
  for(int i = 0; i < N; ++i) {
    if(thread_create(&t[i], test_with_sem, (void*)(i)) < 0) {
      printf(1, "panic at thread create\n");
      exit();
    }
  }
  xem_unlock(&sem);
  for(int i = 0; i < N; ++i) {
    if(thread_join(t[i], &ret) < 0) {
      printf(1, "panic at thread join\n");
      exit();
    }
  }
  printf(1, "\nIts sequence must be sorted\n");

  printf(1, "3. Test with synchronization of a semapore with 3 users\n");
  xem_init(&sem);
  sem.lock_caps = 3;
  xem_wait(&sem);
  for(int i = 0; i < N; ++i) {
    if(thread_create(&t[i], test_with_sem, (void*)(i)) < 0) {
      printf(1, "panic at thread create\n");
      exit();
    }
  }
  xem_unlock(&sem);
  for(int i = 0; i < N; ++i) {
    if(thread_join(t[i], &ret) < 0) {
      printf(1, "panic at thread join\n");
      exit();
    }
  }
  printf(1, "\nIts sequence could be messy\n");
  exit();
}