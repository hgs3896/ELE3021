#pragma once
#include "semaphore.h"

// Semaphores
struct rwlock_t {
  struct spinlock lk; // Spinlock for protecting rwlock_t
  struct xem_t write; // Semaphore for writer lock
  uint num_readers;   // The number of readers
  uint num_writers;   // The number of writers
};