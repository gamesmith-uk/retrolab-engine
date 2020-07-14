#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdint.h>
#include <stdlib.h>

#if __EMSCRIPTEN__
#  include <emscripten/emscripten.h>
#else
#  define EMSCRIPTEN_KEEPALIVE
#endif

#define RAM_SIZE 0x10000
typedef uint16_t ram_type_t;

#endif

// vim:st=4:sts=4:sw=4:expandtab
