#include "util/cvar.h"
#include "util/lock.h"
#include "kernelstart.h"
#include <ykernel.h>

int HandleCvarInit(int *cvar_id){
  // initialize a new cvar
  for(int i = 0; i < MAX_CVARS; i++){
    if(!cvar_table[i].valid){
      cvar_table[i].valid = 1;
      queue_init(&cvar_table[i].wait_queue);
      *cvar_id = i;
      return 0;
    }
  }
  return ERROR;
}

int HandleCvarWait(int cvar_id, int lock_id){
  TracePrintf(1, "Waiting on cvar %d with lock %d\n", cvar_id, lock_id);
  if(cvar_id < 0 || cvar_id >= MAX_CVARS || !cvar_table[cvar_id].valid){
    return ERROR;
  }
  if(lock_id < 0 || lock_id >= MAX_LOCKS || !lock_table[lock_id].valid){
    return ERROR;
  }

  if(lock_table[lock_id].owner != current_process){
    return ERROR;
  }

  TracePrintf(1, "Releasing lock %d\n", lock_id);
  HandleLockRelease(lock_id);
  enqueue(&cvar_table[cvar_id].wait_queue, current_process);
  current_process->state = WAITING;
  ScheduleNextProcess();
  HandleLockAcquire(lock_id);

  return 0;
}

int HandleCvarSignal(int cvar_id){
  TracePrintf(1, "Signaling cvar %d\n", cvar_id);
  if(cvar_id < 0 || cvar_id >= MAX_CVARS || !cvar_table[cvar_id].valid){
    return ERROR;
  }

  if (queue_is_empty(&cvar_table[cvar_id].wait_queue)) {
    return 0;
  }

  pcb_t* next = dequeue(&cvar_table[cvar_id].wait_queue);
  next->state = READY;
  enqueue(ready_queue, next);

  return 0;
}

int HandleCvarBroadcast(int cvar_id){
  TracePrintf(1, "Broadcasting cvar %d\n", cvar_id);
  if(cvar_id < 0 || cvar_id >= MAX_CVARS || !cvar_table[cvar_id].valid){
    TracePrintf(1, "Cvar %d is not valid\n", cvar_id);
    return ERROR;
  }
  while(!queue_is_empty(&cvar_table[cvar_id].wait_queue)){
    TracePrintf(1, "Broadcasting cvar %d\n", cvar_id);
    pcb_t* next = dequeue(&cvar_table[cvar_id].wait_queue);
    next->state = READY;
    enqueue(ready_queue, next);
  }
  return 0;
}

int HandleCvarReclaim(int cvar_id){
  if(cvar_id < 0 || cvar_id >= MAX_CVARS || !cvar_table[cvar_id].valid){
    TracePrintf(1, "Cvar %d is not valid\n", cvar_id);
    return ERROR;
  }

  if(!queue_is_empty(&cvar_table[cvar_id].wait_queue)){
    TracePrintf(1, "Cvar %d has waiting processes\n", cvar_id);
    return ERROR;
  }

  cvar_table[cvar_id].valid = 0;
  return 0;
}
