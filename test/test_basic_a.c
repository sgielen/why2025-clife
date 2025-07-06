#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	void* x = malloc(10);
    for (int i = 0; i < 5; ++i) {
		fprintf(stdout, "Hello ELF world A!\n");
		sleep(1);
	}

	return 0;
}
