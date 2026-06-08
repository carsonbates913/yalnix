#include "util/pipe.h"
#include "kernelstart.h"
#include <ykernel.h>
#include <string.h>

static int min_int(int a, int b) {
  return a < b ? a : b;
}

int HandlePipeInit(int *pipe_id){
  // initialize a new pipe
  for(int i = 0; i < MAX_PIPES; i++){
    if(!pipe_table[i].valid){
      pipe_table[i].valid = 1;
      pipe_table[i].head = 0;
      pipe_table[i].tail = 0;
      pipe_table[i].bytes = 0;
      queue_init(&pipe_table[i].read_queue);
      queue_init(&pipe_table[i].write_queue);
      *pipe_id = i;
      return 0;
    }
  }
  return ERROR;
}

int HandlePipeRead(int pipe_id, void *buffer, int len){
  if(pipe_id < 0 || pipe_id >= MAX_PIPES || !pipe_table[pipe_id].valid){
    TracePrintf(1, "Pipe %d is not valid\n", pipe_id);
    return ERROR;
  }
  if(buffer == NULL || len <= 0){
    TracePrintf(1, "Buffer is null or length is less than 0\n");
    return ERROR;
  }

  pipe_t *pipe = &pipe_table[pipe_id];

  // wait for the pipe to have data
  while (pipe->bytes == 0){
    enqueue(&pipe->read_queue, current_process);
    current_process->state = WAITING;
    ScheduleNextProcess();
  }

  // read the data from the pipe
  int bytes_to_read;
  if(len > pipe->bytes){
    bytes_to_read = pipe->bytes;
  }else{
    bytes_to_read = len;
  }
  memcpy(buffer, pipe->buffer + pipe->head, bytes_to_read);
  pipe->head = (pipe->head + bytes_to_read) % PIPE_BUFFER_LEN;
  pipe->bytes -= bytes_to_read;

  if(!queue_is_empty(&pipe->write_queue)){
    pcb_t *writer_process = dequeue(&pipe->write_queue);
    TracePrintf(1, "Waking up process %d\n", writer_process->pid);
    writer_process->state = READY;
    enqueue(ready_queue, writer_process);
  }
  return bytes_to_read;
}

int HandlePipeWrite(int pipe_id, void *buffer, int len){
  if(pipe_id < 0 || pipe_id >= MAX_PIPES || !pipe_table[pipe_id].valid){
    TracePrintf(1, "Pipe %d is not valid\n", pipe_id);
    return ERROR;
  }

  if(buffer == NULL || len <= 0){
    return ERROR;
  }

  pipe_t *pipe = &pipe_table[pipe_id];

  // wait for the pipe to have space
  while (pipe->bytes == PIPE_BUFFER_LEN){
    enqueue(&pipe->write_queue, current_process);
    current_process->state = WAITING;
    ScheduleNextProcess();
  }

  // write the data to the pipe
  int bytes_to_write = min_int(len, PIPE_BUFFER_LEN - pipe->bytes);
  memcpy(pipe->buffer + pipe->tail, buffer, bytes_to_write);
  pipe->tail = (pipe->tail + bytes_to_write) % PIPE_BUFFER_LEN;
  pipe->bytes += bytes_to_write;

  if(!queue_is_empty(&pipe->read_queue)){
    pcb_t *reader_process = dequeue(&pipe->read_queue);
    TracePrintf(1, "Waking up process %d\n", reader_process->pid);
    reader_process->state = READY;
    enqueue(ready_queue, reader_process);
  }
  TracePrintf(1, "Pipe %d wrote %d bytes\n", pipe_id, bytes_to_write);
  return bytes_to_write;
}
