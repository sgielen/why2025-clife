/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2025 HP van Braam <hp@tmm.cx>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_BADGEVMS

#include "../../SDL3/src/video/SDL_sysvideo.h"
#include "../../SDL3/src/SDL_properties_c.h"
#include "SDL_badgevmsframebuffer_c.h"
#include "SDL_badgevmsvideo.h"

#define BADGEVMS_SURFACE "SDL.internal.window.surface"

bool SDL_BADGEVMS_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch)
{
    SDL_WindowData *data = window->internal;
    SDL_Surface *surface;
    const SDL_PixelFormat surface_format = SDL_PIXELFORMAT_BGR565;
    int w, h;

    if (!data || !data->framebuffer) {
        return SDL_SetError("Window not properly initialized");
    }

    SDL_GetWindowSizeInPixels(window, &w, &h);
    
    // Create SDL surface that wraps our BadgeVMS framebuffer
    surface = SDL_CreateSurfaceFrom(data->framebuffer->w, data->framebuffer->h, 
                                   surface_format, 
                                   data->framebuffer->pixels, 
                                   data->framebuffer->w * 2);
    if (!surface) {
        return false;
    }

    SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), BADGEVMS_SURFACE, surface);
    
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    
    return true;
}

bool SDL_BADGEVMS_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *data = window->internal;
    SDL_Surface *surface;

    if (!data || !data->framebuffer) {
        return SDL_SetError("Window not properly initialized");
    }

    surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), BADGEVMS_SURFACE, NULL);
    if (!surface) {
        return SDL_SetError("Couldn't find BadgeVMS surface for window");
    }

    framebuffer_post(data->framebuffer, true);

    return true;
}

void SDL_BADGEVMS_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_Surface *surface;
    
    surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), BADGEVMS_SURFACE, NULL);
    if (surface) {
        SDL_DestroySurface(surface);
    }
    SDL_ClearProperty(SDL_GetWindowProperties(window), BADGEVMS_SURFACE);
}

#endif // SDL_VIDEO_DRIVER_BADGEVMS
