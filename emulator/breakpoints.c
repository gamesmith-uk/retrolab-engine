#define _GNU_SOURCE

#include "breakpoints.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

static Breakpoint* bkps    = NULL;
static size_t      n_bkps  = 0;
static long        tmp_brk = -1;

void
bkps_clear()
{
    if (bkps) {
        for (size_t i=0; i < n_bkps; ++i)
            free(bkps[i].filename);
        free(bkps);
    }
    n_bkps = 0;
    bkps = NULL;
}

int
bkps_swap(const char* filename, size_t line)
{
    for (size_t i = 0; i < n_bkps; ++i) {
        if (bkps[i].line == line && strcmp(bkps[i].filename, filename) == 0) {
            // remove item and return
            free(bkps[i].filename);
            if (i != (n_bkps - 1))
                memmove(&bkps[i], &bkps[i+1], (n_bkps - i) * sizeof(Breakpoint));
            bkps = realloc(bkps, sizeof(Breakpoint) * --n_bkps);
            return -1;
        }
    }

    // no item was found to remove, add it
    long addr = cpu_addr_from_source(filename, line);
    if (addr < 0)
        return 0;  // not found in source, or not an instruction address
    bkps = realloc(bkps, sizeof(Breakpoint) * ++n_bkps);
    bkps[n_bkps-1] = (Breakpoint) {
        .filename = strdup(filename),
        .line     = line,
        .addr     = addr,
    };
    return 1;
}

void
bkps_set_tmp_brk(long addr)
{
    printf("Added temporary breakpoint at address 0x%lX.\n", addr);
    tmp_brk = addr;
}

size_t bkps_count()
{
    return n_bkps;
}

const Breakpoint*
bkps_item(size_t n)
{
    return &bkps[n];
}

bool
bkps_is_addr(long addr)
{
    if (tmp_brk == addr) {
        tmp_brk = -1;
        return true;
    }
    for (size_t i=0; i < n_bkps; ++i)
        if (bkps[i].addr == addr)
            return true;
    return false;
}

int
bkps_dbg_json(char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    PRINT("\"breakpoints\":[")
    for (size_t i = 0; i < n_bkps; ++i)
        PRINT("{\"filename\":\"%s\",\"line\":%zu},", bkps[i].filename, bkps[i].line)
    if (n_bkps > 0)
        --n;
    PRINT("]")
#undef PRINT
    return n;
}


// vim:st=4:sts=4:sw=4:expandtab
