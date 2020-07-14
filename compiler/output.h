#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <stdlib.h>

#include "bytearray.h"
#include "compctx.h"
#include "debug.h"
#include "error.h"

typedef struct Output Output;

Output*        output_new(uint8_t* binary, size_t binary_sz, DebuggingInfo* debugging_info, Error error);
void           output_free(Output* output);

char*          output_error_message(const Output* output);
const uint8_t* output_binary_data(const Output* output);
size_t         output_binary_size(const Output* output);
const DebuggingInfo* output_debugging_info(const Output* output);

int            output_to_json(const Output* output, char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
