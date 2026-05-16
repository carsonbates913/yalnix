
#include <yuser.h>


#define FORKTIMES 10 
#define PRINTTIMES 3

int main(){
  int i, j=0;
  int pid,me, init;
  int rc;

  TracePrintf(0,"-----------------------------------------\n");  
  TracePrintf(0,"test_fork: fork %d children; each loops %d times\n", FORKTIMES, PRINTTIMES);


  me = GetPid();
  init = me;

  for (i=0; i < FORKTIMES; i++){
    TracePrintf(0,"pid %d  starting fork %d\n", me, i);

    pid = Fork();

    if (pid < 0){
      TracePrintf(0, "pid %d fork %d Error %d, Maybe out of memory?\n", 
		  me, i, pid);
      continue;

    }

    if (pid > 0) {

      TracePrintf(0,"pid %d fork %d was succesful---just launched %d!\n", 
		  me, i, pid);
      continue;
    }

    if (pid == 0) {

      me = GetPid();

      while(j++ < PRINTTIMES){

	TracePrintf(0,"pid %d delaying loop %d \n", me, j);
	rc = Delay(3);
	if (rc) 
	  TracePrintf(0, "pid %d has delay rc %d\n", me, rc);
      }

      if (me != init) {
	TracePrintf(0," %d terminated\n", me);
	Exit(pid);
      } else {
	TracePrintf(0,"pid %d delaying \n", me);
	rc = Delay(5);
	if (rc) 
	  TracePrintf(0, "pid %d has delay rc %d\n", me, rc);
	TracePrintf(0," %d terminated\n", me);
	Exit(pid);
      }

    }
      
  }

  TracePrintf(0,"parent %d sleeping\n", init);

  Delay(70);  // was 60


  TracePrintf(0,"parent %d exiting\n", init);



  Exit(0); // not reached, i think
}


