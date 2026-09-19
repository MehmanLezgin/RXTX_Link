#include "rxtx_packet.h"

uint16_t RXTX_Packet__crc_update(uint16_t crc, uint8_t data)
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

uint16_t RXTX_Packet__calc_crc(const RXTX_Packet *pkt)
{
    uint16_t crc = 0x0000;

    crc = RXTX_Packet__crc_update(crc, pkt->version);
    crc = RXTX_Packet__crc_update(crc, pkt->id & 0xFF);
    crc = RXTX_Packet__crc_update(crc, (pkt->id >> 8) & 0xFF);
    crc = RXTX_Packet__crc_update(crc, pkt->seq & 0xFF);
    crc = RXTX_Packet__crc_update(crc, (pkt->seq >> 8) & 0xFF);
    crc = RXTX_Packet__crc_update(crc, pkt->len & 0xFF);
    crc = RXTX_Packet__crc_update(crc, (pkt->len >> 8) & 0xFF);

    for (uint8_t i = 0; i < pkt->len; i++)
        crc = RXTX_Packet__crc_update(crc, pkt->payload[i]);

    return crc;
}

inline uint16_t RXTX_Packet__calc_full_size(const RXTX_Packet *pkt)
{
    return sizeof(pkt->sync) +
           sizeof(pkt->version) +
           sizeof(pkt->id) +
           sizeof(pkt->seq) +
           pkt->len +
           sizeof(pkt->len) +
           sizeof(pkt->crc);
}