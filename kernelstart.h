#ifndef KERNELSTART_H
#define KERNELSTART_H

#include <hardware.h>
#include "util/queue.h"

#define READY 0
#define RUNNING 1
#define WAITING 2
#define ZOMBIE 3

#define MAX_PROCESSES 32

struct pcb {
  int pid;
  int state;

  pte_t *region1_page_table;

  KernelContext kernel_context;
  UserContext user_context;

  pte_t kernel_stack_frames[KERNEL_STACK_MAXSIZE / PAGESIZE];

  int time;

  struct pcb *parent;
  struct pcb *children;
  struct pcb *sibling;
  struct pcb *next;

  int exit_code;
  char *tty_write_buffer;
  int tty_write_len;
};

typedef struct pcb pcb_t;

struct line_buffer {
  char buffer[TERMINAL_MAX_LINE];
  int length;
  struct line_buffer *next;
};

typedef struct line_buffer line_buffer_t;

struct tty {
  int id;
  line_buffer_t *line_buffer_head;
  line_buffer_t *line_buffer_tail;
  struct process_queue read_queue;
  struct process_queue write_queue;
  struct pcb *current_write_process;
  void *write_buffer;
  int total_write_len;
  int write_progress;
  int is_transmitting;
};
typedef struct tty tty_t;

typedef void (*TrapHandler)(UserContext *);

extern TrapHandler trap_vector[TRAP_VECTOR_SIZE];

extern pcb_t *process_table[MAX_PROCESSES];
extern unsigned int num_pages_per_region;
extern pcb_t *current_process;
extern pcb_t *idle_pcb;
extern pcb_t *init_pcb;
extern pte_t *region0_page_table;
extern pte_t *region1_page_table;
extern tty_t terminal_table[NUM_TERMINALS];
extern process_queue_t *ready_queue;
extern process_queue_t *blocked_queue;
extern process_queue_t *zombie_queue;

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);
KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);
int LoadProgram(char *name, char *args[], pcb_t *proc);
void HandleExit(int status);
void HandleSyscall(UserContext *user_context);

int find_frame(void);
int allocate_frame(int pfn);
int free_frame(int pfn);

void PrintReadyQueue(void);
void DoIdle(void);
void ScheduleNextProcess(void);
void copyKernelStackPages(pcb_t *child);
void setupChildKernelContext(pcb_t *child, pcb_t *parent);

#endif
