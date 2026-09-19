#pragma once
#include <stdint.h>
#include "rxtx_packet.h"

typedef uint8_t (*ReadBuffer_t)(void *context, uint8_t *dest_buffer, const uint16_t max_length);
typedef void (*TxFunc_t)(const uint8_t *packet, const uint16_t size);
typedef void (*ChunkRecv_t)(const uint16_t id, const uint8_t *chunk, const uint16_t size, const uint16_t bytes_recieved);
typedef uint8_t *(*OnPacketRecv_t)(const RXTX_Packet *packet);
typedef uint8_t *(*OnBufferRecvStart_t)(const uint16_t id, const uint16_t size);
typedef void (*OnBufferRecvEnd_t)(const uint16_t id, const uint8_t *buffer, const uint16_t size);

typedef struct
{
    ReadBuffer_t read_buffer;
    TxFunc_t transmit;
    ChunkRecv_t chunk_recv;
    OnBufferRecvStart_t on_buffer_recv_start;
    OnBufferRecvEnd_t on_buffer_recv_end;
    OnPacketRecv_t on_packet_recv;
    void *transport_context;
} TransportInterface_t;