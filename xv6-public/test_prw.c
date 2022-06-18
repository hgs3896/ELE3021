#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define KB *1024
#define TESTFILESIZE (40 KB)
#define BUFSIZE (4 KB)
#define TESTFILEBLOCKS (TESTFILESIZE / BUFSIZE)
#define TESTFILENAME "prw_test.txt"

int fd;
char buf[BUFSIZE];
const int stdout = 1;

void makeTestFile(void);
void test_pread(void);
void test_pwrite(void);

int
main(int argc, char *argv[])
{
  // For fast testing
  set_cpu_share(80);

  makeTestFile();
  test_pread();
  test_pwrite();
  
  unlink(TESTFILENAME);
  exit();
}

void
makeTestFile(void)
{
  if((fd = open(TESTFILENAME, O_CREATE | O_RDWR)) < 0) {
    printf(stdout, "Fail to open file\n");
    exit();
  }

  // Fill the buffer with AB...
  for(int i = 0; i < BUFSIZE; ++i) {
    buf[i] = 'A' + i % 2;
  }

  // Write 40 KiBs
  for(int i = 0; i < TESTFILEBLOCKS; ++i) {
    if(write(fd, buf, BUFSIZE) != BUFSIZE) {
      printf(stdout, "Fail to write file\n");
      exit();
    }
  }

  close(fd);
  memset(buf, 0, BUFSIZE);
}

void
test_pread(void)
{
  printf(stdout, "Start to test pread\n");

  if((fd = open(TESTFILENAME, O_RDONLY)) < 0) {
    printf(stdout, "Fail to open file\n");
    exit();
  }

  for(int i = TESTFILEBLOCKS - 1; i >= 0; --i) {
    printf(stdout, "Checking pread (%d/10) ...\n", TESTFILEBLOCKS - i);
    if(pread(fd, buf, BUFSIZE, BUFSIZE * i) < 0) {
      printf(stdout, "Fail to read file at 0x%x\n", BUFSIZE * i);
      exit();
    }
    for(int j = 0; j < BUFSIZE; ++j) {
      if(buf[j] != 'A' + j % 2) {
        printf(stdout, "value is different\n");
        exit();
      }
    }
  }

  close(fd);
  memset(buf, 0, BUFSIZE);

  printf(stdout, "pread test succeeded\n");
}

void
test_pwrite(void)
{
  printf(stdout, "Start to test pwrite\n");

  // Fill the buffer with CD...
  for(int i = 0; i < BUFSIZE / 2; ++i) {
    buf[i] = 'C' + i % 2;
  }

  if((fd = open(TESTFILENAME, O_WRONLY)) < 0) {
    printf(stdout, "Fail to open file\n");
    exit();
  }

  for(int i = TESTFILEBLOCKS - 1; i >= 0; --i) {
    printf(stdout, "Testing pwrite (%d/10) ...\n", TESTFILEBLOCKS - i);
    if(pwrite(fd, buf, BUFSIZE / 2, BUFSIZE * i + BUFSIZE / 2) < 0) {
      printf(stdout, "Fail to read file at 0x%x\n", BUFSIZE * i + BUFSIZE / 2);
      exit();
    }
  }

  close(fd);
  memset(buf, 0, BUFSIZE);

  if((fd = open(TESTFILENAME, O_RDONLY)) < 0) {
    printf(stdout, "Fail to open file\n");
    exit();
  }

  for(int i = 0; i < TESTFILEBLOCKS; ++i) {
    if(read(fd, buf, BUFSIZE) != BUFSIZE) {
      printf(stdout, "Fail to read file at 0x%x\n", BUFSIZE * i);
      exit();
    }
    for(int j = 0; j < BUFSIZE / 2; ++j) {
      if(buf[j] != 'A' + j % 2) {
        printf(stdout, "value is different\n");
        exit();
      }
    }
    for(int j = BUFSIZE / 2; j < BUFSIZE; ++j) {
      if(buf[j] != 'C' + j % 2) {
        printf(stdout, "value is different\n");
        exit();
      }
    }
  }

  close(fd);
  memset(buf, 0, BUFSIZE);

  printf(stdout, "pwrite test succeeded\n");
}