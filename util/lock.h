#ifndef LOCK_H
#define LOCK_H
#define MAX_LOCKS 10
#include "util/queue.h"

typedef struct lock {
  int valid;
  int locked;
  pcb_t *owner;
  process_queue_t wait_queue;
} lock_t;

extern lock_t lock_table[MAX_LOCKS];

int HandleLockInit(int *lock_id);
int HandleLockAcquire(int lock_id);
int HandleLockRelease(int lock_id);

#endif
