#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include <compiler/output.h>
#include <compiler/compiler.h>
#include <emulator/memory.h>
#include "exec.h"

int exec_compile_to_stdout(const char *filename)
{
    Output* output = compile_file(filename);
    const char* error = output_error_message(output);
    if (error) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    fwrite(output_binary_data(output), output_binary_size(output), 1, stdout);
    output_free(output);
    return 0;
}

int exec_compile_file_to_ram(const char *filename)
{
    Output* output = compile_file(filename);
    const char* error = output_error_message(output);
    if (error) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    ram_load(0, output_binary_data(output), output_binary_size(output));
    output_free(output);
    return 0;
}

static char* read_file_contents(const char* full_path) {
    FILE* f = fopen(full_path, "r");
    if (!f) {
        fprintf(stderr, "Could not open file '%s'.\n", full_path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* content = malloc(fsize + 1);
    fread(content, 1, fsize, f);
    fclose(f);
    content[fsize] = 0;
    return content;
}

int exec_compile_dir_to_ram(const char* filename)
{
    DIR* dp;
    struct dirent *ep;
    dp = opendir(filename);
    if (dp == NULL) {
        fprintf(stderr, "Could not open source directory.\n");
        return 1;
    } else {
        Input* input = input_new();
        size_t plen = strlen(filename);
        while ((ep = readdir(dp))) {
            if (ep->d_type != DT_DIR && strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
                char full_path[plen + strlen(ep->d_name) + 2];
                snprintf(full_path, sizeof full_path, "%s/%s", filename, ep->d_name);
                char* contents = read_file_contents(full_path);
                if (!contents)
                    return 1;
                input_add_file(input, full_path, contents);
                free(contents);
            }
        }

        Output* output = compile_input(input);
        input_free(input);
        const char* error = output_error_message(output);
        if (error) {
            fprintf(stderr, "%s\n", error);
            return 1;
        }
        ram_load(0, output_binary_data(output), output_binary_size(output));
        output_free(output);
    }
    return 1;
}

