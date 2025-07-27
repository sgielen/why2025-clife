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

#include "compositor.h"

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

static window_t         *window_stack      = NULL;
static SemaphoreHandle_t window_stack_lock = NULL;
static SemaphoreHandle_t vsync             = NULL;
static SemaphoreHandle_t ppa               = NULL;

static bool compositor_initialized = false;
static int  cur_fb                 = 1;
uint16_t   *framebuffers[3];

rotation_angle_t rotation = ROTATION_ANGLE_270;

#define WINDOW_MAX_W (FRAMEBUFFER_MAX_W - (2 * BORDER_PX) - SIDE_BAR_PX)
#define WINDOW_MAX_H (FRAMEBUFFER_MAX_H - BORDER_TOP_PX - TOP_BAR_PX)

__attribute__((always_inline)) static inline void push_window(window_t *window) {
    if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get window list mutex");
        abort();
    }

    window_t *head = window_stack;

    if (head) {
        window_t *tail = head->prev;

        window->next = head;
        window->prev = tail;
        head->prev   = window;
        tail->next   = window;
    } else {
        window->prev = window;
        window->next = window;
    }

    window_stack = window;
    xSemaphoreGive(window_stack_lock);
}

__attribute__((always_inline)) static inline void remove_window(window_t *window) {
    if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get window list mutex");
        abort();
    }

    // We are the only window
    if (window->prev == window) {
        window_stack = NULL;
        goto out;
    }
    window_t *prev = window->prev;
    window_t *next = window->next;

    // Only one window left
    if (prev == next) {
        prev->next   = prev;
        prev->prev   = prev;
        window_stack = prev;
        goto out;
    }

    next->prev = prev;
    prev->next = next;
out:
    xSemaphoreGive(window_stack_lock);
}

framebuffer_t *framebuffer_allocate(uint32_t w, uint32_t h) {
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
    framebuffer->framebuffer.format = BADGEVMS_PIXELFORMAT_BGR565;
    // Keep a copy that the user can't easily change
    framebuffer->w                  = w;
    framebuffer->h                  = h;
    framebuffer->format             = BADGEVMS_PIXELFORMAT_BGR565;
    framebuffer->ready              = xSemaphoreCreateBinary();
    atomic_flag_test_and_set(&framebuffer->clean);
    framebuffer->active = true;

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

    return (framebuffer_t *)framebuffer;
}

void framebuffer_free(framebuffer_t *framebuffer) {
    if (framebuffer) {
        if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to get window list mutex");
            abort();
        }

        managed_framebuffer_t *managed_framebuffer = (managed_framebuffer_t *)framebuffer;
        atomic_store_explicit(&managed_framebuffer->active, false, memory_order_release);

        framebuffer_unmap_pages(managed_framebuffer->pages);
        pages_deallocate(managed_framebuffer->pages);
        framebuffer_vaddr_deallocate((uintptr_t)managed_framebuffer->framebuffer.pixels);
        // framebuffers that can be free'd are all part of a window_t
        // free(managed_framebuffer);

        xSemaphoreGive(window_stack_lock);
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

    while (1) {
        xSemaphoreTake(vsync, portMAX_DELAY);

        if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to get window list mutex");
            abort();
        }

        event_t c;
        c.type      = EVENT_NONE;
        ssize_t res = keyboard_device->_read(keyboard_device, 0, &c, sizeof(event_t));

        if (res && window_stack) {
            ESP_LOGV(TAG, "Got scancode %02X mods %02X", c.e.keyboard.scancode, c.e.keyboard.mod);
            if (c.e.keyboard.scancode == KEY_SCANCODE_TAB && c.e.keyboard.mod & BADGEVMS_KMOD_LALT &&
                c.e.keyboard.down) {
                ESP_LOGV(TAG, "ALT-TAB");
                window_stack = window_stack->next;
                c.type       = EVENT_NONE;
            }

            if (c.type != EVENT_NONE) {
                if (xQueueSend(window_stack->event_queue, &c, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "Unable to send event to task");
                }
            }
        }

        bool changes = false;

        if (window_stack) {
            window_t *window = window_stack->prev; // Start with back window

            do {
                changes = true;

                int content_x = window->position.x + BORDER_PX;
                int content_y = window->position.y + BORDER_TOP_PX;

                int rotated_w, rotated_h;
                if (rotation == ROTATION_ANGLE_90 || rotation == ROTATION_ANGLE_270) {
                    rotated_w = window->size.h;
                    rotated_h = window->size.w;
                } else {
                    rotated_w = window->size.w;
                    rotated_h = window->size.h;
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

                managed_framebuffer_t *framebuffer = window->framebuffers[window->cur_fb];

                if (!framebuffer) {
                    goto skip_draw;
                }

                if (atomic_load_explicit(&framebuffer->active, memory_order_acquire) == false) {
                    goto skip_draw;
                }

                ppa_srm_oper_config_t oper_config = {
                    .in.buffer         = framebuffer->framebuffer.pixels,
                    .in.pic_w          = framebuffer->w,
                    .in.pic_h          = framebuffer->h,
                    .in.block_w        = framebuffer->w,
                    .in.block_h        = framebuffer->h,
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
                    window = window->prev;
                    continue;
                }

                xSemaphoreGive(framebuffer->ready);
            skip_draw:
                // Invalidate caches so windowbox drawing doesn't mess up the screen
                esp_cache_msync(framebuffers[cur_fb], FRAMEBUFFER_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
                draw_window_box(framebuffers[cur_fb], window, window == window_stack);
                // Make sure that the PPA will see the current frame including our decorations
                esp_cache_msync(
                    framebuffers[cur_fb],
                    FRAMEBUFFER_BYTES,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE
                );

                window = window->prev;
            } while (window != window_stack->prev);
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

__attribute__((always_inline)) static inline window_size_t window_clamp_size(window_t *window, window_size_t size) {
    window_size_t ret;

    if (window->flags & WINDOW_FLAG_FULLSCREEN) {
        ret.w = size.w > FRAMEBUFFER_MAX_W ? FRAMEBUFFER_MAX_W : size.w;
        ret.h = size.h > FRAMEBUFFER_MAX_H ? FRAMEBUFFER_MAX_H : size.h;
    } else {
        ret.w = size.w > WINDOW_MAX_W ? WINDOW_MAX_W : size.w;
        ret.h = size.h > WINDOW_MAX_H ? WINDOW_MAX_H : size.h;
    }

    return ret;
}

__attribute__((always_inline)) static inline window_coords_t
    window_clamp_position(window_t *window, window_coords_t position) {
    window_coords_t ret;

    if (window->flags & WINDOW_FLAG_FULLSCREEN) {
        ret.x = 0;
        ret.y = 0;
    } else {
        ret.x = (position.x + window->size.w) > FRAMEBUFFER_MAX_W ? 0 : position.x;
        ret.y = (position.y + window->size.h) > FRAMEBUFFER_MAX_H ? 0 : position.y;
    }

    return ret;
}

window_t *window_create(char const *title, window_size_t size, window_flag_t flags) {
    task_info_t *task_info = get_task_info();

    window_t *window = calloc(1, sizeof(window_t));
    if (!window) {
        ESP_LOGW(TAG, "Unable to allocate window");
        goto error;
    }

    if (title) {
        window_title_set(window, title);
        if (!window->title) {
            ESP_LOGW(TAG, "Unable to allocate window title");
            goto error;
        }
    }

    window->event_queue = xQueueCreate(WINDOW_MAX_EVENTS, sizeof(event_t));
    if (!window->event_queue) {
        ESP_LOGW(TAG, "Out of memory trying to allocate window event queue");
        goto error;
    }

    window->flags      = flags;
    window->task_info  = task_info;
    window->position.x = 0;
    window->position.y = 0;
    size               = window_clamp_size(window, size);
    window->size       = size;

    task_record_resource_alloc(RES_WINDOW, window);
    push_window(window);
    return window;
error:
    free(window->title);
    free(window);
    return NULL;
}

void window_destroy(window_t *window) {
    if (!window) {
        return;
    }

    ESP_LOGI(TAG, "Destroying window %p\n", window);

    remove_window(window);
    task_record_resource_free(RES_WINDOW, window);
    vQueueDelete(window->event_queue);

    for (int i = 0; i < WINDOW_MAX_FRAMEBUFFER; ++i) {
        ESP_LOGV(TAG, "Destroying framebuffer %u for window %p", i, window);
        framebuffer_free((framebuffer_t *)window->framebuffers[i]);
    }
    free(window->title);
    free(window);
}

char const *window_title_get(window_t *window) {
    return window->title;
}

void window_title_set(window_t *window, char const *title) {
    free(window->title);
    if (title) {
        window->title = strndup(title, 20);
        if (!window->title) {
            ESP_LOGW(TAG, "Unable to allocate window title");
        }
    }
}

window_coords_t window_position_get(window_t *window) {
    return window->position;
}

window_coords_t window_position_set(window_t *window, window_coords_t coords) {
    coords           = window_clamp_position(window, coords);
    window->position = coords;
    return coords;
}

window_size_t window_size_get(window_t *window) {
    return window->size;
}

window_size_t window_size_set(window_t *window, window_size_t size) {
    size         = window_clamp_size(window, size);
    window->size = size;
    return size;
}

window_flag_t window_flags_get(window_t *window) {
    return window->flags;
}

window_flag_t window_flags_set(window_t *window, window_flag_t flags) {
    window->flags = flags;
    return flags;
}

framebuffer_t *window_framebuffer_allocate(window_handle_t window, window_size_t size, int *num) {
    if (window->num_fb == WINDOW_MAX_FRAMEBUFFER) {
        ESP_LOGW(TAG, "Window already has the max number of framebuffers");
        return NULL;
    }

    size.w = size.w > window->size.w ? window->size.w : size.w;
    size.h = size.h > window->size.h ? window->size.h : size.h;

    size              = window_clamp_size(window, size);
    framebuffer_t *fb = framebuffer_allocate(size.w, size.h);
    if (!fb) {
        ESP_LOGW(TAG, "Unable to allocate framebuffer");
        return NULL;
    }

    window->framebuffers[window->num_fb] = (managed_framebuffer_t *)fb;

    if (num) {
        *num = window->num_fb;
    }

    window->num_fb++;
    return fb;
}

void window_framebuffer_free(window_handle_t window, int num) {
    framebuffer_t *fb = window_framebuffer_get(window, num);

    if (fb) {
        window->cur_fb = num - 1 > 0 ? num - 1 : 0;
        window->num_fb--;
        window->framebuffers[num] = NULL;
        framebuffer_free((framebuffer_t *)window->framebuffers[num]);
    }
}

__attribute__((always_inline)) inline framebuffer_t *window_framebuffer_get(window_handle_t window, int num) {
    if (!window->num_fb) {
        ESP_LOGW(TAG, "No framebuffers to get");
        return NULL;
    }

    if (num > window->num_fb - 1) {
        ESP_LOGW(TAG, "Framebuffer num out of range");
        return NULL;
    }

    return (framebuffer_t *)window->framebuffers[num];
}

void window_framebuffer_update(window_t *window, int num, bool block, window_rect_t *rects, int num_rects) {
    framebuffer_t *fb = window_framebuffer_get(window, num);
    if (!fb) {
        ESP_LOGW(TAG, "Posting non-existant framebuffer");
        return;
    }

    managed_framebuffer_t *managed_framebuffer = (managed_framebuffer_t *)fb;
    if (num != window->cur_fb) {
        window->cur_fb = num;
    }

    atomic_flag_clear(&managed_framebuffer->clean);

    ESP_LOGV(TAG, "Posting framebuffer %p", managed_framebuffer);

    if (block) {
        ESP_LOGV(TAG, "Suspending myself");
        xSemaphoreTake(managed_framebuffer->ready, portMAX_DELAY);
    }
}

event_t window_event_poll(window_t *window, bool block, uint32_t timeout_msec) {
    event_t    e;
    TickType_t wait = block ? portMAX_DELAY : timeout_msec / portTICK_PERIOD_MS;

    if (xQueueReceive(window->event_queue, &e, wait) != pdTRUE) {
        e.type = EVENT_NONE;
    }

    return e;
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
