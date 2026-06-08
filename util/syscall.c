#include "kernelstart.h"
#include "util/pipe.h"
#include "util/lock.h"
#include "util/cvar.h"
#include <hardware.h>
#include <string.h>
#include <yuser.h>
#include <yalnix.h>
#include <ykernel.h>

static int min_int(int a, int b) {
  return a < b ? a : b;
}

int HandleReclaim(int resource_id) {
  // reclaim a resource
  if(resource_id < 0 || resource_id >= MAX_CVARS || !cvar_table[resource_id].valid){
    return ERROR;
  }
  // check if the resource has any waiting processes
  if(!queue_is_empty(&cvar_table[resource_id].wait_queue)){
    return ERROR;
  }
  // set the resource to invalid
  cvar_table[resource_id].valid = 0;
  return 0;
}

static void fork_cleanup(pcb_t *child) {
  // helper function to clean up the child process
  unsigned int kstack_npg = KERNEL_STACK_MAXSIZE / PAGESIZE;
  if (child == NULL) return;
  for (unsigned int i = 0; i < kstack_npg; i++) {
    if (child->kernel_stack_frames[i].valid) {
      free_frame(child->kernel_stack_frames[i].pfn);
    }
  }
  if (child->region1_page_table != NULL) {
    for (unsigned int i = 0; i < MAX_PT_LEN; i++) {
      if (child->region1_page_table[i].valid) {
        free_frame(child->region1_page_table[i].pfn);
      }
    }
    free(child->region1_page_table);
  }
  free(child);
}
 
int HandleFork(void) {
  // fork a new process
  unsigned int kstack_npg        = KERNEL_STACK_MAXSIZE / PAGESIZE;
  unsigned int first_kstack_page = (KERNEL_STACK_BASE - VMEM_0_BASE) / PAGESIZE;
  unsigned int scratch_page      = first_kstack_page - kstack_npg;
 
  pcb_t *child_pcb = (pcb_t *)malloc(sizeof(pcb_t));
  if (child_pcb == NULL) {
    return ERROR;
  }
  memset(child_pcb, 0, sizeof(pcb_t));
 
  child_pcb->state  = READY;
  child_pcb->parent = current_process;
 
  // set the child kernel-stack frames
  for (unsigned int i = 0; i < kstack_npg; i++) {
    int pfn = find_frame();
    if (pfn == ERROR) {
      fork_cleanup(child_pcb);
      return ERROR;
    }
    child_pcb->kernel_stack_frames[i].valid = 1;
    child_pcb->kernel_stack_frames[i].prot  = PROT_READ | PROT_WRITE;
    child_pcb->kernel_stack_frames[i].pfn   = pfn;
  }
 
  // allocate a new region 1 page table for the child
  pte_t *child_r1 = (pte_t *)malloc((VMEM_1_SIZE / PAGESIZE) * sizeof(pte_t));
  if (child_r1 == NULL) {
    fork_cleanup(child_pcb);
    return ERROR;
  }
  memset(child_r1, 0, (VMEM_1_SIZE / PAGESIZE) * sizeof(pte_t));
  child_pcb->region1_page_table = child_r1;
 
  // deep-copy each valid parent page into a fresh frame
  for (int i = 0; i < MAX_PT_LEN; i++) {
    if (current_process->region1_page_table[i].valid != 1) continue;
 
    int new_pfn = find_frame();
    if (new_pfn == ERROR) {
      fork_cleanup(child_pcb);
      return ERROR;
    }
 
    child_r1[i].valid = 1;
    child_r1[i].prot  = current_process->region1_page_table[i].prot;
    child_r1[i].pfn   = new_pfn;
 
    region0_page_table[scratch_page].valid = 1;
    region0_page_table[scratch_page].prot  = PROT_READ | PROT_WRITE;
    region0_page_table[scratch_page].pfn   = new_pfn;
    WriteRegister(REG_TLB_FLUSH, VMEM_0_BASE + (scratch_page << PAGESHIFT));
 
    void *parent_vaddr = (void *)(VMEM_1_BASE + i * PAGESIZE);
    memcpy((void *)(VMEM_0_BASE + (scratch_page << PAGESHIFT)),
           parent_vaddr, PAGESIZE);
 
    region0_page_table[scratch_page].valid = 0;
    region0_page_table[scratch_page].prot  = PROT_NONE;
    region0_page_table[scratch_page].pfn   = 0;
    WriteRegister(REG_TLB_FLUSH, VMEM_0_BASE + (scratch_page << PAGESHIFT));
  }
 
  int child_id = helper_new_pid(child_r1);
  if (child_id == ERROR) {
    fork_cleanup(child_pcb);
    return ERROR;
  }
  child_pcb->pid = child_id;
 
  // link onto the parent's children list
  if (current_process->children == NULL) {
    current_process->children = child_pcb;
  } else {
    pcb_t *sib = current_process->children;
    while (sib->sibling != NULL) sib = sib->sibling;
    sib->sibling = child_pcb;
  }
 
  if (child_id >= 0 && child_id < MAX_PROCESSES) {
    process_table[child_id] = child_pcb;
  }
 
  // child inherits the parent's user context but returns 0 from fork()
  memcpy(&child_pcb->user_context, &current_process->user_context,
         sizeof(UserContext));
  child_pcb->user_context.regs[0] = 0;
 
  // everything above this enqueue is supposed to run
  enqueue(ready_queue, child_pcb);
  KernelContextSwitch(KCCopy, child_pcb, NULL);
 
  if (current_process == child_pcb) {
    return 0;              // child
  }
  return child_pcb->pid;   // parent
}

int HandleExec(char *path, char **args){
    // unmap all the pages in the current process's region1 page table
    for (int i = 0; i < MAX_PT_LEN; i++) {
      if (current_process->region1_page_table[i].valid) {
          free_frame(current_process->region1_page_table[i].pfn);
          current_process->region1_page_table[i].valid = 0;
      }
  }

  // flush the TLB
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  // load new program
  if (LoadProgram(path, args, current_process) == ERROR) {
      HandleExit(ERROR);
      return ERROR;
  }

  return SUCCESS;
  
}

int HandleBrk(void *addr) {
  (void)addr;
  return 0;
}

int HandleDelay(int ticks) {
  if (ticks <= 0) {
    return 0;
  }
  current_process->time = ticks;
  current_process->state = WAITING;
  ScheduleNextProcess();
  TracePrintf(1, "Process %d is waiting for %d ticks\n", current_process->pid, ticks);
  return 0;
}

int HandleWait(int *status_ptr){
  pcb_t *child;

  if (current_process->children == NULL) {
    return ERROR;
  }

  while(1){
    // check if parent has a zombie child
    child = current_process->children;
    while (child != NULL) {
      if (child->state == ZOMBIE) {
        break;
      }
      child = child->sibling;
    }

    if (child != NULL) {

      if (status_ptr != NULL) {
          *status_ptr = child->exit_code;
      }

      int pid = child->pid;

      // remove the child from the parent's children list
      if (current_process->children == child) {
        current_process->children = child->sibling;
      } else {
        pcb_t *prev = current_process->children;
        while (prev->sibling != child) {
          prev = prev->sibling;
        }
        prev->sibling = child->sibling;
      }

      // destroy the child process pcb
      for(int i = 0; i < MAX_PT_LEN; i++) {
        if (child->region1_page_table[i].valid) {
          free_frame(child->region1_page_table[i].pfn);
        }
      }
      free(child->region1_page_table);

      free_frame(child->kernel_stack_frames[0].pfn);
      free_frame(child->kernel_stack_frames[1].pfn);

      helper_retire_pid(pid);
      free(child);
      return pid;
    }

    if (current_process->children == NULL) {
      return ERROR;
    }

    current_process->state = WAITING;

    ScheduleNextProcess();
  }
}

void HandleExit(int status){
  current_process->exit_code = status;
  current_process->state = ZOMBIE;

  pcb_t *parent = current_process->parent;

  if (parent != NULL && parent->state == WAITING) {
    parent->state = READY;
    enqueue(ready_queue, parent);
  }

  ScheduleNextProcess();
}

int HandleGetPid(){
  return current_process->pid;
}

int HandleTtyRead(int tty_id, void *buffer, int len) {
  if (tty_id < 0 || tty_id >= NUM_TERMINALS) return ERROR;
  if (buffer == NULL || len <= 0) return ERROR;

  tty_t *terminal = &terminal_table[tty_id];

  /* Block until a line is available, re-checking each time we wake. */
  while (terminal->line_buffer_head == NULL) {
    enqueue(&terminal->read_queue, current_process);
    current_process->state = WAITING;
    ScheduleNextProcess();
  }

  line_buffer_t *line = terminal->line_buffer_head;
  int read_len;
  if (len >= line->length) {
    read_len = line->length;
    memcpy(buffer, line->buffer, read_len);
    terminal->line_buffer_head = line->next;
    if (terminal->line_buffer_head == NULL) terminal->line_buffer_tail = NULL;
    free(line);
  } else {
    read_len = len;
    memcpy(buffer, line->buffer, read_len);
    memmove(line->buffer, line->buffer + read_len, line->length - read_len);
    line->length -= read_len;
  }
  return read_len;
}

int HandleTtyWrite(int tty_id, void *buffer, int len) {
  if (tty_id < 0 || tty_id >= NUM_TERMINALS){
    return ERROR;
  }
  if (buffer == NULL || len <= 0){
    return ERROR;
  }
  tty_t *term = &terminal_table[tty_id];

  char *kbuf = (char *)malloc(len);
  if (kbuf == NULL) {
    return ERROR;
  }
  memcpy(kbuf, buffer, len);

  term->write_buffer = kbuf;
  term->total_write_len = len;
  term->write_progress = 0;
  term->is_transmitting = 1;
  term->current_write_process = current_process;

  int chunk = min_int(len, TERMINAL_MAX_LINE);
  TtyTransmit(tty_id, kbuf, chunk);
  term->write_progress = chunk;


  current_process->state = WAITING;
  ScheduleNextProcess();

  return len;
}

int HandleDefault(UserContext* user_context){
  return 0;
}

void HandleSyscall(UserContext *user_context) {
  int syscall_number = user_context->code;
  int result = 0;
 
  switch (syscall_number) {
    case YALNIX_FORK:
      result = HandleFork();
      break;
    case YALNIX_EXEC:
      /* FIX: removed the stray `result = 0;` that clobbered Exec's return. */
      result = HandleExec((char *)user_context->regs[0],
                          (char **)user_context->regs[1]);
      break;
    case YALNIX_WAIT:
      result = HandleWait((int *)user_context->regs[0]);
      break;
    case YALNIX_EXIT:
      HandleExit((int)user_context->regs[0]);
      /* never returns for the exiting process */
      break;
    case YALNIX_GETPID:
      result = HandleGetPid();
      break;
    case YALNIX_BRK:
      result = HandleBrk((void *)user_context->regs[0]);
      break;
    case YALNIX_DELAY:
      result = HandleDelay((int)user_context->regs[0]);
      break;
    case YALNIX_TTY_READ:
      result = HandleTtyRead((int)user_context->regs[0],
                             (void *)user_context->regs[1],
                             (int)user_context->regs[2]);
      break;
    case YALNIX_TTY_WRITE:
      result = HandleTtyWrite((int)user_context->regs[0],
                              (void *)user_context->regs[1],
                              (int)user_context->regs[2]);
      break;
    case YALNIX_PIPE_INIT:
      result = HandlePipeInit((int *)user_context->regs[0]);
      break;
    case YALNIX_PIPE_READ:
      result = HandlePipeRead((int)user_context->regs[0],
                              (void *)user_context->regs[1],
                              (int)user_context->regs[2]);
      break;
    case YALNIX_PIPE_WRITE:
      result = HandlePipeWrite((int)user_context->regs[0],
                               (void *)user_context->regs[1],
                               (int)user_context->regs[2]);
      break;
    case YALNIX_LOCK_INIT:
      result = HandleLockInit((int *)user_context->regs[0]);
      break;
    case YALNIX_LOCK_ACQUIRE:
      result = HandleLockAcquire((int)user_context->regs[0]);
      break;
    case YALNIX_LOCK_RELEASE:
      result = HandleLockRelease((int)user_context->regs[0]);
      break;
    case YALNIX_CVAR_INIT:
      result = HandleCvarInit((int *)user_context->regs[0]);
      break;
    case YALNIX_CVAR_WAIT:
      result = HandleCvarWait((int)user_context->regs[0],
                              (int)user_context->regs[1]);
      break;
    case YALNIX_CVAR_SIGNAL:
      result = HandleCvarSignal((int)user_context->regs[0]);
      break;
    case YALNIX_CVAR_BROADCAST:
      result = HandleCvarBroadcast((int)user_context->regs[0]);
      break;
    case YALNIX_RECLAIM:
      result = HandleReclaim((int)user_context->regs[0]);
      break;
    default:
      result = HandleDefault(user_context);
      break;
  }
 
  user_context->regs[0] = result;
}