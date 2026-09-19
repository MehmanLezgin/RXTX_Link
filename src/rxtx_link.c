#include "rxtx_link.h"
#include <string.h>

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
        pkt->payload[link->packet_bytes_recieved++] = byte;
        if (link->packet_bytes_recieved >= pkt->len)
            link->state = STATE_WAIT_CRC_LOW;
        break;

    case STATE_WAIT_CRC_LOW:
        pkt->crc = RXTX__set_u16_low8(pkt->crc, byte);
        link->state = STATE_WAIT_CRC_HIGH;
        break;

    case STATE_WAIT_CRC_HIGH:
        pkt->crc = RXTX__set_u16_high8(pkt->crc, byte);
        link->state = STATE_WAIT_SYNC_LOW;
        link->packet_bytes_recieved = 0;


        RXTX__on_packet_recieved(link);
        break;
    }

    return 0;
}

inline uint8_t RXTX__is_system_response_id(const uint16_t id)
{
    return id >= __RXTX_SYSTEM_RESPONSE_MIN;
}

void RXTX__on_packet_recieved(
    RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;

    // invalid packet
    if (RXTX_Packet__calc_crc(pkt) != pkt->crc)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK_CRC, pkt->seq);
        return;
    }

    // system response
    if (RXTX__is_system_response_id(pkt->id))
    {
        RXTX__handle_system_response(link);
        return;
    }

    RXTX__on_valid_packet_recieved(link);
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
        .len = payload_length};

    if (payload && payload_length)
        memcpy(packet.payload, payload, payload_length);

    packet.crc = RXTX_Packet__calc_crc(&packet);

    RXTX__transmit_packet(link, &packet);
}

void RXTX__send_system_response(
    const RXTX_Link *link,
    const uint8_t id,
    const uint16_t seq)
{
    RXTX_Packet packet = {
        .sync = MAX_RXTX_PACKET_SYNC_U16,
        .version = link->version,
        .id = id,
        .seq = seq,
        .len = 0,
    };

    packet.crc = RXTX_Packet__calc_crc(&packet);

    RXTX__transmit_packet(link, &packet);
}

void RXTX__begin_buffer_recv(const RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;
    RXTX_Session *session = RXTX__get_free_session(link);

    if (session == NULL)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK_BUSY, pkt->seq);
        return;
    }

    if (pkt->len != sizeof(uint16_t))
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK_LEN, pkt->seq);
        return;
    }

    uint16_t incoming_buffer_size = ((uint16_t)pkt->payload[0] << 8) | pkt->payload[1];
    ;
    uint8_t *user_mem = link->transport.on_buffer_recv_start(pkt->id, incoming_buffer_size);

    if (user_mem == NULL)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK_NO_MEM, pkt->seq);
        return;
    }

    RXTX_Session__init(session, pkt->id, user_mem, incoming_buffer_size);
    session->state = SESSION_RECV_ACTIVE;
}

void RXTX__handle_system_response(const RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;

    if (pkt->id == RXTX_TYPE_BUFFER_START)
    {
        RXTX__begin_buffer_recv(link);
        return;
    }

    // nack, ack: ...
}

void RXTX__on_valid_packet_recieved(const RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;

    RXTX_Session *session = RXTX__find_session(link, pkt->id);

    // single packet
    if (session == NULL)
    {
        link->transport.on_packet_recv(pkt);
        return;
    }

    // buffer chunk
    if (pkt->seq != session->expected_seq)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK_SEQ, pkt->seq);
        return;
    }

    uint16_t chunk_size = pkt->len;
    uint16_t bytes_proceed = session->bytes_proceed + chunk_size;

    if (bytes_proceed > session->total_size)
    {
        RXTX__send_system_response(link, RXTX_TYPE_NACK_OVERFLOW, pkt->seq);
        return;
    }

    uint8_t *chunk = pkt->payload;
    uint8_t *dest = session->buffer + session->bytes_proceed;

    memcpy(dest, chunk, chunk_size);
    RXTX__send_system_response(link, RXTX_TYPE_ACK, pkt->seq);

    session->expected_seq++;
    session->bytes_proceed = bytes_proceed;

    if (bytes_proceed == session->total_size)
    {
        link->transport.on_buffer_recv_end(pkt->id, session->buffer, bytes_proceed);
        session->state = SESSION_IDLE;
    }
}

void RXTX__transmit_packet(const RXTX_Link *link, const RXTX_Packet *packet)
{
    uint16_t fullPacketSize = RXTX_Packet__calc_full_size(packet);
    link->transport.transmit((uint8_t *)packet, fullPacketSize);
}

uint8_t RXTX__transmitBuffer(RXTX_Link *link, const uint16_t *buffer, const uint16_t size)
{

    return 1;
}

void RXTX__Update(RXTX_Link *link)
{
    uint8_t temp_buffer[32];
    uint8_t bytes_read = link->transport.read_buffer(
        link->transport.transport_context,
        temp_buffer,
        sizeof(temp_buffer));

    for (uint8_t i = 0; i < bytes_read; i++)
    {
        RXTX__parse_byte(link, temp_buffer[i]);
    }

    // tx:
    // for (uint8_t i = 0; i < RXTX_MAX_SESSIONS; i++)
    // {
    //     RXTX_Session *session = &link->sessions[i];

    //     if (session->state == SESSION_TX_WAIT_ACK && session->ack_recieved)
    //     {
    //     }
    // }
}

RXTX_Session *RXTX__get_free_session(const RXTX_Link *link)
{
    for (uint8_t i = 0; i < RXTX_MAX_SESSIONS; i++)
    {
        RXTX_Session *s = &link->sessions[i];
        if (s->state == SESSION_IDLE)
            return s;
    }

    return NULL;
}

RXTX_Session *RXTX__find_session(const RXTX_Link *link, const uint16_t id)
{
    for (uint8_t i = 0; i < RXTX_MAX_SESSIONS; i++)
    {
        RXTX_Session *s = &link->sessions[i];
        if (s->state != SESSION_IDLE && s->id == id)
            return s;
    }

    return NULL;
}
