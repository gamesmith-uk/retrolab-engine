#include <stdio.h>
#include <string.h>

#include "compiler/compiler.h"
#include "compiler/input.h"
#include "compiler/output.h"
#include "emulator/breakpoints.h"
#include "emulator/cpu.h"
#include "emulator/emulator.h"
#include "emulator/memory.h"
#include "exec/exec.h"

// {{{ test infrastructure

// inspired by http://eradman.com/posts/tdd-in-c.html

static int tests_run = 0;

#define BRED "\e[1;31m"
#define BGRN "\e[0;32m"
#define RST "\e[0m"

#define FAIL() printf("  " BRED "\u2718 " RST "%s() : line %d\n", __func__, __LINE__)
#define _assert(test) do { if (!(test)) { FAIL(); exit(1); } } while(0)
#define verify(test) do { int r=test(); if (r == 0) printf("  " BGRN "\u2714" RST " %s()\n", #test); tests_run++; if(r) return r; } while(0)

static void print_bytes(const uint8_t data[], size_t sz)
{
    printf(BRED "Array found: ");
    if (sz == 0)
        printf("empty array");
    for (size_t i = 0; i < sz; ++i)
        printf("%02X ", data[i]);
    printf("\n" RST);
}

static void debug_emulator()
{
    printf(BRED);
    printf("RAM: ");
    for (size_t i = 0; i < 24; ++i)
        printf("%02X ", ram[i]);
    printf("\n");
    for (uint8_t i = 0; i < 16; ++i) {
        printf("%s = 0x%04X ", cpu_register_name(i), cpu_register(i));
        if (i == 7)
            printf("\n");
    }
    printf("\n" RST);
}

#define assert_c(cmd, ...) {                        \
    Output* o = compile_string(cmd);                \
    const char* error = output_error_message(o);    \
    if (error)                                      \
        fprintf(stderr, BRED "compilation error: %s\n" RST, error); \
    _assert(error == NULL);                         \
    size_t sz = output_binary_size(o);              \
    const uint8_t* data = output_binary_data(o);    \
    if (sz != sizeof((uint8_t[]) { __VA_ARGS__ }) || memcmp(data, (uint8_t[]) { __VA_ARGS__ }, sz) != 0) \
        print_bytes(data, sz);                      \
    _assert(sz == sizeof((uint8_t[]) { __VA_ARGS__ })); \
    _assert(memcmp(data, (uint8_t[]) { __VA_ARGS__ }, sz) == 0);  \
    output_free(o);                                 \
}

#define assert_error(cmd) {                         \
    Output* o = compile_string(cmd);                \
    const char* error = output_error_message(o);    \
    if (!error)                                     \
        fprintf(stderr, BRED "An error was expected." RST); \
    _assert(error);                                 \
    output_free(o);                                 \
}

#define ASSERT_C(name, code, ...) static int name() { assert_c(code, __VA_ARGS__); return 0; }
#define ASSERT_ERROR(name, code)  static int name() { assert_error(code); return 0; }

#define ASSERT_EXEC(name, cmd, ...)                                 \
static int name() {                                                 \
    emulator_init(true);                                            \
    Output* output = compile_string(cmd);                           \
    const char* error = output_error_message(output);               \
    if (!error) {                                                   \
        ram_load(0x0, output_binary_data(output), output_binary_size(output)); \
        cpu_load_debugging_info(output_debugging_info(output));     \
        int i = 0;                                                  \
        __attribute__((unused)) int cpu_step_return;                \
        while ((cpu_PC() < 1 || ram[cpu_PC()] != 0) && i++ < 30)    \
            cpu_step_return = cpu_step();                           \
        if (!(__VA_ARGS__))                                         \
            debug_emulator();                                       \
        _assert((__VA_ARGS__));                                     \
    } else {                                                        \
        printf(BRED "%s\n" RST, error);                             \
        _assert(0);                                                 \
    }                                                               \
    output_free(output);                                            \
    emulator_destroy();                                             \
    return 0;                                                       \
}

// }}}

//
// COMPILER
//

// {{{ preprocessing

static int
input_from_string()
{
    Input* input = input_new_from_string("test1");
    _assert(input_file_count(input) == 2);
    _assert(strcmp(input_filename(input, 0), "retrolab.def") == 0);
    _assert(strcmp(input_filename(input, 1), "main.s") == 0);
    _assert(strcmp(input_source(input, 1), "test1") == 0);
    input_free(input);
    return 0;
};

static int
precompile()
{
    Input* input = input_new();
    input_add_file(input, "bbb.s", "text3");
    input_add_file(input, "aaa.s", "text0");
    input_add_file(input, "main.s", "text1\nxxx");
    input_add_file(input, "retrolab.def", "text2");
    input_add_file(input, "data.bin", "text4");

    char* text = input_precompile(input);
    // printf("%s\n", text);
    const char* expected = "[$retrolab.def$:1] text2\n"
                           "[$main.s$:1] text1\n"
                           "[$main.s$:2] xxx\n"
                           "[$aaa.s$:1] text0\n"
                           "[$bbb.s$:1] text3\n";
    _assert(strcmp(text, expected) == 0);
    free(text);
    input_free(input);
    return 0;
}

static int
preprocessing()
{
    printf("Pre-processing:\n");
    verify(input_from_string);
    verify(precompile);
    printf("\n");
    return 0;
}

// }}}

// {{{ basic compilation: empty file, nop, comments

static int
empty_file()
{
    assert_c("");
    return 0;
}

static int
nop()
{
    assert_c("nop", 0x0);
    assert_c("nop\nnop", 0x0, 0x0);
    return 0;
}

static int
comments()
{
    assert_c("nop  ; hello world", 0x0);
    assert_c("nop  ; hello ; world", 0x0);
    assert_c("nop  ; hello\n  ;test\nnop ; second test", 0x0, 0x0);
    return 0;
}

static int
basic_compilation()
{
    printf("Basic compilation:\n");
    verify(empty_file);
    verify(nop);
    verify(comments);
    printf("\n");
    return 0;
}

// }}}

// {{{ parameters

ASSERT_C(small_literal_pos_dec,   "pushb 32",        0x50, 0x20);
ASSERT_C(small_literal_neg_dec,   "pushb -2",        0x50, 0b01111110);
ASSERT_C(small_literal_neg_dec2,  "pushb -64",       0x50, 0b01000000);
ASSERT_C(small_literal_pos_hex,   "pushb 0x12",      0x50, 0x12);
ASSERT_C(small_literal_neg_hex,   "pushb -0X2",      0x50, 0b01111110);
ASSERT_C(small_literal_bin,       "pushb 0b11100",   0x50, 0x1c);
ASSERT_C(mid_literal_pos,         "pushb 128",       0x50, 0x8a, 128);
ASSERT_C(mid_literal_neg,         "pushb -80",       0x50, 0x8a, 0xb0);
ASSERT_C(large_literal_pos,       "pushb 0x1234",    0x50, 0x8b, 0x34, 0x12);
ASSERT_C(large_literal_neg,       "pushb -5000",     0x50, 0x8b, 0x78, 0xec);
ASSERT_C(large_literal_neg2,      "pushb -128",      0x50, 0x8a, 0x80);
ASSERT_C(large_literal_neg3,      "pushb -129",      0x50, 0x8b, 0x7f, 0xff);
ASSERT_C(char_literal,            "pushb 'A'",       0x50, 0x8a, 'A');
ASSERT_C(char_literal_quote,      "pushb '\\''",     0x50, '\'');
ASSERT_C(char_semicolon,          "pushb ';'",       0x50, ';');
ASSERT_ERROR(large_literal_error, "pushb 0x123456");
ASSERT_ERROR(char_literal_error,  "pushb 'AB'");

ASSERT_C(address8_v8,           "pushb [0x16]",    0x50, 0x8c, 0x16);
ASSERT_C(address16_v8,          "pushb ^[0x16]",   0x50, 0x8d, 0x16);
ASSERT_C(address8_v16,          "pushb [0x4F16]",  0x50, 0x8e, 0x16, 0x4f);
ASSERT_C(address16_v16,         "pushb ^[0x4F16]", 0x50, 0x8f, 0x16, 0x4f);

ASSERT_C(reg,                   "pushb C",         0x50, 0x92);
ASSERT_C(reg_addr8,             "pushb [D]",       0x50, 0xa3);
ASSERT_C(reg_addr16,            "pushb ^[XT]",     0x50, 0xbb);

ASSERT_C(reg_plus_v8,           "pushb [B + 12]",      0x50, 0xc1, 12);
ASSERT_C(reg_plus_v8_neg,       "pushb ^[B - 12]",     0x50, 0xd1, 0xf4);
ASSERT_C(reg_plus_v16,          "pushb [B + 0x1234]",  0x50, 0xe1, 0x34, 0x12);
ASSERT_C(reg_plus_v16_2,        "pushb ^[B + 0x1234]", 0x50, 0xf1, 0x34, 0x12);
ASSERT_C(reg_plus_v16_exc,      "pushb [B + 0x90]",    0x50, 0xe1, 0x90, 0x00);
ASSERT_ERROR(reg_plus_v16_error, "pushb [B + 0x9000]");

static int
parameters()
{
    printf("Parameters:\n");
    verify(small_literal_pos_dec);
    verify(small_literal_neg_dec);
    verify(small_literal_neg_dec2);
    verify(small_literal_pos_hex);
    verify(small_literal_neg_hex);
    verify(small_literal_bin);
    verify(mid_literal_pos);
    verify(mid_literal_neg);
    verify(large_literal_pos);
    verify(large_literal_neg);
    verify(large_literal_neg2);
    verify(large_literal_neg3);
    verify(char_literal);
    verify(char_literal_quote);
    verify(char_semicolon);
    verify(large_literal_error);
    verify(char_literal_error);

    verify(address8_v8);
    verify(address16_v8);
    verify(address8_v16);
    verify(address16_v16);

    verify(reg);
    verify(reg_addr8);
    verify(reg_addr16);

    verify(reg_plus_v8);
    verify(reg_plus_v8_neg);
    verify(reg_plus_v16);
    verify(reg_plus_v16_2);
    verify(reg_plus_v16_exc);
    verify(reg_plus_v16_error);
    printf("\n");
    return 0;
}

// }}}

// {{{ expressions

ASSERT_C(plus,          "pushb 2 + 3",           0x50, 5);
ASSERT_C(plusplus,      "pushb 2 + 3 + 4",       0x50, 9);
ASSERT_C(order,         "pushb 2*3 + 4*5",       0x50, 26);
ASSERT_C(shift,         "pushb 1 << 3",          0x50, 0b1000);
ASSERT_C(expr,          "pushb (1 << 3) | 0b10", 0x50, 0b1010);
ASSERT_C(expr2,         "pushb 1 + 2 * 3",       0x50, 7);
ASSERT_C(expr3,         "pushb (1 + 2) * 3",     0x50, 9);
ASSERT_C(not_op,        "pushb ~0b11010101",     0x50, 0x8b, 0b00101010, 0xff);
ASSERT_C(complex_expr,  "pushb ((2 - 4) * -1 + (1 << 4)) + 1", 0x50, 19);

static int
expressions()
{
    printf("Expressions:\n");
    verify(plus);
    verify(plusplus);
    verify(order);
    verify(shift);
    verify(expr);
    verify(expr2);
    verify(expr3);
    verify(not_op);
    verify(complex_expr);
    printf("\n");
    return 0;
}

// }}}

// {{{ operations

ASSERT_C(dual_parameter,           "mov A, 0x12", 0x2, 0x90, 0x12);
ASSERT_ERROR(dual_parameter_error, "mov 0x12, A");
ASSERT_ERROR(not_error,            "not 0x12");
ASSERT_C(not_ok,                   "not A",       0x15, 0x90);
ASSERT_C(signed_op,                "add$ A, [B]", 0x21, 0x90, 0xa1);
ASSERT_C(jmp_normal,               "jmp 0x5", 0x60, 0x5);
ASSERT_C(jmp_special,              "jmp 0x1234", 0x63, 0x34, 0x12);

// TODO - jmp, sign

static int
operations()
{
    printf("Operations:\n");
    verify(dual_parameter);
    verify(dual_parameter_error);
    verify(not_error);
    verify(not_ok);
    verify(signed_op);
    verify(jmp_normal);
    verify(jmp_special);
    printf("\n");
    return 0;
}

// }}}

// {{{ data

ASSERT_C(data_byte,        "db 0x12",              0x12);
ASSERT_C(data_bytes,       "db 0x12, 0xaf, 0xca",  0x12, 0xaf, 0xca);
ASSERT_C(data_seq,         "db 0, 0, 0, 3",        0, 0, 0, 3);
ASSERT_C(data_word,        "dw 0x1234",            0x34, 0x12);
ASSERT_C(data_words,       "dw 0x1234, 0x08, 'A'", 0x34, 0x12, 0x08, 0x00, 'A', 0x00);
ASSERT_C(data_expression,  "db 100 * 2 + 25, 4",   225, 4);
ASSERT_C(data_negative,    "db -100",              0x9c);
ASSERT_C(ascii,            "db \"AB;!\"",          'A', 'B', ';', '!');
ASSERT_C(ascii2,           "db \"AB\", 13, \"x\"", 'A', 'B', 13, 'x');
ASSERT_C(ascii3,           "db \"AB\\\"CD\"",      'A', 'B', '"', 'C', 'D');    // db "AB\"CD"
ASSERT_C(bss,              "bss 4",                0x0, 0x0, 0x0, 0x0);
ASSERT_ERROR(data_error1,  "db 300");
ASSERT_ERROR(data_error2,  "db -200");
ASSERT_ERROR(ascii_error1, "db \"Hello world!");

static int
data()
{
    printf("Operations:\n");
    verify(data_byte);
    verify(data_bytes);
    verify(data_seq);
    verify(data_word);
    verify(data_words);
    verify(data_expression);
    verify(data_negative);
    verify(data_error1);
    verify(data_error2);
    verify(ascii);
    verify(ascii2);
    verify(ascii3);
    verify(ascii_error1);
    verify(bss);
    printf("\n");
    return 0;
}

// }}}

// {{{ $ and $$

ASSERT_C(dollar_sign,        "db 0, 0, 0, $",                   0, 0, 0, 3);
ASSERT_C(dollar_double_sign, "dw 0 \n db 0, 0, $, 0, $$",       0, 0, 0, 0, 4, 0, 2);
ASSERT_C(dollar_strlen,      "dw 0xFFFF \n db \"Hello\", $-$$", 0xff, 0xff, 'H', 'e', 'l', 'l', 'o', 5);
ASSERT_C(jmp_dollar,         "nop \n jmp $$",                   0x00, 0x60, 0x01);

static int
dollar()
{
    printf("$ and $$:\n");
    verify(dollar_sign);
    verify(dollar_double_sign);
    verify(dollar_strlen);
    verify(jmp_dollar);
    printf("\n");
    return 0;
}


// }}}

// {{{ defines

ASSERT_C(def_simple,       "MYDEF = 0x12 \n mov A, MYDEF",              0x02, 0x90, 0x12);
ASSERT_C(def_overlapping,  "MYDEF = 0x12 \n MY = 0x13 \n db MY, MYDEF", 0x13, 0x12);
ASSERT_C(def_expression,   "mydef = 1 << 3 \n db mydef",                0b1000);
ASSERT_C(def_dollar,       "db 0x0 \n xx = $ \n db xx",                 0x00, 0x01);
ASSERT_C(def_forward,      "mov a, TEST\n TEST=1",                      0x02, 0x90, 0x8b, 0x01, 0x00);
ASSERT_C(def_forward_jmp,  "jmp TEST\n TEST=0xa",                       0x63, 0x0a, 0x00);
ASSERT_ERROR(def_invalid,  "ab@c = 3");
ASSERT_ERROR(redefine_op,  "mov = 3");
ASSERT_ERROR(redefine_def, "xx = 3 \n xx = 4");
ASSERT_ERROR(def_dot,      ".abc = 3");

static int
defines()
{
    printf("Defines:\n");
    verify(def_simple);
    verify(def_overlapping);
    verify(def_expression);
    verify(def_dollar);
    verify(def_forward);
    verify(def_forward_jmp);
    verify(def_invalid);
    verify(redefine_op);
    verify(redefine_def);
    verify(def_dot);
    printf("\n");
    return 0;
}

// }}}

// {{{ labels

ASSERT_C(label_simple_bw,         "nop \n xx: jmp xx",        0x00, 0x60, 0x01);
ASSERT_C(label_simple_bw_newline, "nop \n xx: \n jmp xx",     0x00, 0x60, 0x01);
ASSERT_C(label_simple_fw,         "jmp xx \n nop \n xx: nop", 0x63, 0x04, 0x00, 0x00, 0x00);
ASSERT_C(label_2nd_param_fw,      "mov a, [xx] \n"
                                  "nop \n" 
                                  "xx: nop",                  0x02, 0x90, 0x8e, 0x06, 0x00, 0x00, 0x00);
ASSERT_C(local_label,             "nop \n .xx: jmp .xx",      0x00, 0x60, 0x01);
ASSERT_C(global_local_label,      "aa: \n"
                                  "     nop\n"
                                  ".bb: jmp .bb\n"
                                  "xx: \n"
                                  ".bb: jmp .bb",             0x00, 0x60, 0x1, 0x60, 0x03);
ASSERT_C(global_local_label2,     "     nop\n"
                                  ".bb: jmp .bb\n"
                                  "xx: \n"
                                  ".bb: jmp .bb",             0x00, 0x60, 0x1, 0x60, 0x03);
ASSERT_C(label_strlen,            "nop\n"
                                  "msg: db \"ABC\" \n"
                                  "len = $-msg\n"
                                  "     db len",              0x00, 'A', 'B', 'C', 0x03);
ASSERT_C(label_expr,              "nop\n"
                                  "xx: jmp xx + 4",           0x00, 0x60, 0x05);
ASSERT_ERROR(label_not_found,     "jmp xx");
ASSERT_ERROR(two_labels,          "aa: bb: nop");
ASSERT_ERROR(repeated_label,      "aax: nop\naax: nop");
ASSERT_ERROR(repeated_local_label,"aax: nop\n.bbx: nop\n.bbx: nop");

static int
labels()
{
    printf("Labels:\n");
    verify(label_simple_bw);
    verify(label_simple_bw_newline);
    verify(label_simple_fw);
    verify(label_2nd_param_fw);
    verify(local_label);
    verify(global_local_label);
    verify(global_local_label2);
    verify(label_strlen);
    verify(label_expr);
    verify(label_not_found);
    verify(two_labels);
    verify(repeated_label);
    verify(repeated_local_label);
    printf("\n");
    return 0;
}

// }}}

// {{{ org

ASSERT_C(org_1,        "    mov A, 0x12 \n"
                       "org 6 \n"
                       "    dbg \n",           0x02, 0x90, 0x12, 0x00, 0x00, 0x00, 0x01);
ASSERT_C(org_restore,  "    mov A, 0x12 \n"
                       "org 6 \n"
                       "    dbg \n"
                       "org restore \n"
                       "    dbg",              0x02, 0x90, 0x12, 0x01, 0x00, 0x00, 0x01);
ASSERT_C(org_expr,     "DEF = 4 \n"
                       "    mov A, 0x12 \n"
                       "org DEF + 2 \n"
                       "    dbg \n",           0x02, 0x90, 0x12, 0x00, 0x00, 0x00, 0x01);
ASSERT_C(org_multiple, "    mov A, 0x12 \n"
                       "org 12 \n"
                       "    dbg \n"
                       "org 6 \n"
                       "    iret \n"              
                       "org restore \n"
                       "    dbg",              0x02, 0x90, 0x12, 0x01, 0x00, 0x00, 0x73,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x01);
ASSERT_C(org_dollar,   "org 0x4\n"
                       "msg: db \"Hello\"\n"
                       "test = $ - msg\n"
                       "db test",              0x00, 0x00, 0x00, 0x00, 'H', 'e', 'l', 'l', 'o', 0x05);

static int
org()
{
    printf("Org:\n");
    verify(org_1);
    verify(org_restore);
    verify(org_expr);
    verify(org_multiple);
    verify(org_dollar);
    printf("\n");
    return 0;
}

// }}}

// {{{ debugging information

static int
dbg_simple()
{
    Output* o = compile_string("nop \n \n dbg");
    size_t sz = output_binary_size(o);
    const uint8_t* data = output_binary_data(o);
    _assert(sz == 2);
    _assert(data[0] == 0x0);
    _assert(data[1] == 0x1);
    const DebuggingInfo* dbg = output_debugging_info(o);
    _assert(dbg);
    _assert(dbg->files_sz == 1);
    _assert(strcmp(dbg->files[0], "main.s") == 0);
    _assert(dbg->locations_sz == 2);
    _assert(dbg->locations[0].pc == 0);
    _assert(dbg->locations[0].file_number == 0);
    _assert(dbg->locations[0].line == 1);
    _assert(dbg->locations[1].pc == 1);
    _assert(dbg->locations[1].file_number == 0);
    _assert(dbg->locations[1].line == 3);
    
    char buf[2048];
    debug_json(dbg, buf, sizeof(buf));
    printf("  -> JSON: %s\n", buf);

    output_free(o);
    return 0;
}

static int
dbg_three_files()
{
    Input* input = input_new();
    input_add_file(input, "defines.def", "MYDEF = 0x12");
    input_add_file(input, "aaa.s", "pushb MYDEF");
    input_add_file(input, "main.s", "nop");
    Output* o = compile_input(input);
    input_free(input);

    size_t sz = output_binary_size(o);
    const uint8_t* data = output_binary_data(o);
    _assert(sz == 3);
    _assert(data[0] == 0x0);
    _assert(data[1] == 0x50);
    _assert(data[2] == 0x12);
    const DebuggingInfo* dbg = output_debugging_info(o);
    _assert(dbg);
    _assert(dbg->files_sz == 2);
    _assert(strcmp(dbg->files[0], "main.s") == 0);
    _assert(strcmp(dbg->files[1], "aaa.s") == 0);
    _assert(dbg->locations_sz == 2);
    _assert(dbg->locations[0].pc == 0);
    _assert(dbg->locations[0].file_number == 0);
    _assert(dbg->locations[0].line == 1);
    _assert(dbg->locations[1].pc == 1);
    _assert(dbg->locations[1].file_number == 1);
    _assert(dbg->locations[1].line == 1);

    // print to json
    printf("Output to json result: ");
    char buf[1024 * 1024];
    output_to_json(o, buf, sizeof(buf));
    printf("%s\n", buf);

    output_free(o);

    return 0;
}

static int
debugging()
{
    printf("Debugging information:\n");
    verify(dbg_simple);
    verify(dbg_three_files);
    printf("\n");
    return 0;
}

// }}}

// {{{ other situation found on the wild

ASSERT_C(undef_symbol_plus_one, "mov A, (xx + 1) \n"
                                "xx = 1",           0x02, 0x90, 0x8b, 0x02, 0x00);
ASSERT_C(msglen,                "        mov A, (msglen + 1)\n"
                                "org 7\n"
                                "msg:    db \"abc\"\n"
                                "msglen = $-msg",    0x02, 0x90, 0x8b, 0x04, 0x00, 0x00, 0x00, 'a', 'b', 'c');

static int
on_the_wild()
{
    printf("Found on the wild:\n");
    verify(undef_symbol_plus_one);
    verify(msglen);
    printf("\n");
    return 0;
}

// }}}

// 
// EMULATOR
//

// {{{ special operations

ASSERT_EXEC(nop_op, "nop", cpu_PC() == 0x1);
ASSERT_EXEC(dbg_op, "dbg", cpu_step_return == DEBUGGER_REQUESTED);

static int special_ops()
{
    printf("Special operations:\n");
    verify(nop_op);
    verify(dbg_op);
    printf("\n");
    return 0;
}

// }}}

// {{{ mov [origin]

ASSERT_EXEC(literal_pos,  "mov B, 0x12",       cpu_PC() == 3 && cpu_B() == 0x12);
ASSERT_EXEC(literal_neg,  "mov B, -3",         cpu_PC() == 3 && cpu_B() == 0xfffd);
ASSERT_EXEC(next_v8,      "mov A, 0xF0",       cpu_PC() == 4 && cpu_A() == 0xf0);
ASSERT_EXEC(next_v16,     "mov A, 0xF020",     cpu_PC() == 5 && cpu_A() == 0xf020);
ASSERT_EXEC(next_v8_a,    "mov [0xF0], 0x64\n"
                          "mov A, [0xF0]",     cpu_A() == 0x64);
ASSERT_EXEC(next_v8_ind,  "mov [0xF0], 0x64\n"
                          "mov [0xF1], 0x32\n"
                          "mov A, ^[0xF0]",    ram[0xF0] == 0x64 && ram[0xF1] == 0x32 &&
                                               cpu_PC() == 13 && cpu_A() == 0x3264);
ASSERT_EXEC(next_v16_a,   "mov [0x1F0], 0x64\n"
                          "mov A, [0x1F0]",    cpu_A() == 0x64);
ASSERT_EXEC(next_v16_ind, "mov [0x1F0], 0x64\n"
                          "mov [0x1F1], 0x32\n"
                          "mov A, ^[0x1F0]",   ram[0x1f0] == 0x64 && ram[0x1f1] == 0x32 &&
                                               cpu_PC() == 16 && cpu_A() == 0x3264);
ASSERT_EXEC(reg_a,        "mov [0x1F], 0x64\n"
                          "mov A, 0x1F\n"
                          "mov B, [A]",        ram[0x1f] == 0x64 && cpu_PC() == 11 &&
                                               cpu_A() == 0x1f && cpu_B() == 0x64);
ASSERT_EXEC(reg_ind,      "mov [0x1E], 0x64\n"
                          "mov [0x1F], 0x12\n"
                          "mov A, 0x1E\n"
                          "mov B, ^[A]",       ram[0x1e] == 0x64 && ram[0x1f] == 0x12 && cpu_PC() == 15 &&
                                               cpu_A() == 0x1e && cpu_B() == 0x1264);
ASSERT_EXEC(reg_v8,       "mov [0x1E], 0x64\n"
                          "mov A, 0x10\n"
                          "mov B, [A + 0xE]",  ram[0x1e] == 0x64 && cpu_PC() == 12 &&
                                               cpu_A() == 0x10 && cpu_B() == 0x64);
ASSERT_EXEC(reg_v8_minus, "mov [0x1E], 0x64\n"
                          "mov A, 0x20\n"
                          "mov B, [A - 2]",    ram[0x1e] == 0x64 && cpu_PC() == 12 &&
                                               cpu_A() == 0x20 && cpu_B() == 0x64);
ASSERT_EXEC(reg_v8_addr,  "mov [0x1E], 0x34\n"
                          "mov [0x1F], 0x12\n"
                          "mov A, 0x10\n"
                          "mov B, ^[A + 0xE]", ram[0x1e] == 0x34 && ram[0x1f] == 0x12 && cpu_PC() == 15 &&
                                               cpu_A() == 0x10 && cpu_B() == 0x1234);
ASSERT_EXEC(reg_v16,      "mov [0x11E], 0x64\n"
                          "mov A, 0x10\n"
                          "mov B, [A + 0x10E]", ram[0x11e] == 0x64 && cpu_PC() == 14 &&
                                                cpu_A() == 0x10 && cpu_B() == 0x64);
ASSERT_EXEC(reg_v16_minus, "mov [0x1E], 0x64\n"
                           "mov A, 0x200\n"
                           "mov B, [A - 0x1e2]", ram[0x1e] == 0x64 && cpu_PC() == 15 &&
                                                 cpu_A() == 0x200 && cpu_B() == 0x64);
ASSERT_EXEC(reg_v16_addr, "mov [0x11E], 0x64\n"
                          "mov [0x11F], 0x0F\n"
                          "mov A, 0x10\n"
                          "mov B, ^[A + 0x10E]", ram[0x11e] == 0x64 && ram[0x11f] == 0x0f && cpu_PC() == 19 &&
                                                 cpu_A() == 0x10 && cpu_B() == 0xf64);


static int mov_origin()
{
    printf("mov [origin]:\n");
    verify(literal_pos);
    verify(literal_neg);
    verify(next_v8);
    verify(next_v16);
    verify(next_v8_a);
    verify(next_v8_ind);
    verify(next_v16_a);
    verify(next_v16_ind);
    verify(reg_a);
    verify(reg_ind);
    verify(reg_v8);
    verify(reg_v8_minus);
    verify(reg_v8_addr);
    verify(reg_v16);
    verify(reg_v16_minus);
    verify(reg_v16_addr);
    printf("\n");
    return 0;
}

// }}}

// {{{ mov [destination]

ASSERT_EXEC(d_v8_addr,    "mov [0xF0], 0x64",      cpu_PC() == 5 && ram[0xF0] == 0x64);
ASSERT_EXEC(d_v8_addr2,   "mov [0xF0], 0x64\n"
                          "mov [0xF1], 0x32",      cpu_PC() == 9 && ram[0xF0] == 0x64 && ram[0xF1] == 0x32);
ASSERT_EXEC(d_v8_ind,     "mov ^[0xF0], 0x6412",   cpu_PC() == 6 && ram[0xF0] == 0x12 && ram[0xF1] == 0x64);
ASSERT_EXEC(d_v16_addr,   "mov [0x1A0], 0x64",     cpu_PC() == 6 && ram[0x1A0] == 0x64);
ASSERT_EXEC(d_v16_ind,    "mov ^[0x1A0], 0x6412",  cpu_PC() == 7 && ram[0x1A0] == 0x12 && ram[0x1A1] == 0x64);
ASSERT_EXEC(d_reg_addr,   "mov A, 0x12\n"
                          "mov [A], 0x64",         cpu_PC() == 7 && ram[0x12] == 0x64);
ASSERT_EXEC(d_reg_ind,    "mov A, 0x12\n"
                          "mov ^[A], 0x648A",      cpu_PC() == 8 && ram[0x12] == 0x8a && ram[0x13] == 0x64);
ASSERT_EXEC(d_reg_v8,     "mov A, 0x12\n"
                          "mov [A + 0x20], 0x64",  cpu_PC() == 8 && ram[0x32] == 0x64);
ASSERT_EXEC(d_reg_v8_ind, "mov A, 0x12\n"
                          "mov ^[A + 0x20], 0x648A", cpu_PC() == 9 && ram[0x32] == 0x8a && ram[0x33] == 0x64);
ASSERT_EXEC(d_reg_v16,    "mov A, 0x12\n"
                          "mov [A + 0x120], 0x64",   cpu_PC() == 9 && ram[0x132] == 0x64);
ASSERT_EXEC(d_reg_v16_ind, "mov A, 0x12\n"
                           "mov ^[A + 0x120], 0x648A", cpu_PC() == 10 && ram[0x132] == 0x8a && ram[0x133] == 0x64);

static int mov_dest()
{
    printf("mov [destination]:\n");
    verify(d_v8_addr);
    verify(d_v8_addr2);
    verify(d_v8_ind);
    verify(d_v16_addr);
    verify(d_v16_ind);
    verify(d_reg_addr);
    verify(d_reg_ind);
    verify(d_reg_v8);
    verify(d_reg_v8_ind);
    verify(d_reg_v16);
    verify(d_reg_v16_ind);
    printf("\n");
    return 0;
}

// }}}

// {{{ logic

ASSERT_EXEC(logic_or,           "mov A, 0b1010\n"
                                "mov B, 0b0100\n"
                                "or  B, A",         cpu_B() == 0b1110);
ASSERT_EXEC(logic_and,          "mov A, 0b1010\n"
                                "mov B, 0b0111\n"
                                "and B, A",         cpu_B() == 0b10);
ASSERT_EXEC(logic_xor,          "mov A, 0b1010\n"
                                "mov B, 0b0111\n"
                                "xor B, A",         cpu_B() == 0b1101);
ASSERT_EXEC(logic_shl,          "mov B, 0b10101100\n"
                                "mov A, 2\n"
                                "shl B, A",         cpu_B() == 0b1010110000);
ASSERT_EXEC(logic_shl_8bit,     "mov [0x10], 0b10101100\n"
                                "mov A, 2\n"
                                "shl [0x10], A",    ram[0x10] == 0b10110000);
ASSERT_EXEC(logic_shl_overflow, "mov B, 0xFFFF\n"
                                "mov A, 4\n"
                                "shl B, A",         cpu_B() == 0xfff0 && cpu_OV() == 0xf);
ASSERT_EXEC(logic_shr,          "mov B, 0b10101100\n"
                                "mov A, 2\n"
                                "shr B, A",         cpu_B() == 0b00101011);
ASSERT_EXEC(logic_not,          "mov A, 0b1100001110101100\n"
                                "not A",            cpu_A() == 0b0011110001010011);

static int logic()
{
    printf("Logic:\n");
    verify(logic_or);
    verify(logic_and);
    verify(logic_xor);
    verify(logic_shl);
    verify(logic_shl_8bit);
    verify(logic_shl_overflow);
    verify(logic_shl_overflow);
    verify(logic_shr);
    verify(logic_not);
    printf("\n");
    return 0;
}

// }}}

// {{{ arithmetic

ASSERT_EXEC(add_basic,    "mov A, 12\n"
                          "mov B, 13\n"
                          "add A, B",    cpu_A() == 25);
ASSERT_EXEC(add_overflow, "mov A, 0xFFFE\n"
                          "mov B, 0x5\n"
                          "add A, B",    cpu_A() == 3 && cpu_OV() == 1);
ASSERT_EXEC(add_unsigned, "mov A, 0x9000\n"
                          "mov B, 0x6000\n"
                          "add A, B",    cpu_A() == 0xf000);
ASSERT_EXEC(add_signed1,  "mov A, 40\n"
                          "mov B, -30\n"
                          "add A, B",    cpu_A() == 10);
ASSERT_EXEC(add_signed2,  "mov A, -40\n"
                          "mov B, 30\n"
                          "add A, B",    cpu_A() == 0x10000 - 10);
ASSERT_EXEC(sub_unsigned, "mov A, 3\n"
                          "mov B, 5\n"
                          "sub B, A",    cpu_B() == 2);
ASSERT_EXEC(sub_signed,   "mov A, 5\n"
                          "mov B, 3\n"
                          "sub B, A",    cpu_B() == 0x10000 - 2);
ASSERT_EXEC(sub_signed_8b,"mov A, 5\n"
                          "mov [0x10], 3\n"
                          "sub [0x10], A", ram[0x10] = (0x100 - 2));
ASSERT_EXEC(mul,          "mov A, 0x123\n"
                          "mov B, 0xABCD\n"
                          "mul B, A",    cpu_B() == 0x4a07 && cpu_OV() == 0xc3);
ASSERT_EXEC(mul_signed,   "mov A, 3\n"
                          "mov B, -4\n"
                          "mul B, A",    cpu_B() == (0x10000 - 12));
ASSERT_EXEC(idiv,         "mov B, 50\n"
                          "mov A, 6\n"
                          "div B, A",    cpu_B() == 8);
ASSERT_EXEC(idiv_signed,  "mov B, 50\n"
                          "mov A, -6\n"
                          "div$ B, A",   cpu_B() == (0x10000 - 8));
ASSERT_EXEC(mod,          "mov B, 50\n"
                          "mov A, 6\n"
                          "mod B, A",    cpu_B() == 2);

static int arithmetic()
{
    printf("Arithmetic:\n");
    verify(add_basic);
    verify(add_overflow);
    verify(add_unsigned);
    verify(add_signed1);
    verify(add_signed2);
    verify(sub_unsigned);
    verify(sub_signed);
    verify(sub_signed_8b);
    verify(mul);
    verify(mul_signed);
    verify(idiv);
    verify(idiv_signed);
    verify(mod);
    printf("\n");
    return 0;
}

// }}}

// {{{ skips

ASSERT_EXEC(ifne_not_equal, "mov  A, 12\n"
                            "ifne A, 13\n"
                            "mov  B, 1",   cpu_B() == 1);
ASSERT_EXEC(ifne_equal,     "mov  A, 12\n"
                            "ifne A, 12\n"
                            "mov  B, 1",   cpu_B() == 0);
ASSERT_EXEC(ifeq_not_equal, "mov  A, 12\n"
                            "ifeq A, 13\n"
                            "mov  B, 1",   cpu_B() == 0);
ASSERT_EXEC(ifeq_equal,     "mov  A, 12\n"
                            "ifeq A, 12\n"
                            "mov  B, 1",   cpu_B() == 1);
ASSERT_EXEC(ifgt,           "mov  A, 0xF000\n"
                            "ifgt A, 0x5000\n"
                            "mov  B, 1",   cpu_B() == 1);
ASSERT_EXEC(ifgt_signed,    "mov   A, 0xF000\n"
                            "ifgt$ A, 0x5000\n"
                            "mov   B, 1",  cpu_B() == 0);

static int skips()
{
    printf("Skips:\n");
    verify(ifne_not_equal);
    verify(ifne_equal);
    verify(ifeq_not_equal);
    verify(ifeq_equal);
    verify(ifgt);
    verify(ifgt_signed);
    printf("\n");
    return 0;
}

// }}}

// {{{ stack

ASSERT_EXEC(pushb,    "mov   SP, 0xff\n"
                      "pushb 24",        cpu_SP() == 0xfe && ram[0xff] == 24);
ASSERT_EXEC(pushw_8,  "mov   SP, 0xff\n"
                      "pushw 24",        cpu_SP() == 0xfd && ram[0xfe] == 24 && ram[0xff] == 0);
ASSERT_EXEC(pushw_16, "mov   SP, 0xff\n"
                      "pushw 0x1234",    cpu_SP() == 0xfd && ram[0xfe] == 0x34 && ram[0xff] == 0x12);
ASSERT_EXEC(popb,     "mov   SP, 0xff\n"
                      "pushb 24\n"
                      "popb  A",         cpu_SP() == 0xff && cpu_A() == 24);
ASSERT_EXEC(popw,     "mov   SP, 0xff\n"
                      "pushw 0x1234\n"
                      "popw  A",         cpu_SP() == 0xff && cpu_A() == 0x1234);
ASSERT_EXEC(pusha,    "mov   SP, 0xFF\n"
                      "mov   A, 0xF10\n"
                      "mov   B, 0xF20\n"
                      "pusha\n"
                      "mov   A, 0x111\n"
                      "mov   B, 0x222\n"
                      "popa\n",          cpu_SP() == 0xff && cpu_A() == 0xF10 && cpu_B() == 0xF20);

static int stack()
{
    printf("Stack:\n");
    verify(pushb);
    verify(pushw_8);
    verify(pushw_16);
    verify(popb);
    verify(popw);
    verify(pusha);
    printf("\n");
    return 0;
}

// }}}

// {{{ jump

ASSERT_EXEC(jmp_forward, "       jmp .next\n"
                         "       mov B, 2\n"
                         ".next: mov A, 2\n", cpu_A() == 2 && cpu_B() == 0);
ASSERT_EXEC(jmp_ret,     "    mov A, 0xF0\n"
                         "    jsr .next\n"
                         "    mov [A], 2\n"
                         "    add A, 1\n"
                         "    jmp .end\n"
                         ".next:\n"
                         "    mov [A], 1\n"
                         "    add A, 1\n"
                         "    ret\n"
                         ".end:\n"
                         "    mov [A], 3",   ram[0xf0] == 1 && ram[0xf1] == 2 && ram[0xf2] == 3);

static int jumps()
{
    printf("Jumps:\n");
    verify(jmp_forward);
    verify(jmp_ret);
    printf("\n");
    return 0;
}

// }}}

// {{{ interrupts

int interrupt_hit = 0;

static void dev_request(uint16_t x)
{
    interrupt_hit = x;
}

static int dev()
{
    emulator_init(true);
    Output* output = compile_string("dev 0x18, 0x24EF");
    size_t sz = output_binary_size(output);
    const uint8_t* data = output_binary_data(output);
    ram_load(0x0, data, sz);
    cpu_set_hardware_fpointer(0x18, dev_request);
    cpu_step();
    output_free(output);
    emulator_destroy();
    _assert(interrupt_hit == 0x24EF);
    return 0;
}

ASSERT_EXEC(ivec_int, "    ivec 0x18, interrupt\n"
                      "    int  0x18, 0x1234\n"
                      "    mov B, 1\n"
                      "    jmp done\n"
                      "interrupt:\n"
                      "    mov A, XT\n"
                      "done:", cpu_A() == 0x1234 && cpu_B() == 0);
ASSERT_EXEC(ienab,    "    ivec 0x18, .interrupt\n"
                      "    ienab 0\n"
                      "    int  0x18, 0x0\n"
                      "    mov B, 1\n"
                      "    jmp .done\n"
                      ".interrupt:\n"
                      "    mov A, 1\n"
                      "    iret\n"
                      ".done:", cpu_A() == 0 && cpu_B() == 1);

static int wait()
{
    emulator_init(true);
    Output* output = compile_string("ivec 0x18, .interrupt\n"
                                "    wait\n"
                                ".interrupt:\n"
                                "    mov A, 1");

    size_t sz = output_binary_size(output);
    const uint8_t* data = output_binary_data(output);
    ram_load(0x0, data, sz);
    cpu_step(); 
    cpu_step(); 
    _assert(cpu_PC() == 6);
    for (int i = 0; i < 10; ++i)
        cpu_step();
    _assert(cpu_PC() == 6);  // PC did not move
    cpu_interrupt(0x18, 0x0);
    _assert(cpu_A() == 0);
    cpu_step();
    cpu_step();
    _assert(cpu_A() == 1);
    output_free(output);
    emulator_destroy();
    return 0;
}

static int interrupts()
{
    printf("Interrupts:\n");
    verify(dev);
    verify(ivec_int);
    verify(ienab);
    verify(wait);
    printf("\n");
    return 0;
}

// }}}

// {{{ external devices

ASSERT_EXEC(_memcpy, "mov  [0x30], 4\n"
                     "mov  [0x31], 3\n"
                     "mov  [0x32], 2\n"
                     "mov  [0x33], 1\n"
                     "mov  X, 0x30\n"
                     "mov  F, 0x50\n"
                     "mov  Y, 3\n"
                     "dev  DEV_MEM_MGR, MEM_CPY",
                     ram[0x50] == 4 && ram[0x51] == 3 && ram[0x52] == 2 && ram[0x53] == 0);

ASSERT_EXEC(_memset, "mov  X, 0x10\n"
                     "mov  Y, 3\n"
                     "mov  F, 'x'\n"
                     "dev  DEV_MEM_MGR, MEM_SET",
                     ram[0x10] == 'x' && ram[0x11] == 'x' && ram[0x12] == 'x' && ram[0x14] != 'x');

ASSERT_EXEC(_random, "mov A, [CPU_RANDOM]\n"
                     "mov B, [CPU_RANDOM]\n"
                     "mov C, [CPU_RANDOM]", cpu_A() != cpu_B() && cpu_B() != cpu_C());

static int external()
{
    printf("External:\n");
    verify(_memcpy);
    verify(_memset);
    verify(_random);
    printf("\n");
    return 0;
}

// }}}

// {{{ emulator debugging information

static int
emulator_json()
{
    emulator_init(true);
    Output* output = compile_string("mov A, 0x12");
    ram_load(0x0, output_binary_data(output), output_binary_size(output));
    output_free(output);

    cpu_step();

    static char dbg[10000];
    emulator_dbg_json(0, dbg, sizeof(dbg));
    printf("--> %s\n", dbg);

    emulator_destroy();
    return 0;
}

static int
emulator_debug()
{
    printf("Emulator debugging information:\n");
    verify(emulator_json);
    printf("\n");
    return 0;
}

// }}}

// {{{ breakpoints

static int bkp()
{
    emulator_init(true);
    Output* output = compile_string("nop\nnop\nnop");
    size_t sz = output_binary_size(output);
    const uint8_t* data = output_binary_data(output);
    ram_load(0x0, data, sz);
    cpu_load_debugging_info(output_debugging_info(output));

    _assert(bkps_swap("main.s", 2) == 1);
    _assert(bkps_is_addr(1));
    _assert(!bkps_is_addr(0));

    _assert(bkps_swap("main.s", 2) == -1);
    _assert(!bkps_is_addr(1));

    output_free(output);
    emulator_destroy();
    return 0;
}

static int breakpoints()
{
    printf("Breakpoints:\n");
    verify(bkp);
    printf("\n");
    return 0;
}

// }}}

// {{{ compiler execution

static int exec_dir() {
    _assert(exec_compile_dir_to_ram("../test-dir") == 0);
    _assert(ram_get(0) == 0x2);
    _assert(ram_get(1) == 0x90);
    _assert(ram_get(2) == 0x8a);
    _assert(ram_get(3) == 0x40);
    return 0;
}

static int execution()
{
    printf("Execution:\n");
    verify(exec_dir);
    printf("\n");
    return 0;
}

// }}}

// 
// MAIN
//

int main()
{
    int compiler = preprocessing()
                 + basic_compilation()
                 + parameters()
                 + expressions()
                 + operations()
                 + data()
                 + dollar()
                 + defines()
                 + labels()
                 + org()
                 + debugging()
                 + on_the_wild();
    int emulator = special_ops()
                 + mov_origin()
                 + mov_dest()
                 + logic()
                 + arithmetic()
                 + skips()
                 + stack()
                 + jumps()
                 + interrupts()
                 + external()
                 + emulator_debug()
                 + breakpoints()
                 + execution();
    int result = compiler + emulator;
    if (result == 0)
        printf("All tests passed " BGRN ":)" RST "\n");
    else
        printf("Some tests failed " BRED ":(" RST "\n");
    printf("Tests run: %d\n", tests_run);

    return result != 0;
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
