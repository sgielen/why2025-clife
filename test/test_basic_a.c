#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *f = fopen("SYS$OUTPUT", "w+");
    printf("Got file %p\n", f);

	void* x = malloc(10);
	while (1) {
		fprintf(f, "Hello ELF world A!\n");
        fflush(f);
		sleep(1);
	}

    fclose(f);
	return 0;
}
