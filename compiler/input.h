#ifndef INPUT_H_
#define INPUT_H_

#include <stdlib.h>

typedef struct Input Input;

Input*      input_new();
Input*      input_new_from_string(const char* text);
void        input_free(Input* sf);

size_t      input_add_file(Input* input, const char* filename, const char* source);

size_t      input_file_count(Input* input);
const char* input_filename(Input* input, size_t idx);
const char* input_source(Input* input, size_t idx);
char*       input_precompile(Input* input);

#endif

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
