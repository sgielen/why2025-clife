#include <string>
#include <algorithm>
#include <ctime>
#include "clife/clife.hpp"
#include "clife/util.hpp"
#include <cassert>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
extern "C" {
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
}

// TODO: if we could link stdlib, we wouldn't have to provide the following
// stubs:
extern "C" {
void __assert_func(const char *file, int line, const char *func, const char *failedexpr) {
    printf("assertion failed: %s:%d\n", file, line);
    abort();
}

void _ZSt17__throw_bad_allocv() {
    printf("throw bad allocv\n");
    abort();
}

void _ZSt20__throw_length_errorPKc() {
    printf("throw length error\n");
    abort();
}

void _ZSt28__throw_bad_array_new_lengthv() {
    printf("bad array new length\n");
    abort();
}

// mangled implementation of operator new, see below
void *_Znwj(unsigned int sz) {
    return calloc(1, sz);
}

// mangled implementation of operator delete, see below
void _ZdlPvj(void *ptr, unsigned int sz) {
    free(ptr);
}
}

/*
// TODO: this should add an implementation of operator new to the object file
// (symbol called _Znwj), but it doesn't?
__attribute__((used))
void *operator new(unsigned int sz) {
    return malloc(sz);
}
*/

/*
// TODO: this should add an implementation of operator delete to the object
// file (symbol called _ZdlPvj), but it doesn't?
__attribute__((used))
void operator delete(void *ptr, unsigned int sz) {
    free(ptr);
}
*/

inline void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v,
                uint8_t *r, uint8_t *g, uint8_t *b);

#define STEP_RATE_IN_MILLISECONDS  125
#define BLOCK_SIZE_IN_PIXELS 12
#define SDL_WINDOW_WIDTH           (BLOCK_SIZE_IN_PIXELS * GAME_WIDTH)
#define SDL_WINDOW_HEIGHT          (BLOCK_SIZE_IN_PIXELS * GAME_HEIGHT)

#define GAME_WIDTH  60U
#define GAME_HEIGHT 60U

static void set_rect_xy_(SDL_FRect *r, short x, short y) {
    r->x = (float)(x * BLOCK_SIZE_IN_PIXELS);
    r->y = (float)(y * BLOCK_SIZE_IN_PIXELS);
}

struct MulticolorValue {
	bool value;
	uint8_t hue;
	uint8_t age;

	MulticolorValue() : value(false), hue(0), age(0){}
	MulticolorValue(std::vector<MulticolorValue> vec) : value(true), age(0) {
		std::vector<int> hues;
		for(auto const &cell : vec) {
			if(cell.value) {
				int hue = int(cell.hue) * 360 / UINT8_MAX;
				hues.push_back(hue);
			}
		}
		assert(hues.size() == 3);
		std::sort(hues.begin(), hues.end());
		int range1 = hues[2] - hues[0];
		int range2 = (hues[0] + 360) - hues[1];
		int range3 = (hues[1] + 360) - hues[2];
		int avghue;
		if(std::min(range1, std::min(range2, range3)) == range1) {
			avghue = (hues[0] + hues[1] + hues[2]) / 3;
		} else if(std::min(range1, std::min(range2, range3)) == range2) {
			avghue = (hues[0] + hues[1] + hues[2] + 360) / 3;
		} else {
			avghue = (hues[0] + hues[1] + hues[2] + 720) / 3;
		}
		avghue += 5;
		avghue %= 360;
		hue = avghue * UINT8_MAX / 360;
	}
	MulticolorValue(bool value) : value(value), hue(0), age(0) {
		if(value) {
			char color = rand() & 6;
			hue = (UINT8_MAX / 6) * color;
		}
	}

	void age_once() {
		age = (age + 1) % 50;
	}

	operator bool() const { return value; }
	std::string hash() const {
		if(value) {
			return std::string("1") + char(hue);
		} else {
			return "0\x00";
		}
	}

	char getChar() const {
		return (value ? 'o' : ' ');
	}

	void begin_screen(std::ostream &os) const {}
	void end_screen(std::ostream &os) const {}
	void begin_line(std::ostream &os) const {}
	void end_line(std::ostream &os) const {}

	void print(std::ostream &os) const {}
};

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
	GameOfLifeField<MulticolorValue> *field;
    BloomFilter  *bloom_filter;
    int           stop_counter;
    Uint64        last_step;
} AppState;

template <typename FieldType>
bool check_stop_condition(FieldType &field, BloomFilter &bloom_filter, int &stop_counter) {
	uint64_t hash = field_hash(field);
	if (bloom_filter.test(hash)) {
		if (++stop_counter >= 50)
			return true;
		return false;
	} else {
		stop_counter = 0;
		return false;
	}
}

uint64_t field_hash(GameOfLifeField<MulticolorValue> &field) {
    // TODO: implement a proper hash.
    uint64_t result = 0;
    for (uint64_t y = 0; y < field.get_height(); ++y) {
        auto const &row = field[y];
        for (uint64_t x = 0; x < field.get_width(); ++x) {
            auto const &cell = row[x];
            if (cell.value) {
                result += ((x + 1) << 8) * (y + 1);
            }
        }
    }
	return result;
}

void init(AppState *as) {
    printf("reinit Game of Life\n");
    as->field = new GameOfLifeField<MulticolorValue>(GAME_WIDTH, GAME_HEIGHT);
    as->field->generateRandom(25);
    as->bloom_filter = new BloomFilter();
    as->stop_counter = 0;
}

void step(AppState *as) {
	if (check_stop_condition(*(as->field), *(as->bloom_filter), as->stop_counter)) {
        init(as);
        return;
    }

    as->field->nextState();
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (!SDL_SetAppMetadata("Game of life, colored", "1.0", "com.github.sgielen.clife")) {
        return SDL_APP_FAILURE;
    }

    // TODO: set app metadata

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    as->window = SDL_CreateWindow("gameoflife_color", SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, SDL_WINDOW_FULLSCREEN);
    if (!as->window) {
        printf("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check display capabilities
    SDL_DisplayID          display      = SDL_GetDisplayForWindow(as->window);
    SDL_DisplayMode const *current_mode = SDL_GetCurrentDisplayMode(display);
    if (current_mode) {
        printf(
            "Current display mode: %dx%d @%.2fHz, format: %s",
            current_mode->w,
            current_mode->h,
            current_mode->refresh_rate,
            SDL_GetPixelFormatName(current_mode->format)
        );
    }

    // Create renderer
    as->renderer = SDL_CreateRenderer(as->window, NULL);
    if (!as->renderer) {
        printf("Failed to create renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check renderer properties
    SDL_PropertiesID props = SDL_GetRendererProperties(as->renderer);
    if (props) {
        char const *name = SDL_GetStringProperty(props, SDL_PROP_RENDERER_NAME_STRING, "Unknown");
        printf("Renderer: %s", name);

        SDL_PixelFormat const *formats =
            (SDL_PixelFormat const *)SDL_GetPointerProperty(props, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
        if (formats) {
            printf("Supported texture formats:");
            for (int j = 0; formats[j] != SDL_PIXELFORMAT_UNKNOWN; j++) {
                printf("  Format %d: %s", j, SDL_GetPixelFormatName(formats[j]));
            }
        }
    }

    as->last_step = SDL_GetTicks();
    init(as);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *as = (AppState*) appstate;
    Uint64 const now = SDL_GetTicks();

    while ((now - as->last_step) >= STEP_RATE_IN_MILLISECONDS) {
        step(as);
        as->last_step += STEP_RATE_IN_MILLISECONDS;
    }

    SDL_FRect rect;
    rect.w = rect.h = BLOCK_SIZE_IN_PIXELS;
    SDL_SetRenderDrawColor(as->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(as->renderer);

    uint8_t r, g, b;

    auto & field = *(as->field);    
    for (int y = 0; y < field.get_height(); ++y) {
        auto const &row = field[y];
        for (int x = 0; x < field.get_width(); ++x) {
            auto const &cell = row[x];
            set_rect_xy_(&rect, x, y);
            if (cell.value) {
                int v = std::max(255, 255 - cell.age * 2);
                hsv_to_rgb(cell.hue, 255, v, &r, &g, &b);
                SDL_SetRenderDrawColor(as->renderer, r, g, b, SDL_ALPHA_OPAQUE);
                SDL_RenderFillRect(as->renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(as->renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *as = (AppState*) appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            switch (event->key.key) {
            case SDLK_ESCAPE:
            case 1073741865:
                return SDL_APP_SUCCESS;
            default:
                printf("unknown key %d\n", event->key.key);
                // reinit
                init(as);
                break;
            }
            break;
        default: break;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (appstate != NULL) {
        AppState *as = (AppState *)appstate;
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
    }
}

void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v,
                uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        // Achromatic (grey)
        *r = *g = *b = v;
        return;
    }

    // Convert hue from [0,255] to sector [0,5]
    uint16_t region = (uint16_t)h * 6 / 256; // 0-5
    uint16_t remainder = ((uint16_t)h * 6) % 256; // position within sector, scaled to 0-255

    uint16_t p = (uint16_t)v * (255 - s) / 255;
    uint16_t q = (uint16_t)v * (255 - ((uint16_t)s * remainder) / 255) / 255;
    uint16_t t = (uint16_t)v * (255 - ((uint16_t)s * (255 - remainder)) / 255) / 255;

    switch (region) {
        case 0:
            *r = v; *g = t; *b = p;
            break;
        case 1:
            *r = q; *g = v; *b = p;
            break;
        case 2:
            *r = p; *g = v; *b = t;
            break;
        case 3:
            *r = p; *g = q; *b = v;
            break;
        case 4:
            *r = t; *g = p; *b = v;
            break;
        default: // case 5:
            *r = v; *g = p; *b = q;
            break;
    }
}
