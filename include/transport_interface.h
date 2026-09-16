#pragma once
#include <stdint.h>

typedef void (*TxFunc_t)(const uint8_t *packet, uint16_t size);

typedef struct
{
    TxFunc_t transmit;
} TransportInterface_t;
