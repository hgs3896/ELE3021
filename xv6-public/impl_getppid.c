#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

// Simple system call
int
getppid(void)
{
    struct proc* parent = myproc()->parent;
    if(!parent)
        panic("Parent proc instance is null.");
    return parent->pid;
}

// Wrapper for getppid
int
sys_getppid(void)
{
    return getppid();
}