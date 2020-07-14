#include "timer.h"

#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "mmap.h"
#include "memory.h"

#define TIMERS 4

void timer_init()
{
    for (size_t i = 0; i < TIMERS; ++i) {
        ram[TIMER_FRAME_0] = 0x0;
        ram[TIMER_FRAME_0 + 1] = 0x0;
    }
}

void timer_frame_step()
{
    for (size_t i = 0; i < TIMERS; ++i) {
        uint16_t pos = TIMER_FRAME_0 + (i*2);
        uint16_t timer = ram[pos] | (ram[pos+1] << 8);
        --timer;
        ram[pos] = timer & 0xff;
        ram[pos + 1] = timer >> 8;
        if (timer == 0)
            cpu_interrupt(INT_TIMER, XT_TIMER_0 + i);
    }
}

int
timer_dbg_json(char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    PRINT("\"timer\":{\"timerFrame\":[");
    PRINT("%d,", ram_get16(TIMER_FRAME_0));
    PRINT("%d,", ram_get16(TIMER_FRAME_1));
    PRINT("%d,", ram_get16(TIMER_FRAME_2));
    PRINT("%d]", ram_get16(TIMER_FRAME_3));
    PRINT("}");
#undef PRINT
    return n;
}

// vim:st=4:sts=4:sw=4:expandtab
