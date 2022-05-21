#pragma once
#define NPROC        64  // maximum number of processes
#define NLWPS        64  // maximum number of light-weight processes
#define KSTACKSIZE 4096  // size of per-lwp kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE      256  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks
#define USERTOP      0x7fffe000 // top of user stack
#define NPAGESPERLWP 6 // maximum number of pages that a lwp can use
