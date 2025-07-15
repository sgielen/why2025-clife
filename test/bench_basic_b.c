#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#define NUM_ITERATIONS 10000
#define ALLOC_SIZE 4096

volatile int should_exit = 0;

int main(int argc, char *argv[]) {
    struct timeval start, end;
    gettimeofday(&start, NULL);
    char task_id = (char)getpid();

    char* ptrs[100];

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Allocate some memory
        char* ptr = malloc(ALLOC_SIZE);
        if (ptr) {
            memset(ptr, task_id, ALLOC_SIZE);
            ptrs[i % 100] = ptr;

            // Free older allocations to avoid OOM
            if (i > 50 && ptrs[(i - 50) % 100]) {
                if (ptrs[(i - 50) % 100]) {
                    for (int k = 0; k < ALLOC_SIZE; ++k) {
                        volatile char t;
                        // Read all memory back
                        t = ptrs[(i - 50) % 100][k];

                        if (t != task_id) {
                            printf("Error unexpected memory contents! expected 0x%X got 0x%X (%c) in ptr number %i\n", task_id, t, t, (i - 50) % 100);
                            return 1;
                        }
                        ptrs[(i - 50) % 100][k] = '$';
                    }
                    free(ptrs[(i - 50) % 100]);
                    ptrs[(i - 50) % 100] = NULL;
                }
            }
        }

        // Force context switch every few iterations
        if (i % 10 == 0) {
            usleep(1);
        }
    }

    // Clean up remaining allocations
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) free(ptrs[i]);
    }

    gettimeofday(&end, NULL);

    long microseconds = (end.tv_sec - start.tv_sec) * 1000000 +
                       (end.tv_usec - start.tv_usec);

    printf("Task %d completed in %ld microseconds\n", task_id, microseconds);
    printf("Task %d: %ld microseconds per iteration\n", task_id, microseconds / NUM_ITERATIONS);
}
