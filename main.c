#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>

#include <SDL2/SDL.h>

#include "global.h"
#include "emulator/emulator.h"
#include "emulator/video.h"
#include "compiler/compiler.h"

#include "emulator/memory.h"

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
            { 0, 0, 0, 0 },
        };

        int opt_idx;
        c = getopt_long(argc, argv, "r:c:s:", long_options, &opt_idx);
        if (c == -1)
            break;
        switch (c) {
            case 'r':
                emulator_load_rom(optarg);
                break;
            case 'c': {
                    Output* output = compile_file(optarg);
                    const char* error = output_error_message(output);
                    if (error) {
                        fprintf(stderr, "%s\n", error);
                        exit(1);
                    }
                    fwrite(output_binary_data(output), output_binary_size(output), 1, stdout);
                    output_free(output);
                    exit(0);
                }
                break;
            case 's': {
                    Output* output = compile_file(optarg);
                    const char* error = output_error_message(output);
                    if (error) {
                        fprintf(stderr, "%s\n", error);
                        exit(1);
                    }
                    ram_load(0, output_binary_data(output), output_binary_size(output));
                    output_free(output);
                }
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
