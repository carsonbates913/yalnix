#include <yuser.h>


int Fork (UserContext* user_context){
  ProcessControlBlock* parent = current_process;
  ProcessControlBlock* child = (ProcessControlBlock*) malloc(sizeof(ProcessControlBlock));
  KernelContextSwitch(KCCopy, child, NULL);

  child->user_context.regs[0] = 0;
  user_context->regs[0] = child->pid;

  child->parent = parent;
  parent->children[parent->child_count++] = child;

  return child->pid;
}

int Exec (char * filename, char ** args){
  int result = LoadProgram(filename, args);
  if(result == ERROR){
    return ERROR;
  }
  return result;
}

int Wait (int *){
  //
}