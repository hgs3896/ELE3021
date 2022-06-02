#pragma once
#include "spinlock.h"

// Semapores
struct xem_t {
  int lock_caps;     // Capacity of this semapore
  struct spinlock lk; // spinlock protecting this semapore

  // For debugging:
  char *name; // Name of lock.
  int pid;    // Process holding lock
};