#include "hardware.h"

static char free_frames[MAX_PMEM_SIZE];
static int isVMEnabled = 0;
static void* current_brk;
static void* (*interrupt_vector_table[TRAP_VECTOR_SIZE])(UserContext*);

typedef struct pcb {
  int pid;
  UserContext* user_context;
  pte_t* page_table;
} pcb_t;

//helper function to find a free frame. Simply search all the possible frames and return the first free one.
int find_frame() {
  for (int i = 0; i < (MAX_PMEM_SIZE / PAGESIZE); i++) {
    if (free_frames[i] == 0) {
      free_frames[i] = 1;
      return i;
    }
  }
  return -1;
}

int SetKernelBrk(void * addr){
  if (isVMEnabled == 0){
    // If the VM is not enabled, we are dealing with physical memory. So we just set the current_brk to the address. 
    TracePrintf(1, "VM is not enabled\n");
    current_brk = addr;
    return 0;
  } else {
    TracePrintf(1, "VM is enabled\n");

    //we first need to find out whether the new address is less than or greater than the current_brk. That will decide if we are allocating new frames or freeing frames. Additionally, we need to find out how many frames we are allocating or freeing.

    int num_current_pages = (current_brk - VMEM_0_BASE) / PAGESIZE;
    int num_new_pages = (addr - VMEM_0_BASE) / PAGESIZE;
    if (addr < current_brk) {
      for (int i = num_new_pages; i <= num_current_pages; i++) {
        int pfn = region0_page_table[i].pfn;
        region0_page_table[i].valid = 0;
        free_frames[pfn] = 0;
      }
        //flush the TLB so that we don't access the old frames which are now free
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    } else {
      for (int i = num_current_pages; i <= num_new_pages; i++) {
        int pfn = find_frame();
        region0_page_table[i].pfn = pfn;
        region0_page_table[i].valid = 1;
        region0_page_table[i].prot = PROT_READ | PROT_WRITE | PROT_EXEC;
        free_frames[pfn] = 1;
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

  int num_frames = pmem_size / PAGESIZE;

  //initialize the region 0 page table
  int num_vpn_0 = VMEM_0_SIZE / PAGESIZE;
  pte_t* region0_page_table = malloc(num_vpn_0 * sizeof(pte_t));
  for (int i = 0; i < num_vpn_0; i++) {
    region0_page_table[i].valid = 0;
    region0_page_table[i].prot = PROT_NONE;
    region0_page_table[i].pfn = 0;
  }

  num_frames0_used = PAGESIZE / (num_vpn_0 * sizeof(pte_t));

  //initialize the region 1 page table
  int num_vpn_1 = VMEM_1_SIZE / PAGESIZE;
  pte_t* region1_page_table = malloc(num_vpn_1 * sizeof(pte_t));
  for (int i = 0; i < num_vpn_1; i++) {
    region1_page_table[i].valid = 0;
    region1_page_table[i].prot = PROT_NONE;
    region1_page_table[i].pfn = 0;
  }
  num_frames1_used = PAGESIZE / (num_vpn_1 * sizeof(pte_t));

    //update the number of frames which are no longer free
    for (int i = 0; i < num_frames; i++) {
      if(i < (orig_kernel_brk_page + num_frames0_used + num_frames1_used /* since we added the region0+page_table and region1+page_table*/ )) {
        free_frames[i] = 1;
      } else {
        free_frames[i] = 0;
      }
    }

  //create the interrupt/exception/trap handler
  WriteRegister(REG_PTBR0, (unsigned int) region0_page_table);
  WriteRegister(REG_PTLR0, (unsigned int) region0_page_table + VMEM_0_SIZE);
  WriteRegister(REG_PTBR1, (unsigned int) region1_page_table);
  WriteRegister(REG_PTLR1, (unsigned int) region1_page_table + VMEM_1_SIZE);

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

  //make sure to add the interrupt_vector_table address to the hardware register REG_VECTOR_BASE
  WriteRegister(REG_VECTOR_BASE, (unsigned int) interrupt_vector_table);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  //enable virtual memory
  WriteRegister(REG_VM_ENABLE, 1);
  isVMEnabled = 1;
  TracePrintf(0, "Virtual memory enabled\n");

  //set the kernel brk to the _orig_kernel_brk_page
  current_brk = (void*) _orig_kernel_brk_page;

  idle_pcb.pid = helper_new_pid(region1_page_table);
  idle_pcb.page_table = region1_page_table;
  idle_pcb.user_context = *initial_user_context;

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