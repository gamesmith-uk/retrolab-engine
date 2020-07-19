#ifndef SYMTBL_H_
#define SYMTBL_H_

#include <limits.h>
#include <stdbool.h>

#define SYMBOL_NOT_FOUND LONG_MIN
#define SYMBOL_ALREADY_EXISTS -1

typedef struct SymbolTable SymbolTable;

SymbolTable* symtbl_new();
void         symtbl_free(SymbolTable* tbl);

void         symtbl_set_global(SymbolTable* tbl, const char* name);
int          symtbl_add_symbol(SymbolTable* tbl, const char* name, int value, bool update_global);
long         symtbl_value(const SymbolTable* tbl, const char* name);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
