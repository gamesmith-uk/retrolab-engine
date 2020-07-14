#define _GNU_SOURCE

#include "../global.h"
#include "input.h"
#include "retrolab.def.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SourceFile {
    char* filename;
    char* source;
} SourceFile;

typedef struct Input {
    SourceFile* files;
    size_t      count;
} Input;

Input* EMSCRIPTEN_KEEPALIVE
input_new()
{
    Input* input = calloc(1, sizeof(Input));
    return input;
}

Input*
input_new_from_string(const char* text)
{
    Input* input = input_new();
    input->count = 2;
    input->files = calloc(2, sizeof(SourceFile));
    input->files[0].filename = strdup("retrolab.def");
    input->files[0].source = strdup(retrolab_def);
    input->files[1].filename = strdup("main.s");
    input->files[1].source = strdup(text);
    return input;
}

void EMSCRIPTEN_KEEPALIVE
input_free(Input* input)
{
    for (size_t i = 0; i < input->count; ++i) {
        free(input->files[i].filename);
        free(input->files[i].source);
    }
    free(input->files);
    free(input);
}

size_t
input_file_count(Input* input)
{
    return input->count;
}

const char*
input_filename(Input* input, size_t idx)
{
    return input->files[idx].filename;
}

const char*
input_source(Input* input, size_t idx)
{
    return input->files[idx].source;
}

size_t EMSCRIPTEN_KEEPALIVE
input_add_file(Input* input, const char* filename, const char* source)
{
    size_t i = input->count;
    input->files = realloc(input->files, (i + 1) * sizeof(SourceFile));
    input->files[i].filename = strdup(filename);
    input->files[i].source = strdup(source);
    ++input->count;
    return input->count;
}

// {{{ precompilation static methods

static bool
ends_with(const char* s, const char* end)
{
    size_t len_s = strlen(s),
           len_end = strlen(end);
    return len_s >= len_end && strcmp(&s[len_s - len_end], end) == 0;
}


static char*
create_input_from_string(Input* input)
{
    char* r = calloc(1, 1);
    r[0] = '\0';
    size_t current_sz = 0;
    for (size_t i = 0; i < input->count; ++i) {
        if (!ends_with(input->files[i].filename, ".s") && !ends_with(input->files[i].filename, ".def"))
            continue;
        size_t line = 1;
        char* nxt;
        char* current = input->files[i].source;
        do {
            size_t old_sz = current_sz;
            nxt = strchrnul(current, '\n');                 // find next enter
            int header_sz = snprintf(NULL, 0, "[$%s$:%zu] ",  // calculate header size '[file.s:1] '
                    input->files[i].filename, line);
            current_sz += header_sz + (nxt - current) + 1;  // header + line + '\n'
            r = realloc(r, current_sz + 1);                 // full line + '\0'
            snprintf(&r[old_sz], current_sz - old_sz + 1, "[$%s$:%zu] %s\n",
                    input->files[i].filename, line, current);
            ++line;
            current = nxt + 1;
        } while (nxt[0] != '\0');
    }
    if (!r)
        return strdup("");
    return r;
}
 
static int
sort_file_compare(const void* a, const void* b)
{
    const SourceFile *sa = a, *sb = b;
    bool sa_is_def = ends_with(sa->filename, ".def"),
         sb_is_def = ends_with(sb->filename, ".def");
    if (sa_is_def && !sb_is_def)
        return -1;
    else if (!sa_is_def && sb_is_def)
        return 1;
    else if (strcmp(sa->filename, "main.s") == 0)
        return -1;
    else if (strcmp(sb->filename, "main.s") == 0)
        return 1;
    return strcmp(sa->filename, sb->filename);
}

// }}}

char* input_precompile(Input* input)
{
    qsort(input->files, input->count, sizeof(SourceFile), sort_file_compare);
    return create_input_from_string(input);
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
