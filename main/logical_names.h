#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void  logical_names_system_init();
int   logical_name_set(char const *logical_name, char const *target, bool is_terminal);
void  logical_name_del(char const *logical_name);
char *logical_name_resolve(char *logical_name);
char *logical_name_resolve_const(char const *logical_name);
