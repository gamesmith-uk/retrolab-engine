#include "memory.h"

#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "../constants/mmap.h"
#include "cpu.h"
#include "video.h"

#define MEMSZ  0x10000  // 64 kB

uint8_t ram[MEMSZ];

typedef struct LastUpdated {
    long addr;
    long addr2;
} LastUpdated;
#define NO_ADDRESS -1
LastUpdated last_updated = { NO_ADDRESS, NO_ADDRESS };

void
ram_init()
{
    memset(ram, 0, MEMSZ);
    last_updated = (LastUpdated) { NO_ADDRESS, NO_ADDRESS };
}

uint8_t
ram_get(uint16_t addr)
{
    return ram[addr];
}

uint16_t
ram_get16(uint16_t addr)
{
    return ram[addr] | (ram[addr+1] << 8);
}

void
ram_set(uint16_t addr, uint8_t data)
{
    ram[addr] = data;
    last_updated.addr = addr;
    last_updated.addr2 = NO_ADDRESS;
    /*
#if !HEADLESS
    if (addr >= MMAP_VIDEO_START)
        video_set(addr, data);
#endif
    */
}

void
ram_set_bypass(uint16_t addr, uint8_t data)
{
    ram[addr] = data;
}

void
ram_set16(uint16_t addr, uint16_t data)
{
    ram[addr] = (data & 0xff);
    ram[addr+1] = (data >> 8);
    last_updated.addr = addr;
    last_updated.addr2 = addr + 1;
}

void
ram_load(uint16_t start, const uint8_t* data, size_t sz)
{
    if (start + sz > 0xFFFF)
        abort();
    memcpy(&ram[start], data, sz);
}

int
ram_dbg_json(size_t memory_block, char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    PRINT("\"memory\":{\"data\":[");
    for (size_t i = 0; i < 0x100; ++i)
        PRINT("%d%s", ram[(memory_block * 0x100) + i], (i != 0xff) ? "," : "");
    PRINT("],");
    PRINT("\"lastUpdated\":[");
    if (last_updated.addr != NO_ADDRESS)
        PRINT("%ld", last_updated.addr);
    if (last_updated.addr != NO_ADDRESS && last_updated.addr2 != NO_ADDRESS)
        PRINT(",");
    if (last_updated.addr2 != NO_ADDRESS)
        PRINT("%ld", last_updated.addr2);
    PRINT("],");
    PRINT("\"stack\":[");
    for (size_t i = 0; i < 32; ++i)
        PRINT("%d%s", ram[cpu_SP() + i + 1], (i != 31) ? "," : "");
    PRINT("]}");
#undef PRINT
    return n;
}

// vim:st=4:sts=4:sw=4:expandtab
