#pragma once
#include "lwp.h"
#include "defs.h"
#include "sleeplock.h"

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory(including text, data/bss, heap) (bytes)
  pde_t* pgdir;                // Page table
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  uint cticks;                 // Consumed tick count at the given level of the queue
  uint yield_by :  4;          // the process is yield by stride = 1, mlfq = 2
  uint lev      : 28;          // Level in MLFQ(0~NMLFQ), otherwise Stride Level
  int lwp_idx;                 // Current LWP index
  struct lwp *lwps[NLWPS];     // LWPs
  int lwp_cnt;                 // LWP counter
  struct sleeplock lock;       // Lock object for exec
};

inline struct lwp**
mylwp1(struct proc* p)
{
  return &p->lwps[p->lwp_idx];
}

inline struct lwp*
mylwp(struct proc* p)
{
  return *mylwp1(p);
}

inline uint
stack_base_lwp(struct lwp **p_lwp)
{
  if(p_lwp == 0)
    panic("null pointer exception of lwp");
  return USERTOP - (p_lwp - myproc()->lwps) * NPAGESPERLWP * PGSIZE;
}

inline uint
stack_top_lwp(struct lwp **p_lwp)
{
  uint base = stack_base_lwp(p_lwp);
  return base - (*p_lwp)->stack_sz;
}

inline uint
is_stack_addr(uint addr)
{
  struct proc* curproc = myproc();

  for(struct lwp** p_lwp = curproc->lwps;p_lwp<&curproc->lwps[NLWPS];++p_lwp){
    if(*p_lwp && addr < stack_base_lwp(p_lwp) && addr >= stack_top_lwp(p_lwp)){
      return 1;
    }
  }
  return 0;
}

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
