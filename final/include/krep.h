#ifndef KREP_H
#define KREP_H
#include "types.h"
int kernel_replace(uint32_t target_addr, const void *new_code, uint32_t code_size);
int kernel_replace_file(const char *new_kernel_file_path, uint32_t new_kernel_addr);
#endif