#pragma once
#include <stdint.h>

#define MAX_RXTX_PACKET_SIZE 256
#define MAX_RXTX_PACKET_SYNC_BYTE_LOW 0xAA
#define MAX_RXTX_PACKET_SYNC_BYTE_HIGH 0x77

#define MAX_RXTX_PACKET_SYNC_U16 (uint16_t)((MAX_RXTX_PACKET_SYNC_BYTE_LOW << 8) | MAX_RXTX_PACKET_SYNC_BYTE_HIGH)

/*
    [ sync      ]   -   16
    [ version   ]   -   8
    [ id        ]   -   16
    [ seq       ]   -   16
    [ length    ]   -   16
    [ payload   ]   -   0-255
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

uint16_t RXTX_Packet_crc_update(uint16_t crc, uint8_t data)
{
    crc = crc ^ ((uint16_t)data << 8);

    for (uint8_t i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) & 0x1021;
        else
            crc <<= 1;
    }

    return crc;
}

uint16_t RXTX_Packet_CalcCRC(const RXTX_Packet *pkt)
{
    uint16_t crc = 0x0000;

    crc = RXTX_Packet_crc_update(crc, pkt->version);
    crc = RXTX_Packet_crc_update(crc, pkt->id & 0xFF);
    crc = RXTX_Packet_crc_update(crc, (pkt->id >> 8) & 0xFF);
    crc = RXTX_Packet_crc_update(crc, pkt->seq & 0xFF);
    crc = RXTX_Packet_crc_update(crc, (pkt->seq >> 8) & 0xFF);
    crc = RXTX_Packet_crc_update(crc, pkt->len & 0xFF);
    crc = RXTX_Packet_crc_update(crc, (pkt->len >> 8) & 0xFF);

    for (uint8_t i = 0; i < pkt->len; i++)
        crc = RXTX_Packet_crc_update(crc, pkt->payload[i]);

    return crc;
}

inline uint16_t RXTX_Packet_CalcFullSize(const RXTX_Packet *pkt)
{
    return sizeof(pkt->sync) +
           sizeof(pkt->version) +
           sizeof(pkt->id) +
           sizeof(pkt->seq) +
           pkt->len +
           sizeof(pkt->len) +
           sizeof(pkt->crc);
}