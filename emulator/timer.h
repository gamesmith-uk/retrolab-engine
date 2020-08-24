#ifndef TIMER_H_
#define TIMER_H_

#include <stddef.h>

void timer_init();
void timer_frame_step();
void timer_reset();

int  timer_dbg_json(char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab
