#include <yuser.h>

int main(int argc, char **argv) {
    TracePrintf(0, "test: my pid is %d\n", GetPid());

    TracePrintf(0, "test: about to delay 3 ticks\n");
    Delay(3);
    TracePrintf(0, "test: back from delay!\n");

    TracePrintf(0, "test: about to delay 5 ticks\n");
    Delay(5);
    TracePrintf(0, "test: back from second delay!\n");

    while (1) {
        Pause();
    }
}