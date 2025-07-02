#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	void* x = malloc(10);
	while (1) {
		printf("Hello ELF world A!\n");
		sleep(1);
	}
	return 0;
}
