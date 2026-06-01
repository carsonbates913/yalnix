#include <kernelstart.h>
#include <hardware.h>
#include <string.h>
#include <yuser.h>
#include <yalnix.h>


int HandleTtyRead(int tty_iid, void* buffer, int len){
  tty_t *terminal = &terminal_table[tty_iid];

  if (terminal->line_buffer_head == NULL){
    terminal->read_queue.head = current_process;
    terminal->read_queue.tail = current_process;
    current_process->state = BLOCKED;
  }

  line_buffer_t *line_buffer = terminal->line_buffer_head;

  int read_len;
  if (line_buffer->length < len){
    read_len = line_buffer->length;
  } else {
    read_len = len;
  }

  memcpy(buffer, line_buffer->buffer, read_len);

  free(line_buffer);
  return read_len;
}

int HandleTtyWrite(int tty_id, void *buffer, int len) {
  tty_t *term = &terminal_table[tty_id];

  char *kbuf = (char *)malloc(len);
  memcpy(kbuf, buffer, len);

  if (term->is_transmitting) {
      enqueue(&term->write_queue, current_process);

      pcb_t *next = dequeue(ready_queue);
      if (next == NULL) next = idle_pcb;
      pcb_t *old = current_process;
      current_process = next;
      KernelContextSwitch(KCSwitch, old, next);

  }

  int chunk = (len < TERMINAL_MAX_LINE) ? len : TERMINAL_MAX_LINE;
  TtyTransmit(tty_id, kbuf, chunk);

  term->write_buffer = kbuf;
  term->total_write_len = len;
  term->write_progress = chunk;
  term->is_transmitting = 1;
  term->current_write_process = current_process;

  pcb_t *next = dequeue(ready_queue);
  if (next == NULL) next = idle_pcb;
  pcb_t *old = current_process;
  current_process = next;
  KernelContextSwitch(KCSwitch, old, next);
  return len;
}

int HandleDefault(UserContext* user_context){
  return 0;
}

int HandleFork(UserContext* user_context){
  return 0;
}

int HandleExec(UserContext* user_context){
  return 0;
}

int HandleWait(UserContext* user_context){
  return 0;
}

void HandleSyscall(UserContext* user_context){
  int syscall_number = user_context->vector;
  switch(syscall_number){
    /*case YALNIX_FORK:
      return HandleFork(user_context);
    case YALNIX_EXEC:
      return HandleExec(user_context);
    case YALNIX_WAIT:
      return HandleWait(user_context);
    case YALNIX_EXIT:
      return HandleExit(user_context);
    case YALNIX_BRK:
      return HandleBrk(user_context);
    case YALNIX_DELAY:
      return HandleDelay(user_context);*/
    case YALNIX_TTY_READ:
      user_context->regs[0] = HandleTtyRead((int)user_context->regs[0], (void*)user_context->regs[1], (int)user_context->regs[2]);
      return;
    case YALNIX_TTY_WRITE:
      user_context->regs[0] = HandleTtyWrite((int)user_context->regs[0], (void*)user_context->regs[1], (int)user_context->regs[2]);
      return;
    /*default:
      return HandleDefault(user_context);*/
  }
}