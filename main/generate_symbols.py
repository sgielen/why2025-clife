#!/usr/bin/env python3

import sys
import yaml

TEMPLATE = """
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <string.h>
#include <stdint.h>

{includes}

struct esp_elfsym {{
    const char  *name;
    const void  *sym;
}};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
{definitions}
#pragma GCC diagnostic pop

struct esp_elfsym why2025_elfsyms[] = {{
{symbols}
}};

#define NUM_SYMBOLS {num_symbols}

int symbol_compare(const void *a, const void *b) {{
    const struct esp_elfsym *sym_a = (const struct esp_elfsym *)a;
    const struct esp_elfsym *sym_b = (const struct esp_elfsym *)b;
    return strcmp(sym_a->name, sym_b->name);
}}

uintptr_t elf_find_sym(const char *sym_name) {{
    struct esp_elfsym key = {{ .name = sym_name }};
    const struct esp_elfsym *res;

    res = (const struct esp_elfsym *)bsearch(
        &key,
        why2025_elfsyms,
        NUM_SYMBOLS,
        sizeof(struct esp_elfsym),
        symbol_compare
    );

    if (res) {{
        return (uintptr_t)res->sym;
    }}

    return (uintptr_t)NULL;
}}
"""

symbols = []
seen_symbols = []
def add_sym(sym, wrap):
    if sym in seen_symbols:
        print(f"Duplicate symbol {sym} exiting")
        exit(1)

    seen_symbols.append(sym)

    if wrap:
        symbols.append(f'{{"{sym}", &why_{sym}}}')
    else:
        symbols.append(f'{{"{sym}", &{sym}}}')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {argv[0]} symbol_file.yml output_source.c")
        exit(1)

    seen_symbols = []

    input_symbols = {}
    include = []
    symbol_definitions = []
    num_symbols = 0

    with open(sys.argv[1], 'r') as file:
        input_symbols = yaml.safe_load(file)

    if input_symbols['simple_function']:
        num_symbols += len(input_symbols['simple_function'])
        for sym in input_symbols['simple_function']:
            add_sym(sym, False)

    if input_symbols['simple_function_extern']:
        num_symbols += len(input_symbols['simple_function_extern'])
        for sym in input_symbols['simple_function_extern']:
            symbol_definitions.append(f"extern void {sym}();")
            add_sym(sym, False)

    if input_symbols['include']:
        for file in input_symbols['include']:
            include.append(f"#include <{file}>")

    if input_symbols['simple_object']:
        num_symbols += len(input_symbols['simple_object'])
        for sym in input_symbols['simple_object']:
            symbol_definitions.append(f"extern int {sym};")
            add_sym(sym, False)

    if input_symbols['wrapped_function']:
        num_symbols += len(input_symbols['wrapped_function'])
        for sym in input_symbols['wrapped_function']:
            symbol_definitions.append(f"extern void why_{sym}();")
            add_sym(sym, True)

    if input_symbols['wrapped_object']:
        num_symbols += len(input_symbols['wrapped_object'])
        for sym in input_symbols['wrapped_object']:
            symbol_definitions.append(f"extern int why_{sym};")
            add_sym(sym, True)

    symbols.sort()
    with open(sys.argv[2], 'w') as file:
        file.write(TEMPLATE.format(
            num_symbols = num_symbols,
            includes = "\n".join(include),
            definitions = "\n".join(symbol_definitions),
            symbols = ",\n".join(symbols))
        )
  
