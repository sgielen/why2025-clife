#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	void* x = malloc(10);
	fprintf(stdout, "Hello ELF world A!\n");

    FILE *f = fopen("FLASH0:TESTFILE", "r");
    printf("fopen: %p\n", f);
    if (f) {
        char *x = malloc(1024);
        size_t r = fread(x, 1, 1000, f);
        printf("Read: %zi bytes\n", r);
        x[r] = '\0';
        printf("String: %s\n", x);
    }

	return 0;
}
