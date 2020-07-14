#include "compiler.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "input.h"
#include "compctx.h"
#include "output.h"
#include "parser.h"

Output* EMSCRIPTEN_KEEPALIVE
compile_input(Input* input)
{
    char* code = input_precompile(input);
    Output* output = compile(code);
    free(code);
    return output;
}

Output*
compile_string(const char* source)
{
    Input* input = input_new_from_string(source);
    Output* output = compile_input(input);
    input_free(input);
    return output;
}

Output*
compile_file(const char* filename)
{
    char* source = NULL;
    FILE* fp = fopen(filename, "r");
    if (fp != NULL) {
        if (fseek(fp, 0L, SEEK_END) == 0) {
            long bufsize = ftell(fp);
            source = malloc(bufsize + 1);
            fseek(fp, 0L, SEEK_SET);
            size_t len = fread(source, 1, bufsize, fp);
            if (ferror(fp) != 0)
                fputs("Error reading file", stderr);
            else
                source[len++] = '\0';
        }
    }
    fclose(fp);
    Output* output = compile_string(source);
    free(source);
    return output;
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
