#include "RXTX_Link.h"
#include <string.h>

// acknowledgement
#define RXTX_TYPE_ACK 0xFF
#define RXTX_TYPE_NACK 0xFE
#define RXTX_TYPE_BUFFER_END 0xFD
#define RXTX_TYPE_BUFFER_START 0xFC

inline uint16_t RXTX__set_u16_low8(uint16_t dest, uint8_t src)
{
    return dest | (uint16_t)src;
}

inline uint16_t RXTX__set_u16_high8(uint16_t dest, uint8_t src)
{
    return dest | (uint16_t)src << 8;
}

uint8_t RXTX__parse_byte(RXTX_Link *link, uint8_t byte)
{
    RXTX_Packet *pkt = &link->packet;

    switch (link->state)
    {
    case STATE_WAIT_SYNC_LOW:
        if (byte == MAX_RXTX_PACKET_SYNC_BYTE_LOW)
        {
            pkt->sync = RXTX__set_u16_low8(pkt->sync, byte);
            link->state = STATE_WAIT_SYNC_HIGH;
        }
        break;

    case STATE_WAIT_SYNC_HIGH:
        if (byte == MAX_RXTX_PACKET_SYNC_BYTE_HIGH)
        {
            pkt->sync = RXTX__set_u16_high8(pkt->sync, byte);
            link->state = STATE_WAIT_VERSION;
        }
        break;

    case STATE_WAIT_VERSION:
        pkt->version = byte;
        link->state = STATE_WAIT_ID_LOW;
        break;

    case STATE_WAIT_ID_LOW:
        pkt->id = RXTX__set_u16_low8(pkt->id, byte);
        link->state = STATE_WAIT_ID_HIGH;
        break;

    case STATE_WAIT_ID_HIGH:
        pkt->id = RXTX__set_u16_high8(pkt->id, byte);
        link->state = STATE_WAIT_SEQ_LOW;
        break;

    case STATE_WAIT_SEQ_LOW:
        pkt->seq = RXTX__set_u16_low8(pkt->seq, byte);
        link->state = STATE_WAIT_SEQ_HIGH;
        break;

    case STATE_WAIT_SEQ_HIGH:
        pkt->seq = RXTX__set_u16_high8(pkt->seq, byte);
        link->state = STATE_WAIT_LEN_LOW;
        break;

    case STATE_WAIT_LEN_LOW:
        pkt->len = RXTX__set_u16_low8(pkt->len, byte);
        link->state = STATE_WAIT_LEN_HIGH;
        break;

    case STATE_WAIT_LEN_HIGH:
        pkt->len = RXTX__set_u16_high8(pkt->len, byte);

        if (pkt->len > MAX_RXTX_PACKET_SIZE)
            link->state = STATE_WAIT_SYNC_LOW;
        else
            link->state = (pkt->len != 0) ? STATE_WAIT_PAYLOAD : STATE_WAIT_CRC_LOW;

        break;

    case STATE_WAIT_PAYLOAD:
        pkt->payload[link->bytes_recieved++] = byte;
        if (link->bytes_recieved >= pkt->len)
            link->state = STATE_WAIT_CRC_LOW;
        break;

    case STATE_WAIT_CRC_LOW:
        pkt->crc = RXTX__set_u16_low8(pkt->crc, byte);
        link->state = STATE_WAIT_CRC_HIGH;
        break;

    case STATE_WAIT_CRC_HIGH:
        pkt->crc = RXTX__set_u16_high8(pkt->crc, byte);
        link->state = STATE_WAIT_SYNC_LOW;

        RXTX__on_packet_recieved(link);
        break;
    }

    return 0;
}

void RXTX__on_packet_recieved(
    RXTX_Link *link
)
{
    RXTX_Packet *pkt = &link->packet;

    // invalid packet
    if (RXTX_Packet_CalcCRC(pkt) != pkt->crc)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK, pkt->seq);
        return;
    }

    // system response
    if (pkt->id == RXTX_TYPE_ACK || pkt->id == RXTX_TYPE_NACK)
    {
        RXTX__handle_system_response(link, pkt->id, pkt->seq);
        return;
    }

    if (pkt->seq != link->expected_seq)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK, pkt->seq);
        return;
    }

    RXTX__send_system_response(link, RXTX_TYPE_ACK, pkt->seq);

    link->expected_seq++;
}

void RXTX__send_packet(
    const RXTX_Link *link,
    const uint16_t id,
    const uint16_t seq,
    const uint8_t *payload,
    const uint16_t payload_length)
{
    RXTX_Packet packet = {
        .sync = MAX_RXTX_PACKET_SYNC_U16,
        .version = link->version,
        .id = id,
        .seq = seq,
        .len = payload_length
    };


    if (payload && payload_length)
        memcpy(packet.payload, payload, payload_length);

    packet.crc = RXTX_Packet_CalcCRC(&packet);

    RXTX__transmit_packet(link, &packet);
}

void RXTX__send_system_response(
    const RXTX_Link *link,
    const uint8_t id, 
    const uint16_t seq
)
{
    RXTX_Packet packet = {
        .sync = MAX_RXTX_PACKET_SYNC_U16,
        .version = link->version,
        .id = id,
        .seq = seq,
        .len = 0,
    };
    
    packet.crc = RXTX_Packet_CalcCRC(&packet);

    RXTX__transmit_packet(link, &packet);
}

void RXTX__handle_system_response(const RXTX_Link *link, const uint8_t id, const uint16_t seq)
{
    
}

void RXTX__on_valid_packet_recieved(const RXTX_Link *link)
{

}

void RXTX__transmit_packet(const RXTX_Link *link, const RXTX_Packet *packet)
{
    uint16_t fullPacketSize = RXTX_Packet_CalcFullSize(packet);
    link->transport.transmit((uint8_t *)&packet, fullPacketSize);
}

