#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <ykernel.h>
#include <fcntl.h>
#include <unistd.h>
#include <ykernel.h>
#include <load_info.h>
#include "kernelstart.h"
#include "trap.h"

pcb_t *process_table[10];

pcb_t *current_process;
pcb_t *idle_pcb;
pcb_t *init_pcb;

pte_t *idle_page_table;
pte_t *init_page_table;

tty_t terminal_table[NUM_TERMINALS];

static process_queue_t ready_queue_storage;
process_queue_t *ready_queue = &ready_queue_storage;

static process_queue_t blocked_queue_storage;
process_queue_t *blocked_queue = &blocked_queue_storage;

static process_queue_t zombie_queue_storage;
process_queue_t *zombie_queue = &zombie_queue_storage;

TrapHandler trap_vector[TRAP_VECTOR_SIZE];

unsigned int is_vm_enabled;

char free_frames[MAX_PMEM_SIZE / PAGESIZE];

void *current_brk;

pte_t *region0_page_table;
pte_t *region1_page_table;

static KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);
static pcb_t *MakeIdleProcess(UserContext *user_context);
static pcb_t *MakeInitProcess(UserContext *user_context);

unsigned int num_pages_per_region = (VMEM_REGION_SIZE / PAGESIZE);
unsigned int first_kernel_stack_page = (KERNEL_STACK_BASE - VMEM_0_BASE) / PAGESIZE;
unsigned int kernel_stack_maxsize = KERNEL_STACK_MAXSIZE / PAGESIZE;

int find_frame(void) {
  for (int i = 0; i < (MAX_PMEM_SIZE / PAGESIZE); i++) {
    if (free_frames[i] == 0) {
      allocate_frame(i);
      return i;
    }
  }
  return -1;
}

int allocate_frame(int pfn) {
  free_frames[pfn] = 1;
  return 0;
}

int free_frame(int pfn) {
  free_frames[pfn] = 0;
  return 0;
}

int SetKernelBrk(void *addr) {
  if ((unsigned long)addr < (unsigned long)VMEM_0_BASE ||
      (unsigned long)addr >= (unsigned long)VMEM_0_LIMIT) {
    return ERROR;
  }

  if (is_vm_enabled == 0) {
    TracePrintf(1, "VM is not enabled\n");
    current_brk = addr;
    return 0;
  }

  TracePrintf(1, "VM is enabled\n");

    //we first need to find out whether the new address is less than or greater than the current_brk. That will decide if we are allocating new frames or freeing frames. Additionally, we need to find out how many frames we are allocating or freeing.

    /*unsigned int current_page =
      UP_TO_PAGE((char *)current_brk - (char *)VMEM_0_BASE) >> PAGESHIFT;
  unsigned int new_page =
      UP_TO_PAGE((char *)addr - (char *)VMEM_0_BASE) >> PAGESHIFT;*/

    unsigned int current_page = UP_TO_PAGE((current_brk - VMEM_0_BASE)) >> PAGESHIFT;
    unsigned int new_page = UP_TO_PAGE((addr - VMEM_0_BASE)) >> PAGESHIFT;

  if (new_page == current_page) {
    current_brk = addr;
    return 0;
  }

  if (new_page < current_page) {
    for (unsigned int i = new_page; i <= current_page; i++) {
      int pfn = region0_page_table[i].pfn;
            region0_page_table[i].valid = 0;
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
    }
  }
  current_brk = addr;
  return 0;
}



int
 LoadProgram(char *name, char *args[], pcb_t *proc) 
 
 {
   int fd;
   int (*entry)();
   struct load_info li;
   int i;
   char *cp;
   char **cpp;
   char *cp2;
   int argcount;
   int size;
   int text_pg1;
   int data_pg1;
   int data_npg;
   int stack_npg;
   long segment_size;
   char *argbuf;
 
   
   /*
    * Open the executable file 
    */
   if ((fd = open(name, O_RDONLY)) < 0) {
     TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
     return ERROR;
   }
 
   if (LoadInfo(fd, &li) != LI_NO_ERROR) {
     TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
     close(fd);
     return (-1);
   }
 
   if (li.entry < VMEM_1_BASE) {
     TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
     close(fd);
     return ERROR;
   }
 
   /*
    * Figure out in what region 1 page the different program sections
    * start and end
    */
   text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
   data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
   data_npg = li.id_npg + li.ud_npg;
   /*
    *  Figure out how many bytes are needed to hold the arguments on
    *  the new stack that we are building.  Also count the number of
    *  arguments, to become the argc that the new "main" gets called with.
    */
   size = 0;
   for (i = 0; args[i] != NULL; i++) {
     TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
     size += strlen(args[i]) + 1;
   }
   argcount = i;
 
   TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);
   
   /*
    *  The arguments will get copied starting at "cp", and the argv
    *  pointers to the arguments (and the argc value) will get built
    *  starting at "cpp".  The value for "cpp" is computed by subtracting
    *  off space for the number of arguments (plus 3, for the argc value,
    *  a NULL pointer terminating the argv pointers, and a NULL pointer
    *  terminating the envp pointers) times the size of each,
    *  and then rounding the value *down* to a double-word boundary.
    */
   cp = ((char *)VMEM_1_LIMIT) - size;
 
   cpp = (char **)
     (((int)cp - 
       ((argcount + 3 + POST_ARGV_NULL_SPACE) *sizeof (void *))) 
      & ~7);
 
   /*
    * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
    * reserved above the stack pointer, before the arguments.
    */
   cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;
 
 
 
   TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n",
         li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);
 
 
   /* 
    * Compute how many pages we need for the stack */
   stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;
 
   TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n",
         li.t_npg + data_npg, stack_npg);
 
 
   /* leave at least one page between heap and stack */
   if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN) {
     close(fd);
     return ERROR;
   }
 
   /*
    * This completes all the checks before we proceed to actually load
    * the new program.  From this point on, we are committed to either
    * loading succesfully or killing the process.
    */
 
   /*
    * Set the new stack pointer value in the process's UserContext
    */
 
   /* 
    * ==>> (rewrite the line below to match your actual data structure) 
    * ==>> proc->uc.sp = cp2; 
    */

  TracePrintf(1, "Setting stack pointer to %p\n", cp2);
   proc->user_context.sp = cp2;
 
   /*
    * Now save the arguments in a separate buffer in region 0, since
    * we are about to blow away all of region 1.
    */

  TracePrintf(1, "Allocating %d bytes for arguments\n", size);
   cp2 = argbuf = (char *)malloc(size);

   if(!cp2){
    return ERROR;
   }
 
   /* 
    * ==>> You should perhaps check that malloc returned valid space 
    */
 
   for (i = 0; args[i] != NULL; i++) {
     TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
     strcpy(cp2, args[i]);
     cp2 += strlen(cp2) + 1;
   }
 
   /*
    * Set up the page tables for the process so that we can read the
    * program into memory.  Get the right number of physical pages
    * allocated, and set them all to writable.
    */
 
   /* ==>> Throw away the old region 1 virtual address space by
    * ==>> curent process by walking through the R1 page table and,
    * ==>> for every valid page, free the pfn and mark the page invalid.
    */

    TracePrintf(1, "Throwing away the old region 1 virtual address space\n");
    pte_t* region1_page_table = proc->region1_page_table;
    for(int i = 0; i < num_pages_per_region; i++){
      if(region1_page_table[i].valid == 1){
        free_frame(region1_page_table[i].pfn);
      }
      region1_page_table[i].pfn = 0;
      region1_page_table[i].valid = 0;
      region1_page_table[i].prot = PROT_WRITE;
    }
 
   /*
    * ==>> Then, build up the new region1.  
    * ==>> (See the LoadProgram diagram in the manual.)
    */

    TracePrintf(1, "Building up the new region1\n");
    TracePrintf(1, "Allocating %d pages for text\n", li.t_npg);

    for(int i = 0; i < li.t_npg; i++){
      region1_page_table[text_pg1 + i].pfn = find_frame();
      region1_page_table[text_pg1 + i].valid = 1;
      region1_page_table[text_pg1 + i].prot = PROT_READ | PROT_WRITE;
    }
 
   /*
    * ==>> First, text. Allocate "li.t_npg" physical pages and map them starting at
    * ==>> the "text_pg1" page in region 1 address space.
    * ==>> These pages should be marked valid, with a protection of
    * ==>> (PROT_READ | PROT_WRITE).
    */

    TracePrintf(1, "Allocating %d pages for data\n", data_npg);

    for(int i = 0; i < data_npg; i++){
      region1_page_table[data_pg1 + i].pfn = find_frame();
      region1_page_table[data_pg1 + i].valid = 1;
      region1_page_table[data_pg1 + i].prot = PROT_READ | PROT_WRITE;
    }

 
   /*
    * ==>> Then, data. Allocate "data_npg" physical pages and map them starting at
    * ==>> the  "data_pg1" in region 1 address space.
    * ==>> These pages should be marked valid, with a protection of
    * ==>> (PROT_READ | PROT_WRITE).
    */
 
   /* 
    * ==>> Then, stack. Allocate "stack_npg" physical pages and map them to the top
    * ==>> of the region 1 virtual address space.
    * ==>> These pages should be marked valid, with a
    * ==>> protection of (PROT_READ | PROT_WRITE).
    */

    TracePrintf(1, "Allocating %d pages for stack\n", stack_npg);

    for(unsigned int i = 0; i < stack_npg; i++){
      region1_page_table[num_pages_per_region - stack_npg + i].pfn = find_frame();
      region1_page_table[num_pages_per_region - stack_npg + i].valid = 1;
      region1_page_table[num_pages_per_region - stack_npg + i].prot = PROT_READ | PROT_WRITE;
    }
 
   /*
    * ==>> (Finally, make sure that there are no stale region1 mappings left in the TLB!)
    */

    TracePrintf(1, "Flushing the TLB\n");
    WriteRegister(REG_PTBR1, (unsigned int)proc->region1_page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
 
   /*
    * All pages for the new address space are now in the page table.  
    */
 
   /*
    * Read the text from the file into memory.
    */

    TracePrintf(1, "Reading the text from the file into memory\n");
   lseek(fd, li.t_faddr, SEEK_SET);
   segment_size = li.t_npg << PAGESHIFT;
   if (read(fd, (void *) li.t_vaddr, segment_size) != segment_size) {
     close(fd);
     return KILL;   // see ykernel.h
   }
 
   /*
    * Read the data from the file into memory.
    */
   lseek(fd, li.id_faddr, 0);
   segment_size = li.id_npg << PAGESHIFT;
 
   if (read(fd, (void *) li.id_vaddr, segment_size) != segment_size) {
     close(fd);
     return KILL;
   }
 
 
   close(fd);			/* we've read it all now */
 
 
   /*
    * ==>> Above, you mapped the text pages as writable, so this code could write
    * ==>> the new text there.
    *
    * ==>> But now, you need to change the protections so that the machine can execute
    * ==>> the text.
    *
    * ==>> For each text page in region1, change the protection to (PROT_READ | PROT_EXEC).
    * ==>> If any of these page table entries is also in the TLB, 
    * ==>> you will need to flush the old mapping. 
    */

    TracePrintf(1, "Changing the protections for the text pages\n");

    for (int i = text_pg1; i < text_pg1 + (int)li.t_npg; i++) {
      region1_page_table[i].prot = PROT_READ | PROT_EXEC;
    }

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
 
 
   
   /*
    * Zero out the uninitialized data area
    */
   bzero((void *)li.id_end, li.ud_end - li.id_end);
 
   /*
    * Set the entry point in the process's UserContext
    */
 
   /* 
    * ==>> (rewrite the line below to match your actual data structure) 
    * ==>> proc->uc.pc = (caddr_t) li.entry;
    */
    TracePrintf(1, "Setting entry point to %p\n", (caddr_t) li.entry);
    proc->user_context.pc = (caddr_t) li.entry;
   /*
    * Now, finally, build the argument list on the new stack.
    */
 
 
   memset(cpp, 0x00, VMEM_1_LIMIT - ((int) cpp));
 
   *cpp++ = (char *)argcount;		/* the first value at cpp is argc */
   cp2 = argbuf;
   for (i = 0; i < argcount; i++) {      /* copy each argument and set argv */
     *cpp++ = cp;
     strcpy(cp, cp2);
     cp += strlen(cp) + 1;
     cp2 += strlen(cp2) + 1;
   }
   free(argbuf);
   *cpp++ = NULL;			/* the last argv is a NULL pointer */
   *cpp++ = NULL;			/* a NULL pointer for an empty envp */
 
   return SUCCESS;
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

  unsigned int total_frames = pmem_size / PAGESIZE;

  TracePrintf(1, "Original kernel brk page: %d\n", (unsigned)_orig_kernel_brk_page);
  current_brk =
      (void *)(VMEM_0_BASE + (unsigned)_orig_kernel_brk_page * PAGESIZE);

  region0_page_table =
      (pte_t *)malloc(num_pages_per_region * sizeof(pte_t));

  for (unsigned int i = 0; i < num_pages_per_region; i++) {
    if (i < (unsigned)_first_kernel_text_page) {
      region0_page_table[i].valid = 0;
      region0_page_table[i].prot = PROT_READ | PROT_WRITE;
      region0_page_table[i].pfn = 0;
    } else if (i < (unsigned)_first_kernel_data_page) {
      region0_page_table[i].valid = 1;
      region0_page_table[i].prot = PROT_READ | PROT_EXEC;
      region0_page_table[i].pfn = i;
    } else if (i < (unsigned)_orig_kernel_brk_page) {
      region0_page_table[i].valid = 1;
      region0_page_table[i].prot = PROT_READ | PROT_WRITE;
      region0_page_table[i].pfn = i;
    } else if (i >= first_kernel_stack_page) {
      region0_page_table[i].valid = 1;
      region0_page_table[i].prot = PROT_READ | PROT_WRITE;
      region0_page_table[i].pfn = i;
    } else {
      region0_page_table[i].valid = 0;
      region0_page_table[i].prot = 0;
      region0_page_table[i].pfn = 0;
    }
  }

  TracePrintf(1, "Region 0 page table: %p\n", region0_page_table);

  region1_page_table =
      (pte_t *)malloc(num_pages_per_region * sizeof(pte_t));
  for (unsigned int i = 0; i < num_pages_per_region; i++) {
    region1_page_table[i].valid = 0;
    region1_page_table[i].prot = 0;
    region1_page_table[i].pfn = 0;
  }

  for (int i = 0; i < total_frames; i++) {
    if (i < UP_TO_PAGE((current_brk - VMEM_0_BASE)) >> PAGESHIFT) {
      free_frames[i] = 1;
    } else {
      free_frames[i] = 0;
    }
  }

  WriteRegister(REG_PTBR0, (unsigned int)region0_page_table);
  WriteRegister(REG_PTLR0, (unsigned int)num_pages_per_region);
  WriteRegister(REG_PTBR1, (unsigned int)region1_page_table);
  WriteRegister(REG_PTLR1, (unsigned int)num_pages_per_region);

  WriteRegister(REG_VM_ENABLE, 1);
  is_vm_enabled = 1;
  TracePrintf(0, "Virtual memory enabled\n");

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  for (int i = 0; i < TRAP_VECTOR_SIZE; i++) {
    trap_vector[i] = HandleTrapUnknown;
  }

  trap_vector[TRAP_KERNEL] = HandleTrapKernel;
  trap_vector[TRAP_CLOCK] = HandleTrapClock;
  trap_vector[TRAP_ILLEGAL] = HandleTrapIllegal;
  trap_vector[TRAP_MEMORY] = HandleTrapMemory;
  trap_vector[TRAP_MATH] = HandleTrapMath;
  trap_vector[TRAP_TTY_RECEIVE] = HandleTrapTtyReceive;
  trap_vector[TRAP_TTY_TRANSMIT] = HandleTrapTtyTransmit;
  trap_vector[TRAP_DISK] = HandleTrapDisk;

  WriteRegister(REG_VECTOR_BASE, (unsigned int)trap_vector);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  TracePrintf(1, "Idle process starting: %p\n");

  idle_pcb = MakeIdleProcess(initial_user_context);
  if (idle_pcb == NULL) {
    TracePrintf(0, "MakeIdleProcess failed\n");
    return;
  }

  process_table[0] = idle_pcb;

  TracePrintf(1, "Idle process pid %d pcb %p\n", idle_pcb->pid, idle_pcb);
  process_table[0] = idle_pcb;
  current_process = idle_pcb;

  TracePrintf(1, "Return context pc %p sp %p\n",
              initial_user_context->pc, initial_user_context->sp);

  WriteRegister(REG_PTBR1, (unsigned int)idle_pcb->region1_page_table);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  init_pcb = MakeInitProcess(initial_user_context);
  if (init_pcb == NULL) {
    TracePrintf(0, "MakeInitProcess failed\n");
    return;
  }

  process_table[1] = init_pcb;

  TracePrintf(1, "Init process pid %d pcb %p\n", init_pcb->pid, init_pcb);
  WriteRegister(REG_PTBR1, (unsigned int)init_pcb->region1_page_table);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  TracePrintf(1, "Switching to init process\n");

  KernelContextSwitch(KCCopy, init_pcb, NULL);

  if(current_process == init_pcb){
    idle_pcb->state = RUNNING;
    init_pcb->state = READY;
    ready_queue->head = init_pcb;
    ready_queue->tail = init_pcb;
  }else {
    char* init_program = argv[0];
    if(LoadProgram(init_program, argv, init_pcb) != 0){
      TracePrintf(0, "LoadProgram failed\n");
      Halt();
    }
    current_process = init_pcb;
  }

  *initial_user_context = current_process->user_context;
  
  TracePrintf(1, "Leaving KernelStart\n");
}

void DoIdle(void) {
  while (1) {
    TracePrintf(1, "DoIdle\n");
    Pause();
  }
}

static KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void * not_used) {
  (void)not_used;
  pcb_t *new_pcb = (pcb_t *)new_pcb_p;

  TracePrintf(1, "Copying kernel context\n");
  memcpy(&new_pcb->kernel_context, kc_in, sizeof(KernelContext));

  TracePrintf(1, "Copying kernel stack frames\n");
  unsigned int temp_kernel_stack_page = first_kernel_stack_page - kernel_stack_maxsize;

  TracePrintf(1, "Copying kernel stack frames to region0 page table\n");
  for (unsigned int i = 0; i < kernel_stack_maxsize; i++) {
    TracePrintf(1, "Copying kernel stack frame %d to region0 page table\n", i);
    region0_page_table[temp_kernel_stack_page + i] = new_pcb->kernel_stack_frames[i];
  }

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  TracePrintf(1, "Copying kernel stack frames to kernel stack base\n");

  TracePrintf(1, "temp_kernel_stack_page: %d\n", temp_kernel_stack_page);
  TracePrintf(1, "KERNEL_STACK_BASE: %p\n", KERNEL_STACK_BASE);
  TracePrintf(1, "KERNEL_STACK_MAXSIZE: %d\n", KERNEL_STACK_MAXSIZE);
  TracePrintf(1, "first_kernel_stack_page: %d\n", first_kernel_stack_page);

  TracePrintf(1, "temp_kernel_stack_page * PAGESIZE + VMEM_0_BASE: %p\n", temp_kernel_stack_page * PAGESIZE + VMEM_0_BASE);
  TracePrintf(1, "KERNEL_STACK_BASE: %p\n", KERNEL_STACK_BASE);
  TracePrintf(1, "KERNEL_STACK_MAXSIZE: %d\n", KERNEL_STACK_MAXSIZE);
  TracePrintf(1, "first_kernel_stack_page: %d\n", first_kernel_stack_page);
  TracePrintf(1, "kernel_stack_maxsize: %d\n", kernel_stack_maxsize);

  memcpy((void *)(temp_kernel_stack_page * PAGESIZE + VMEM_0_BASE), (void *) KERNEL_STACK_BASE, KERNEL_STACK_MAXSIZE);

  TracePrintf(1, "Clearing kernel stack frames in region0 page table\n");
  for (unsigned int i = 0; i < kernel_stack_maxsize; i++) {
    region0_page_table[temp_kernel_stack_page + i].pfn = 0;
    region0_page_table[temp_kernel_stack_page + i].valid = 0;
    region0_page_table[temp_kernel_stack_page + i].prot = 0;
  }

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

  TracePrintf(1, "Returning kernel context\n");
  return kc_in;
}

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p,
                          void *next_pcb_p) {
  pcb_t *curr_pcb = (pcb_t *)curr_pcb_p;
  pcb_t *next_pcb = (pcb_t *)next_pcb_p;
  unsigned int base_kernel_stack_page =
      (KERNEL_STACK_BASE - VMEM_0_BASE) / PAGESIZE;
  unsigned int nstack =
      KERNEL_STACK_MAXSIZE / PAGESIZE;
  unsigned int i;

  memcpy(&curr_pcb->kernel_context, kc_in, sizeof(KernelContext));

  for (i = 0; i < nstack; i++) {
    curr_pcb->kernel_stack_frames[i] =
        region0_page_table[base_kernel_stack_page + i];
    region0_page_table[base_kernel_stack_page + i] =
        next_pcb->kernel_stack_frames[i];
  }

  current_process = next_pcb;
  return &next_pcb->kernel_context;
}

static pcb_t *MakeIdleProcess(UserContext *user_context) {
  pcb_t *pcb = (pcb_t *)malloc(sizeof(pcb_t));
  memset(pcb, 0, sizeof(pcb_t));

  if (pcb == NULL) {
    return NULL;
  }
  memset(pcb, 0, sizeof(pcb_t));

  pcb->state = READY;
  pcb->parent = NULL;
  pcb->sibling = NULL;
  pcb->next = NULL;
  pcb->time = 0;
  pcb->priority = 0;
  pcb->exit_code = 0;

  memset(&pcb->kernel_context, 0, sizeof(KernelContext));
  memcpy(&pcb->user_context, user_context, sizeof(UserContext));


  unsigned int base = (KERNEL_STACK_BASE - VMEM_0_BASE) / PAGESIZE;
  unsigned int max_num_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;
  for (unsigned int i = 0; i < max_num_pages; i++) {
    pcb->kernel_stack_frames[i] = region0_page_table[base + i];
  }

  pcb->region1_page_table = region1_page_table;

  pcb->pid = helper_new_pid(pcb->region1_page_table);
  if (pcb->pid < 0) {
    free(pcb);
    return NULL;
  }

  TracePrintf(1, "New pid: %d\n", pcb->pid);


  {
    int pfn = find_frame();
    pcb->region1_page_table[(VMEM_1_SIZE / PAGESIZE) - 1].valid = 1;
    pcb->region1_page_table[(VMEM_1_SIZE / PAGESIZE) - 1].prot = PROT_READ | PROT_WRITE;
    pcb->region1_page_table[(VMEM_1_SIZE / PAGESIZE) - 1].pfn = pfn;
  }

  pcb->user_context.pc = (void *)DoIdle;
  pcb->user_context.sp = (void *)(VMEM_1_LIMIT - 4);
  TracePrintf(1, "User context: %p\n", &pcb->user_context);
  TracePrintf(1, "User context pc: %p\n", pcb->user_context.pc);
  TracePrintf(1, "User context sp: %p\n", pcb->user_context.sp);

  return pcb;
}

static pcb_t *MakeInitProcess(UserContext *user_context) {
  pcb_t *pcb = (pcb_t *)malloc(sizeof(pcb_t));
  if (pcb == NULL) {
    return NULL;
  }

  memset(pcb, 0, sizeof(pcb_t));

  pcb->state = READY;
  pcb->parent = NULL;
  pcb->sibling = NULL;
  pcb->next = NULL;
  pcb->time = 0;
  pcb->priority = 0;
  pcb->exit_code = 0;

  memset(&pcb->kernel_context, 0, sizeof(KernelContext));
  memcpy(&pcb->user_context, user_context, sizeof(UserContext));

  init_page_table =
      (pte_t *)malloc((VMEM_1_SIZE / PAGESIZE) * sizeof(pte_t));
  if (init_page_table == NULL) {
    free(pcb);
    return NULL;
  }
  memset(init_page_table, 0, (VMEM_1_SIZE / PAGESIZE) * sizeof(pte_t));

  pcb->region1_page_table = init_page_table;

  pcb->pid = helper_new_pid(init_page_table);

  pcb->kernel_stack_frames[0].valid = 1;
  pcb->kernel_stack_frames[0].prot = PROT_READ | PROT_WRITE;
  pcb->kernel_stack_frames[0].pfn = find_frame();
  pcb->kernel_stack_frames[1].valid = 1;
  pcb->kernel_stack_frames[1].prot = PROT_READ | PROT_WRITE;
  pcb->kernel_stack_frames[1].pfn = find_frame();

  memcpy(&pcb->user_context, user_context, sizeof(UserContext));

  return pcb;
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
