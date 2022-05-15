#ifndef __LWP_H__
#define __LWP_H__

#include "types.h"

enum lwpstate { LWP_UNUSED = 0, LWP_EMBRYO = 1, LWP_SLEEPING = 2, LWP_RUNNABLE = 3, LWP_RUNNING = 4, LWP_ZOMBIE = 5 };
struct lwp{
  thread_t tid;             // LWP Identifier
  uint stack_sz;            // Size of stack memory (bytes)
  enum lwpstate state;      // LWP State
  char *kstack;             // Bottom of kernel stack for this lwp
  char *ustack;             // Top of user stack for this lwp
  struct trapframe *tf;     // Pointer to its trapframe
  struct context *context;  // Pointer to its context
  void *chan;               // Channel to get sleep
};

struct lwp* alloclwp(void);
void dealloclwp(struct lwp* unused);
void swtchlwp(void);

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

int sys_thread_create(void);
int sys_thread_exit(void);
int sys_thread_join(void);

#endif