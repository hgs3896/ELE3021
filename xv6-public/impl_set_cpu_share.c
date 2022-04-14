#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "schedulers.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int set_cpu_share(int share){
    return 0;
}

int sys_set_cpu_share(void){
    int share = 0;
    if(argint(0, &share) < 0)
        return -1;
    if(share > 80 || share <= 0)
        return -1;
    return set_cpu_share(share);
}