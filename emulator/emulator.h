#ifndef EMULATOR_H_
#define EMULATOR_H_

#include <stdbool.h>
#include <stddef.h>

#include "cpu.h"

typedef void(*BreakpointListener)();

void emulator_init(bool reset_memory);
CpuError emulator_step();
CpuError emulator_frame();
void emulator_destroy();

void emulator_load_rom(const char* file);
void emulator_hard_reset();
void emulator_soft_reset();

void emulator_suspend_execution();
void emulator_set_break_at_eof();
void emulator_bkp_hit_set_fptr(BreakpointListener bkp_fptr);

int  emulator_dbg_json(size_t memory_block, char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab
