// cs58 test code, Dartmouth

#include <yuser.h>

int main(void){


  int lock;
  int cvar;
  int pid;
  int rc;

 
  TracePrintf(0,"-----------------------------------------------\n");
  TracePrintf(0,"test_cvar: parent waits; child delays and sigs\n");
  TracePrintf(0,"if the parent waits before child sigs...parent should wake\n");

  rc = LockInit(&lock);
  if (rc)
    TracePrintf(0,"LockInit nonzero rc %d\n", rc);

  rc = CvarInit(&cvar);
  if (rc)
    TracePrintf(0,"CvarInit nonzero rc %d\n", rc);

  pid = Fork();
  if (pid < 0) {
    TracePrintf(0, "fork error! %d\n", pid);
    Exit(0);
  }

  if ( 0 == pid ) {
    Delay(5);
    TracePrintf(0,"child signaling\n");
    rc = CvarSignal(cvar);
    if (rc)
      TracePrintf(0,"CvarSignal nonzero rc %d\n", rc);
    Exit(0);
  } else {

    rc = Acquire(lock);
    if (rc)
      TracePrintf(0,"Acquire nonzero rc %d\n", rc);

    TracePrintf(0, "Parent Acquired Lock\n");
    
    TracePrintf(0, "Parent CvarWaiting\n");
    rc = CvarWait( cvar,lock);
    if (rc)
      TracePrintf(0,"CvarWait nonzero rc %d\n", rc);
    TracePrintf(0, "Parent Exiting \n");
    Exit(0);
  }
}
