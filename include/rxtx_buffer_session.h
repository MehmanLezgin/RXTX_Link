#pragma once
#include <stdint.h>

typedef enum
{
    SESSION_IDLE,
    SESSION_RECV_ACTIVE,
    SESSION_TX_WAIT_ACK,
    SESSION_TX_SENDING
} RXTX_BufferSessionState;

typedef struct
{
    uint16_t id;
    uint8_t *buffer;
    uint16_t total_size;
    uint16_t bytes_proceed;
    uint16_t expected_seq;
    RXTX_BufferSessionState state;
    uint32_t last_activity_ms;
    uint8_t ack_recieved;
} RXTX_Session;


void RXTX_Session__init(
    RXTX_Session *s,
    const uint16_t id,
    uint8_t *buffer,
    const uint16_t total_size,
    const uint32_t time_ms
);