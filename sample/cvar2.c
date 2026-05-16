// cs58 test
#include <yuser.h>

int main(void){
  int lock;
  int cvar;
  int pid;
  int rc;

  TracePrintf(0,"-----------------------------------------------\n");
  TracePrintf(0,"test_cvar2: two children wait, parent broadcasts\n");
  TracePrintf(0,"if both children wait before, they should both wake up\n");


  rc = LockInit(&lock);
  if (rc)
    TracePrintf(0,"LockInit nonzero rc %d\n", rc);

  rc = CvarInit(&cvar);
  if (rc)
    TracePrintf(0,"CvarInit nonzero rc %d\n", rc);

  pid = Fork();

  if (pid < 0) {
    TracePrintf(0,"Fork error %d\n", pid);
    Exit(0);
  }

  if ( 0 == pid ){
    rc = Acquire(lock);
    if (rc)
      TracePrintf(0,"Child 1 Acquire nonzero %d\n", rc);

    TracePrintf(0, "Child 1 Waiting\n");
    rc = CvarWait(cvar,lock);
    if (rc)
      TracePrintf(0,"Child 1 CvarWait nonzero %d\n", rc);
    else TracePrintf(0,"Child 1 woke up\n");


    rc = Release(lock);
    if (rc)
      TracePrintf(0,"Child 1 Release nonzero %d\n", rc);
    else 
      TracePrintf(0,"Child 1 released the lock!!!\n");

    TracePrintf(0, "Child 1 Exiting\n");
    Exit(0);

  } else {
     pid = Fork();	

     if (pid < 0) {
       TracePrintf(0,"Fork error %d\n", pid);
       Exit(0);
     }

     if ( 0 == pid ){
       rc = Acquire(lock);
       if (rc)
	 TracePrintf(0,"Child 2 Acquire nonzero %d\n", rc);

       TracePrintf(0, "Child 2 Waiting\n");
       rc = CvarWait(cvar,lock);
       if (rc)
	 TracePrintf(0,"Child 2 CvarWait nonzero %d\n", rc);
       else TracePrintf(0,"Child 2 woke up\n");

       rc = Release(lock);
       if (rc)
	 TracePrintf(0,"Child 2 Release nonzero %d\n", rc);

       TracePrintf(0, "Child 2 Exiting\n");
     }  else {
       TracePrintf(0,"Parent delaying\n");
       rc = Delay(10);
       if (rc)
	 TracePrintf(0,"Parent delay nonzero rc %d\n", rc);
       else TracePrintf(0,"Parent done delaying\n");

       TracePrintf(0, "Parent Broadcasting\n");
       rc = CvarBroadcast(cvar);   //child2 maybe can not see it..
       if (rc)
	 TracePrintf(0,"broadcast nonzero %d\n",rc);
       TracePrintf(0, "Parent delaying for a long time\n");
       rc = Delay(20);
       if (rc) 
	 TracePrintf(0,"delay rc nonzero %d---did children finish?\n", rc);
       TracePrintf(0,"Parent exiting\n");
       Exit(0);

     }
  }
}
