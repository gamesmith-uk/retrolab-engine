#include "cpu.h"

#include "breakpoints.h"
#include "memory.h"
#include "mmap.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !__EMSCRIPTEN__
#  define SUPPORT_DEBUG 1
#endif

static reg_t reg[16] = { 0 };
static bool skip_next = false;
static bool debugging_mode = false;
static CpuError _cpu_error = CPU_ERROR_NO_ERROR;

static Interrupts ints = {
    .vector    = { NO_INTERRUPT },
    .queue     = {},
    .queue_idx = 0,
    .active    = true,
    .happening = false,
    .waiting   = false,
    .ret_addr  = 0,
};

static void(*hw_fptr[256])(uint16_t data) = { NULL };

static DebuggingInfo* dbg = NULL;
static bool           break_next = false;

#define A  (reg[0x0])
#define B  (reg[0x1])
#define C  (reg[0x2])
#define D  (reg[0x3])
#define E  (reg[0x4])
#define F  (reg[0x5])
#define I  (reg[0x6])
#define J  (reg[0x7])
#define K  (reg[0x8])
#define X  (reg[0x9])
#define Y  (reg[0xa])
#define XT (reg[0xb])
#define SP (reg[0xc])
#define FP (reg[0xd])
#define PC (reg[0xe])
#define OV (reg[0xf])

// {{{ memory manager

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

static void memory_manager(uint16_t command)
{
    switch (command) {
        case MEM_CPY:
            memmove(&ram[F], &ram[X], min(Y, 0xFFFF - Y + 1));
            break;
        case MEM_SET:
            memset(&ram[X], F & 0xff, min(Y, 0xfff - Y + 1));
            break;
        default:
            break;
    }
}

// }}}

// {{{ initialization

void
cpu_init()
{
    _cpu_error = CPU_ERROR_NO_ERROR;
    memset(reg, 0, sizeof(reg));  // reset registers
    cpu_set_hardware_fpointer(DEV_MEM_MGR, memory_manager);
    skip_next = false;
    memset(&ints, 0, sizeof(Interrupts));
    for (size_t i = 0; i < 256; ++i)
        ints.vector[i] = NO_INTERRUPT;
    ints.active = true;

    ram[CPU_VERSION_MAJOR] = 1;
    ram[CPU_VERSION_MINOR] = 1;

    SP = STACK_BOTTOM;
    PC = 0;
}

void
cpu_destroy()
{
    debug_free(dbg);
    dbg = NULL;
}

// }}}

// {{{ information

reg_t
cpu_register(uint8_t idx)
{
    if (idx > sizeof(reg) / sizeof(reg_t)) {
#if !TESTING
        fprintf(stderr, "Trying to access invalid register %d.\n", idx);
        abort();
#endif
    }
    return reg[idx];
}

const char*
cpu_register_name(uint8_t idx)
{
    static const char* regs[] = { 
        "A", "B", "C", "D", "E", "F", "I", "J", "K", "X", "Y", 
        "XT", "SP", "FP", "PC", "OV"
    };
    if (idx <= 0xf)
        return regs[idx];
    else
        return "??";
}

// }}}

// {{{ parameter parsing

typedef enum {
    NO_PARAMETER, DIRECT, INDIRECT, INDIRECT_WORD, REGISTER,
} ParameterType;

typedef struct Parameter {
    uint16_t      dest;
    uint16_t      value;
    ParameterType type;
    int16_t       sum;
#ifdef SUPPORT_DEBUG
    char          debug[30];
#endif
} Parameter;

inline static reg_t
fetch_par(reg_t pc, Parameter* p)
{
#ifdef SUPPORT_DEBUG
#  define DEBUG_REG(...) { if (debugging_mode) snprintf(p->debug, sizeof p->debug, __VA_ARGS__); }
#else
#  define DEBUG_REG(...)
#endif
    static const uint8_t NEXT_V8             = 0x8a,
                         NEXT_V16            = 0x8b,
                         ADDR_NEXT_V8        = 0x8c,
                         ADDR_NEXT_V8_WORD   = 0x8d,
                         ADDR_NEXT_V16       = 0x8e,
                         ADDR_NEXT_V16_WORD  = 0x8f,
                         REG                 = 0x90,
                         ADDR_REG            = 0xa0,
                         ADDR_REG_WORD       = 0xb0,
                         ADDR_REG_V8         = 0xc0,
                         ADDR_REG_V8_WORD    = 0xd0,
                         ADDR_REG_V16        = 0xe0,
                         ADDR_REG_V16_WORD   = 0xf0;

    static const uint8_t LITERAL_VALUE_NEG_MAX = 0x7f,
                         LITERAL_VALUE_POS_MAX = 0x3f,
                         LITERAL_ABS_MASK      = 0b00111111,
                         NREGS                 = 16;

    p->sum = 0;
    uint8_t b8 = ram[pc];
    if (b8 <= LITERAL_VALUE_POS_MAX) {
        p->type = DIRECT;
        p->value = b8;
        DEBUG_REG("0x%02X", p->value)
        return pc + 1;
    } else if (b8 <= LITERAL_VALUE_NEG_MAX) {
        p->type = DIRECT;
        p->value = (b8 & LITERAL_ABS_MASK) | 0xFF00 | ~LITERAL_ABS_MASK;  // only consider then first 6 bits, negate the rest
        DEBUG_REG("0x%02X", p->value)
        return pc + 1;
    } else if (b8 == NEXT_V8) {
        p->type = DIRECT;
        p->value = ram[pc+1];
        DEBUG_REG("0x%02X", p->value)
        return pc + 2;
    } else if (b8 == NEXT_V16) {
        p->type = DIRECT;
        p->value = ram[pc+1] | (ram[pc+2] << 8);
        DEBUG_REG("0x%04X", p->value)
        return pc + 3;
    } else if (b8 == ADDR_NEXT_V8) {
        p->type = INDIRECT;
        p->dest = ram[pc+1];
        p->value = ram[p->dest];
        DEBUG_REG("[0x%02X]", p->dest)
        return pc + 2;
    } else if (b8 == ADDR_NEXT_V8_WORD) {
        p->type = INDIRECT_WORD;
        p->dest = ram[pc+1];
        p->value = ram[p->dest] | (ram[p->dest+1] << 8);
        DEBUG_REG("^[0x%02X]", p->dest)
        return pc + 2;
    } else if (b8 == ADDR_NEXT_V16) {
        p->type = INDIRECT;
        p->dest = ram[pc+1] | (ram[pc+2] << 8);
        p->value = ram[p->dest];
        DEBUG_REG("[0x%04X]", p->dest)
        return pc + 3;
    } else if (b8 == ADDR_NEXT_V16_WORD) {
        p->type = INDIRECT_WORD;
        p->dest = ram[pc+1] | (ram[pc+2] << 8);
        p->value = ram[p->dest] | (ram[p->dest+1] << 8);
        DEBUG_REG("^[0x%04X]", p->dest)
        return pc + 3;
    } else if (b8 >= REG && b8 < (REG + NREGS)) {
        p->type = REGISTER;
        p->dest = b8 - REG;
        p->value = reg[b8 - REG];
        DEBUG_REG("%s", cpu_register_name(p->dest))
        return pc + 1;
    } else if (b8 >= ADDR_REG && b8 < (ADDR_REG + NREGS)) {
        p->type = INDIRECT;
        p->dest = reg[b8 - ADDR_REG];
        p->value = ram[p->dest];
        DEBUG_REG("[%s]", cpu_register_name(b8 - ADDR_REG))
        return pc + 1;
    } else if (b8 >= ADDR_REG_WORD && b8 < (ADDR_REG_WORD + NREGS)) {
        p->type = INDIRECT_WORD;
        p->dest = reg[b8 - ADDR_REG_WORD];
        p->value = ram[p->dest] | (ram[p->dest+1] << 8);
        DEBUG_REG("[%s]", cpu_register_name(b8 - ADDR_REG_WORD))
        return pc + 1;
    } else if (b8 >= ADDR_REG_V8 && b8 < (ADDR_REG_V8 + NREGS)) {
        p->type = INDIRECT;
        p->sum = ram[pc + 1];
        p->dest = reg[b8 - ADDR_REG_V8] + p->sum;
        p->value = ram[p->dest];
        DEBUG_REG("[%s + 0x%02X]", cpu_register_name(b8 - ADDR_REG_V8), (uint8_t) p->sum)
        return pc + 2;
    } else if (b8 >= ADDR_REG_V8_WORD && b8 < (ADDR_REG_V8_WORD + NREGS)) {
        p->type = INDIRECT_WORD;
        p->sum = ram[pc + 1];
        p->dest = reg[b8 - ADDR_REG_V8_WORD] + p->sum;
        p->value = ram[p->dest] | (ram[p->dest+1] << 8);
        DEBUG_REG("^[%s + 0x%02X]", cpu_register_name(b8 - ADDR_REG_V8_WORD), (uint8_t) p->sum)
        return pc + 2;
    } else if (b8 >= ADDR_REG_V16 && b8 < (ADDR_REG_V16 + NREGS)) {
        p->type = INDIRECT;
        p->sum = ram[pc + 1] | (ram[pc + 2] << 8);
        p->dest = reg[b8 - ADDR_REG_V16] + p->sum;
        p->value = ram[p->dest];
        DEBUG_REG("[%s + 0x%04X]", cpu_register_name(b8 - ADDR_REG_V16), (uint16_t) p->sum)
        return pc + 3;
    } else if (b8 >= ADDR_REG_V16_WORD && b8 < (ADDR_REG_V16_WORD + NREGS)) {
        p->type = INDIRECT_WORD;
        p->sum = ram[pc + 1] | (ram[pc + 2] << 8);
        p->dest = reg[b8 - ADDR_REG_V16_WORD] + p->sum;
        p->value = ram[p->dest] | (ram[p->dest+1] << 8);
        DEBUG_REG("^[%s + 0x%04X]", cpu_register_name(b8 - ADDR_REG_V16_WORD), (uint16_t) p->sum)
        return pc + 3;
    } else {
        abort();
    }
}

inline static void
set_par(const Parameter* dest, uint16_t value)
{
    switch (dest->type) {
        case DIRECT:
            break;  // default, ignore
        case INDIRECT:
            ram_set(dest->dest, value);
            break;
        case INDIRECT_WORD:
            ram_set16(dest->dest, value);
            break;
        case REGISTER:
            reg[dest->dest] = value;
            break;
        default:
            abort();
    }
}


static void
set_par_with_overflow(const Parameter* dest, uint32_t value)
{
    switch (dest->type) {
        case DIRECT:
            break;  // default, ignore
        case INDIRECT:
            ram_set(dest->dest, (uint8_t) value);
            break;
        case INDIRECT_WORD:
            ram_set16(dest->dest, value);
            break;
        case REGISTER:
            reg[dest->dest] = (uint16_t) value;
            break;
        default:
            abort();
    }
    OV = (value >> 16) & 0xffff;
}

// }}}

// {{{ interrupts

void
cpu_interrupt(uint8_t number, uint16_t xt_value)
{
    if (ints.active && ints.vector[number] != NO_INTERRUPT && ints.queue_idx < 0xFF) {
        ints.queue[ints.queue_idx] = (QueuedInterrupt) { .interrupt = number, .xt_value = xt_value };
        ++ints.queue_idx;
        ints.waiting = false;
    }
}

__attribute__((unused)) bool
cpu_waiting_for_interrupt()
{
    return ints.waiting;
}

static QueuedInterrupt
check_for_interrupt()
{
    if (ints.active && !ints.happening && ints.queue_idx > 0) {
        QueuedInterrupt interrupt = ints.queue[0];   // get interrupt
        memmove(&ints.queue[0], &ints.queue[1], 0xfe * sizeof(ints.queue[0]));  // remove from queue
        --ints.queue_idx;
        return interrupt;
    }
    return (QueuedInterrupt) { .interrupt = NO_INTERRUPT };
}

static uint16_t
enter_interrupt(uint8_t number)
{
    ints.happening = true; // set as happening
    ints.ret_addr = PC; // save return address
    PC = ints.vector[number];
    return PC;
}

static uint16_t
leave_interrupt()
{
    ints.happening = false;
    PC = ints.ret_addr;
    return PC;
}

// }}}

// {{{ external hardware

void
cpu_set_hardware_fpointer(uint8_t hw, void(*fptr)(__attribute__((unused)) uint16_t data))
{
    hw_fptr[hw] = fptr;
}

// }}}

// {{{ instruction execution

static int
cpu_execute_instruction(uint8_t op, const Parameter* par1, const Parameter* par2, __attribute__((unused)) char** op_name)
{
#ifdef SUPPORT_DEBUG
#define SET_OP_NAME(op_str) {*op_name=(op_str);}
#else
#define SET_OP_NAME(op_str)
#endif

    // execute instruction
    switch (op) {
    case 0x0:  // NOP
        SET_OP_NAME("NOP")
        break;
    case 0x1:  // DBG
        SET_OP_NAME("DBG")
        return DEBUGGER_REQUESTED;
    case 0x2:  // MOV
        SET_OP_NAME("MOV")
        set_par(par1, par2->value);
        break;

    case 0x10:  // OR
        SET_OP_NAME("OR")
        set_par(par1, par1->value | par2->value);
        break;
    case 0x11:  // AND
        SET_OP_NAME("AND")
        set_par(par1, par1->value & par2->value);
        break;
    case 0x12:  // XOR
        SET_OP_NAME("XOR")
        set_par(par1, par1->value ^ par2->value);
        break;
    case 0x13:  // SHL
        SET_OP_NAME("SHL")
        set_par_with_overflow(par1, ((uint32_t) par1->value) << par2->value);
        break;
    case 0x14:  // SHR
        SET_OP_NAME("SHR")
        set_par(par1, par1->value >> par2->value);
        break;
    case 0x15:  // NOT
        SET_OP_NAME("NOT")
        set_par(par1, ~par1->value);
        break;

    case 0x20:  // ADD
        SET_OP_NAME("ADD")
        set_par_with_overflow(par1, par1->value + par2->value);
        break;
    case 0x22:  // SUB
        SET_OP_NAME("SUB")
        set_par_with_overflow(par1, par1->value - par2->value);
        break;
    case 0x24:  // MUL
        SET_OP_NAME("MUL")
        set_par_with_overflow(par1, par1->value * par2->value);
        break;
    case 0x26:  // DIV
        SET_OP_NAME("DIV")
        if (par2->value == 0)
            cpu_interrupt(INT_CPU, XT_CPU_DIVZERO);
        else
            set_par(par1, par1->value / par2->value);
        break;
    case 0x27:  // DIV$
        SET_OP_NAME("DIV$")
        if (par2->value == 0)
            cpu_interrupt(INT_CPU, XT_CPU_DIVZERO);
        else
            set_par(par1, par1->value / (int16_t)par2->value);
        break;
    case 0x29:  // MOD
        SET_OP_NAME("MOD")
        if (par2->value == 0)
            cpu_interrupt(INT_CPU, XT_CPU_DIVZERO);
        else
            set_par(par1, par1->value % par2->value);
        break;
    case 0x2a:  // INC
        SET_OP_NAME("INC")
        set_par_with_overflow(par1, par1->value + 1);
        break;
    case 0x2b:  // DEC
        SET_OP_NAME("DEC")
            set_par_with_overflow(par1, par1->value - 1);
            break;

    case 0x30: // IFNE
        SET_OP_NAME("IFNE")
        if (par1->value == par2->value)
            skip_next = true;
        break;
    case 0x31: // IFEQ
        SET_OP_NAME("IFEQ")
        if (par1->value != par2->value)
            skip_next = true;
        break;
    case 0x32: // IFGT
        SET_OP_NAME("IFGT")
        if (par1->value <= par2->value)
            skip_next = true;
        break;
    case 0x33: // IFGT$
        SET_OP_NAME("IFGT$")
        if ((int16_t) par1->value <= (int16_t) par2->value)
            skip_next = true;
        break;
    case 0x35: // IFLT
        SET_OP_NAME("IFLT")
        if (par1->value >= par2->value)
            skip_next = true;
        break;
    case 0x36: // IFLT$
        SET_OP_NAME("IFLT$")
        if ((int16_t) par1->value >= (int16_t) par2->value)
            skip_next = true;
        break;
    case 0x38: // IFGE
        SET_OP_NAME("IFGE")
        if (par1->value < par2->value)
            skip_next = true;
        break;
    case 0x39: // IFGE$
        SET_OP_NAME("IFGE$")
        if ((int16_t) par1->value < (int16_t) par2->value)
            skip_next = true;
        break;
    case 0x3C: // IFLE
        SET_OP_NAME("IFLE")
        if (par1->value > par2->value)
            skip_next = true;
        break;
    case 0x3D: // IFLE$
        SET_OP_NAME("IFLE$")
        if ((int16_t) par1->value > (int16_t) par2->value)
            skip_next = true;
        break;

#define PUSH16(n) { ram_set_bypass(SP--, ((n) >> 8) & 0xff); ram_set_bypass(SP--, (n) & 0xff); }
#define POP16() (SP += 2, ram[SP-1] | (ram[SP] << 8))
    case 0x50: // PUSHB
        SET_OP_NAME("PUSHB")
        ram_set_bypass(SP--, par1->value);
        break;
    case 0x51: // PUSHW
        SET_OP_NAME("PUSHW")
        PUSH16(par1->value)
        break;
    case 0x52: // POPB
        SET_OP_NAME("POPB")
        set_par(par1, ram[++SP]);
        break;
    case 0x53: // POPW
        SET_OP_NAME("POP16")
        set_par(par1, POP16());
        break;
    case 0x54: // PUSHA
        SET_OP_NAME("PUSHA")
        PUSH16(A)
        PUSH16(B)
        PUSH16(C)
        PUSH16(D)
        PUSH16(E)
        PUSH16(F)
        PUSH16(I)
        PUSH16(J)
        PUSH16(K)
        PUSH16(X)
        PUSH16(Y)
        PUSH16(FP)
        PUSH16(OV)
        break;
    case 0x55: // POPA
        SET_OP_NAME("POPA")
        OV = POP16();
        FP = POP16();
        Y = POP16();
        X = POP16();
        K = POP16();
        J = POP16();
        I = POP16();
        F = POP16();
        E = POP16();
        D = POP16();
        C = POP16();
        B = POP16();
        A = POP16();
        break;
    case 0x56: // POPN
        SET_OP_NAME("POPN")
        SP += par1->value;
        break;
#undef PUSH16
#undef POP16

    case 0x60: // JMP
        SET_OP_NAME("JMP")
        PC = par1->value;
        break;
    case 0x61: // JSR
        SET_OP_NAME("JSR")
        ram_set_bypass(SP--, PC >> 8);  // push next instruction PC onto stack
        ram_set_bypass(SP--, PC & 0xff);
        PC = par1->value;
        break;
    case 0x62: // RET
        SET_OP_NAME("RET")
        PC = ram[++SP];
        PC |= ram[++SP] << 8;
        break;

    case 0x70: // DEV
        SET_OP_NAME("DEV")
        if (hw_fptr[par1->value & 0xff])
            hw_fptr[par1->value & 0xff](par2->value & 0xffff);
        break;
    case 0x71: // IVEC
        SET_OP_NAME("IVEC")
        ints.vector[par1->value & 0xff] = par2->value;
        break;
    case 0x72: // INT
        SET_OP_NAME("INT")
        cpu_interrupt(par1->value & 0xff, par2->value);
        break;
    case 0x73: // IRET
        SET_OP_NAME("IRET")
        if (ints.happening)
            leave_interrupt();
        else
            cpu_interrupt(INT_CPU, XT_CPU_IRET);
        break;
    case 0x74: // WAIT
        SET_OP_NAME("WAIT")
        ints.waiting = true;
        break;
    case 0x75: // IENAB
        SET_OP_NAME("IENAB")
        ints.active = par1->value & 1;
        break;

    default:
#if !TESTING
        fprintf(stderr, "Invalid CPU operation 0x%02X in PC 0x%X.\n", op, PC-1);
#endif
        _cpu_error = CPU_ERROR_INVALID_OPCODE;
        return PC;
    }
    return PC;
#undef SET_OP_NAME
}

// }}}

// {{{ step

static const uint8_t n_parameters[256] = {
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F      
    0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0 - special / mov
    2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1 - logic
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 0, 0, 0, 0,  // 2 - arithmetic
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0,  // 3 - skip
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 4
    1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 5 - stack
    1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 6 - jumps
    2, 2, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 7 - i/o
};

static __attribute__((unused)) void cpu_print_debug(reg_t pc, char* op, Parameter* par1, Parameter* par2);

int
cpu_step()
{
    _cpu_error = CPU_ERROR_NO_ERROR;

    __attribute__((unused)) reg_t original_pc = PC;

    // set random
    int r = rand();   // NOLINT
    ram[CPU_RANDOM] = r & 0xff;
    ram[CPU_RANDOM+1] = (r >> 8) & 0xff;

    // waiting for interrupts?
    if (ints.waiting)
        return PC;

    // check for interrupts
    QueuedInterrupt interrupt = check_for_interrupt();
    if (interrupt.interrupt != NO_INTERRUPT) {
        XT = interrupt.xt_value;
        return enter_interrupt(interrupt.interrupt);
    }

    // read next instruction
    uint8_t op = ram[PC++];

    // deal with special jmp case
    if (op == 0x63) {
        uint16_t next = ram[PC++];
        next |= ram[PC++] << 8;
        if (skip_next)
            skip_next = false;
        else
            PC = next;
        return PC;
    }

    // read parameters
    Parameter par1 = {}, par2 = {};
    if (n_parameters[op] >= 1)
        PC = fetch_par(PC, &par1); // dest
    if (n_parameters[op] >= 2)
        PC = fetch_par(PC, &par2); // origin

    // break next?
    if (break_next) {
        bkps_set_tmp_brk(PC);
        break_next = false;
    }
    
    // skip next?
    if (skip_next) {
        skip_next = false;
        return PC;
    }

    char* op_str = NULL;
    int ret = cpu_execute_instruction(op, &par1, &par2, &op_str);
#if !__EMSCRIPTEN__
    if (debugging_mode)
        cpu_print_debug(original_pc, op_str, &par1, &par2);
#endif
    if (skip_next)
        cpu_step();
    return ret;
}

long
cpu_addr_from_source(const char* filename, size_t line)
{
    return debug_find_pc(dbg, filename, line);
}

// }}}

// {{{ breakpoints

__attribute__((unused)) void
cpu_break_next()
{
    break_next = true;
}

// }}}

// {{{ debugging info

CpuError
cpu_error()
{
    return _cpu_error;
}

void
cpu_set_debugging_mode(bool v)
{
    debugging_mode = v;
}

void
cpu_load_debugging_info(const DebuggingInfo* ndbg)
{
    dbg = debug_copy(ndbg);
}

int
cpu_dbg_json(char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }

    PRINT("\"cpu\":{")
    PRINT("\"version\":[%d,%d],", ram[CPU_VERSION_MAJOR], ram[CPU_VERSION_MINOR])
    PRINT("\"random\":%d,", ram_get16(CPU_RANDOM))
    PRINT("\"pc\":%d,", cpu_PC())

    // errors
    switch (_cpu_error) { // NOLINT(hicpp-multiway-paths-covered)
        case CPU_ERROR_NO_ERROR: break;
        case CPU_ERROR_INVALID_OPCODE:
        PRINT("\"error\":\"Invalid opcode 0x%02X in address 0x%04X.\",", ram[PC-1], PC-1)
            break;
    }

    // current source location
    if (dbg) {
        for (size_t i = 0; i < dbg->locations_sz; ++i) {
            if (cpu_PC() == dbg->locations[i].pc) {
                PRINT("\"currentSourceLocation\":{\"filename\":\"%s\",\"lineNumber\":%zu},",
                        dbg->files[dbg->locations[i].file_number], dbg->locations[i].line)
                goto found;
            }
        }
    }
    PRINT("\"currentSourceLocation\":{\"filename\":\"\",\"lineNumber\":-1},")
found:
    
    // registers
    PRINT("\"registers\":[")
    for (int i = 0; i < 16; ++i)
        PRINT("{\"name\":\"%s\",\"value\":%d}%s", cpu_register_name(i), cpu_register(i), (i != 15) ? "," : "")
    PRINT("],")
    
    // interrupts
    int comma = 0;
    PRINT("\"waitingForInterrupt\":%s,", ints.waiting? "true" : "false")
    PRINT("\"interrupts\":{")
    PRINT("\"vector\":{")
    for (size_t i = 0; i < 256; ++i)
        if (ints.vector[i] != NO_INTERRUPT)
            PRINT("%s\"%zu\":%d", comma++ ? "," : "", i, ints.vector[i])
    PRINT("},")
    PRINT("\"queuedInterrupts\":[")
    for (int i = 0; i < ints.queue_idx; ++i) {
        PRINT("{\"interrupt\":%d,\"xtValue\":%d}%s", 
                ints.queue[i].interrupt, ints.queue[i].xt_value, (i != (ints.queue_idx - 1)) ? "," : "")
    }
    PRINT("],")
    PRINT("\"active\":%s,", ints.active ? "true" : "false")
    PRINT("\"happening\":%s,", ints.happening ? "true" : "false")
    PRINT("\"returnAddress\":%d", ints.ret_addr)
    PRINT("}")

    PRINT("}")
#undef PRINT
    return n;
}

static void
cpu_disassemble_instruction(char* buf, int bufsz, char* op, Parameter *par1, Parameter *par2)
{
#if SUPPORT_DEBUG
    if (par1->type == NO_PARAMETER) {
        snprintf(buf, bufsz, "%s", op);
    } else if (par2->type == NO_PARAMETER) {
        snprintf(buf, bufsz, "%s %s", op, par1->debug);
    } else {
        snprintf(buf, bufsz, "%s %s, %s", op, par1->debug, par2->debug);
    }
#else
    (void) buf; (void) bufsz; (void) op; (void) par1; (void) par2;
#endif
}

void
cpu_print_debug(reg_t pc, char* op, Parameter* par1, Parameter* par2)
{
    char buf[50];
    cpu_disassemble_instruction(buf, 50, op, par1, par2);
    printf("Executed: (PC = 0x%0X): %s\n", pc, buf);
    printf("A: 0x%04X   E: 0x%04X    K: 0x%04X   SP: 0x%04X\n", A, E, K, SP);
    printf("B: 0x%04X   F: 0x%04X    X: 0x%04X   FP: 0x%04X\n", B, F, X, FP);
    printf("C: 0x%04X   I: 0x%04X    Y: 0x%04X   PC: 0x%04X\n", C, I, Y, PC);
    printf("D: 0x%04X   J: 0x%04X   XT: 0x%04X   OV: 0x%04X\n", D, J, XT, OV);
    printf("Stack: %02X %02X %02X %02X %02X %02X %02X %02X\n",
        ram[SP+1], ram[SP+2], ram[SP+3], ram[SP+4], ram[SP+5], ram[SP+6], ram[SP+7], ram[SP+8]);
    printf("--------------------------------------------------------\n");
}

// }}}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
