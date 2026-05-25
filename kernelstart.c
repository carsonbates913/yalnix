#include "hardware.h"

static char free_frames[MAX_PMEM_SIZE / PAGESIZE];
static int isVMEnabled = 0;
static void* current_brk;
static void* (*interrupt_vector_table[TRAP_VECTOR_SIZE])(UserContext*);
static pte_t* region0_page_table;
static pte_t* region1_page_table;
static pcb_t* idle_pcb;
static pcb_t* current_process;
static int num_frames0_used;
static int num_frames1_used;
static pcb_t* process_table[10];

struct process_control_block {
  int pid;
  UserContext* user_context;
  pte_t* region1_page_table;
  KernelContext* kernel_context;

  struct process_control_block* parent;
  struct process_control_block* children[10];
  pte_t* kernel_stack_frames[KERNEL_STACK_MAXSIZE / PAGESIZE];
  int child_count;
  int status; // 0: running, 1: waiting, 2: terminated
  int exit_code;
};

typdef struct process_control_block ProcessControlBlock;

struct user_context {
  int vector;		/* vector number */
  int code;		/* additional "code" for vector */
  void *addr;		/* offending address, if any */
  void *pc;		/* PC at time of exception */
  void *sp;		/* SP at time of exception */
  void *ebp;              // base pointer at time of exception
  u_long regs[GREGS];     /* general registers at time of exception */
};

typedef struct user_context UserContext;

// the kernel context in Yalnix.  This is opaque to you
struct kernel_context {
  LinuxContext lc;
  unsigned int kstack_cs;
};

typedef struct kernel_context KernelContext;



//helper function to find a free frame. Simply search all the possible frames and return the first free one.
int find_frame() {
  for (int i = 0; i < (MAX_PMEM_SIZE / PAGESIZE); i++) {
    if (free_frames[i] == 0) {
      allocate_frame(i);
      return i;
    }
  }
  return -1;
}

int allocate_frame(int pfn){
  free_frames[pfn] = 1;
  return 0;
}

int free_frame(int pfn){
  free_frames[pfn] = 0;
  return 0;
}

int SetKernelBrk(void * addr){

  //check if the address is outside th
  if(VMEM_0_BASE <= addr && addr >= VMEM_0_LIMIT){
    return ERROR;
  }

  if (isVMEnabled == 0){
    // If the VM is not enabled, we are dealing with physical memory. So we just set the current_brk to the address. 
    TracePrintf(1, "VM is not enabled\n");
    current_brk = addr;
    return 0;
  } else {
    TracePrintf(1, "VM is enabled\n");

    //we first need to find out whether the new address is less than or greater than the current_brk. That will decide if we are allocating new frames or freeing frames. Additionally, we need to find out how many frames we are allocating or freeing.

    unsigned int current_page = UP_TO_PAGE((current_brk - VMEM_0_BASE)) >> PAGESHIFT;
    unsigned int new_page = UP_TO_PAGE((addr - VMEM_0_BASE)) >> PAGESHIFT;

    if(new_page == current_page){
      current_brk = addr;
      return 0;
    }

    if (new_page < current_page) {
      for (unsigned int i = new_page; i <= current_page; i++) {
        int pfn = region0_page_table[i].pfn;
        free_frame(pfn);
      }
        //flush the TLB so that we don't access the old frames which are now free
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    } else {
      for (unsigned int i = current_page; i <= new_page; i++) {
        int pfn = find_frame();
        region0_page_table[i].pfn = pfn;
        region0_page_table[i].valid = 1;
        region0_page_table[i].prot = PROT_READ | PROT_WRITE | PROT_EXEC;
        allocate_frame(pfn);
      }
    }
    current_brk = addr;
    return 0;
  }

}

void KernelStart (char** argv, unsigned int pmem_size, UserContext *initial_user_context) {

  if (pmem_size > MAX_PMEM_SIZE) {
    TracePrintf(1, "PMEM size is too large\n");
    return;
  }

  if (pmem_size < MIN_PMEM_SIZE) {
    TracePrintf(1, "PMEM size is too small\n");
    return;
  }

  int total_frames = pmem_size / PAGESIZE;

  //initialize the region 0 page table
  region0_page_table = (pte_t*) malloc(VMEM_0_SIZE / PAGESIZE * sizeof(pte_t));
  for (unsigned int i = 0; i < VMEM_0_SIZE / PAGESIZE; i++) {
    if ( i < current_brk ) {
      region0_page_table[i].valid = 1;
      region0_page_table[i].prot = PROT_READ | PROT_WRITE;
      region0_page_table[i].pfn = i;
    } else {
      region0_page_table[i].valid = 0;
      region0_page_table[i].prot = 0;
    }
  }

  //initialize the region 1 page table
  region1_page_table = (pte_t*) malloc(VMEM_1_SIZE / PAGESIZE * sizeof(pte_t));
  for (unsigned int i = 0; i < VMEM_1_SIZE / PAGESIZE; i++) {
    region1_page_table[i].valid = 0;
    region1_page_table[i].prot = 0;
  }

  //update the number of frames which are no longer free
  for (int i = 0; i < num_frames; i++) {
    if(i < current_brk) {
      free_frames[i] = 1;
    } else {
      free_frames[i] = 0;
    }
  }

  //create the interrupt/exception/trap handler
  WriteRegister(REG_PTBR0, (unsigned int) region0_page_table);
  WriteRegister(REG_PTLR0, (unsigned int) VMEM_0_SIZE / PAGESIZE);
  WriteRegister(REG_PTBR1, (unsigned int) region1_page_table);
  WriteRegister(REG_PTLR1, (unsigned int) VMEM_1_SIZE / PAGESIZE);

  //enable virtual memory
  WriteRegister(REG_VM_ENABLE, 1);
  isVMEnabled = 1;
  TracePrintf(0, "Virtual memory enabled\n");

  for(int i = 0; i < TRAP_VECTOR_SIZE; i++){
    interrupt_vector_table[i] = handle_trap_unknown;
  }

  interrupt_vector_table[TRAP_KERNEL] = handle_trap_kernel;
  interrupt_vector_table[TRAP_CLOCK] = handle_trap_clock;
  interrupt_vector_table[TRAP_ILLEGAL] = handle_trap_illegal;
  interrupt_vector_table[TRAP_MEMORY] = handle_trap_memory;
  interrupt_vector_table[TRAP_MATH] = handle_trap_math;
  interrupt_vector_table[TRAP_TTY_RECEIVE] = handle_trap_tty_receive;
  interrupt_vector_table[TRAP_TTY_TRANSMIT] = handle_trap_tty_transmit;
  interrupt_vector_table[TRAP_DISK] = handle_trap_disk;

  //make sure to add the interrupt_vector_table address to the hardware register REG_VECTOR_BASE
  WriteRegister(REG_VECTOR_BASE, (unsigned int) interrupt_vector_table);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  //create the idle process
  idle_pcb = MakeIdleProcess(initial_user_context);



  process_table[0] = idle_process;
  current_process = idle_process;

  init_pcb = MakeInitProcess( initial_user_context );

  KernelContextSwitch(KCCopy, init_process, NULL);


  initial_user_context->pc = (void*) DoIdle;
  initial_user_context->sp = (void*) KERNEL_STACK_BASE;

  TracePrintf(1, "Leaving KernelStart\n");

}

void DoIdle(void) {
  while (1) {
      TracePrintf(1, "DoIdle\n");
      Pause();
  }
}

static KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used){
  ProcessControlBlock* new_pcb = *new_pcb_p;
  memcpy(&new_pcb->kernel_context, kc_in, sizeOf(KernelContext));
  unsigned int base_kernel_stack_page = (KERNEL_STACK_BASE - VMEM_0_BASE) / PAGESIZE;

  unsigned int saved_kernel_stack_page = base_kernel_stack_page - (KERNEL_STACK_MAXSIZE / PAGESIZE);

  for(unsigned int i = saved_kernel_stack_page; i < (KERNEL_STACK_MAXSIZE / PAGESIZE)){
    region0_page_table[saved_kernel_stack_page + i] = new_pcb->kerne_stack_frames[i];
  }

  memcpy((void*)(temp_base_page * PAGESIZE), (void*) KERNEL_STACK_BASE, KERNEL_STACK_MAXSIZE);

  for (unsigned int i = 0; i < KERNEL_STACK_MAXSIZE; i++) {
    region0_page_table[saved_kernel_stack_page + i].pfn = 0;
    region0_page_table[saved_kernel_stack_page + i].valid = 0;
    region0_page_table[saved_kernel_stack_page + i].prot = 0;
  }
  return kc_in;
}

static KernelContext KCSwitch(KernelContext* kc_in, void* curr_pcb_p, void* next_pcb_p){
  ProcessControlBlock* curr_pcb = (ProcessControlBlock*) curr_pcb_p;
  ProcessControlBlock* next_pcb = (ProcessControlBlock*) next_pcb_p;

  memcpy(&curr_pcb->kernel_context, kc_in, sizeOf(KernelContext));

  //save the current process's kernel stack to its process control block
  unsigned int base_kernel_stack_page = (KERNEL_STACK_BASE - VMEM_0_BASE) / PAGESIZE;
  for(int i = 0; i < base_kernel_stack_page; i++) {
    curr_pcb->kernel_stack_frames[i] = region0_page_table[base_kernel_stack_page + i];
    region0_page_table[base_kernel_stack_page + i] = next_pcb->kernel_stack_frames[i];
  }

  for(int i = 0; i < base_kernel_stack_page; i++) {
    currrent_process->kernel_stack_frames[i] = region0_page_table[base_kernel_stack_page + i]
  }

  current_process = next_pcb;
  return &next_pcb->kernel_context;

}

static ProcessControlBlock* MakeIdleProcess( UserContext* user_context){
  ProcessControlBlock* idle_pcb = (ProcessControlBlock*) malloc(sizeOf(ProcessControlBlock));

  //establish new page table
  idle_page_table = (pte_t*) malloc(VMEM_1_SIZE / PAGESIZE * sizeof(pte_t));

  int new_pid = helper_new_pid(idle_page_table);
  idle_pcb->pid = new_pid;
  idle_pcb->user_context = user_context;

  return idle_pcb;
}

static ProcessControlBlock* MakeInitProcess( UserContext* user_context){
  ProcessControlBlock* init_pcb = (ProcessControlBlock*) malloc(sizeOf(ProcessControlBlock));

  //establish new page table
  idle_page_table = (pte_t*) malloc(VMEM_1_SIZE / PAGESIZE * sizeof(pte_t));

  int new_pid = helper_new_pid(idle_page_table);
  idle_pcb->pid = new_pid;
  idle_pcb->user_context = user_context;

  return idle_pcb;
}

static ProcessControlBlock* 


void handle_trap_kernel(UserContext* user_context){
  TracePrintf(1, "Kernel trap\n");
  TracePrintf(1, "User context: %p\n", user_context);
  TracePrintf(1, "Syscall number: %d\n", user_context->regs[0]);
  TracePrintf(1, "Syscall argument 1: %d\n", user_context->vector);
}

/*
switch statement on syscall number
if fork:
  create child process
  copy memory
  schedule child

if Exec:
  load program and replace current process
  change program counter

  If Exit:
    kill current process
    schedule next process
If Delay:
  put process in queue
  context switch
*/

void handle_trap_clock(UserContext* user_context){
  TracePrintf(1, "Clock trap\n");
}

/*
increment clock counter
wake up processes that are ready to run
decrement delay counter
if time up, put process in queue and schedule another process
*/

void handle_trap_illegal(UserContext* user_context){
  TracePrintf(1, "Illegal trap\n");
}

/*
output illegal instruction error
kill process
schedule next process
*/

void handle_trap_memory(UserContext* user_context){
  TracePrintf(1, "Memory trap\n");
}

/*
if inside stack region
  allocate new stack page
  update page tables
else if page is swapped out
  load page from disk
  update page tables
else
  segfault
  kill process
*/

void handle_trap_math(UserContext* user_context){
  TracePrintf(1, "Math trap\n");
}

/*
check for divide by zero, overflow, or floating point exception
kill process
schedule next process
*/

void handle_trap_tty_receive(UserContext* user_context){
  TracePrintf(1, "TTY receive trap\n");
}

/*
read characters from terminal
add data to input buffer
wake process that is waiting to read from terminal
*/

void handle_trap_tty_transmit(UserContext* user_context){
  TracePrintf(1, "TTY transmit trap\n");
}

/*
identify terminal with completed transmission
show it as available
if more queued output, start next transmission
wake processes blocked on that terminal
*/

void handle_trap_disk(UserContext* user_context){
  TracePrintf(1, "Disk trap\n");
}

/*
identify completed disk operation
wake processes blocked on that disk operation
start next disk request
*/

void handle_trap_unknown(UserContext* user_context){
  TracePrintf(1, "Unknown trap\n");
}
