#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void logical_names_system_init();
int logical_name_set(const char *logical_name, const char *target, bool is_terminal);
void logical_name_del(const char *logical_name);
char *logical_name_resolve(char *logical_name);
char *logical_name_resolve_const(const char *logical_name);
