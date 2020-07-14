/* see: https://github.com/meyerd/flex-bison-example */

%{
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

char *strdup(const char *s);

#define YY_DECL int yylex()

#include "output.h"
#include "compctx.h"
#include "parser.h"

%}

%option noyywrap
%option warn nodefault
%option caseless
%option noyywrap never-interactive nounistd
/* %option debug */

hexa        -?0x[0-9A-F]+
decimal     -?[0-9]+
binary      0b[01]+
filename    \$.*\$:
string      \"([^\"]|\\\")*?\" 
identifier  \.?[a-z_][a-z0-9_]*

%%

"nop"       { return T_NOP; }
"dbg"       { return T_DBG; }
"mov"       { return T_MOV; }
"or"        { return T_OR; }
"and"       { return T_AND; }
"xor"       { return T_XOR; }
"shl"       { return T_SHL; }
"shr"       { return T_SHR; }
"not"       { return T_NOT; }
"add$"      { return T_ADDS; }
"sub$"      { return T_SUBS; }
"mul$"      { return T_MULS; }
"div$"      { return T_DIVS; }
"add"       { return T_ADD; }
"sub"       { return T_SUB; }
"mul"       { return T_MUL; }
"div"       { return T_DIV; }
"mod"       { return T_MOD; }
"ifne"      { return T_IFNE; }
"ifeq"      { return T_IFEQ; }
"ifgt$"     { return T_IFGTS; }
"ifgt%"     { return T_IFGTF; }
"ifgt"      { return T_IFGT; }
"iflt$"     { return T_IFLTS; }
"iflt%"     { return T_IFLTF; }
"iflt"      { return T_IFLT; }
"ifge$"     { return T_IFGES; }
"ifge%"     { return T_IFGEF; }
"ifge"      { return T_IFGE; }
"ifle$"     { return T_IFLES; }
"ifle%"     { return T_IFLEF; }
"ifle"      { return T_IFLE; }
"pushb"     { return T_PUSHB; }
"pushw"     { return T_PUSHW; }
"popb"      { return T_POPB; }
"popw"      { return T_POPW; }
"pusha"     { return T_PUSHA; }
"popa"      { return T_POPA; }
"jmp"       { return T_JMP; }
"jsr"       { return T_JSR; }
"ret"       { return T_RET; }
"dev"       { return T_DEV; }
"ivec"      { return T_IVEC; }
"int"       { return T_INT; }
"iret"      { return T_IRET; }
"wait"      { return T_WAIT; }
"ienab"     { return T_IENAB; }

"db"        { return T_DB; }
"dw"        { return T_DW; }
"bss"       { return T_BSS; }
"org"       { return T_ORG; }
"restore"   { return T_RESTORE; }

"$$"        { return T_DBL_DOLLAR; }
"a"         { return T_A; }
"b"         { return T_B; }
"c"         { return T_C; }
"d"         { return T_D; }
"e"         { return T_E; }
"f"         { return T_F; }
"i"         { return T_I; }
"j"         { return T_J; }
"k"         { return T_K; }
"x"         { return T_X; }
"y"         { return T_Y; }
"xt"        { return T_XT; }
"sp"        { return T_SP; }
"fp"        { return T_FP; }
"pc"        { return T_PC; }
"ov"        { return T_OV; }

'\\''        { yylval.number = '\''; return T_NUMBER; }
'.'          { yylval.number = yytext[1]; return T_NUMBER; }
{decimal}    { yylval.number = strtoull(yytext, NULL, 10); return T_NUMBER; }
{hexa}       { if (yytext[0] == '-') yytext[2] = '-'; yylval.number = strtoull(&yytext[2], NULL, 16); return T_NUMBER; }
{binary}     { yylval.number = strtoull(&yytext[2], NULL, 2); return T_NUMBER; }
{filename}   { yylval.string = calloc(1, strlen(yytext) - 2); strncpy(yylval.string, &yytext[1], strlen(yytext) - 3); return T_FILENAME; }
{identifier} { yylval.string = strdup(yytext); return T_IDENTIFIER; }

{string}     { yylval.string = calloc(1, strlen(yytext) - 1); strncpy(yylval.string, &yytext[1], strlen(yytext) - 2); return T_STRING; }
[ \t]+       /* ignore whitespaces */
;.*$         /* remove comments */
\n           { return T_ENTER; }
.            { return *yytext; }

%%

// avoid warnings
void xxx() {
    (void) input;
    (void) yyunput;
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
