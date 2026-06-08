#include "util/trap.h"
#include <hardware.h>
#include <string.h>
#include <yalnix.h>
#include <ykernel.h>
#include <stdlib.h>

static int growUserStack(UserContext *user_context) {
  unsigned int fault = (unsigned int)(unsigned long)user_context->addr;

  //check if the fault is in region 1
  if (fault < VMEM_1_BASE || fault >= VMEM_1_LIMIT) {
    return 0;
  }

  pte_t *pt = current_process->region1_page_table;
  unsigned int fault_page = (fault - VMEM_1_BASE) >> PAGESHIFT;

  //find the bottom of the stack
  int stack_bottom = -1;
  for (int i = (int)num_pages_per_region - 1; i >= 0; i--) {
    if (pt[i].valid) {
      stack_bottom = i;
    } else {
      break;
    }
  }
  if (stack_bottom < 0) {
    return 0;   //no stack mapped -- should not happen
  }

  //check if the fault is below the stack bottom
  if (fault_page >= (unsigned int)stack_bottom) {
    return 0;
  }

  //find the top of the heap/data region
  int heap_top = -1;
  for (int i = stack_bottom - 1; i >= 0; i--) {
    if (pt[i].valid) {
      heap_top = i;
      break;
    }
  }

  //check if the fault is below the heap top
  if (heap_top >= 0 && fault_page <= (unsigned int)(heap_top + 1)) {
    return 0;
  }

  for (unsigned int p = fault_page; p < (unsigned int)stack_bottom; p++) {
    int pfn = find_frame();
    if (pfn == ERROR) {
      //out of memory: roll back the pages we just added so we neither leak frames nor leave a half-grown stack.
      for (unsigned int q = fault_page; q < p; q++) {
        free_frame(pt[q].pfn);
        pt[q].valid = 0;
        pt[q].prot = PROT_NONE;
        pt[q].pfn = 0;
      }
      WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
      return 0;
    }
    pt[p].pfn = pfn;
    pt[p].valid = 1;
    pt[p].prot = PROT_READ | PROT_WRITE;
  }

  //flush all of region 1
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
  return 1;
}

void HandleTrapKernel(UserContext *user_context) {
  TracePrintf(1, "Kernel trap (code 0x%x)\n", user_context->code);
  memcpy(&current_process->user_context, user_context, sizeof(UserContext));
  HandleSyscall(&current_process->user_context);
  //HandleSyscall may have switched processes; copy back whatever is current.
  memcpy(user_context, &current_process->user_context, sizeof(UserContext));
}

static void wakeDelayedProcesses(void) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    pcb_t *pcb = process_table[i];
    if (pcb == NULL || pcb->state != WAITING || pcb->time <= 0) {
      continue;
    }
    pcb->time--;
    if (pcb->time == 0) {
      pcb->state = READY;
      enqueue(ready_queue, pcb);
    }
  }
}

void HandleTrapClock(UserContext *user_context) {
  TracePrintf(1, "Clock trap\n");
  memcpy(&current_process->user_context, user_context, sizeof(UserContext));
  wakeDelayedProcesses();
  ScheduleNextProcess();
  memcpy(user_context, &current_process->user_context, sizeof(UserContext));
}

void HandleTrapIllegal(UserContext *user_context) {
  TracePrintf(0, "Illegal instruction: pid %d pc %p\n",
              current_process->pid, user_context->pc);
  memcpy(&current_process->user_context, user_context, sizeof(UserContext));
  HandleExit(KILL);
  memcpy(user_context, &current_process->user_context, sizeof(UserContext));
}

void HandleTrapMemory(UserContext *user_context) {
  TracePrintf(1, "Memory trap at %p (code 0x%x)\n",
              user_context->addr, user_context->code);

  if (growUserStack(user_context)) {
    return;   //fixed up; return and let the instruction retry
  }

  TracePrintf(0, "Fatal memory fault: pid %d addr %p pc %p\n",
              current_process->pid, user_context->addr, user_context->pc);
  memcpy(&current_process->user_context, user_context, sizeof(UserContext));
  HandleExit(KILL);
  memcpy(user_context, &current_process->user_context, sizeof(UserContext));
}

void HandleTrapMath(UserContext *user_context) {
  //a math fault must kill the process.
  TracePrintf(0, "Math fault: pid %d pc %p\n",
              current_process->pid, user_context->pc);
  memcpy(&current_process->user_context, user_context, sizeof(UserContext));
  HandleExit(KILL);
  memcpy(user_context, &current_process->user_context, sizeof(UserContext));
}

void HandleTrapTtyReceive(UserContext *user_context) {
  //receive a tty input
  int tty_id = user_context->code;
  TracePrintf(1, "TTY receive trap (tty %d)\n", tty_id);

  if (tty_id < 0 || tty_id >= NUM_TERMINALS) {
    TracePrintf(0, "HandleTrapTtyReceive: bad tty id %d\n", tty_id);
    return;
  }
  tty_t *terminal = &terminal_table[tty_id];

  //allocate a new line buffer
  line_buffer_t *line_buffer = malloc(sizeof(line_buffer_t));
  if (line_buffer == NULL) {
    
    //out of memory, drop the input
    char tmp[TERMINAL_MAX_LINE];
    TtyReceive(tty_id, tmp, TERMINAL_MAX_LINE);
    TracePrintf(0, "HandleTrapTtyReceive: out of memory, dropped input\n");
    return;
  }

  //receive the input
  line_buffer->length = TtyReceive(tty_id, line_buffer->buffer, TERMINAL_MAX_LINE);
  line_buffer->next = NULL;

  //set the line buffer
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
  int tty_id = user_context->code;
  TracePrintf(1, "TTY transmit trap (tty %d)\n", tty_id);

  if (tty_id < 0 || tty_id >= NUM_TERMINALS) {
    TracePrintf(0, "HandleTrapTtyTransmit: bad tty id %d\n", tty_id);
    return;
  }
  tty_t *terminal = &terminal_table[tty_id];

  //check if the current write process is null and if the terminal is not transmitting
  if (terminal->current_write_process == NULL && !terminal->is_transmitting) {
    TracePrintf(1, "HandleTrapTtyTransmit: spurious transmit on tty %d\n", tty_id);
    return;
  }

  if (terminal->write_progress < terminal->total_write_len) {
    //more of the current buffer to send
    int remaining = terminal->total_write_len - terminal->write_progress;
    int chunk = (remaining < TERMINAL_MAX_LINE) ? remaining : TERMINAL_MAX_LINE;
    TtyTransmit(tty_id, terminal->write_buffer + terminal->write_progress, chunk);
    terminal->write_progress += chunk;
    return;
  }

  //current write finished: free its buffer and wake the writer
  free(terminal->write_buffer);
  terminal->write_buffer = NULL;
  terminal->is_transmitting = 0;

  if (terminal->current_write_process != NULL) {
    terminal->current_write_process->state = READY;
    enqueue(ready_queue, terminal->current_write_process);
    terminal->current_write_process = NULL;
  }

  //start the next queued writer
  if (!queue_is_empty(&terminal->write_queue)) {
    pcb_t *next_writer = dequeue(&terminal->write_queue);

    terminal->write_buffer = next_writer->tty_write_buffer;
    terminal->total_write_len = next_writer->tty_write_len;
    terminal->write_progress = 0;
    terminal->is_transmitting = 1;
    terminal->current_write_process = next_writer;

    int chunk = (terminal->total_write_len < TERMINAL_MAX_LINE) ? terminal->total_write_len : TERMINAL_MAX_LINE;
    TtyTransmit(tty_id, terminal->write_buffer, chunk);
    terminal->write_progress = chunk;
  }
}

void HandleTrapDisk(UserContext *user_context) {
  TracePrintf(1, "Disk trap\n");
  // not doing this :)))))
}

void HandleTrapUnknown(UserContext *user_context) {
  //not sure what to do here
  TracePrintf(0, "Unknown trap: vector %d pid %d pc %p\n", user_context->vector, current_process->pid, user_context->pc); (void)user_context;
}