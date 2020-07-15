#ifndef RETROLAB_EXEC_H
#define RETROLAB_EXEC_H

int exec_compile_to_stdout(const char *filename);
int exec_compile_file_to_ram(const char* filename);
int exec_compile_dir_to_ram(const char* filename);

#endif //RETROLAB_EXEC_H
