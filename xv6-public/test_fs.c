#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define KB *1024
#define MB *1024 * 1024
#define MAX_FDS 10

char buf[8 KB];

const int stdout = 1;
int fds[MAX_FDS];
int fd_idx;

static int
strcmpn(const char *p, const char *q, int sz)
{
  while(*p && *p == *q && sz)
    p++, q++, sz--;
  return sz ? (uchar)*p - (uchar)*q : 0;
}

void
closeAllFDs(void)
{
  int i;
  for(i = 0; i < fd_idx; ++i)
    close(fds[i]);
  fd_idx = 0;
}

void
createTestFile(uint filesize)
{
  int i;
  int idx = fd_idx++;

  char filename[] = "testFile .txt";
  filename[8] = (char)('0' + idx);

  printf(stdout, "Create Test %d Started!\n", idx);

  if((fds[idx] = open(filename, O_CREATE | O_RDWR)) < 0) {
    printf(stdout, "error: creat file failed!\n");
    exit();
  }

  const uint rep = filesize / 32;
  for(i = 0; i < rep; i++) {
    if(write(fds[idx], "aaaaaaaaaaaaaaa\n", 16) != 16) {
      printf(stdout, "error: write aa %d new file failed\n", i);
      exit();
    }
    if(write(fds[idx], "bbbbbbbbbbbbbbb\n", 16) != 16) {
      printf(stdout, "error: write bb %d new file failed\n", i);
      exit();
    }
  }

  printf(stdout, "Create Test %d Finished!\n", idx);
}

void
readTestFile(int idx)
{
  int i;
  char filename[] = "testFile .txt";
  filename[8] = (char)('0' + idx);

  printf(stdout, "Read Test %d Started!\n", idx);

  if((fds[idx] = open(filename, O_RDONLY)) < 0) {
    printf(stdout, "error: open file failed!\n");
    exit();
  }

  while((i = read(fds[idx], buf, sizeof(buf)))) {
    for(int j = 0; j < i; j += 32) {
      if(strcmpn("aaaaaaaaaaaaaaa\nbbbbbbbbbbbbbbb\n", &buf[j], 16)) {
        printf(stdout, "error: incorrect value detected at %d!!\n", j);
        printf(stdout, &buf[j]);
        return;
      }
    }
  }

  printf(stdout, "Read Test %d Finished!\n", idx);
}

void
removeTestFile(int idx)
{
  char filename[] = "testFile .txt";
  filename[8] = (char)('0' + idx);
  if(unlink(filename) < 0) {
    printf(1, "Failed to remove %s\n", filename);
    exit();
  }
}

void
create_test(void)
{
  createTestFile(1 KB);
  createTestFile(4 KB);
  createTestFile(15 KB);
  createTestFile(1 MB);
  createTestFile(16 MB);

  closeAllFDs();
}

void
read_test(void)
{
  readTestFile(0);
  readTestFile(1);
  readTestFile(2);
  readTestFile(3);
  readTestFile(4);

  closeAllFDs();

  removeTestFile(0);
  removeTestFile(1);
  removeTestFile(2);
  removeTestFile(3);
  removeTestFile(4);
}

void
stress_test(void)
{
  for(int i = 0; i < 4; ++i) {
    printf(stdout, "Stress Test %d Started\n", i);
    createTestFile(16 MB);
    closeAllFDs();
    removeTestFile(0);
    printf(stdout, "Stress Test %d Finished\n", i);
  }
}

int
main(int argc, char *argv[])
{
  set_cpu_share(80);

  create_test();

  read_test();

  stress_test();

  exit();
}