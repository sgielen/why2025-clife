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

#include "../../SDL3/src/events/SDL_events_c.h"
#include "../../SDL3/src/events/SDL_keyboard_c.h"

#include "SDL_badgevmsvideo.h"
#include "SDL_badgevmsevents_c.h"

void BADGEVMS_PumpEvents(SDL_VideoDevice *_this)
{
    event_t badgevms_event;
    SDL_Event sdl_event;

    // Poll all available events
    while (true) {
        badgevms_event = event_poll(false, 0);
        
        if (badgevms_event.type == EVENT_NONE) {
            break;  // No more events
        }
        
        SDL_zero(sdl_event);
        
        switch (badgevms_event.type) {
            case EVENT_QUIT:
                sdl_event.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&sdl_event);
                break;
                
            case EVENT_KEY_DOWN:
            case EVENT_KEY_UP:
                sdl_event.type = badgevms_event.e.keyboard.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                sdl_event.key.scancode = badgevms_event.e.keyboard.scancode;
                sdl_event.key.key = badgevms_event.e.keyboard.key;
                sdl_event.key.mod = badgevms_event.e.keyboard.mod;
                sdl_event.key.down = badgevms_event.e.keyboard.down;
                sdl_event.key.repeat = badgevms_event.e.keyboard.repeat;
                sdl_event.key.timestamp = SDL_GetTicksNS();
                SDL_PushEvent(&sdl_event);
                
                if (badgevms_event.e.keyboard.down && badgevms_event.e.keyboard.text != 0) {
                    char text[2] = { badgevms_event.e.keyboard.text, 0 };
                    SDL_SendKeyboardText(text);
                }
                break;
                
            case EVENT_WINDOW_RESIZE:
                break;
                
            default:
                // Unknown event type, ignore
                break;
        }
    }
}

#endif // SDL_VIDEO_DRIVER_BADGEVMS
