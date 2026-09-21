#include "rxtx_packet.h"
#include <string.h>
#include <math.h>

uint16_t __RXTX_Packet__crc_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }

    return crc;
}
uint16_t __RXTX_Packet__calc_crc(const RXTX_Packet *pkt)
{
    uint16_t crc = 0x0000;

    crc = __RXTX_Packet__crc_update(crc, pkt->version);
    crc = __RXTX_Packet__crc_update(crc, pkt->id & 0xFF);
    crc = __RXTX_Packet__crc_update(crc, (pkt->id >> 8) & 0xFF);
    crc = __RXTX_Packet__crc_update(crc, pkt->seq & 0xFF);
    crc = __RXTX_Packet__crc_update(crc, (pkt->seq >> 8) & 0xFF);
    crc = __RXTX_Packet__crc_update(crc, pkt->len & 0xFF);
    crc = __RXTX_Packet__crc_update(crc, (pkt->len >> 8) & 0xFF);

    for (uint8_t i = 0; i < pkt->len; i++)
        crc = __RXTX_Packet__crc_update(crc, pkt->payload[i]);

    return crc;
}

RXTX_Packet RXTX_Packet__create(
    uint16_t sync,
    uint8_t version,
    uint16_t id,
    uint16_t seq,
    uint16_t len,
    const uint8_t* payload
)
{
    RXTX_Packet packet = {
        .sync = MAX_RXTX_PACKET_SYNC_U16,
        .version = version,
        .id = id,
        .seq = seq,
        .len = len};

    if (payload && len)
        memcpy(packet.payload, payload, len <= MAX_RXTX_PAYLOAD_LEN ? len : MAX_RXTX_PAYLOAD_LEN);

    packet.crc = __RXTX_Packet__calc_crc(&packet);
    return packet;
}

void RXTX_Packet__to_wire_packet(
    const RXTX_Packet *packet,
    uint8_t *dest_buffer)
{
    uint8_t *ptr = dest_buffer;

    memcpy(ptr, &packet->sync, sizeof(packet->sync));
    ptr += sizeof(packet->sync);

    *ptr++ = packet->version;

    memcpy(ptr, &packet->id, sizeof(packet->id));
    ptr += sizeof(packet->id);

    memcpy(ptr, &packet->seq, sizeof(packet->seq));
    ptr += sizeof(packet->seq);

    memcpy(ptr, &packet->len, sizeof(packet->len));
    ptr += sizeof(packet->len);

    memcpy(ptr, packet->payload, packet->len);
    ptr += packet->len;

    memcpy(ptr, &packet->crc, sizeof(packet->crc));
}