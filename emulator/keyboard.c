#include "keyboard.h"

#include "cpu.h"
#include "../constants/mmap.h"

static uint16_t
add_mod(uint16_t key)
{
    SDL_Keymod mod = SDL_GetModState();
    if (mod & KMOD_SHIFT)
        key |= (1 << 9);
    if (mod & KMOD_CTRL)
        key |= (1 << 10);
    return key;
}

void
keyboard_interrupt(SDL_Event* e)
{
    switch (e->type) {
        case SDL_KEYDOWN:  // special keys
            if (e->key.repeat == 0) {
                uint16_t key = 0;
                switch (e->key.keysym.sym) {
                    case SDLK_BACKSPACE:
                    case SDLK_TAB:
                    case SDLK_RETURN:
                    case SDLK_ESCAPE:
                    case SDLK_DELETE:
                        key = e->key.keysym.sym;
                        break;
                    case SDLK_INSERT:   key = 0xf0; break;
                    case SDLK_HOME:     key = 0xf1; break;
                    case SDLK_END:      key = 0xf2; break;
                    case SDLK_PAGEUP:   key = 0xf3; break;
                    case SDLK_PAGEDOWN: key = 0xf4; break;
                    case SDLK_RIGHT:    key = 0xf5; break;
                    case SDLK_LEFT:     key = 0xf6; break;
                    case SDLK_UP:       key = 0xf7; break;
                    case SDLK_DOWN:     key = 0xf8; break;
                }
                if (key == 0)
                    return;
                add_mod(key);
                cpu_interrupt(INT_KEYBOARD, key);
            }
            break;
        case SDL_TEXTINPUT: {  // regular keys
                uint16_t key = e->text.text[0];
                add_mod(key);
                cpu_interrupt(INT_KEYBOARD, key);
            }
            break;
    }
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
