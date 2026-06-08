#ifndef TRAP_H
#define TRAP_H

#include "kernelstart.h"

void HandleTrapKernel(UserContext *user_context);
void HandleTrapClock(UserContext *user_context);
void HandleTrapIllegal(UserContext *user_context);
void HandleTrapMemory(UserContext *user_context);
void HandleTrapMath(UserContext *user_context);
void HandleTrapTtyReceive(UserContext *user_context);
void HandleTrapTtyTransmit(UserContext *user_context);
void HandleTrapDisk(UserContext *user_context);
void HandleTrapUnknown(UserContext *user_context);

#endif
