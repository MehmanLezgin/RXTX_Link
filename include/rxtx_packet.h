#pragma once
#include <stdint.h>


#ifndef MAX_RXTX_PAYLOAD_LEN
#define MAX_RXTX_PAYLOAD_LEN 4
#endif

#ifndef MAX_RXTX_PACKET_SYNC_BYTE_LOW
#define MAX_RXTX_PACKET_SYNC_BYTE_LOW 0xAA
#endif

#ifndef MAX_RXTX_PACKET_SYNC_BYTE_HIGH
#define MAX_RXTX_PACKET_SYNC_BYTE_HIGH 0x77
#endif


#define MAX_RXTX_PACKET_SYNC_U16 (((uint16_t) MAX_RXTX_PACKET_SYNC_BYTE_HIGH << 8) | MAX_RXTX_PACKET_SYNC_BYTE_LOW)

/*
    [ sync      ]   -   16
    [ version   ]   -   8
    [ id        ]   -   16
    [ seq       ]   -   16
    [ length    ]   -   16
    [ payload   ]   -   0-256
    [ crc       ]   -   16

*/
#pragma pack(push, 1)
typedef struct
{
    uint16_t sync;
    uint8_t version;
    uint16_t id;
    uint16_t seq;
    uint16_t len;
    uint8_t payload[MAX_RXTX_PAYLOAD_LEN];
    uint16_t crc;
} RXTX_Packet;
#pragma pack(pop)

uint16_t __RXTX_Packet__crc_update(uint16_t crc, uint8_t data);
uint16_t __RXTX_Packet__calc_crc(const RXTX_Packet *pkt);
static inline uint16_t RXTX_Packet__calc_full_size(const RXTX_Packet *pkt)
{
    return sizeof(pkt->sync) +
           sizeof(pkt->version) +
           sizeof(pkt->id) +
           sizeof(pkt->seq) +
           pkt->len +
           sizeof(pkt->len) +
           sizeof(pkt->crc);
}

RXTX_Packet RXTX_Packet__create(
    uint16_t sync,
    uint8_t version,
    uint16_t id,
    uint16_t seq,
    uint16_t len,
    const uint8_t* payload
);

void RXTX_Packet__to_wire_packet(const RXTX_Packet *packet, uint8_t* dest_buffer);