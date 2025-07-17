#include "generated_symbols.h"

#include <stdlib.h>

#include <string.h>

int symbol_compare(void const *a, void const *b) {
    const struct esp_elfsym *sym_a = (const struct esp_elfsym *)a;
    const struct esp_elfsym *sym_b = (const struct esp_elfsym *)b;
    return strcmp(sym_a->name, sym_b->name);
}

uintptr_t elf_find_sym(char const *sym_name) {
    struct esp_elfsym key = {.name = name};

    return (
        const struct esp_elfsym *
    )bsearch(&key, g_why2025_libc_elfsyms, NUM_SYMBOLS, sizeof(struct esp_elfsym), symbol_compare);
}
