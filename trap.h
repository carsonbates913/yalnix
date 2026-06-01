#ifndef TRAP_H
#define TRAP_H

#include <kernelstart.h>

void HandleTrapKernel(UserContext *user_context);
void HandleTrapClock(UserContext *user_context);
void HandleTrapIllegal(UserContext *user_context);
void HandleTrapMemory(UserContext *user_context);
void HandleTrapMath(UserContext *user_context);
void HandleTrapTtyReceive(UserContext *user_context);
void HandleTrapTtyTransmit(UserContext *user_context);
void HandleTrapDisk(UserContext *user_context);
void HandleTrapUnknown(UserContext *user_context);
void HandleTrap(UserContext *user_context);

#endif
#include <kernelstart.h>
#include <hardware.h>
#include <string.h>
#include <yuser.h>
#include <yalnix.h>
#include "kernelstart.h"
#include <stdlib.h>
#include <math.h>

#ifndef TRAP_H
#define TRAP_H

static void HandleTrapKernel(UserContext* user_context);
static void HandleTrapClock(UserContext* user_context);
static void HandleTrapIllegal(UserContext* user_context);
static void HandleTrapMemory(UserContext* user_context);
static void HandleTrapMath(UserContext* user_context);
static void HandleTrapTtyReceive(UserContext* user_context);
static void HandleTrapTtyTransmit(UserContext* user_context);
static void HandleTrapDisk(UserContext* user_context);
static void HandleTrapUnknown(UserContext* user_context);
#endif