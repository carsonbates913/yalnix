
#include <yuser.h>


#define DELAY1 10
#define DELAY2 5




int main(){
  int pid, child1, child2, pid1, pid2;
  int status = 0;
  int rc;

  TracePrintf(0,"-----------------------------------------\n");
  TracePrintf(0,"test_delay: parent  waits for TWO children to delay nprmally and exit\n");

  child1 = Fork();
  if (child1 < 0) {
    TracePrintf(0,"BAD: first fork failed with rc %d\n",rc);
    Exit(-1);
  }

  if (child1 == 0) {
    
    TracePrintf(0,"child1 delaying %d\n", DELAY1);

    rc = Delay(DELAY1);
    if (rc)
      TracePrintf(0,"child1 delay nonzero rc %d\n", rc);
    
    TracePrintf(0,"child1 back from delay\n");

    pid1 = GetPid();

    TracePrintf(0,"child1 getpid returns %d\n",pid1);
    TracePrintf(0,"Child1 Process exit 23!\n");	
    Exit(23);
  }

  child2 = Fork();
  if (child2 < 0) {
    TracePrintf(0,"BAD: second fork failed with rc %d\n",rc);
    Exit(-1);
  }

  if (child2 == 0) {
    
    TracePrintf(0,"child2 delaying %d\n", DELAY2);

    rc = Delay(DELAY2);
    if (rc)
      TracePrintf(0,"child2 delay nonzero rc %d\n", rc);
    
    TracePrintf(0,"child2 back from delay\n");

    pid2 = GetPid();

    TracePrintf(0,"child2 getpid returns %d\n",pid2);
    TracePrintf(0,"Child2 Process exit 42!\n");	
    Exit(42);
  }

  TracePrintf(0,"Parent Process waiting for children!\n");
  pid = Wait(&status);
  TracePrintf(0,"preant wait got pid %d and status %d!\n",pid,status);	

  TracePrintf(0,"Parent Process waiting for children!\n");
  pid = Wait(&status);
  TracePrintf(0,"preant wait got pid %d and status %d!\n",pid,status);	

  
}

