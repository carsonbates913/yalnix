#ifndef _YUSER_H_
#define _YUSER_H_

#include <ylib.h>

// syscall wrappers

// spring 2024: deleted unuseds syscalls, and added Shared_Pages

extern int Nop (int,int,int,int);

/*
Needed Data Structures:
Process Control Block
Page Table

*/

/*
Fork:
1. Create a new process id number
2. Create a new process control block with the process id and the parent process id
3. Create a new page table for the process + add its address to the process control block
4. Add the process control block to the process table
5. Return the new process id
*/
extern int Fork (void);

/*
Exec:
1. Find the process control block with the process id
2. Find the page table for the process
3. Load the page table into the processor
4. 
*/
extern int Exec (char *, char **);

/*
1. Retrieve the current process id 
2. Find the process control block with the process id
3. Free all the memory allocated to the process
4. 
3. find the page table address and free it
4. fr
*/
extern void Exit (int);

extern int Wait (int *);
extern int GetPid (void);
extern int Brk (void *);
extern int Delay (int);
extern int TtyRead (int, void *, int);
extern int TtyWrite (int, void *, int);
extern int Register (unsigned int);
extern int Send (void *, int);
extern int Receive (void *);
extern int ReceiveSpecific (void *, int);
extern int Reply (void *, int);
extern int Forward (void *, int, int);
extern int CopyFrom (int, void *, void *, int);
extern int CopyTo (int, void *, void *, int);
extern int ReadSector (int, void *);
extern int WriteSector (int, void *);

extern int PipeInit (int *);
extern int PipeRead (int, void *, int);
extern int PipeWrite (int, void *, int);

extern int SemInit (int *, int);
extern int SemUp (int);
extern int SemDown (int);
extern int LockInit (int *);
extern int Acquire (int);
extern int Release (int);
extern int CvarInit (int *);
extern int CvarWait (int, int);
extern int CvarSignal (int);
extern int CvarBroadcast (int);

extern int Reclaim (int);

extern int Custom0 (int,int,int,int);
extern int Custom1 (int,int,int,int);
extern int Custom2 (int,int,int,int);

extern int Shared_Pages(int);

/*
 * A Yalnix library function: TtyPrintf(num, format, args) works like
 * printf(format, args) on terminal num.
 */
extern int TtyPrintf (int, char *, ...);





#endif
