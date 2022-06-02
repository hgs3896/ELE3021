#pragma once
#include "semaphore.h"

// Semaphores
struct rwlock_t {
  struct xem_t lk;    // Semaphore for handle rwlock_t
  struct xem_t write; // Semaphore for writer lock
  uint num_readers;   // The number of readers
};