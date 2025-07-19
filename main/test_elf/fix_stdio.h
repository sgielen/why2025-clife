#include <stdio.h>
#undef stdin
#undef stdout
#undef stderr

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;

/* The stdin, stdout, and stderr symbols are described as macros in the C
 * standard. */
#define stdin  stdin
#define stdout stdout
#define stderr stderr
