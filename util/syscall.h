#ifndef SYSCALL_H
#define SYSCALL_H

#include "kernelstart.h"

int HandleFork(void);
int HandleExec(char *path, char **args);
void HandleExit(int status);
int HandleWait(int *status);
int HandleGetPid(void);
int HandleReclaim(int resource_id);
int HandleBrk(void *addr);
int HandleDelay(int time);
int HandleTtyRead(int tty_id, void *buf, int len);
int HandleTtyWrite(int tty_id, void *buf, int len);
int HandlePipeInit(int *pipe_id);
int HandlePipeRead(int pipe_id, void *buf, int len);
int HandlePipeWrite(int pipe_id, void *buf, int len);
int HandleLockInit(int *lock_id);
int HandleAcquire(int lock_id);
int HandleRelease(int lock_id);
int HandleCvarInit(int *cvar_id);
int HandleCvarWait(int cvar_id, int timeout);
int HandleCvarSignal(int cvar_id);
int HandleCvarBroadcast(int cvar_id);

#endif