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

#include <stdint.h>

#define FRAMEBUFFER_MAX_W 720
#define FRAMEBUFFER_MAX_H 720
#define FRAMEBUFFER_BPP   2 // 16 bits per pixel

#define FRAMEBUFFER_BYTES (FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_W * FRAMEBUFFER_BPP)

typedef struct {
    uint32_t  w;
    uint32_t  h;
    uint16_t *pixels;
} framebuffer_t;

__attribute__((always_inline)) static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

__attribute__((always_inline)) static inline void
    convert_rgb888_to_rgb565(uint8_t *rgb888, uint16_t *rgb565, int pixel_count) {
    for (int i = 0; i < pixel_count; i++) {
        uint8_t r = rgb888[i * 3 + 0];
        uint8_t g = rgb888[i * 3 + 1];
        uint8_t b = rgb888[i * 3 + 2];
        rgb565[i] = rgb888_to_rgb565(r, g, b);
    }
}
