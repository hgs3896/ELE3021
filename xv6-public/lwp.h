#ifndef __LWP_H__
#define __LWP_H__

#include "types.h"
#include "spinlock.h"

enum lwpstate {
  LWP_UNUSED,
  LWP_EMBRYO,
  LWP_SLEEPING,
  LWP_RUNNABLE,
  LWP_RUNNING,
  LWP_ZOMBIE,
};

struct proc;
struct lwp {
  thread_t tid;            // LWP Identifier
  thread_t ptid;           // Parent LWP ID
  uint stack_sz;           // Size of stack memory (bytes)
  enum lwpstate state;     // LWP State
  char *kstack;            // Bottom of kernel stack for this lwp
  struct trapframe *tf;    // Pointer to its trapframe
  struct context *context; // Pointer to its context
  void *chan;              // Channel to fall a sleep
  void *ret_val;           // Return value
};

/*
 * Wrapper functions for system calls
 */
int sys_thread_create(void);
int sys_thread_exit(void);
int sys_thread_join(void);

/*
 * int thread_create(
 *   thread_t *thread,               // a pointer to thread ID
 *   void *(*start_routine)(void *), // a pointer to the entry point of a lwp
 *   void *arg                       // an argument to pass
 * )
 * creates a LWP by allocating resources for a new light-weight process(thread) at the current process
 */
int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg);

/*
 * int thread_join(
 *   thread_t thread, // a thread ID to join
 *   void **ret_val   // a pointer to return value from callee thread
 *  )
 * 
 */
int thread_join(thread_t thread, void **ret_val);

/*
 * void thread_exit(
 *   void *ret_val   // return value to pass to caller thread
 * )
 * 
 */
void thread_exit(void *ret_val);

/*
 * struct lwp* alloclwp()
 * returns a pointer to an allocated lwp struct otherwise -1
 */
struct lwp *alloclwp(void);

/*
 * void dealloclwp()
 * deallocates an unused lwp struct
 */
void dealloclwp(struct lwp *unused);

/*
 * void swtchlwp(void)
 * is same as swtchlwp1(void), but it holds ptable.lock
 */
void swtchlwp(struct lwp **p_lwp);

/*
 * void swtchlwp1(void)
 * switches the kernel stack of the current lwp
 */
void swtchlwp1(struct lwp **p_lwp);

/*
 * struct lwp **get_runnable_lwp(struct proc *p)
 * fetch a pointer to a runnable lwp pointer in the given process(p)
 * otherwise, return 0
 */
struct lwp **get_runnable_lwp(struct proc *p);

/*
 * void print_lwps(const struct proc *p)
 * print all available lwps in the given process(p)
 */
void print_lwps(const struct proc *p);

/*
 * int copy_lwp(struct lwp *dst, const struct lwp *src)
 * copy all contents of src lwp to those of dst lwp
 */
int copy_lwp(struct lwp *dst, const struct lwp *src);

#endif