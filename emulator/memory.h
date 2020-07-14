#ifndef MEMORY_H_
#define MEMORY_H_

#include <stdint.h>
#include <stdlib.h>

extern uint8_t ram[];

void     ram_init();

uint8_t  ram_get(uint16_t addr);
uint16_t ram_get16(uint16_t addr);
void     ram_set(uint16_t addr, uint8_t data);
void     ram_set_bypass(uint16_t addr, uint8_t data);
void     ram_set16(uint16_t addr, uint16_t data);

void     ram_load(uint16_t start, const uint8_t* data, size_t sz);

int      ram_dbg_json(size_t memory_block, char* buf, size_t bufsz);

#endif

// vim:st=4:sts=4:sw=4:expandtab
