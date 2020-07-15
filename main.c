#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include <dirent.h>

#include <SDL2/SDL.h>

#include "global.h"
#include "emulator/emulator.h"
#include "emulator/video.h"
#include "compiler/compiler.h"

#include "emulator/memory.h"

#include "exec/exec.h"

static void
main_loop()
{
#if __EMSCRIPTEN__
    emscripten_set_main_loop(emulator_step, 0, 1);
#else
    while (video_running()) {
        unsigned int current_time = SDL_GetTicks();
        emulator_frame();
        unsigned int new_time = SDL_GetTicks();
        long idle_time = (16L - (new_time - current_time));
        if (idle_time > 0)
            SDL_Delay(idle_time);
    }
#endif
}

void
parse_args(int argc, char* argv[])
{
    int c;
    while (1) {
        static struct option long_options[] = {
            { "rom",          required_argument, 0, 'r' },
            { "compile-file", required_argument, 0, 'c' },
            { "source-file",  required_argument, 0, 's' },
            { "source-dir",   required_argument, 0, 'd' },
            { 0, 0, 0, 0 },
        };

        int opt_idx;
        c = getopt_long(argc, argv, "r:c:s:d:", long_options, &opt_idx);
        if (c == -1)
            break;
        switch (c) {
            case 'r':
                emulator_load_rom(optarg);
                break;
            case 'c':
                exit(exec_compile_to_stdout(optarg));
                break;
            case 's':
                if (exec_compile_file_to_ram(optarg) != 0)
                    exit(1);
                break;
            case 'd':
                if (exec_compile_dir_to_ram(optarg) != 0)
                    exit(1);
                break;
        }
    }
}

int
main(int argc, char* argv[])
{
#if !__EMSCRIPTEN__
    emulator_init(true);
    parse_args(argc, argv);
    main_loop();
    video_destroy();
    emulator_destroy();
#else
    (void) argc;
    (void) argv;
    (void) main_loop;
#endif
    return 0;
}

// vim:st=4:sts=4:sw=4:expandtab
