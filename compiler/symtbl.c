#define _GNU_SOURCE

#include "symtbl.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../global.h"

typedef struct Symbol {
    char* name;
    long  value;
} Symbol;

typedef struct SymbolTable {
    char*       global;
    Symbol*     symbols;
    size_t      symbols_sz;
} SymbolTable;


SymbolTable*
symtbl_new()
{
    SymbolTable* tbl = calloc(1, sizeof(SymbolTable));
    return tbl;
}

void
symtbl_free(SymbolTable* tbl)
{
    for (size_t i = 0; i < tbl->symbols_sz; ++i)
        free(tbl->symbols[i].name);
    free(tbl->symbols);
    free(tbl->global);
    free(tbl);
}

void
symtbl_set_global(SymbolTable* tbl, const char* name)
{
    free(tbl->global);
    if (name == NULL)
        tbl->global = NULL;
    else
        tbl->global = strdup(name);
}

static void
make_identifier_name(const SymbolTable* tbl, const char* name, char* new_name, size_t max_sz)
{
    if (name[0] != 0 && name[0] == '.' && tbl->global) {
        snprintf(new_name, max_sz, "%s%s", tbl->global, name);
    } else {
        strncpy(new_name, name, max_sz - 1);
        new_name[max_sz - 1] = '\0';
    }
}

static long
symtbl_find(const SymbolTable* tbl, const char* name)
{
    for (size_t i = 0; i < tbl->symbols_sz; ++i)
        if (strcmp(tbl->symbols[i].name, name) == 0)
            return tbl->symbols[i].value;
    return SYMBOL_NOT_FOUND;
}

int
symtbl_add_symbol(SymbolTable* tbl, const char* name, int value, bool update_global)
{
    if (update_global && name[0] != '.') {
        free(tbl->global);
        tbl->global = strdup(name);
    }

    static char new_name[1024];
    make_identifier_name(tbl, name, new_name, sizeof(new_name));
    if (symtbl_find(tbl, new_name) != SYMBOL_NOT_FOUND)
        return -1;
    ++tbl->symbols_sz;
    tbl->symbols = realloc(tbl->symbols, tbl->symbols_sz * sizeof(Symbol));
    tbl->symbols[tbl->symbols_sz-1].name = strdup(new_name);
    tbl->symbols[tbl->symbols_sz-1].value = value;

    return 0;
}

long
symtbl_value(const SymbolTable* tbl, const char* name)
{
    static char new_name[1024];
    make_identifier_name(tbl, name, new_name, sizeof(new_name));
    return symtbl_find(tbl, new_name);
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
