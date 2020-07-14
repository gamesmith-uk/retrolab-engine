#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include <SDL2/SDL.h>

void joystic_update_state();
void joystick_interrupt(SDL_Event* e);

int  joystick_dbg_json(char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab
