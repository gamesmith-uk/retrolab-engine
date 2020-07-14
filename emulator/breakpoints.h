#ifndef BREAKPOINTS_H_
#define BREAKPOINTS_H_

#include <stdbool.h>
#include <stddef.h>

typedef struct Breakpoint {
    char*   filename;
    size_t  line;
    long    addr;
} Breakpoint;

void              bkps_clear();
int               bkps_swap(const char* filename, size_t line);

void              bkps_set_tmp_brk(long addr);

bool              bkps_is_addr(long addr);

int               bkps_dbg_json(char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab
