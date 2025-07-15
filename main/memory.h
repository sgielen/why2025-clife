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

#pragma once

#include "dlmalloc.h"
#include "esp_log.h"
#include "soc/soc.h"

#define VADDR_START      SOC_EXTRAM_LOW
#define VADDR_TASK_START VADDR_START + SOC_MMU_PAGE_SIZE
#define ADDR_TO_PADDR(a) (a - VADDR_START)
#define PADDR_TO_ADDR(a) (a + VADDR_START)

typedef struct allocation_range_s {
    uintptr_t                  vaddr_start;
    uintptr_t                  paddr_start;
    size_t                     size;
    struct allocation_range_s *next;
} allocation_range_t;

typedef struct task_info task_info_t;

void *why_sbrk(intptr_t increment);
void  memory_init();
