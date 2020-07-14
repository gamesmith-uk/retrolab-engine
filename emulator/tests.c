#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "cpu.h"
#include "memory.h"

static uint8_t hw_called = 0;

static void call_hw(uint8_t value)
{
    hw_called = value;
}

int main(int argc, char* argv[])
{
    if (argc <= 1) {
        fprintf(stderr, "Usage: %s 00 A0 FF ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ram_init();
    cpu_init();
    cpu_set_hardware_fpointer(0x18, call_hw);  // this is used on the "dev" test

    for (int i = 1; i < argc; ++i) {
        uint8_t byte = (uint8_t) strtol(argv[i], NULL, 16);
        ram_set((uint16_t) (i - 1), byte);
    }

    while (cpu_register(0xe) < argc - 1) {
        if (cpu_waiting_for_interrupt())  // this is used on the "wait" test
            cpu_interrupt(0x18);
        int n = cpu_step();
        if (n == DEBUGGER_REQUESTED) {
            printf("{ \"action\": \"debuggerRequested\" }\n");
            return 0;
        } else if (n < 0) {
            printf("{ \"error\": \"Invalid opcode 0x%02X in PC 0x%02X\" }", -n, cpu_register(0xe));
            return EXIT_FAILURE;
        }
    }
    
    printf("{ \"pc\": %d, \"hwCalled\": %d, \"ram\": [", cpu_register(0xe), hw_called);
    for (uint16_t i = 0; i < 0x200; ++i)
        printf("%d%s", ram[i], (i == 0x1FF ? "" : ","));
    printf("], \"reg\": [");
    for (size_t i = 0; i < 16; ++i)
        printf("%d%s", cpu_register(i), (i == 15 ? "" : ","));
    printf("] }\n");
    return 0;
}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
