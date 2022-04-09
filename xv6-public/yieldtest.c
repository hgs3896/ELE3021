#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int i, pid;
  if((pid = fork()) < 0) {
    exit();
  }

  if(pid == 0) {
    for(i = 0; i < 10; ++i) {
      yield();
      printf(1, "Child\n");
    }
  } else {
    for(i = 0; i < 10; ++i) {
      yield();
      printf(1, "Parent\n");
    }
    wait();
  }

  exit();
}