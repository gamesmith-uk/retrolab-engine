#define _GNU_SOURCE

#include "debug.h"

#include <stdio.h>
#include <string.h>

DebuggingInfo*
debug_new()
{
    return calloc(1, sizeof(DebuggingInfo));
}

void
debug_free(DebuggingInfo* dbg)
{
    if (dbg == NULL)
        return;
    for (size_t i = 0; i < dbg->files_sz; ++i) {
        free(dbg->files[i]);
    }
    free(dbg->files);
    free(dbg->locations);
    free(dbg);
}

DebuggingInfo*
debug_copy(const DebuggingInfo* old)
{
    DebuggingInfo* new = debug_new();
    new->files_sz = old->files_sz;
    new->files = calloc(old->files_sz, sizeof(char*));
    for (size_t i = 0; i < old->files_sz; ++i)
        new->files[i] = strdup(old->files[i]);
    new->locations_sz = old->locations_sz;
    new->locations = calloc(old->locations_sz, sizeof(Location));
    memcpy(new->locations, old->locations, old->locations_sz * sizeof(Location));

    return new;
}

uint16_t
dbg_find_or_add_file(DebuggingInfo* dbg, const char* filename)
{
    for (size_t i = 0; i < dbg->files_sz; ++i)
        if (strcmp(dbg->files[i], filename) == 0)
            return i;
    ++dbg->files_sz;
    dbg->files = realloc(dbg->files, dbg->files_sz * sizeof(char*));
    dbg->files[dbg->files_sz - 1] = strdup(filename);
    return dbg->files_sz - 1;
}

void
debug_add_line(DebuggingInfo* dbg, ram_type_t pc, const char* filename, size_t line)
{
    int file = dbg_find_or_add_file(dbg, filename);
    ++dbg->locations_sz;
    dbg->locations = realloc(dbg->locations, dbg->locations_sz * sizeof(Location));
    dbg->locations[dbg->locations_sz - 1] = (Location) {
        .pc = pc,
        .file_number = file,
        .line = line
    };
}

long
debug_find_pc(const DebuggingInfo* dbg, const char* filename, size_t line)
{
    size_t filenumber = 0;
    for (size_t i = 0; i < dbg->files_sz; ++i) {
        if (strcmp(dbg->files[i], filename) == 0) {
            filenumber = 0;
            goto cont;
        }
    }
    return -1;
cont:

    for (size_t i = 0; i < dbg->locations_sz; ++i)
        if (dbg->locations[i].file_number == filenumber && dbg->locations[i].line == line)
            return dbg->locations[i].pc;
    return -1;
}

int
debug_json(const DebuggingInfo* dbg, char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    PRINT("{\"files\":[");
    for (size_t i = 0; i < dbg->files_sz; ++i)
        PRINT("\"%s\",", dbg->files[i]);
    if (dbg->files_sz > 0)
        --n;
    PRINT("],\"locations\":[");
    for (size_t i = 0; i < dbg->locations_sz; ++i) {
        Location* loc = &dbg->locations[i];
        PRINT("[%d,%d,%zu],", loc->pc, loc->file_number, loc->line);
    }
    if (dbg->locations_sz > 0)
        --n;
    PRINT("]");
    PRINT("}");
#undef PRINT
    return n;
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
