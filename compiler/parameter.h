#ifndef PARAMETER_H_
#define PARAMETER_H_

#include <stdbool.h>
#include <stdint.h>

#include "bytearray.h"
#include "compctx.h"

ByteArray immediate_number(CompilationContext* cc, long number, bool force_v16);
ByteArray next_number(CompilationContext* cc, uint8_t prefix8, uint8_t prefix16, long number, bool force_v16);
ByteArray next_number_sign(CompilationContext* cc, uint8_t prefix8, uint8_t prefix16, long number, bool force_v16);
long check_limit(CompilationContext* cc, long value, long min, long max);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
