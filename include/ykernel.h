/* spring 2024: added helper_force_free */

#ifndef _ykernel_h
#define _ykernel_h

#include <ylib.h>
#include <yalnix.h>
#include <hardware.h>

int helper_new_pid( struct pte *ptbr1);
/*
1. find the process control block with the pid.
2. free the memory allocated to the process
3. free the page table
4. remove the process from the process table
5. return success
*/
void helper_retire_pid(int pid);
/*
1. release the pid when the process exist
2. return success
*/
void helper_abort(char *msg);
/*
1. print the message
2. kill the process
3. return success
*/
void helper_maybort(char *msg);
/*
1. print the message
2. kill the process
3. return success
*/
void helper_check_heap(char *msg);
/*
1. check if the frame is free
2. if it is, return success
3. if it is not, return error
4. free the frame
5. return success
*/
void helper_force_free(int frame);
/*
1. check if the frame is free
2. if it is, return success
3. if it is not, return error
4. free the frame
5. return success
*/

#ifndef SUCCESS
#define	SUCCESS			( 0)
#endif

#ifndef ERROR
#define	ERROR			(-1)
#endif

// A special error for LoadProgram:
// the operation cannot be completed, but the old
// region1 has already been thrown away
#ifndef KILL
#define	KILL                    (-2)
#endif




#endif
