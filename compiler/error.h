#ifndef ERROR_H_
#define ERROR_H_

#include <stdlib.h>

typedef struct Error {
    char*  message;
    char*  filename;
    size_t line;
} Error;

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
