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

#include "badgevms_config.h"
#include "compositor_private.h"
#include "device.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"
#include "event.h"
#include "font.h"
#include "memory.h"
#include "pixel_formats.h"
#include "pixel_functions.h"
#include "task.h"
#include "window_decorations.h"

#include <stdatomic.h>

#include <sys/time.h>

#define TAG "compositor"

static TaskHandle_t  compositor_handle;
static lcd_device_t *lcd_device;
static device_t     *keyboard_device;

static managed_framebuffer_t *window_stack      = NULL;
static SemaphoreHandle_t      window_stack_lock = NULL;
static SemaphoreHandle_t      vsync             = NULL;
static SemaphoreHandle_t      ppa               = NULL;

static bool compositor_initialized = false;
static int  cur_fb                 = 1;
uint16_t   *framebuffers[3];

rotation_angle_t rotation = ROTATION_ANGLE_270;

__attribute__((always_inline)) static inline void push_framebuffer(managed_framebuffer_t *framebuffer) {
    if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get window list mutex");
        abort();
    }

    managed_framebuffer_t *head = window_stack;

    if (head) {
        managed_framebuffer_t *tail = head->prev;

        framebuffer->next = head;
        framebuffer->prev = tail;
        head->prev        = framebuffer;
        tail->next        = framebuffer;
    } else {
        framebuffer->prev = framebuffer;
        framebuffer->next = framebuffer;
    }

    window_stack = framebuffer;
    xSemaphoreGive(window_stack_lock);
}

__attribute__((always_inline)) static inline void remove_framebuffer(managed_framebuffer_t *framebuffer) {
    if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get window list mutex");
        abort();
    }

    xSemaphoreGive(window_stack_lock);
}

static int fb_count = 0;

framebuffer_t *framebuffer_allocate(uint32_t w, uint32_t h) {
    task_info_t *task_info = get_task_info();
    ESP_LOGW(TAG, "Mapping framebuffer for task %u", task_info->pid);

    size_t    num_pages        = 0;
    size_t    framebuffer_size = w * h * FRAMEBUFFER_BPP;
    uintptr_t vaddr_start      = framebuffer_vaddr_allocate(framebuffer_size, &num_pages);
    if (!vaddr_start) {
        ESP_LOGE(TAG, "No vaddr space for frame buffer");
        return NULL;
    }

    managed_framebuffer_t *framebuffer = calloc(1, sizeof(managed_framebuffer_t));
    if (!framebuffer) {
        ESP_LOGE(TAG, "No kernel RAM for frame buffer container");
        framebuffer_vaddr_deallocate(vaddr_start);
        return NULL;
    }

    allocation_range_t *tail_range = NULL;
    if (!pages_allocate(vaddr_start, num_pages, &framebuffer->pages, &tail_range)) {
        ESP_LOGE(TAG, "No physical memory pages for frame buffer");
        framebuffer_vaddr_deallocate(vaddr_start);
        free(framebuffer);
        return NULL;
    }

    ESP_LOGI(
        TAG,
        "Attempting to map our vaddr %p head_range %p tail_range %p",
        (void *)framebuffer->pages->vaddr_start,
        framebuffer->pages,
        tail_range
    );
    framebuffer_map_pages(framebuffer->pages, tail_range);

    framebuffer->framebuffer.pixels = (uint16_t *)vaddr_start;
    framebuffer->framebuffer.w      = w;
    framebuffer->framebuffer.h      = h;
    framebuffer->x                  = 0;
    framebuffer->y                  = 0;
    framebuffer->task_info          = task_info;
    framebuffer->ready              = xSemaphoreCreateBinary();
    atomic_flag_test_and_set(&framebuffer->clean);

    if (fb_count > 3) {
        framebuffer->x = (((fb_count - 3)) * (w / 3) - (2 * BORDER_PX));
        framebuffer->y = (((fb_count - 3)) * (h / 3) - BORDER_TOP_PX);
    }
    fb_count++;

    // vTaskPrioritySet(framebuffer->task_info->handle, TASK_PRIORITY_FOREGROUND);
    ESP_LOGW(
        TAG,
        "Allocated framebuffer at %p, pixels at %p, size %zi, dimensions %u x %u",
        framebuffer,
        framebuffer->framebuffer.pixels,
        num_pages,
        framebuffer->framebuffer.w,
        framebuffer->framebuffer.h
    );

    if (compositor_initialized) {
        // If we aren't initialized yet we're allocating lcd driver framebuffers,
        // they are not windows
        push_framebuffer(framebuffer);
    }

    return (framebuffer_t *)framebuffer;
}

void framebuffer_free(framebuffer_t *framebuffer) {
    managed_framebuffer_t *managed_framebuffer = (managed_framebuffer_t *)framebuffer;
    framebuffer_vaddr_deallocate((uintptr_t)managed_framebuffer->framebuffer.pixels);
    pages_deallocate(managed_framebuffer->pages);
    free(managed_framebuffer);
}

void framebuffer_post(framebuffer_t *framebuffer, bool block) {
    managed_framebuffer_t *managed_framebuffer = (managed_framebuffer_t *)framebuffer;
    atomic_flag_clear(&managed_framebuffer->clean);

    ESP_LOGV(TAG, "Posting framebuffer %p", framebuffer);

    if (block) {
        ESP_LOGV(TAG, "Suspending myself");
        xSemaphoreTake(managed_framebuffer->ready, portMAX_DELAY);
    }
}

IRAM_ATTR static void on_refresh(void *ignored) {
    xSemaphoreGive(vsync);

    // if (compositor_initialized) {
    // }
}

IRAM_ATTR static bool
    ppa_event_callback(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data) {
    xSemaphoreGive(ppa);
    return true;
}

static void IRAM_ATTR NOINLINE_ATTR compositor(void *ignored) {
    static ppa_client_handle_t ppa_srm_handle = NULL;

    ppa_client_config_t ppa_srm_config = {
        .oper_type             = PPA_OPERATION_SRM,
        .max_pending_trans_num = 2,
    };

    ppa_event_callbacks_t ppa_srm_callbacks = {
        .on_trans_done = ppa_event_callback,
    };

    ppa_register_client(&ppa_srm_config, &ppa_srm_handle);
    ppa_client_register_event_callbacks(ppa_srm_handle, &ppa_srm_callbacks);

    ESP_LOGI(TAG, "framebuffers[0] = %p, framebuffers[1] = %p", framebuffers[0], framebuffers[1]);
    while (1) {
        xSemaphoreTake(vsync, portMAX_DELAY);

        if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to get window list mutex");
            abort();
        }

        event_t c;
        c.type      = EVENT_NONE;
        ssize_t res = keyboard_device->_read(keyboard_device, 0, &c, sizeof(event_t));

        if (window_stack) {
            if (c.e.keyboard.scancode == KEY_SCANCODE_TAB && c.e.keyboard.mod == BADGEVMS_KMOD_LALT &&
                c.e.keyboard.down) {
                window_stack = window_stack->next;
                c.type       = EVENT_NONE;
            }

            if (c.type != EVENT_NONE) {
                if (xQueueSend(window_stack->task_info->event_queue, &c, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "Unable to send event to task");
                }
            }
        }

        bool changes = false;

        if (window_stack) {
            managed_framebuffer_t *framebuffer = window_stack->prev; // Start with back window

            do {
                changes = true;

                int content_x = framebuffer->x + BORDER_PX;
                int content_y = framebuffer->y + BORDER_TOP_PX;

                int rotated_w, rotated_h;
                if (rotation == ROTATION_ANGLE_90 || rotation == ROTATION_ANGLE_270) {
                    rotated_w = framebuffer->framebuffer.h;
                    rotated_h = framebuffer->framebuffer.w;
                } else {
                    rotated_w = framebuffer->framebuffer.w;
                    rotated_h = framebuffer->framebuffer.h;
                }

                if (content_x + rotated_w > FRAMEBUFFER_MAX_W || content_y + rotated_h > FRAMEBUFFER_MAX_H) {
                    printf(
                        "ERROR: Window %dx%d at (%d,%d) would exceed screen bounds %dx%d after rotation\n",
                        rotated_w,
                        rotated_h,
                        content_x,
                        content_y,
                        FRAMEBUFFER_MAX_W,
                        FRAMEBUFFER_MAX_H
                    );

                    // Clamp to screen bounds
                    if (content_x + rotated_w > FRAMEBUFFER_MAX_W) {
                        content_x = FRAMEBUFFER_MAX_W - rotated_w;
                    }
                    if (content_y + rotated_h > FRAMEBUFFER_MAX_H) {
                        content_y = FRAMEBUFFER_MAX_H - rotated_h;
                    }
                    if (content_x < 0)
                        content_x = 0;
                    if (content_y < 0)
                        content_y = 0;

                    printf("Clamped to (%d,%d)\n", content_x, content_y);
                }

                int fb_x = 0, fb_y = 0;
                rotate_coordinates(content_x, content_y, rotation, &fb_x, &fb_y);

                int final_fb_x = fb_x;
                int final_fb_y = fb_y;

                ppa_srm_rotation_angle_t ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
                // Adjust for rotation anchor point
                if (rotation == ROTATION_ANGLE_270) {
                    final_fb_x   = fb_x;
                    final_fb_y   = fb_y - rotated_h + 1;
                    ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
                } else if (rotation == ROTATION_ANGLE_180) {
                    final_fb_x   = fb_x - rotated_w + 1;
                    final_fb_y   = fb_y - rotated_h + 1;
                    ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
                } else if (rotation == ROTATION_ANGLE_90) {
                    final_fb_x   = fb_x - rotated_h + 1;
                    ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
                }

                // Final bounds check after adjustment
                if (final_fb_x < 0 || final_fb_y < 0 || final_fb_x + rotated_w > FRAMEBUFFER_MAX_W ||
                    final_fb_y + rotated_h > FRAMEBUFFER_MAX_H) {

                    printf(
                        "ERROR: Final position (%d,%d) with size %dx%d exceeds bounds\n",
                        final_fb_x,
                        final_fb_y,
                        rotated_w,
                        rotated_h
                    );

                    // Skip this window or clamp further
                    framebuffer = framebuffer->prev;
                    continue;
                }

                ppa_srm_oper_config_t oper_config = {
                    .in.buffer         = framebuffer->framebuffer.pixels,
                    .in.pic_w          = framebuffer->framebuffer.w,
                    .in.pic_h          = framebuffer->framebuffer.h,
                    .in.block_w        = framebuffer->framebuffer.w,
                    .in.block_h        = framebuffer->framebuffer.h,
                    .in.block_offset_x = 0,
                    .in.block_offset_y = 0,
                    .in.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,

                    .out.buffer         = framebuffers[cur_fb],
                    .out.buffer_size    = FRAMEBUFFER_BYTES,
                    .out.pic_w          = FRAMEBUFFER_MAX_W,
                    .out.pic_h          = FRAMEBUFFER_MAX_H,
                    .out.block_offset_x = final_fb_x,
                    .out.block_offset_y = final_fb_y,
                    .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,

                    .rotation_angle = ppa_rotation,
                    .scale_x        = 1.0,
                    .scale_y        = 1.0,
                    .rgb_swap       = 0,
                    .byte_swap      = 0,
                    .mode           = PPA_TRANS_MODE_BLOCKING,
                };

#if 0
        printf("Compositing %ldx%ld at (%d,%d) -> rotated %dx%d at (%d,%d)\n",
               framebuffer->framebuffer.w, framebuffer->framebuffer.h,
               content_x, content_y,
               rotated_w, rotated_h,
               final_fb_x, final_fb_y);
#endif

                esp_err_t ppa_result = ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
                if (ppa_result != ESP_OK) {
                    printf("PPA operation failed: %s\n", esp_err_to_name(ppa_result));
                    framebuffer = framebuffer->prev;
                    continue;
                }

                xSemaphoreGive(framebuffer->ready);

                // Invalidate caches so windowbox drawing doesn't mess up the screen
                esp_cache_msync(framebuffers[cur_fb], FRAMEBUFFER_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
                draw_window_box(framebuffers[cur_fb], framebuffer, framebuffer == window_stack);
                // Make sure that the PPA will see the current frame including our decorations
                esp_cache_msync(
                    framebuffers[cur_fb],
                    FRAMEBUFFER_BYTES,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE
                );

                framebuffer = framebuffer->prev;
            } while (framebuffer != window_stack->prev);
        }

        xSemaphoreGive(window_stack_lock);

        if (changes) {
            lcd_device->_draw(lcd_device, 0, 0, FRAMEBUFFER_MAX_W, FRAMEBUFFER_MAX_H, framebuffers[cur_fb]);
            cur_fb = (cur_fb + 1) % 3;
        }
    }
}

void get_screen_info(int *width, int *height, pixel_format_t *format, float *refresh_rate) {
    *width        = FRAMEBUFFER_MAX_W;
    *height       = FRAMEBUFFER_MAX_H;
    *format       = BADGEVMS_PIXELFORMAT_BGR565;
    *refresh_rate = FRAMEBUFFER_MAX_REFRESH;
}

void compositor_init(char const *lcd_device_name, char const *keyboard_device_name) {
    ESP_LOGI(TAG, "Initializing");

    lcd_device = (lcd_device_t *)device_get(lcd_device_name);
    if (!lcd_device) {
        ESP_LOGE(TAG, "Unable to access the LCD device '%s'", lcd_device_name);
        return;
    }

    keyboard_device = (device_t *)device_get(keyboard_device_name);
    if (!keyboard_device) {
        ESP_LOGE(TAG, "Unable to access the keyboard device '%s'", keyboard_device_name);
        // return;
    }

    lcd_device->_getfb(lcd_device, 0, (void *)&framebuffers[0]);
    lcd_device->_getfb(lcd_device, 1, (void *)&framebuffers[1]);
    lcd_device->_getfb(lcd_device, 2, (void *)&framebuffers[2]);

    memset(framebuffers[0], 0xaa, FRAMEBUFFER_BYTES);
    memset(framebuffers[1], 0xaa, FRAMEBUFFER_BYTES);
    memset(framebuffers[2], 0xaa, FRAMEBUFFER_BYTES);

    esp_cache_msync(framebuffers[0], FRAMEBUFFER_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    esp_cache_msync(framebuffers[1], FRAMEBUFFER_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    esp_cache_msync(framebuffers[2], FRAMEBUFFER_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    cur_fb = 1;

    ESP_LOGW(TAG, "Got framebuffers 0: %p, 1: %p, 2: %p", framebuffers[0], framebuffers[1], framebuffers[2]);
    lcd_device->_set_refresh_cb(lcd_device, NULL, on_refresh);

    window_stack_lock = xSemaphoreCreateMutex();
    vsync             = xSemaphoreCreateBinary();
    ppa               = xSemaphoreCreateBinary();
    create_kernel_task(compositor, "Compositor", 4096, NULL, 20, &compositor_handle, 0);
    compositor_initialized = true;
}
