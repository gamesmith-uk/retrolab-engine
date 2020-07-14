#define _GNU_SOURCE

#include "compctx.h"
#include "error.h"
#include "output.h"
#include "parser.h"
#include "symtbl.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct CompilationContext {
    struct {
        ram_type_t   pc;
        char*        file;
        int          line;
    }              current;
    struct {
        uint8_t*     data;
        size_t       sz;
    }              binary;
    SymbolTable*   symtbl;
    DebuggingInfo* debugging_info;
    Error          error;
    long           saved_org;
    ram_type_t     pc;
    uint8_t        pass;
    struct {
        ram_type_t*  data;
        size_t       sz;
    }              pending;
} CompilationContext;

// {{{ constructor / destructor

CompilationContext*
cc_new()
{
    CompilationContext* cc = calloc(1, sizeof(CompilationContext));
    cc->debugging_info = debug_new();
    cc->symtbl = symtbl_new();
    cc->saved_org = -1;
    cc->pass = 1;
    return cc;
}

void
cc_free(CompilationContext* cc)
{
    free(cc->current.file);
    free(cc->binary.data);
    free(cc->error.message);
    free(cc->pending.data);
    symtbl_free(cc->symtbl);
    debug_free(cc->debugging_info);
    free(cc);
}

Output*
cc_move_to_output(CompilationContext* cc)
{
    Output* output = output_new(cc->binary.data, cc->binary.sz, cc->debugging_info, cc->error);
    free(cc->current.file);
    free(cc->pending.data);
    symtbl_free(cc->symtbl);
    free(cc);
    return output;
}

// }}}

// {{{ current

void
cc_set_current_fileline(CompilationContext* cc, const char* file, size_t line)
{
    cc->current.pc = cc->pc;
    free(cc->current.file);
    cc->current.file = strdup(file);
    cc->current.line = line;
}

ram_type_t
cc_current_expression_pc(const CompilationContext* cc)
{
    return cc->current.pc;
}

// }}}

// {{{ pc

ram_type_t
cc_pc(const CompilationContext* cc)
{
    return cc->pc;
}

// }}}

// {{{ symbols

void
cc_register_label(CompilationContext* cc, const char* name)
{
    if (cc->pass == 1)
        symtbl_add_symbol(cc->symtbl, name, cc->pc, true);
    else if (name[0] != '.')
        symtbl_set_global(cc->symtbl, name);
}

void
cc_register_define(CompilationContext* cc, const char* name, long value)
{
    if (name[0] == '.') {
        yyerror(cc, "Definitions can't start with a dot (.)");
        return;
    }

    if (cc->pass == 1) {
        if (symtbl_add_symbol(cc->symtbl, name, value, false) != 0) {
            yyerror(cc, "Symbol '%s' alredy exists", name);
        }
    }
}

static void
register_pending_symbol(CompilationContext* cc)
{
    ++cc->pending.sz;
    cc->pending.data = realloc(cc->pending.data, cc->pending.sz * sizeof(cc->pending.data[0]));
    cc->pending.data[cc->pending.sz - 1] = cc->pc;
}

long
cc_symbol_value(CompilationContext* cc, const char* name)
{
    long value = symtbl_value(cc->symtbl, name);
    if (value == SYMBOL_NOT_FOUND) {
        if (cc->pass == 1) {
            /* If a symbol is not found on the 1st pass, we need to reserve 2 bytes, because
             * we don't know if the symbol will be < 0xff or > 0xff. We store this in a
             * pending-symbol table, so that the 2nd pass knows that in need to use 2 bytes,
             * event if the symbol ends up being <= 0xff. */
            register_pending_symbol(cc);
            return 0x0;
        } else {
            yyerror(cc, "Symbol '%s' not defined", name);
            return 0;
        }
    }
    return value;
}

static int compareaddr(const void* a, const void* b) {
    return (*(ram_type_t*)a - *(ram_type_t*)b);
}

bool
cc_is_addr_pending(CompilationContext* cc, ram_type_t pc)
{
    return bsearch(&pc, cc->pending.data, cc->pending.sz, sizeof(ram_type_t), compareaddr);
}

// }}}

// {{{ org

void
cc_save_org(CompilationContext* cc, ram_type_t pc)
{
    if (cc->saved_org == -1)
        cc->saved_org = cc->current.pc;
    cc->pc = pc;
}

void
cc_restore_org(CompilationContext* cc)
{
    long r = cc->saved_org;
    cc->saved_org = -1;
    cc->pc = r;
}

// }}}

// {{{ debugging info

const DebuggingInfo*
cc_debugging_info(const CompilationContext* cc)
{
    return cc->debugging_info;
}

void
cc_add_debugging_info(CompilationContext* cc)
{
    if (cc->pass == 2)
        debug_add_line(cc->debugging_info, cc->pc, cc->current.file, cc->current.line);
}

// }}}

// {{{ binary

static uint8_t*
cc_limit_sz_to_pc(CompilationContext* cc)
{
    if (cc->pass == 2) {
        if (cc->binary.sz < cc->pc) {
            size_t old_sz = cc->binary.sz;
            cc->binary.sz = cc->pc;
            cc->binary.data = realloc(cc->binary.data, cc->binary.sz);
            memset(&cc->binary.data[old_sz], 0, cc->binary.sz - old_sz);
        }
        return cc->binary.data;
    }
    return NULL;
}

void
cc_add_byte(CompilationContext* cc, uint8_t byte)
{
    ++cc->pc;
    if (cc->pass == 2) {
        cc->binary.data = cc_limit_sz_to_pc(cc);
        cc->binary.data[cc->pc - 1] = byte;
    }
}

void
cc_add_word(CompilationContext* cc, uint16_t word)
{
    cc->pc += 2;
    if (cc->pass == 2) {
        cc->binary.data = cc_limit_sz_to_pc(cc);
        cc->binary.data[cc->pc - 2] = word & 0xff;
        cc->binary.data[cc->pc - 1] = word >> 8;
    }
}

void
cc_add_string(CompilationContext* cc, size_t sz, char* str)
{
    // remove escaped quotes
    char* s = str;
    while ((s = strchr(s, '\\'))) {
        if (s[1] == '"') {
            s = memmove(s, s + 1, strlen(s));
            --sz;
        }
    }

    // copy string
    size_t old_pc = cc->pc;
    cc->pc += sz;
    if (cc->pass == 2) {
        cc->binary.data = cc_limit_sz_to_pc(cc);
        memcpy(&cc->binary.data[old_pc], str, sz);
    }
}

void
cc_add_zeroes(CompilationContext* cc, size_t sz)
{
    size_t old_pc = cc->pc;
    cc->pc += sz;
    if (cc->pass == 2) {
        cc->binary.data = cc_limit_sz_to_pc(cc);
        memset(&cc->binary.data[old_pc], 0, sz);
    }
}

void
cc_add_bytes(CompilationContext* cc, ByteArray ba)
{
    size_t old_pc = cc->pc;
    cc->pc += ba.sz;
    if (cc->pass == 2) {
        cc->binary.data = cc_limit_sz_to_pc(cc);
        memcpy(&cc->binary.data[old_pc], ba.bytes, ba.sz);
    }
}

void
cc_replace_special_jmp(CompilationContext* cc)
{
    // HACK
    if (cc->pass == 2) {
        bool shrink = (cc->pc == cc->binary.sz);
        cc->binary.data[cc->pc - 4] = 0x63;  // special jmp
        cc->binary.data[cc->pc - 3] = cc->binary.data[cc->pc - 2];  // move next v16 on byte back
        cc->binary.data[cc->pc - 2] = cc->binary.data[cc->pc - 1];
        if (shrink) {
            --cc->binary.sz;
            cc->binary.data = realloc(cc->binary.data, cc->binary.sz);
        }
    }
    --cc->pc;
}

void
cc_overwrite_word(CompilationContext* cc, ram_type_t addr, uint16_t value)
{
    if (cc->pass == 2) {
        cc->binary.data[addr] = value & 0xff;
        cc->binary.data[addr + 1] = value >> 8;
    }
}

// }}}

// {{{ error

void
cc_set_error(CompilationContext* cc, const char* message)
{
    // set other attributes
    if (cc->current.file)
        cc->error.filename = strdup(cc->current.file);
    else
        cc->error.filename = strdup("nofile");
    cc->error.line = cc->current.line;
    
    // set message
    const char* fmt = "%s in %s:%d";
    int sz = snprintf(NULL, 0, fmt, message, cc->error.filename, cc->error.line) + 1;
    cc->error.message = malloc(sz);
    snprintf(cc->error.message, sz, fmt, message, cc->error.filename, cc->error.line);
}

const char*
cc_error_message(const CompilationContext* cc)
{
    return cc->error.message;
}

// }}}

// {{{ second pass

void
cc_advance_to_second_pass(CompilationContext* cc)
{
    cc->pass = 2;
    cc->pc = 0;
    cc->saved_org = -1;
    qsort(cc->pending.data, cc->pending.sz, sizeof(cc->pending.data[0]), compareaddr);
    symtbl_set_global(cc->symtbl, NULL);
    assert(cc->binary.sz == 0);
    assert(cc->binary.data == NULL);
}

// }}}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
