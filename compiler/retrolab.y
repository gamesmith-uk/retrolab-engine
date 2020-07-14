%{
#define _GNU_SOURCE

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "bytearray.h"
#include "compctx.h"
#include "output.h"
#include "lex.h"
#include "parameter.h"

extern int yylex();
extern int yyparse();
extern FILE* yyin;

#define BYTE(n)  (cc_add_byte(cc, (n)))
#define WORD(n)  (cc_add_word(cc, (n)))
#define BYTES(n) (cc_add_bytes(cc, (n)))
#define V_16     (cc_is_addr_pending(cc, cc_pc(cc)))

%}

%code requires {
struct Output* compile(const char* processed_source);
void           yyerror(CompilationContext* cc, const char* fmt, ...);
}

%union {
    uint8_t   byte;
    char*     string;
    long      number;
    ByteArray bytes;
    bool      is_v16;
}

%token <string> T_FILENAME T_STRING T_IDENTIFIER
%token <number> T_NUMBER T_DBL_DOLLAR
%token <byte>   T_A T_B T_C T_D T_E T_F T_I T_J T_K T_X T_Y T_XT T_SP T_FP T_PC T_OV

%token <byte>   T_NOP T_DBG T_MOV T_OR T_AND T_XOR T_SHL T_SHR T_NOT T_ADD T_SUB T_MUL
%token <byte>   T_DIV T_MOD T_IFNE T_IFEQ T_IFGT T_IFLT T_IFGE T_IFLE T_PUSHB T_PUSHW
%token <byte>   T_PUSHA T_POPA
%token <byte>   T_POPB T_POPW T_JMP T_JSR T_RET T_DEV T_IVEC T_INT T_IRET T_WAIT T_IENAB
%token <byte>   T_ADDS T_SUBS T_MULS T_DIVS
%token <byte>   T_IFGTS T_IFLTS T_IFGES T_IFLES T_PUSHBS T_PUSHWS
%token <byte>   T_IFGTF T_IFLTF T_IFGEF T_IFLEF T_PUSHBF T_PUSHWF

%token T_DB T_DW T_BSS T_ORG T_RESTORE

%token T_ENTER

%type <byte> register
%type <number> expr plusminus 
%type <is_v16> parameter dest_parameter

%parse-param {CompilationContext* cc}

%left '+' '-'
%left '*' '/' '%'
%left '<' '>'
%left '&'
%left '^'
%left '|'
%nonassoc '~' UMINUS

%%

/* --- source code --- */

sourcelines: sourcelines sourceline
           | sourceline;

sourceline: '[' T_FILENAME T_NUMBER ']'  { 
                cc_set_current_fileline(cc, $2, $3);
                free($2);
             } line T_ENTER
          ;

line: define
    | label { cc_add_debugging_info(cc); } instruction
    | label data
    | { cc_add_debugging_info(cc); } instruction  
    | data
    | label
    | org
    | /* empty line */
    ;

label: T_IDENTIFIER ':'        { cc_register_label(cc, $1); free($1); }
     ;

define: T_IDENTIFIER '=' expr  { cc_register_define(cc, $1, $3); free($1); }
      ;

org: T_ORG expr                { cc_save_org(cc, $2); }
       | T_ORG T_RESTORE       { cc_restore_org(cc);  }
   ;

/* --- instructions --- */

instruction: T_NOP   { BYTE(0x00); }
           | T_DBG   { BYTE(0x01); }
           | T_MOV   { BYTE(0x02); } dest_parameter ',' parameter
           | T_OR    { BYTE(0x10); } dest_parameter ',' parameter
           | T_AND   { BYTE(0x11); } dest_parameter ',' parameter
           | T_XOR   { BYTE(0x12); } dest_parameter ',' parameter
           | T_SHL   { BYTE(0x13); } dest_parameter ',' parameter
           | T_SHR   { BYTE(0x14); } dest_parameter ',' parameter
           | T_NOT   { BYTE(0x15); } dest_parameter
           | T_ADD   { BYTE(0x20); } dest_parameter ',' parameter
           | T_ADDS  { BYTE(0x21); } dest_parameter ',' parameter
           | T_SUB   { BYTE(0x22); } dest_parameter ',' parameter
           | T_SUBS  { BYTE(0x23); } dest_parameter ',' parameter
           | T_MUL   { BYTE(0x24); } dest_parameter ',' parameter
           | T_MULS  { BYTE(0x25); } dest_parameter ',' parameter
           | T_DIV   { BYTE(0x26); } dest_parameter ',' parameter
           | T_DIVS  { BYTE(0x27); } dest_parameter ',' parameter
           | T_MOD   { BYTE(0x29); } dest_parameter ',' parameter
           | T_IFNE  { BYTE(0x30); } parameter ',' parameter
           | T_IFEQ  { BYTE(0x31); } parameter ',' parameter
           | T_IFGT  { BYTE(0x32); } parameter ',' parameter
           | T_IFGTS { BYTE(0x33); } parameter ',' parameter
           | T_IFLT  { BYTE(0x35); } parameter ',' parameter
           | T_IFLTS { BYTE(0x36); } parameter ',' parameter
           | T_IFGE  { BYTE(0x38); } parameter ',' parameter
           | T_IFGES { BYTE(0x39); } parameter ',' parameter
           | T_IFLE  { BYTE(0x3b); } parameter ',' parameter
           | T_IFLES { BYTE(0x3c); } parameter ',' parameter
           | T_PUSHB { BYTE(0x50); } parameter
           | T_PUSHW { BYTE(0x51); } parameter
           | T_POPB  { BYTE(0x52); } parameter
           | T_POPW  { BYTE(0x53); } parameter
           | T_PUSHA { BYTE(0x54); }
           | T_POPA  { BYTE(0x55); }
           | T_JMP   { BYTE(0x60); } parameter  { if ($3) cc_replace_special_jmp(cc); }
           | T_JSR   { BYTE(0x61); } parameter
           | T_RET   { BYTE(0x62); }
           | T_DEV   { BYTE(0x70); } parameter ',' parameter
           | T_IVEC  { BYTE(0x71); } parameter ',' parameter
           | T_INT   { BYTE(0x72); } parameter ',' parameter
           | T_IRET  { BYTE(0x73); }
           | T_WAIT  { BYTE(0x74); }
           | T_IENAB { BYTE(0x75); } parameter
           ;

/* --- data --- */

data: T_DB  byte_list
    | T_DW  word_list
    | T_BSS expr                  { cc_add_zeroes(cc, $2); }
    ;

byte_list: byte
         | byte_list ',' byte
         ;

byte: expr                        { BYTE(check_limit(cc, $1, -0x80, 0xff)); }
    | T_STRING                    { cc_add_string(cc, strlen($1), $1); free($1); }
    ;

word_list: expr                   { WORD(check_limit(cc, $1, -0x8000, 0xffff)); }
         | word_list ',' expr     { WORD(check_limit(cc, $3, -0x8000, 0xffff)); }
         ;

/* --- parameters --- */

parameter: expr                   { ByteArray a = immediate_number(cc, $1, V_16); BYTES(a); $$ = (a.sz == 3); /* is_v16? */ }
         | dest_parameter
         ;

dest_parameter: '[' expr ']'                         { BYTES(next_number(cc, 0x8c, 0x8e, $2, V_16)); }
              | '^' '[' expr ']'                     { BYTES(next_number(cc, 0x8d, 0x8f, $3, V_16)); }
              | register                             { BYTE(0x90 + $1); }
              | '[' register ']'                     { BYTE(0xa0 + $2); }
              | '^' '[' register ']'                 { BYTE(0xb0 + $3); }
              | '[' register plusminus expr ']'      { BYTES(next_number_sign(cc, 0xc0 + $2, 0xe0 + $2, $3 * $4, V_16)); }
              | '^' '[' register plusminus expr ']'  { BYTES(next_number_sign(cc, 0xd0 + $3, 0xf0 + $3, $4 * $5, V_16)); }
              ;

expr: expr '+' expr          { $$ = $1 + $3; }
    | expr '-' expr          { $$ = $1 - $3; }
    | expr '*' expr          { $$ = $1 * $3; }
    | expr '/' expr          { $$ = $1 / $3; }
    | expr '%' expr          { $$ = $1 % $3; }
    | expr '&' expr          { $$ = $1 & $3; }
    | expr '^' expr          { $$ = $1 ^ $3; }
    | expr '|' expr          { $$ = $1 | $3; }
    | expr '<' '<' expr      { $$ = $1 << $4; }
    | expr '>' '>' expr      { $$ = $1 >> $4; }
    | '~' expr               { $$ = ~$2; }
    | '-' expr %prec UMINUS  { $$ = -$2; }
    | '(' expr ')'           { $$ = $2; }
    | '$'                    { $$ = cc_pc(cc); }
    | T_DBL_DOLLAR           { $$ = cc_current_expression_pc(cc); }
    | T_IDENTIFIER           { $$ = cc_symbol_value(cc, $1); free($1); }
    | T_NUMBER
    ;

register: T_A   { $$ = 0x0; }
        | T_B   { $$ = 0x1; }
        | T_C   { $$ = 0x2; }
        | T_D   { $$ = 0x3; }
        | T_E   { $$ = 0x4; }
        | T_F   { $$ = 0x5; }
        | T_I   { $$ = 0x6; }
        | T_J   { $$ = 0x7; }
        | T_K   { $$ = 0x8; }
        | T_X   { $$ = 0x9; }
        | T_Y   { $$ = 0xa; }
        | T_XT  { $$ = 0xb; }
        | T_SP  { $$ = 0xc; }
        | T_FP  { $$ = 0xd; }
        | T_PC  { $$ = 0xe; }
        | T_OV  { $$ = 0xf; }
        ;

plusminus: '+' { $$ = 1;  }
         | '-' { $$ = -1; }
         ;

%%

Output*
compile(const char* processed_source)
{
    CompilationContext* cc = cc_new();

    // 1st pass
    YY_BUFFER_STATE buffer_state = yy_scan_string(processed_source);
    yyparse(cc);
    yy_delete_buffer(buffer_state);

    // 2nd pass
    if (cc_error_message(cc) == NULL) {
        cc_advance_to_second_pass(cc);
        YY_BUFFER_STATE buffer_state = yy_scan_string(processed_source);
        yyparse(cc);
        yy_delete_buffer(buffer_state);
    }

    return cc_move_to_output(cc);
}

void yyerror(CompilationContext* cc, const char* fmt, ...) {
    static char errbuf[4096];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);

    cc_set_error(cc, errbuf);
    // fprintf(stderr, "Parse error: %s in %s:%d\n", s, cc_current_filename(cc), cc_current_line(cc));
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
