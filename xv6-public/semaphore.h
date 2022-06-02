#pragma once
#include "spinlock.h"

// Semaphores
struct xem_t {
  int lock_caps;     // Capacity of this semaphore
  struct spinlock lk; // spinlock protecting this semaphore

  // For debugging:
  char *name; // Name of lock.
  int pid;    // Process holding lock
};