#ifndef CVAR_H
#define CVAR_H
#define MAX_CVARS 10
#include "util/queue.h"

typedef struct cvar {
  int valid;
  process_queue_t wait_queue;
} cvar_t;

extern cvar_t cvar_table[MAX_CVARS];

int HandleCvarInit(int *cvar_id);
int HandleCvarWait(int cvar_id, int lock_id);
int HandleCvarSignal(int cvar_id);
int HandleCvarBroadcast(int cvar_id);
int HandleCvarReclaim(int cvar_id);

#endif
