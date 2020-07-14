#ifndef COMPCTX_H_
#define COMPCTX_H_

#include <stdbool.h>

#include "../global.h"
#include "bytearray.h"
#include "debug.h"

typedef struct CompilationContext CompilationContext;
struct Output;

CompilationContext* cc_new();
void                cc_free(CompilationContext* cc);
struct Output*      cc_move_to_output(CompilationContext* cc);

// current
ram_type_t          cc_current_expression_pc(const CompilationContext* cc);
void                cc_set_current_fileline(CompilationContext* cc, const char* file, size_t line);

// pc
ram_type_t          cc_pc(const CompilationContext* cc);

// symbols
void                cc_register_label(CompilationContext* cc, const char* name);
void                cc_register_define(CompilationContext* cc, const char* name, long value);
long                cc_symbol_value(CompilationContext* cc, const char* name);
bool                cc_is_addr_pending(CompilationContext* cc, ram_type_t pc);

// org
void                cc_save_org(CompilationContext* cc, ram_type_t pc);
void                cc_restore_org(CompilationContext* cc);

// debugging info
const DebuggingInfo* cc_debugging_info(const CompilationContext* cc);
void                 cc_add_debugging_info(CompilationContext* cc);

// binary
void                cc_add_byte(CompilationContext* cc, uint8_t byte);
void                cc_add_word(CompilationContext* cc, uint16_t word);
void                cc_add_string(CompilationContext* cc, size_t sz, char* str);
void                cc_add_zeroes(CompilationContext* cc, size_t sz);
void                cc_add_bytes(CompilationContext* cc, ByteArray ba);
void                cc_replace_special_jmp(CompilationContext* cc);
void                cc_overwrite_word(CompilationContext* cc, ram_type_t addr, uint16_t value);

// errors
void                cc_set_error(CompilationContext* cc, const char* message);
const char*         cc_error_message(const CompilationContext* cc);

// pass
void                cc_advance_to_second_pass(CompilationContext* cc);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
