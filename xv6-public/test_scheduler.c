/**
 * This program runs various workloads cuncurrently.
 */

#include "types.h"
#include "stat.h"
#include "user.h"

#define LIFETIME (1000)        /* (ticks) */
#define COUNT_PERIOD (1000000) /* (iteration) */

#define MLFQ_LEVEL (3) /* Number of level(priority) of MLFQ scheduler */
#define MAX_NUM_PROCESSES                                                      \
  (10) /* Max number of processes to handle during processes */

/**
 * This function requests portion of CPU resources with given parameter
 * value by calling set_cpu_share() system call.
 * It reports the cnt value which have been accumulated during LIFETIME.
 */
void
test_stride(int portion, int pipe)
{
  int cnt = 0;
  int i = 0;
  int start_tick;
  int curr_tick;

  if(set_cpu_share(portion) != 0) {
    printf(1, "FAIL : set_cpu_share\n");
    return;
  }

  /* Get start tick */
  start_tick = uptime();

  for(;;) {
    i++;
    if(i >= COUNT_PERIOD) {
      cnt++;
      i = 0;

      /* Get current tick */
      curr_tick = uptime();

      if(curr_tick - start_tick > LIFETIME) {
        /* Time to terminate */
        break;
      }
    }
  }

  /* Report */
  printf(1, "STRIDE(%d%%), cnt : %d\n", portion, cnt);
  printf(pipe, "%d\n", cnt);

  return;
}

/**
 * This function request to make this process scheduled in MLFQ.
 * MLFQ_NONE			: report only the cnt value
 * MLFQ_LEVCNT			: report the cnt values about each level
 * MLFQ_YIELD			: yield itself, report only the cnt value
 * MLFQ_YIELD_LEVCNT	: yield itself, report the cnt values about each level
 */
enum { MLFQ_NONE, MLFQ_LEVCNT, MLFQ_YIELD, MLFQ_LEVCNT_YIELD };
void
test_mlfq(int type, int pipe)
{
  int cnt_level[MLFQ_LEVEL] = {0, 0, 0};
  int cnt = 0;
  int i = 0;
  int curr_mlfq_level;
  int start_tick;
  int curr_tick;

  /* Get start tick */
  start_tick = uptime();

  for(;;) {
    i++;
    if(i >= COUNT_PERIOD) {
      cnt++;
      i = 0;

      if(type == MLFQ_LEVCNT || type == MLFQ_LEVCNT_YIELD) {
        /* Count per level */
        curr_mlfq_level = getlev(); /* getlev : system call */
        cnt_level[curr_mlfq_level]++;
      }

      /* Get current tick */
      curr_tick = uptime();

      if(curr_tick - start_tick > LIFETIME) {
        /* Time to terminate */
        break;
      }

      if(type == MLFQ_YIELD || type == MLFQ_LEVCNT_YIELD) {
        /* Yield process itself, not by timer interrupt */
        yield();
      }
    }
  }

  /* Report */
  if(type == MLFQ_LEVCNT || type == MLFQ_LEVCNT_YIELD) {
    printf(1, "MLfQ(%s), cnt : %d, lev[0] : %d, lev[1] : %d, lev[2] : %d\n",
           type == MLFQ_LEVCNT ? "compute" : "yield", cnt, cnt_level[0],
           cnt_level[1], cnt_level[2]);
  } else {
    printf(1, "MLfQ(%s), cnt : %d\n", type == MLFQ_NONE ? "compute" : "yield",
           cnt);
  }

  printf(pipe, "%d\n", cnt);

  return;
}

struct workload {
  void (*func)(int, int);
  int arg;
};

void
dotest(struct workload *workloads)
{
  int pid;
  int i;

  int size = 0;
  for(struct workload *p_workload = workloads;
      p_workload != workloads + MAX_NUM_PROCESSES && p_workload->func;
      ++p_workload) {
    ++size;
  }
  int pipes[MAX_NUM_PROCESSES][2];
  for(i = 0; i < size; ++i) {
    if(pipe(pipes[i]) < 0) {
      printf(1, "pipe failure");
      exit();
    }
  }

  int results[MAX_NUM_PROCESSES];

  for(i = 0; i < size; i++) {
    pid = fork();
    if(pid > 0) {
      close(pipes[i][1]);
      /* Parent */
      continue;
    } else if(pid == 0) {
      /* Child */
      void (*func)(int, int) = workloads[i].func;
      int arg = workloads[i].arg;
      /* Do this workload */
      close(pipes[i][0]);
      func(arg, pipes[i][1]);
      exit();
    } else {
      printf(1, "FAIL : fork\n");
      exit();
    }
  }

  for(i = 0; i < size; i++) {
    wait();
  }

  int total = 0;
  char buf[1000];
  for(i = 0; i < size; ++i) {
    read(pipes[i][0], buf, 999);
    results[i] = atoi(buf);
    total += results[i];
  }

  printf(1, "Total: %d\n", total);

  exit();
}

int
main(int argc, char *argv[])
{
  struct workload workloads[][MAX_NUM_PROCESSES] = {
      {
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
      },
      {
          {test_mlfq, MLFQ_LEVCNT},
          {test_mlfq, MLFQ_LEVCNT},
          {test_mlfq, MLFQ_LEVCNT},
          {test_mlfq, MLFQ_LEVCNT},
          {test_mlfq, MLFQ_LEVCNT},
      },
      {
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_YIELD},
      },
      {
          {test_mlfq, MLFQ_LEVCNT_YIELD},
          {test_mlfq, MLFQ_LEVCNT_YIELD},
          {test_mlfq, MLFQ_LEVCNT_YIELD},
          {test_mlfq, MLFQ_LEVCNT_YIELD},
          {test_mlfq, MLFQ_LEVCNT_YIELD},
      },
      {
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_YIELD},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_YIELD},
      },
      {
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
          {test_stride, 8},
      },
      {
          {test_stride, 1},
          {test_stride, 3},
          {test_stride, 5},
          {test_stride, 7},
          {test_stride, 11},
          {test_stride, 13},
          {test_stride, 17},
          {test_stride, 23},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
      },
      {
          {test_stride, 5},
          {test_stride, 5},
          {test_stride, 5},
          {test_stride, 5},
          {test_stride, 5},
          {test_stride, 10},
          {test_stride, 10},
          {test_stride, 15},
          {test_stride, 20},
          {test_mlfq, MLFQ_NONE},
      },
      {
          {test_stride, 5},
          {test_stride, 5},
          {test_stride, 10},
          {test_stride, 10},
          {test_stride, 20},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
          {test_mlfq, MLFQ_NONE},
      },
      {
          {test_stride, 5},
          {test_mlfq, MLFQ_NONE},
          {test_stride, 5},
          {test_mlfq, MLFQ_NONE},
          {test_stride, 10},
          {test_mlfq, MLFQ_NONE},
          {test_stride, 15},
          {test_mlfq, MLFQ_NONE},
          {test_stride, 45},
          {test_mlfq, MLFQ_NONE},
      },
  };

  if(argc != 2) {
    printf(1, "%d\n", argc);
    printf(1, "test_scheduler num\n");
    exit();
  }

  int test = atoi(argv[1]);

  dotest(workloads[test]);
  exit();
}
