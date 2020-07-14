#include "parameter.h"

#include <stdio.h>

#include "parser.h"

ByteArray
immediate_number(CompilationContext* cc, long number, bool force_v16)
{
    if (number >= 0 && number < 0x3f && !force_v16) {
        return (ByteArray) { .sz = 1, .bytes = { number } };
    } else if (number < 0 && number >= -64 && !force_v16) {
        return (ByteArray) { .sz = 1, .bytes = { ((uint8_t) number) & 0b01111111 } };
    } else if (number < 0x100 && number >= -0x80 && !force_v16) {
        return (ByteArray) { .sz = 2, .bytes = { 0x8a, (uint8_t) number } };
    } else if (number < 0x10000 && number >= -0x8000) {
        uint16_t value = (uint16_t) number;
        return (ByteArray) { .sz = 3, .bytes = { 0x8b, value & 0xff, value >> 8 } };
    }
    yyerror(cc, "Value too high (range: -0x8000 ~ 0xFFFF, received: 0x%X)", number);
    return (ByteArray) { .sz = 0 };
}

ByteArray
next_number(CompilationContext* cc, uint8_t prefix8, uint8_t prefix16, long number, bool force_v16)
{
    if (number < 0x100 && number >= -0x80 && !force_v16) {
        return (ByteArray) { .sz = 2, .bytes = { prefix8, (uint8_t) number } };
    } else if (number < 0x10000 && number >= -0x8000) {
        uint16_t value = (uint16_t) number;
        return (ByteArray) { .sz = 3, .bytes = { prefix16, value & 0xff, value >> 8 } };
    }
    yyerror(cc, "Value too high (range: -0x8000 ~ 0xFFFF, received: 0x%X)", number);
    return (ByteArray) { .sz = 0 };
}

ByteArray
next_number_sign(CompilationContext* cc, uint8_t prefix8, uint8_t prefix16, long number, bool force_v16)
{
    if (number < 0x80 && number >= -0x80 && !force_v16) {
        return (ByteArray) { .sz = 2, .bytes = { prefix8, (uint8_t) number } };
    } else if (number < 0x8000 && number >= -0x8000) {
        uint16_t value = (uint16_t) number;
        return (ByteArray) { .sz = 3, .bytes = { prefix16, value & 0xff, value >> 8 } };
    }
    yyerror(cc, "Value too high (range: -0x8000 ~ 0x7FFF, received: 0x%X)", number);
    return (ByteArray) { .sz = 0 };
}

long
check_limit(CompilationContext* cc, long value, long min, long max)
{
    static char buf[1024];
    if (value < min || value > max) {
        snprintf(buf, sizeof(buf), "Value too high (0x%lX ~ 0x%lX)", min, max);
        yyerror(cc, buf);
        return 0;
    } else {
        return value;
    }
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
