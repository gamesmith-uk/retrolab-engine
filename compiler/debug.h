#ifndef DEBUG_H_
#define DEBUG_H_

#include "../global.h"

typedef struct Location {
    ram_type_t pc;
    uint16_t   file_number;
    size_t     line;
} Location;

typedef struct DebuggingInfo {
    char**    files;
    size_t    files_sz;
    Location* locations;
    size_t    locations_sz;
} DebuggingInfo;


DebuggingInfo* debug_new();
void           debug_free(DebuggingInfo* dbg);
DebuggingInfo* debug_copy(const DebuggingInfo* dbg);

void           debug_add_line(DebuggingInfo* dbg, ram_type_t pc, const char* filename, size_t line);
long           debug_find_pc(const DebuggingInfo* dbg, const char* filename, size_t line);

int            debug_json(const DebuggingInfo* dbg, char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
