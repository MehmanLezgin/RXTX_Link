#pragma once
#include <stdint.h>


#ifndef MAX_RXTX_PACKET_SIZE
#define MAX_RXTX_PACKET_SIZE 256
#endif

#ifndef MAX_RXTX_PACKET_SYNC_BYTE_LOW
#define MAX_RXTX_PACKET_SYNC_BYTE_LOW 0xAA
#endif

#ifndef MAX_RXTX_PACKET_SYNC_BYTE_HIGH
#define MAX_RXTX_PACKET_SYNC_BYTE_HIGH 0x77
#endif


#define MAX_RXTX_PACKET_SYNC_U16 (uint16_t)((MAX_RXTX_PACKET_SYNC_BYTE_LOW << 8) | MAX_RXTX_PACKET_SYNC_BYTE_HIGH)

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
    uint16_t id;
    uint16_t seq;
    uint16_t len;
    uint16_t crc;
    uint8_t version;
    uint8_t payload[MAX_RXTX_PACKET_SIZE];
} RXTX_Packet;
#pragma pack(pop)

uint16_t RXTX_Packet__crc_update(uint16_t crc, uint8_t data);
uint16_t RXTX_Packet__calc_crc(const RXTX_Packet *pkt);
inline uint16_t RXTX_Packet__calc_full_size(const RXTX_Packet *pkt);