#ifndef REQUEST_H_ 
#define REQUEST_H_

#include <stdint.h>
#include <stdlib.h>

#include "input.h"
#include "output.h"

Output* compile_input(Input* input);
Output* compile_string(const char* source);
Output* compile_file(const char* filename);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
