#include "joystick.h"

#include <stdio.h>
#include <SDL2/SDL.h>

#include "cpu.h"
#include "memory.h"
#include "mmap.h"

void
joystic_update_state()
{
    const Uint8* s = SDL_GetKeyboardState(NULL);
    uint8_t r = 0;
    if (s[SDL_SCANCODE_UP])
        r |= 1;
    if (s[SDL_SCANCODE_DOWN])
        r |= (1 << 1);
    if (s[SDL_SCANCODE_LEFT])
        r |= (1 << 2);
    if (s[SDL_SCANCODE_RIGHT])
        r |= (1 << 3);
    if (s[SDL_SCANCODE_Z])
        r |= (1 << 4);
    if (s[SDL_SCANCODE_X])
        r |= (1 << 5);
    if (s[SDL_SCANCODE_A])
        r |= (1 << 6);
    if (s[SDL_SCANCODE_S])
        r |= (1 << 7);
    ram[JOYSTICK_STATE] = r;
}

void
joystick_interrupt(SDL_Event* e)
{
    if (e->key.repeat == 0 && (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP)) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:
            case SDLK_DOWN:
            case SDLK_LEFT:
            case SDLK_RIGHT:
            case SDLK_z:
            case SDLK_x:
            case SDLK_a:
            case SDLK_s: {
                    joystic_update_state();
                    uint8_t state = ram[JOYSTICK_STATE];
                    cpu_interrupt(INT_JOYSTICK, state);
                }
                break;
        }
    }
}

int
joystick_dbg_json(char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    PRINT("\"joystick\":{");
    PRINT("\"up\":%s,",    ram[JOYSTICK_STATE] & 1 ? "true" : "false");
    PRINT("\"down\":%s,",  (ram[JOYSTICK_STATE] >> 1) & 1 ? "true" : "false");
    PRINT("\"left\":%s,",  (ram[JOYSTICK_STATE] >> 2) & 1 ? "true" : "false");
    PRINT("\"right\":%s,", (ram[JOYSTICK_STATE] >> 3) & 1 ? "true" : "false");
    PRINT("\"z\":%s,",     (ram[JOYSTICK_STATE] >> 4) & 1 ? "true" : "false");
    PRINT("\"x\":%s,",     (ram[JOYSTICK_STATE] >> 5) & 1 ? "true" : "false");
    PRINT("\"a\":%s,",     (ram[JOYSTICK_STATE] >> 6) & 1 ? "true" : "false");
    PRINT("\"s\":%s",      (ram[JOYSTICK_STATE] >> 7) & 1 ? "true" : "false");
    PRINT("}");
#undef PRINT
    return n;
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
