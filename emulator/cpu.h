#ifndef CPU_H_
#define CPU_H_

#include <stdbool.h>
#include <stdint.h>

#include "interrupts.h"
#include "../compiler/debug.h"

typedef uint16_t reg_t;

#define DEBUGGER_REQUESTED -0x100

typedef enum {
    CPU_ERROR_NO_ERROR        = 0,
    CPU_ERROR_INVALID_OPCODE  = 1,
} CpuError;

void        cpu_init();
void        cpu_destroy();

int         cpu_step();
void        cpu_interrupt(uint8_t number, uint16_t xt_value);
void        cpu_set_hardware_fpointer(uint8_t hw, void(*fptr)(uint16_t data));
bool        cpu_waiting_for_interrupt();

const char* cpu_register_name(uint8_t idx);
reg_t       cpu_register(uint8_t idx);

void        cpu_set_debugging_mode(bool v);
void        cpu_load_debugging_info(const DebuggingInfo* dbg);
int         cpu_dbg_json(char* buf, size_t bufsz);
CpuError    cpu_error();

long        cpu_addr_from_source(const char* filename, size_t line);

void        cpu_break_next();

#define cpu_A()  cpu_register(0x0)
#define cpu_B()  cpu_register(0x1)
#define cpu_C()  cpu_register(0x2)
#define cpu_D()  cpu_register(0x3)
#define cpu_E()  cpu_register(0x4)
#define cpu_F()  cpu_register(0x5)
#define cpu_I()  cpu_register(0x6)
#define cpu_J()  cpu_register(0x7)
#define cpu_K()  cpu_register(0x8)
#define cpu_X()  cpu_register(0x9)
#define cpu_Y()  cpu_register(0xa)
#define cpu_XT() cpu_register(0xb)
#define cpu_SP() cpu_register(0xc)
#define cpu_FP() cpu_register(0xd)
#define cpu_PC() cpu_register(0xe)
#define cpu_OV() cpu_register(0xf)

#endif

// vim:st=4:sts=4:sw=4:expandtab
