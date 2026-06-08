#ifndef PIPE_H
#define PIPE_H
#define MAX_PIPES 10
#include <yalnix.h>
#include "util/queue.h"

typedef struct pipe {
  char buffer[PIPE_BUFFER_LEN];
  int head;
  int tail;
  int bytes;

  process_queue_t read_queue;
  process_queue_t write_queue;

  int valid;
} pipe_t;

extern pipe_t pipe_table[MAX_PIPES];

int HandlePipeInit(int *pipe_id);
int HandlePipeRead(int pipe_id, void *buffer, int len);
int HandlePipeWrite(int pipe_id, void *buffer, int len);

#endif
