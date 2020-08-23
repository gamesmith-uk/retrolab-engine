#include "emulator.h"

#define _GNU_SOURCE

#include <stdio.h>

#include "breakpoints.h"
#include "cpu.h"
#include "joystick.h"
#include "memory.h"
#include "timer.h"
#include "video.h"

#define STEPS_PER_FRAME 64800     // 4 Mhz   (3.88 Mhz)

static bool execution_suspended = false;
static bool end_of_frame = false;
static int steps_left = STEPS_PER_FRAME;
static void (*breakpoint_hit_fptr)() = NULL;
static bool break_at_end_of_frame = false;

void
emulator_init(bool reset_memory)
{
    if (reset_memory) {
        ram_init();
        ram[0x0] = 0x60;  // jmp start (TODO - replace for ROM message)
    }
    cpu_init();
    timer_init();
#ifndef HEADLESS
    video_init();
#endif
}

CpuError
emulator_step()
{
    end_of_frame = false;
    cpu_step();
    --steps_left;

    // is it the end of frame?
    if (steps_left == 0) {
#ifndef HEADLESS
        video_tick();
#endif
        timer_frame_step();
        steps_left = STEPS_PER_FRAME;
        end_of_frame = true;

        // was is supposed to break at the end of the frame?
        if (breakpoint_hit_fptr && break_at_end_of_frame) {
            breakpoint_hit_fptr();
            break_at_end_of_frame = false;
            return cpu_error();
        }
    }    
    
    // check for breakpoint hit
    if (breakpoint_hit_fptr && bkps_is_addr(cpu_PC())) {
        breakpoint_hit_fptr();
        end_of_frame = true;
    }

    return cpu_error();
}

CpuError
emulator_frame()
{
    // size_t t = SDL_GetTicks();
    for (;;) {
        CpuError e = emulator_step();
        if (e != CPU_ERROR_NO_ERROR)
            return e;
        if (end_of_frame)
            break;
    }
    // printf("%zu\n", SDL_GetTicks() - t);
    return CPU_ERROR_NO_ERROR;
}

void
emulator_destroy()
{
#if !HEADLESS
    video_destroy();
#endif
    cpu_destroy();
    bkps_clear();
}

void
emulator_load_rom(const char* filename)
{
    uint16_t pos = 0;
    FILE* f = fopen(filename, "rb");
    while(!feof(f))
        pos += fread(&ram[pos], 1, 1024, f);
    fclose(f);
}

void
emulator_hard_reset()
{
    emulator_destroy();
    execution_suspended = false;
    emulator_init(true);
}

void
emulator_soft_reset()
{
    emulator_destroy();
    execution_suspended = false;
    emulator_init(false);
}

void
emulator_suspend_execution()
{
    execution_suspended = true;
}

void
emulator_set_break_at_eof()
{
    break_at_end_of_frame = true;
}

void
emulator_bkp_hit_set_fptr(BreakpointListener bkp_fptr)
{
    breakpoint_hit_fptr = bkp_fptr;
}

int
emulator_dbg_json(size_t memory_block, char* buf, size_t bufsz)
{
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    int n = 0;
    PRINT("{")
    n += cpu_dbg_json(&buf[n], bufsz - n);
    PRINT(",")
    n += ram_dbg_json(memory_block, &buf[n], bufsz - n);
    PRINT(",")
    n += timer_dbg_json(&buf[n], bufsz - n);
    PRINT(",")
    n += video_dbg_json(&buf[n], bufsz - n);
    PRINT(",")
    n += joystick_dbg_json(&buf[n], bufsz - n);
    PRINT(",")
    n += bkps_dbg_json(&buf[n], bufsz - n);
    PRINT(",\"archVersion\":\"" VERSION "\"")
    PRINT("}")
    return (long) n > (long) bufsz;  // return -1 if allocated string is not big enough
#undef PRINT
}

// vim:st=4:sts=4:sw=4:expandtab
