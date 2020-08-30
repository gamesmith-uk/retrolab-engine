#include "../global.h"

#include <stdbool.h>
#include <stdio.h>

#include "breakpoints.h"
#include "cpu.h"
#include "emulator.h"
#include "memory.h"
#include "video.h"
#include "../compiler/output.h"
#include "mmap.h"

// {{{ init / stop / main loop

int EMSCRIPTEN_KEEPALIVE
init(bool reset_memory)
{
    emulator_init(reset_memory);
    printf("Emulator initialized from C side.\n");
    video_init();
    printf("Video initialized from C side.\n");
    return 0;
}

int EMSCRIPTEN_KEEPALIVE
stop_main_loop()
{
    printf("Cancelling main loop.\n");
    emscripten_cancel_main_loop();
    printf("Main loop cancelled.\n");
    // emscripten_exit_with_live_runtime();
    return 0;
}

static void
run_emulator_frame(void* error_callback) {
    if (emulator_frame() != CPU_ERROR_NO_ERROR) {
        printf("An error happened during execution. The main loop will be now canceled.\n");
        stop_main_loop();
        ((void(*)()) error_callback)();
    }
}

int EMSCRIPTEN_KEEPALIVE
main_loop(void (*error_callback)())
{
    printf("Starting main loop in C side.\n");
    emscripten_set_main_loop_arg(run_emulator_frame, error_callback, 0, 1);
    printf("Main loop set up.\n");
    return 0;
}

int EMSCRIPTEN_KEEPALIVE
reset()
{
    emulator_hard_reset();
    return 0;
}

CpuError EMSCRIPTEN_KEEPALIVE
step()
{
    return emulator_step();
}

CpuError EMSCRIPTEN_KEEPALIVE
frame()
{
    return emulator_frame();
}

int EMSCRIPTEN_KEEPALIVE
destroy()
{
    printf("Destroying video...\n");
    video_destroy();
    printf("Destroying emulator...\n");
    emulator_destroy();
    return 0;
}

// }}}

// {{{ emulator info

int EMSCRIPTEN_KEEPALIVE
emulator_state(size_t memory_block, char* buf, size_t bufsz)
{
    return emulator_dbg_json(memory_block, buf, bufsz);
}

// }}}

// {{{ emulator loading

int EMSCRIPTEN_KEEPALIVE
load_memory(const uint8_t* data, size_t sz)
{
    emulator_init(true);
    if (ram_load(0, data, sz) > 0) {
        printf("%zu bytes of memory loaded.\n", sz);
        return 0;
    } else {
        fprintf(stderr, "Could not load memory data.\n");
        return -1;
    }
}

int EMSCRIPTEN_KEEPALIVE
load_output(Output* output)
{
    emulator_init(true);
    if (ram_load(0, output_binary_data(output), output_binary_size(output)) > 0) {
        cpu_load_debugging_info(output_debugging_info(output));
        return 0;
    } else {
        fprintf(stderr, "Could not load memory data.\n");
        return -1;
    }
}

// }}}

// {{{ emulator control

int EMSCRIPTEN_KEEPALIVE
send_keypress(uint16_t key)
{
    cpu_interrupt(INT_KEYBOARD, key);
    return 0;
}

int EMSCRIPTEN_KEEPALIVE
set_joystick(uint8_t state)
{
    cpu_interrupt(INT_JOYSTICK, state);
    ram[JOYSTICK_STATE] = state;
    return 0;
}

int EMSCRIPTEN_KEEPALIVE
screen_update()
{
    video_tick();
    return 0;
}

// }}}

// {{{ breakpoints

bool EMSCRIPTEN_KEEPALIVE
swap_breakpoint(const char* filename, size_t line)
{
    return bkps_swap(filename, line);
}

int EMSCRIPTEN_KEEPALIVE
add_breakpoint_listener(intptr_t listener)
{
    emulator_bkp_hit_set_fptr((BreakpointListener) listener);
    return 0;
}

int EMSCRIPTEN_KEEPALIVE
break_at_end_of_frame()
{
    emulator_set_break_at_eof();
    return 0;
}

int EMSCRIPTEN_KEEPALIVE
break_next()
{
    cpu_break_next();
    return 0;
}

bool EMSCRIPTEN_KEEPALIVE
next_is_subroutine()
{
    return cpu_next_is_subroutine();
}

// }}}

// {{{ debugging info

int EMSCRIPTEN_KEEPALIVE
debug_to_json(const Output* output, char* buf, size_t bufsz)
{
    return debug_json(output_debugging_info(output), buf, bufsz);
}

const DebuggingInfo* EMSCRIPTEN_KEEPALIVE
debugging_info_create()
{
    return debug_new();
}

void EMSCRIPTEN_KEEPALIVE
debugging_info_add(DebuggingInfo* dbg, ram_type_t pc, const char* filename, size_t line)
{
    debug_add_line(dbg, pc, filename, line);
}

void EMSCRIPTEN_KEEPALIVE
debugging_info_load_and_free(DebuggingInfo* dbg)
{
    cpu_load_debugging_info(dbg);
    debug_free(dbg);
}

// }}}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
