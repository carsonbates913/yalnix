#include "util/lock.h"
#include "kernelstart.h"
#include <hardware.h>
#include <ykernel.h>


static int valid_user_ptr(void *p, int len) {
  unsigned long s = (unsigned long)p, e;
  if (p == NULL || len < 0) return 0;
  e = s + (unsigned long)len;
  if (s < VMEM_1_BASE || e > VMEM_1_LIMIT || e < s) return 0;
  return 1;
}

// Initialize a lock
int HandleLockInit(int *lock_id) {
  if (!valid_user_ptr(lock_id, sizeof(int))) {
    return ERROR;
  }
  for (int i = 0; i < MAX_LOCKS; i++) {
    if (!lock_table[i].valid) {
      lock_table[i].valid = 1;
      lock_table[i].locked = 0;
      lock_table[i].owner = NULL;
      queue_init(&lock_table[i].wait_queue);
      *lock_id = i;
      return 0;
    }
  }
  return ERROR;
}

// Acquire a lock
int HandleLockAcquire(int lock_id) {
  if (lock_id < 0 || lock_id >= MAX_LOCKS || !lock_table[lock_id].valid) {
    return ERROR;
  }

  if (lock_table[lock_id].locked && lock_table[lock_id].owner == current_process) {
    return ERROR;
  }

  TracePrintf(1, "Acquiring lock %d\n", lock_id);

  if (!lock_table[lock_id].locked) {
    lock_table[lock_id].locked = 1;
    lock_table[lock_id].owner = current_process;
    TracePrintf(1, "Acquired lock %d (uncontended)\n", lock_id);
    return 0;
  }

  // Block and wait for a handoff
  enqueue(&lock_table[lock_id].wait_queue, current_process);
  TracePrintf(1, "Enqueued process %d on lock %d\n", current_process->pid, lock_id);
  current_process->state = WAITING;
  ScheduleNextProcess();

  // Handoff complete
  TracePrintf(1, "Process %d received handoff of lock %d\n",
              current_process->pid, lock_id);
  return 0;
}

// Release a lock
int HandleLockRelease(int lock_id) {
  if (lock_id < 0 || lock_id >= MAX_LOCKS || !lock_table[lock_id].valid) {
    TracePrintf(1, "Lock %d is not valid\n", lock_id);
    return ERROR;
  }
  if (!lock_table[lock_id].locked) {
    return ERROR;
  }
  if (lock_table[lock_id].owner != current_process) {
    return ERROR;   // only the owner may release
  }

  TracePrintf(1, "Releasing lock %d\n", lock_id);

  if (!queue_is_empty(&lock_table[lock_id].wait_queue)) {
    // Direct handoff to the next waiter
    pcb_t *next = dequeue(&lock_table[lock_id].wait_queue);
    next->state = READY;
    lock_table[lock_id].owner = next;
    lock_table[lock_id].locked = 1;
    enqueue(ready_queue, next);
  } else {
    lock_table[lock_id].owner = NULL;
    lock_table[lock_id].locked = 0;
  }

  return 0;
}