#ifndef __SLEEPLOCK_H
#define __SLEEPLOCK_H

#include "type.h"
#include "spinlock.h"

struct spinlock;

// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

#endif
