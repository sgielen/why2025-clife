#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    pid_t pid = getpid();
    char *mymem = malloc(5 * 1024 * 1024);
    memset(mymem, (char)pid + '0', 79);
    mymem[79] = '\0';

    while(1) {
        printf("Hello from pid %i\n", pid);
        printf("Memtest %s\n", mymem);
        sleep(5);
    }
}
