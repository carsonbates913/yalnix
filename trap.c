#include <kernelstart.h>
#include <hardware.h>
#include <string.h>
#include <yuser.h>
#include <yalnix.h>
#include <stdlib.h>
#include <math.h>

void HandleTrapKernel(UserContext* user_context){
  TracePrintf(1, "Kernel trap\n");
}
void HandleTrapClock(UserContext* user_context){
  TracePrintf(1, "Clock trap\n");
  memcpy(&current_process->user_context, user_context, sizeof(UserContext));

  if(current_process != idle_pcb){
    current_process->next = NULL;
    if(ready_queue->tail == NULL){
      ready_queue->head = current_process;
      ready_queue->tail = current_process;
    }else{
      ready_queue->tail->next = current_process;
      ready_queue->tail = current_process;
    }
  }

  pcb_t *next_process;

  if(ready_queue->head != NULL){
    next_process = ready_queue->head;
    ready_queue->head = next_process->next;
    if(ready_queue->head == NULL){
      ready_queue->tail = NULL;
    }
  }else{
    next_process = idle_pcb;
  }

  next_process->state = RUNNING;
  if(next_process != current_process){
    KernelContextSwitch(KCSwitch, current_process, next_process);
    current_process = next_process;
  }


  *user_context = current_process->user_context;
}

void HandleTrapIllegal(UserContext* user_context){
  TracePrintf(1, "Illegal trap\n");
}
void HandleTrapMemory(UserContext* user_context){
  TracePrintf(1, "Memory trap\n");
}
void HandleTrapMath(UserContext* user_context){
  TracePrintf(1, "Math trap\n");
}

void HandleTrapTtyReceive(UserContext *user_context) {
  TracePrintf(1, "TTY receive trap\n");
  int tty_id = user_context->code;
  tty_t *terminal = &terminal_table[tty_id];

  line_buffer_t *line_buffer = malloc(sizeof(line_buffer_t));
  line_buffer->length = TtyReceive(tty_id, line_buffer->buffer, TERMINAL_MAX_LINE);
  line_buffer->next = NULL;

  if (terminal->line_buffer_head == NULL) {
      terminal->line_buffer_head = line_buffer;
      terminal->line_buffer_tail = line_buffer;
  } else {
      terminal->line_buffer_tail->next = line_buffer;
      terminal->line_buffer_tail = line_buffer;
  }

  if (!queue_is_empty(&terminal->read_queue)) {
      pcb_t *reader = dequeue(&terminal->read_queue);
      reader->state = READY;
      enqueue(ready_queue, reader);
  }
}

void HandleTrapTtyTransmit(UserContext *user_context) {
  TracePrintf(1, "TTY transmit trap\n");

  int tty_id = user_context->code;
  tty_t *terminal = &terminal_table[tty_id];

  if (terminal->write_progress < terminal->total_write_len) {
      int remaining = terminal->total_write_len - terminal->write_progress;
      int chunk = (remaining < TERMINAL_MAX_LINE) ? remaining : TERMINAL_MAX_LINE;
      TtyTransmit(tty_id, terminal->write_buffer + terminal->write_progress, chunk);
      terminal->write_progress += chunk;
  } else {
      free(terminal->write_buffer);
      terminal->is_transmitting = 0;

      enqueue(&ready_queue, terminal->current_write_process);
      terminal->current_write_process = NULL;

      if (!queue_is_empty(&terminal->write_wait_queue)) {
          pcb_t *next_writer = dequeue(&terminal->write_wait_queue);
          enqueue(&ready_queue, next_writer);
      }
  }
}

void HandleTrapDisk(UserContext* user_context){
  TracePrintf(1, "Disk trap\n");
}
void HandleTrapUnknown(UserContext* user_context){
  TracePrintf(1, "Unknown trap\n");
}

void HandleTrap(UserContext* user_context){
  switch(user_context->vector){
    case TRAP_KERNEL:
      HandleTrapKernel(user_context);
      break;
    case TRAP_CLOCK:
      HandleTrapClock(user_context);
      break;
    case TRAP_ILLEGAL:
      HandleTrapIllegal(user_context);
      break;
    case TRAP_MEMORY:
      HandleTrapMemory(user_context);
      break;
    case TRAP_MATH:
      HandleTrapMath(user_context);
      break;
    case TRAP_TTY_RECEIVE:
      HandleTrapTtyReceive(user_context);
      break;
    case TRAP_TTY_TRANSMIT:
      HandleTrapTtyTransmit(user_context);
      break;
    case TRAP_DISK:
      HandleTrapDisk(user_context);
      break;
    default:
      HandleTrapUnknown(user_context);
      break;
  }
}