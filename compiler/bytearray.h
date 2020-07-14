#ifndef BYTE_ARRAY_H_
#define BYTE_ARRAY_H_

#include <stdint.h>

typedef struct ByteArray {
    uint8_t sz;
    uint8_t bytes[16];
} ByteArray;

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
