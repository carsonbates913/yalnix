#include <stdio.h>
#include <yuser.h>

int main() {
    TtyPrintf(1, "hello from init!\n");

    TtyPrintf(1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA done!\n");

    TtyPrintf(1, "first\n");
    TtyPrintf(1, "second\n");
    TtyPrintf(1, "third\n");

    while (1) Pause();
}