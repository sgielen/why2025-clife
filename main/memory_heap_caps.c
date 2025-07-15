/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "esp_attr.h"

// There is no need to wrap the _prefer versions as they just call the base version
extern void *__real_heap_caps_malloc_base( size_t size, uint32_t caps);
extern void *__real_heap_caps_aligned_alloc_base(size_t alignment, size_t size, uint32_t caps);
extern void *__real_heap_caps_calloc_base( size_t n, size_t size, uint32_t caps);
extern void *__real_heap_caps_realloc_base( void *ptr, size_t size, uint32_t caps);
extern void *__real_heap_caps_malloc(size_t size, uint32_t caps);
extern void *__real_heap_caps_malloc_default(size_t size);
extern void *__real_heap_caps_realloc_default(void *ptr, size_t size );
extern void __real_heap_caps_free(void *ptr);
extern void *__real_heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
extern void *__real_heap_caps_calloc(size_t n, size_t size, uint32_t caps);
extern void *__real_heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
extern void __real_heap_caps_aligned_free(void *ptr);
extern void *__real_heap_caps_aligned_calloc(size_t alignment, size_t n, size_t size, uint32_t caps);
extern size_t __real_heap_caps_get_total_size(uint32_t caps);
extern size_t __real_heap_caps_get_free_size(uint32_t caps);
extern size_t __real_heap_caps_get_minimum_free_size(uint32_t caps);
extern size_t __real_heap_caps_get_largest_free_block(uint32_t caps);

IRAM_ATTR void *__wrap_heap_caps_malloc_base( size_t size, uint32_t caps) {
    return __real_heap_caps_malloc_base(size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_aligned_alloc_base(size_t alignment, size_t size, uint32_t caps) {
    return __real_heap_caps_aligned_alloc_base(alignment, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_calloc_base( size_t n, size_t size, uint32_t caps) {
    return __real_heap_caps_calloc_base(n, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_realloc_base( void *ptr, size_t size, uint32_t caps) {
    return __real_heap_caps_realloc_base(ptr, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_malloc(size_t size, uint32_t caps) {
    return __real_heap_caps_malloc(size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_malloc_default(size_t size) {
    return __real_heap_caps_malloc_default(size);
}

IRAM_ATTR void *__wrap_heap_caps_realloc_default(void *ptr, size_t size ) {
    return __real_heap_caps_realloc_default(ptr, size );
}

IRAM_ATTR void __wrap_heap_caps_free(void *ptr) {
    __real_heap_caps_free(ptr);
}

IRAM_ATTR void *__wrap_heap_caps_realloc(void *ptr, size_t size, uint32_t caps) {
    return __real_heap_caps_realloc(ptr, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    return __real_heap_caps_calloc(n, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
    return __real_heap_caps_aligned_alloc(alignment, size, caps);
}

IRAM_ATTR void __wrap_heap_caps_aligned_free(void *ptr) {
    __real_heap_caps_aligned_free(ptr);
}

IRAM_ATTR void *__wrap_heap_caps_aligned_calloc(size_t alignment, size_t n, size_t size, uint32_t caps) {
    return __real_heap_caps_aligned_calloc(alignment, n, size, caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_total_size(uint32_t caps) {
    return __real_heap_caps_get_total_size(caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_free_size(uint32_t caps) {
    return __real_heap_caps_get_free_size(caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_minimum_free_size(uint32_t caps) {
    return __real_heap_caps_get_minimum_free_size(caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_largest_free_block(uint32_t caps) {
    return __real_heap_caps_get_largest_free_block(caps);
}

