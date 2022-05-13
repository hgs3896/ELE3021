#include "types.h"

#ifndef __LWP_H__
#define __LWP_H__
typedef uint thread_t;
enum lwpstate { UNUSED = 0, SLEEPING = 2, RUNNABLE = 3, RUNNING = 4, ZOMBIE = 5 };
struct lwp{
  thread_t tid;             // LWP Identifier
  enum lwpstate state;      // LWP State
  char *kstack;             // Bottom of kernel stack for this lwp
  char *ustack;             // Top of user stack for this lwp
  struct trapframe *tf;     // Pointer to its trapframe
  struct context *context;  // Pointer to its context
  void *chan;               // Channel to get sleep
};

int
thread_create(
    thread_t *thread,
    void *(*start_routine)(void *),
    void *arg
);

int
thread_join(
    thread_t thread,
    void **ret_val
);

void
thread_exit(
    void *ret_val
);

#endif