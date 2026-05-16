#include "yuser.h"

int main(void){
  int lock;
  int pid;
  int rc;

 
  TracePrintf(0,"-----------------------------------------------\n");
  TracePrintf(0,"test_lock: child blocks until parent releases \n");

  rc = LockInit(&lock);
  if (rc)
    TracePrintf(0,"LockInit nonzero rc %d\n",rc);
  else 
    TracePrintf(0,"Lock %d initialized\n", lock);

  TracePrintf(0,"Parent acquiring lock\n");
  rc = Acquire(lock);
  if (rc)
    TracePrintf(0,"Acquire nonzero rc %d\n",rc);

  
  pid = Fork();

  if (pid < 0) {
    TracePrintf(0,"fork error %d\n", rc);
    Exit(-1);
  }

  if ( 0 == pid ){

    
    TracePrintf(0,"child trying to acquire lock\n");
    rc = Acquire(lock);
    if (rc)
      TracePrintf(0,"child acquire nonzero rc %d\n",rc);
    else 
      TracePrintf(0,"child acquired lock OK\n");

    Exit(0);
  }
  else
    {
      
      TracePrintf(0,"parent delaying the first time\n");
      rc = Delay(5);
      if (rc) 
	TracePrintf(0,"parent delay nonzero rc %d\n",rc);
      
      TracePrintf(0,"Parent releasing lock\n");


      rc = Release(lock);
      if (rc)
	TracePrintf(0,"release returned nonzero %d\n",rc);


      TracePrintf(0,"parent delaying for a long time, to let child run\n");
      rc = Delay(20);
      if (rc) 
	TracePrintf(0,"parent delay nonzero rc %d\n",rc);

      TracePrintf(0,"parent exiting\n");
      Exit(0);
    }
}












