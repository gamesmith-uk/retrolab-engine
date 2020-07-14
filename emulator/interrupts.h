#ifndef INTERRUPTS_H_
#define INTERRUPTS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct QueuedInterrupt {
    uint8_t  interrupt;
    uint16_t xt_value;
} QueuedInterrupt;

typedef struct Interrupts {
    uint8_t         vector[256];
    QueuedInterrupt queue[256];
    uint8_t         queue_idx;
    bool            active;
    bool            happening;
    bool            waiting;
    uint16_t        ret_addr;
} Interrupts;

#define NO_INTERRUPT        0xff

#endif

// vim:st=4:sts=4:sw=4:expandtab
