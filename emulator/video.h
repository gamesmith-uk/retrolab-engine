#ifndef VIDEO_H_
#define VIDEO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void video_init();
void video_destroy();
void video_reset();

void video_tick();
bool video_running();

void video_set(uint16_t addr, uint8_t data);

int  video_dbg_json(char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab
