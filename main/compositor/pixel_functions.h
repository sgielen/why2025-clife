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

#include "badgevms_config.h"

#include <stdint.h>

typedef enum {
    ROTATION_ANGLE_0,
    ROTATION_ANGLE_90,
    ROTATION_ANGLE_180,
    ROTATION_ANGLE_270,
} rotation_angle_t;

__attribute__((always_inline)) inline static void
    rotate_coordinates(int x, int y, rotation_angle_t rotation, int *fb_x, int *fb_y) {
    switch (rotation) {
        case ROTATION_ANGLE_0:
            *fb_x = x;
            *fb_y = y;
            break;

        case ROTATION_ANGLE_90:
            *fb_x = (FRAMEBUFFER_MAX_H - 1) - y;
            *fb_y = x;
            break;

        case ROTATION_ANGLE_180:
            *fb_x = (FRAMEBUFFER_MAX_W - 1) - x;
            *fb_y = (FRAMEBUFFER_MAX_H - 1) - y;
            break;

        case ROTATION_ANGLE_270:
            *fb_x = y;
            *fb_y = (FRAMEBUFFER_MAX_W - 1) - x;
            break;
    }
}

void draw_pixel_rotated(uint16_t *fb, int x, int y, uint16_t color);
void draw_filled_rect_rotated(uint16_t *fb, int x, int y, int width, int height, uint16_t color);
void draw_rect_rotated(uint16_t *fb, int x, int y, int width, int height, uint16_t color);
int  char_to_font_index(char c);
void draw_char_rotated(uint16_t *fb, char c, int x, int y, uint16_t color);
void draw_text_rotated(uint16_t *fb, char const *text, int x, int y, uint16_t color);
