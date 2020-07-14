#define _GNU_SOURCE

#include "output.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "error.h"

typedef struct Output {
    uint8_t*       binary;
    size_t         binary_sz;
    DebuggingInfo* debugging_info;
    Error          error;
} Output;

Output*
output_new(uint8_t* binary, size_t binary_sz, DebuggingInfo* debugging_info, Error error)
{
    Output* o = calloc(1, sizeof(Output));
    o->binary = binary;
    o->binary_sz = binary_sz;
    o->debugging_info = debugging_info;
    o->error = error;
    return o;
}

void EMSCRIPTEN_KEEPALIVE
output_free(Output* o)
{
    debug_free(o->debugging_info);
    free(o->binary);
    free(o->error.message);
    free(o->error.filename);
    free(o);
}

char*
output_error_message(const Output* output)
{
    return output->error.message;
}

const uint8_t*
output_binary_data(const Output* output)
{
    return output->binary;
}

size_t
output_binary_size(const Output* output)
{
    return output->binary_sz;
}

const DebuggingInfo*
output_debugging_info(const Output* output)
{
    return output->debugging_info;
}

int EMSCRIPTEN_KEEPALIVE
output_to_json(const Output* output, char* buf, size_t bufsz)
{
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    const char* error = output->error.message;
    int n = 0;

    if (error) {
        const char* filename = output->error.filename;
        size_t line = output->error.line;
        if (filename) {
            PRINT("{\"error\":\"%s\",\"file\":\"%s\",\"line\":%zu}", error, filename, line);
        } else {
            PRINT("{\"error\":\"%s\"}", error);
        }
    } else {
        PRINT("{\"binary\":[");
        for (size_t i = 0; i < output->binary_sz; ++i)
            PRINT("%d%s", output->binary[i], (i < (output->binary_sz-1)) ? "," : "");
        PRINT("]");

        /*
        const DebuggingInfo* dbg = output->debugging_info;
        PRINT(",\"debuggingInfo\":{");
        for (size_t i = 0; i < dbg->locations_sz; ++i) {
            Location* lc = &dbg->locations[i];
            PRINT("\"%d\":{\"fileNumber\":%d,\"lineNumber\":%zu}%s", lc->pc, lc->file_number, lc->line,
                (i < (dbg->locations_sz-1)) ? "," : "");
        }
        PRINT("},");
        PRINT("\"debuggingFiles\":[");
        for (size_t i = 0; i < dbg->files_sz; ++i)
            PRINT("\"%s\"%s", dbg->files[i], (i < (dbg->files_sz-1)) ? "," : "");
        PRINT("]");
        */
        
        PRINT("}");
    }

    return (long) n > (long) bufsz;  // return -1 if allocated string is not big enough
#undef PRINT
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
