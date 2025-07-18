#include <stdio.h>
#include <stddef.h>
#include <unistd.h>  // for usleep
#include <time.h>    // for clock_gettime
#include <string.h>  // for memcpy
#include "compositor.h"

int main(int argc, char *argv[]) {
    framebuffer_t *framebuffer = framebuffer_allocate(720, 720);
    int frame = 0;
    const long target_frame_time_us = 16667;  // 60 FPS in microseconds
    
    // FPS tracking variables
    int frames_rendered = 0;
    struct timespec fps_start_time;
    clock_gettime(CLOCK_MONOTONIC, &fps_start_time);
    
    while (1) {
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        // Calculate horizontal offset based on frame
        // The pattern repeats every 720 pixels, so we use modulo to wrap
        int offset = (frame * 3) % 720;  // Move 3 pixels per frame
        
        // Create one template row with the current pattern
        static uint16_t row_template[720];
        for (int x = 0; x < 720; x++) {
            int shifted_x = (x + offset) % 720;
            if (shifted_x < 720 / 3) {
                row_template[x] = 0x1F;    // Blue
            } else if (shifted_x < 720 * 2 / 3) {
                row_template[x] = 0x7E0;   // Green
            } else {
                row_template[x] = 0xF800;  // Red
            }
        }
        
        // Copy this row 720 times to fill the entire framebuffer
        uint16_t *pixels = (uint16_t *)framebuffer->pixels;
        for (int y = 0; y < 720; y++) {
            memcpy(&pixels[y * 720], row_template, 720 * sizeof(uint16_t));
        }
        
        struct timespec post_start_time;
        clock_gettime(CLOCK_MONOTONIC, &post_start_time);
        
        framebuffer_post(framebuffer, true);
        
        // Calculate how long the frame took
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        long render_time_us = (post_start_time.tv_sec - start_time.tv_sec) * 1000000L + 
                             (post_start_time.tv_nsec - start_time.tv_nsec) / 1000L;
        long post_time_us = (end_time.tv_sec - post_start_time.tv_sec) * 1000000L + 
                           (end_time.tv_nsec - post_start_time.tv_nsec) / 1000L;
        long elapsed_us = render_time_us + post_time_us;
        
        // Sleep for the remaining time to maintain 60 FPS
        long sleep_time = target_frame_time_us - elapsed_us;
        if (sleep_time > 0) {
            usleep(sleep_time);
        }
        
        frame++;
        frames_rendered++;
        
        // Check if a second has passed and print FPS
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long fps_elapsed_ms = (current_time.tv_sec - fps_start_time.tv_sec) * 1000L + 
                             (current_time.tv_nsec - fps_start_time.tv_nsec) / 1000000L;
        
        if (fps_elapsed_ms >= 1000) {  // One second has passed
            double actual_fps = (double)frames_rendered / (fps_elapsed_ms / 1000.0);
            printf("FPS: %.1f (last frame: render=%ldus, post=%ldus, total=%ldus)\n", 
                   actual_fps, render_time_us, post_time_us, elapsed_us);
            
            // Reset for next second
            frames_rendered = 0;
            fps_start_time = current_time;
        }
    }
}
