#include "types.h"
#include "user.h"
#include "types.h"
#include "fcntl.h"

#define NUM_THREADS (60)
#define READER_CNT  (55)
#define WRITER_CNT  (NUM_THREADS - READER_CNT)
#define NUM_BLK     (250)

thread_safe_guard *f;
thread_t tid[NUM_THREADS];

void *reader(void *);
void *writer(void *);

int
main(int argc, char *argv[])
{
  char buf[512];
  int fd;
  void *ret;

  if((fd = open("test_cio.txt", O_CREATE | O_RDWR)) < 0) {
    printf(1, "Failed to create file\n");
    exit();
  }

  if((f = thread_safe_guard_init(fd)) == 0) {
    printf(1, "Failed to initialize thread safe guard\n");
    exit();
  }

  // Fill with 0
  memset(buf, '0', sizeof buf);
  for(int i = 0; i < NUM_BLK; ++i){
    write(fd, buf, sizeof buf);
  }

  for(int i = 0; i < NUM_THREADS; ++i) {
    if(thread_create(&tid[i], i < WRITER_CNT ? writer : reader, (void *)i) <
       0) {
      printf(1, "Failed to create a thread\n");
      exit();
    }
  }
  for(int i = 0; i < NUM_THREADS; ++i) {
    if(thread_join(tid[i], &ret) < 0) {
      printf(1, "Failed to join a thread\n");
      exit();
    }
  }

  thread_safe_guard_destroy(f);

  printf(1, "Concurrent Read/Write Test Succeed\n");

  close(fd);
  exit();
}

void *
reader(void *arg)
{
  char buf[512];
  int id = (int)arg;

  const int step = NUM_BLK / READER_CNT;

  for(int i = 0; i < NUM_BLK; i += step) {
    for(int j = i; j < i + step; ++j) {
      thread_safe_pread(f, buf, sizeof buf, 512 * j);
      for(int k = 1;k < sizeof buf; ++k){
        if(buf[k-1] != buf[k]){
          printf(1, "Data Race Deteced %d\n", id);
          exit();
        }
      }
    }
  }

  thread_exit((void*)0);
  return 0;
}

void *
writer(void *arg)
{
  char buf[512];
  int id = (int)arg;

  memset(buf, 'A' + (id % 26), sizeof buf);

  const int step = NUM_BLK / WRITER_CNT;
  const int start = step * id;
  int end = step * (id + 1);
  if(end > NUM_BLK)
    end = NUM_BLK;

  for(int i = start; i < end; i++) {
    thread_safe_pwrite(f, buf, sizeof buf, 512 * i);
    sleep(5 * (1 + id));
  }

  thread_exit((void*)0);
  return 0;
}