
#include <yuser.h>



/* test pipe 0 */

char *me = "test_pipe_w2r";


#define MAXSIZE 10

int pipe_id = -1;

void
writer(int num, char *buf, int len) {
  int rc = 42;


  TracePrintf(0,"writer %d writing\n",num);

  rc = PipeWrite(pipe_id, buf, len);
  TracePrintf(0,"writer %d rc = %d (%d)\n",num, rc,len);

  TracePrintf(0,"writer %d sleeping\n",num);

  Delay(10);
  Exit(0);
}

void
reader(int num, int len) {
  int rc = 42;
  char buf[MAXSIZE];


  TracePrintf(0,"reader %d reading\n",num);

  rc = PipeRead(pipe_id, buf, len);
  TracePrintf(0,"reader %d rc = %d (%d)\n",num, rc,len);
  buf[len] = 0x00;
  TracePrintf(0,"reader %d got [%s]\n",num, buf);

  TracePrintf(0,"reader %d sleeping\n",num);

  Delay(10);
  Exit(0);
}





int
main()
{
  int  rc = 42;



  TracePrintf(0,"-----------------------------------------\n");
  rc = GetPid();

  TracePrintf(0, "%s: Hello, world!  init is pid %d\n",me,rc);

  rc = 42;
  rc = PipeInit(&pipe_id);

  TracePrintf(0,"%s: PipeInit returned %d; pipe has id %d\n",me, rc, pipe_id);


  rc = 42;

  rc = Fork();
  if (0==rc) {
    writer(1,"abcd",4);
  }

  rc = Fork();
  if (0==rc) {
    writer(2,"wxyx",4);
  }


  TracePrintf(0,"parent delaying\n");
  Delay(5);
  TracePrintf(0,"parent back\n");

  rc = Fork();
  if (0==rc) {
    reader(1,4);
  }

  rc = Fork();
  if (0==rc) {
    reader(2,4);
  }
  

  TracePrintf(0,"parent delaying again\n");
  Delay(15);
  TracePrintf(0,"parent back\n");
  

  Exit(0);
}
