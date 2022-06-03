#include "types.h"
#include "stat.h"
#include "user.h"

xem_t print_sem;
rwlock_t rwlock;

#define REP 50
#define NTHREADS 63
#define READER_WRITER_RATIO 0.99

void * reader_with_rwlock(void *arg);
void * writer_with_rwlock(void *arg);
void * reader2_with_sem(void *arg);
void * writer2_with_sem(void *arg);
void * reader2_with_rwlock(void *arg);
void * writer2_with_rwlock(void *arg);

int
main(int argc, char *argv[])
{
  void *ret;
  int start_tick;

  thread_t t[NTHREADS];

  xem_init(&print_sem);
  rwlock_init(&rwlock);

  printf(1, "1. Logging the lock acquisition of a RW lock\n");
  for(int i = 0; i < NTHREADS; ++i) {
    void* (*start_routine)(void *) = i >= READER_WRITER_RATIO * NTHREADS ? writer_with_rwlock : reader_with_rwlock;
    if(thread_create(&t[i], start_routine, (void *)(i)) < 0) {
      printf(1, "panic at thread create\n");
      exit();
    }
  }
  for(int i = 0; i < NTHREADS; ++i) {
    if(thread_join(t[i], &ret) < 0) {
      printf(1, "panic at thread join\n");
      exit();
    }
  }

  int result[2];
  printf(1, "2. Lock Efficiency Test\n");
  printf(1, "\ta. Using a simple binary semaphore\n");
  start_tick = uptime();
  for(int i = 0; i < NTHREADS; ++i) {
    void* (*start_routine)(void *) = i >= READER_WRITER_RATIO * NTHREADS ? writer2_with_sem : reader2_with_sem;
    if(thread_create(&t[i], start_routine, (void *)(i)) < 0) {
      printf(1, "panic at thread create\n");
      exit();
    }
  }
  for(int i = 0; i < NTHREADS; ++i) {
    if(thread_join(t[i], &ret) < 0) {
      printf(1, "panic at thread join\n");
      exit();
    }
  }
  result[0] = uptime() - start_tick;
  
  printf(1, "\tb. Using a readers-writer lock\n");
  start_tick = uptime();
  for(int i = 0; i < NTHREADS; ++i) {
    void* (*start_routine)(void *) = i >= READER_WRITER_RATIO * NTHREADS ? writer2_with_rwlock : reader2_with_rwlock;
    if(thread_create(&t[i], start_routine, (void *)(i)) < 0) {
      printf(1, "panic at thread create\n");
      exit();
    }
  }
  for(int i = 0; i < NTHREADS; ++i) {
    if(thread_join(t[i], &ret) < 0) {
      printf(1, "panic at thread join\n");
      exit();
    }
  }
  result[1] = uptime() - start_tick;
  printf(1, "2-a) %d ticks, 2-b) %d ticks\n", result[0], result[1]);
  exit();
}

void *
reader_with_rwlock(void *arg)
{
  int id = (int)arg;

  for(int rep = 0; rep < REP; ++rep) {
    rwlock_acquire_readlock(&rwlock);
    xem_wait(&print_sem);
    printf(1, "Reader Accquired %d\n", id);
    xem_unlock(&print_sem);
    rwlock_release_readlock(&rwlock);
  }
  thread_exit(0);
  return 0;
}

void *
writer_with_rwlock(void *arg)
{
  int id = (int)arg;

  for(int rep = 0; rep < REP; ++rep) {
    rwlock_acquire_writelock(&rwlock);
    
    xem_wait(&print_sem);
    printf(1, "Writer Accquired %d\n", id);
    xem_unlock(&print_sem);

    volatile int n = 0;
    while(++n < 10000000); // Do something

    xem_wait(&print_sem);
    printf(1, "Writer Released %d\n", id);
    xem_unlock(&print_sem);

    rwlock_release_writelock(&rwlock);
  }

  thread_exit(0);
  return 0;
}

volatile
int data[10000];

void *
reader2_with_sem(void *arg)
{
  int sum;

  for(int rep = 0; rep < REP; ++rep) {
    sum = 0;  
    xem_wait(&print_sem);
    for(int i = 0; i < 10000; ++i)
      sum += data[i];
    if(!(sum == 0 || sum == 5000 * 10001))
      printf(1, "bad\n");
    xem_unlock(&print_sem);
  }

  thread_exit(0);
  return 0;
}

void *
writer2_with_sem(void *arg)
{
  int id = (int)arg;

  for(int rep = 0; rep < REP; ++rep) {
    if(id % 2 == 0){
      xem_wait(&print_sem);
      for(int i = 0; i < 10000; ++i)
        data[i] = i + 1;
      xem_unlock(&print_sem);
    }else{
      xem_wait(&print_sem);
      for(int i = 0; i < 10000; ++i)
        data[i] = 10000 - i;
      xem_unlock(&print_sem);
    }
  }

  thread_exit(0);
  return 0;
}

void *
reader2_with_rwlock(void *arg)
{
  int sum;

  for(int rep = 0; rep < REP; ++rep) {
    sum = 0;  
    rwlock_acquire_readlock(&rwlock);
    for(int i = 0; i < 10000; ++i)
      sum += data[i];
    if(!(sum == 0 || sum == 5000 * 10001))
      printf(1, "bad\n");
    rwlock_release_readlock(&rwlock);
  }

  thread_exit(0);
  return 0;
}

void *
writer2_with_rwlock(void *arg)
{
  int id = (int)arg;

  for(int rep = 0; rep < REP; ++rep) {
    if(id % 2 == 0){
      rwlock_acquire_writelock(&rwlock);
      for(int i = 0; i < 10000; ++i)
        data[i] = i + 1;
      rwlock_release_writelock(&rwlock);
    }else{
      rwlock_acquire_writelock(&rwlock);
      for(int i = 0; i < 10000; ++i)
        data[i] = 10000 - i;
      rwlock_release_writelock(&rwlock);
    }
  }

  thread_exit(0);
  return 0;
}