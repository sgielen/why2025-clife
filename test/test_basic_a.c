#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	fprintf(stdout, "Hello ELF world A!\n");

    {
    void* x = malloc(256 * 1024);
    printf("Got pointer %p\n", x);
    }
    FILE *f = fopen("FLASH0:TESTFILE", "r");
    printf("fopen: %p\n", f);
    if (f) {
        char *x = malloc(1024);
        size_t r = fread(x, 1, 1000, f);
        printf("Read: %zi bytes\n", r);
        x[r] = '\0';
        printf("String: %s\n", x);
        fclose(f);
    }

    {
    void* x = malloc(256 * 1024);
    printf("Got pointer %p\n", x);
    }

    //f = fopen("SEARCH:ANOTHERFILE.NAME", "r");
    f = fopen("SEARCH:NONEXISTENT", "r");
    printf("fopen: %p\n", f);
    if (f) {
        char *x = malloc(1024);
        size_t r = fread(x, 1, 1000, f);
        printf("Read: %zi bytes\n", r);
        x[r] = '\0';
        printf("String: %s\n", x);
        fclose(f);
    } else {
        printf("********** Failed to open File (%s)!\n", strerror(errno));
    }

    {
    void* x = malloc(256 * 1024);
    printf("Got pointer %p\n", x);
    }
    f = fopen("FLASH0:[SUBDIR.ANOTHER]NEW_FILE", "w+");
    printf("fopen: %p\n", f);
    if (f) {
        const char *l = "This is a test write";
        size_t r = fwrite(l, 1, strlen(l), f);
        printf("Write %zi bytes\n", r);
        fclose(f);
    }

    f = fopen("FLASH0:[SUBDIR.ANOTHER]NEW_FILE", "r");
    printf("fopen: %p\n", f);
    if (f) {
        char *x = malloc(1024);
        size_t r = fread(x, 1, 1000, f);
        printf("Read: %zi bytes\n", r);
        x[r] = '\0';
        printf("String: %s\n", x);
    }
    {
    void* x = malloc(256 * 1024);
    printf("Got pointer %p\n", x);
    }
	return 0;
}
