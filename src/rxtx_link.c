#include "rxtx_link.h"
#include "rxtx_buffer_session.h"
#include <string.h>

RXTX_Link RXTX_Link__create(TransportInterface_t transport, uint8_t version)
{
    RXTX_Link link = {
        .transport = transport,
        .last_byte_time = 0,
        .packet_bytes_recieved = 0,
        .state = STATE_WAIT_SYNC_LOW,
        .version = version};

    return link;
}

void __RXTX__parse_byte(RXTX_Link *link, uint8_t byte)
{
    RXTX_Packet *pkt = &link->packet;
    uint8_t is_valid_byte = 1;

    switch (link->state)
    {
    case STATE_WAIT_SYNC_LOW:
        if (byte == MAX_RXTX_PACKET_SYNC_BYTE_LOW)
        {
            pkt->sync = __RXTX__set_u16_low8(pkt->sync, byte);
            link->state = STATE_WAIT_SYNC_HIGH;
        }
        else
            is_valid_byte = 0;
        break;

    case STATE_WAIT_SYNC_HIGH:
        if (byte == MAX_RXTX_PACKET_SYNC_BYTE_HIGH)
        {
            pkt->sync = __RXTX__set_u16_high8(pkt->sync, byte);
            link->state = STATE_WAIT_VERSION;
        }
        else
            is_valid_byte = 0;
        break;

    case STATE_WAIT_VERSION:
        pkt->version = byte;
        link->state = STATE_WAIT_ID_LOW;
        break;

    case STATE_WAIT_ID_LOW:
        pkt->id = __RXTX__set_u16_low8(pkt->id, byte);
        link->state = STATE_WAIT_ID_HIGH;
        break;

    case STATE_WAIT_ID_HIGH:
        pkt->id = __RXTX__set_u16_high8(pkt->id, byte);
        link->state = STATE_WAIT_SEQ_LOW;
        break;

    case STATE_WAIT_SEQ_LOW:
        pkt->seq = __RXTX__set_u16_low8(pkt->seq, byte);
        link->state = STATE_WAIT_SEQ_HIGH;
        break;

    case STATE_WAIT_SEQ_HIGH:
        pkt->seq = __RXTX__set_u16_high8(pkt->seq, byte);
        link->state = STATE_WAIT_LEN_LOW;
        break;

    case STATE_WAIT_LEN_LOW:
        pkt->len = __RXTX__set_u16_low8(pkt->len, byte);
        link->state = STATE_WAIT_LEN_HIGH;
        break;

    case STATE_WAIT_LEN_HIGH:
        pkt->len = __RXTX__set_u16_high8(pkt->len, byte);

        if (pkt->len > MAX_RXTX_PAYLOAD_LEN)
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
        pkt->crc = __RXTX__set_u16_low8(pkt->crc, byte);
        link->state = STATE_WAIT_CRC_HIGH;
        break;

    case STATE_WAIT_CRC_HIGH:
        pkt->crc = __RXTX__set_u16_high8(pkt->crc, byte);
        link->state = STATE_WAIT_SYNC_LOW;
        link->packet_bytes_recieved = 0;

        __RXTX__on_packet_recieved(link);
        break;
    default:
        is_valid_byte = 0;
        break;
    }

    if (is_valid_byte)
        link->last_byte_time = __RXTX__get_millis(link);
}

void __RXTX__on_packet_recieved(
    RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;

    uint16_t calculated = __RXTX_Packet__calc_crc(pkt);

    // invalid packet
    if (__RXTX_Packet__calc_crc(pkt) != pkt->crc)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_NACK_CRC, pkt->id, pkt->seq);
        return;
    }

    // system response
    if (__RXTX__is_system_response_id(pkt->id))
    {
        __RXTX__handle_system_response(link);
        return;
    }

    __RXTX__on_valid_packet_recieved(link);
}

void __RXTX__transmit_packet(
    const RXTX_Link *link,
    const RXTX_Packet *packet)
{
    uint16_t full_size = RXTX_Packet__calc_full_size(packet);
    uint8_t temp_buffer[sizeof(RXTX_Packet)];
    RXTX_Packet__to_wire_packet(packet, temp_buffer);

    link->transport.transmit(link->transport.transport_context, temp_buffer, full_size);
}

void __RXTX__send_packet(
    const RXTX_Link *link,
    const uint16_t id,
    const uint16_t seq,
    const uint8_t *payload,
    const uint16_t payload_length)
{
    RXTX_Packet packet = RXTX_Packet__create(
        MAX_RXTX_PACKET_SYNC_U16,
        link->version,
        id,
        seq,
        payload_length,
        payload);

    __RXTX__transmit_packet(link, &packet);
}

void __RXTX__send_system_response(
    const RXTX_Link *link,
    const uint16_t sys_res_id,
    const uint16_t id,
    const uint16_t seq)
{
    uint8_t payload[2];
    payload[0] = id >> 8;
    payload[1] = id & 0x00FF;

    RXTX_Packet packet = RXTX_Packet__create(
        MAX_RXTX_PACKET_SYNC_U16,
        link->version,
        sys_res_id,
        seq,
        sizeof(payload),
        payload);

    __RXTX__transmit_packet(link, &packet);
}

void __RXTX__begin_buffer_recv(RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;
    RXTX_Session *session = __RXTX__get_free_session(link);

    if (session == NULL)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_NACK_BUSY, pkt->id, pkt->seq);
        return;
    }

    if (pkt->len != sizeof(uint16_t) * 2)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_NACK_LEN, pkt->id, pkt->seq);
        return;
    }

    uint16_t incoming_buffer_id = __read_uint16_be(&pkt->payload[0]);
    uint16_t incoming_buffer_size = __read_uint16_be(&pkt->payload[2]);

    uint8_t *user_mem = link->transport.on_buffer_recv_start(incoming_buffer_id, incoming_buffer_size);

    if (user_mem == NULL)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_NACK_NO_MEM, pkt->id, pkt->seq);
        return;
    }

    RXTX_Session__init(session, incoming_buffer_id, user_mem, incoming_buffer_size, __RXTX__get_millis(link));
    session->state = SESSION_RECV_ACTIVE;
    __RXTX__send_system_response(link, RXTX_TYPE_ACK, incoming_buffer_id, pkt->seq);
}

void __RXTX__handle_system_response(RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;

    if (pkt->id == RXTX_TYPE_BUFFER_START)
    {
        __RXTX__begin_buffer_recv(link);
        return;
    }

    uint16_t ackSessionId = __read_uint16_be(pkt->payload);
    RXTX_Session *session = __RXTX__find_session(link, ackSessionId);

    if (
        session == NULL ||
        (session->state != SESSION_TX_WAIT_ACK &&
         session->state != SESSION_TX_HANDSHAKE_WAIT_ACK))
        return;

    __RXTX__update_session_last_activity(link, session);

    switch (pkt->id)
    {
    case RXTX_TYPE_ACK:
    {
        if (pkt->seq != session->expected_seq)
            break;

        uint8_t advance_bytes = session->state != SESSION_TX_HANDSHAKE_WAIT_ACK;
        session->ack_recieved = 1;
        session->state = SESSION_TX_SENDING;
        __RXTX__process_transmit_session(link, session, advance_bytes);
        break;
    }

    case RXTX_TYPE_NACK_CRC:
    case RXTX_TYPE_NACK_SEQ:
    case RXTX_TYPE_NACK_LEN:
    case RXTX_TYPE_NACK_OVERFLOW:
    {
        uint16_t ackSessionId = __read_uint16_be(pkt->payload);
        RXTX_Session *session = __RXTX__find_session(link, ackSessionId);

        if (session == NULL)
            break;

        session->ack_recieved = 1;
        __RXTX__process_transmit_session(link, session, 0);
        break;
    }

    case RXTX_TYPE_NACK_NO_MEM:
    case RXTX_TYPE_NACK_BUSY:
    case RXTX_TYPE_NACK_TIMEOUT:
        session->state = SESSION_IDLE;
        break;
    }
}

#include <stdio.h>

void __RXTX__on_valid_packet_recieved(RXTX_Link *link)
{
    RXTX_Packet *pkt = &link->packet;

    RXTX_Session *session = __RXTX__find_session(link, pkt->id);

    link->transport.on_packet_recv(pkt);

    // single packet
    if (session == NULL)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_ACK, pkt->id, pkt->seq);
        return;
    }

    if (pkt->seq != session->expected_seq)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_NACK_SEQ, pkt->id, pkt->seq);
        return;
    }

    __RXTX__update_session_last_activity(link, session);

    // buffer chunk
    uint16_t chunk_size = pkt->len;
    uint16_t bytes_proceed = session->bytes_proceed + chunk_size;

    if (bytes_proceed > session->total_size)
    {
        __RXTX__send_system_response(link, RXTX_TYPE_NACK_OVERFLOW, pkt->id, pkt->seq);
        return;
    }

    uint8_t *chunk_ptr = session->buffer + session->bytes_proceed;

    memcpy(chunk_ptr, pkt->payload, chunk_size);
    __RXTX__send_system_response(link, RXTX_TYPE_ACK, pkt->id, pkt->seq);

    session->expected_seq++;
    session->bytes_proceed = bytes_proceed;

    if (bytes_proceed == session->total_size)
    {
        link->transport.on_buffer_recv_end(pkt->id, session->buffer, bytes_proceed);
        session->state = SESSION_IDLE;
    }
}

uint8_t RXTX__transmit_data(
    RXTX_Link *link,
    const uint16_t id,
    uint8_t *payload,
    const uint16_t len)
{
    RXTX_Session *session = __RXTX__get_free_session(link);

    if (session == NULL)
        return 0;

    RXTX_Session__init(session, id, payload, len, __RXTX__get_millis(link));

    uint8_t bufferStartPayload[4];
    bufferStartPayload[0] = id >> 8;
    bufferStartPayload[1] = id & 0x00FF;
    bufferStartPayload[2] = len >> 8;
    bufferStartPayload[3] = len & 0x00FF;

    session->ack_recieved = 0;
    session->state = SESSION_TX_HANDSHAKE_WAIT_ACK;

    __RXTX__send_packet(link, RXTX_TYPE_BUFFER_START, session->expected_seq, bufferStartPayload, sizeof(bufferStartPayload));

    return 1;
}

void RXTX__update(RXTX_Link *link)
{
    uint8_t temp_buffer[64];
    uint8_t bytes_read = link->transport.read_buffer(
        link->transport.transport_context,
        temp_buffer,
        sizeof(temp_buffer));

    if (bytes_read > 0)
    {
        if (__RXTX__is_byte_read_timeout(link))
            link->state = STATE_WAIT_SYNC_LOW;

        for (uint8_t i = 0; i < bytes_read; i++)
            __RXTX__parse_byte(link, temp_buffer[i]);
    }

    // tx:
    for (uint8_t i = 0; i < RXTX_MAX_SESSIONS; i++)
    {
        RXTX_Session *session = &link->sessions[i];
        if (__RXTX__handle_timeout(link, session))
            continue;

        __RXTX__process_transmit_session(link, session, 0);
    }
}

void __RXTX__process_transmit_session(
    const RXTX_Link *link,
    RXTX_Session *session,
    const uint8_t advance_bytes)
{
    if (session->state != SESSION_TX_SENDING)
        return;

    if (advance_bytes)
    {
        uint16_t last_chunk_size = RXTX_Session__get_chunk_size(session);
        session->bytes_proceed += last_chunk_size;
        session->expected_seq++;
    }

    uint16_t remaining = session->total_size - session->bytes_proceed;

    if (remaining == 0)
    {
        session->state = SESSION_IDLE;
        return;
    }

    uint16_t chunk_size = RXTX_Session__get_chunk_size(session);
    uint8_t *chunk_ptr = session->buffer + session->bytes_proceed;

    session->ack_recieved = 0;

    __RXTX__send_packet(
        link,
        session->id,
        session->expected_seq,
        chunk_ptr,
        chunk_size);

    __RXTX__update_session_last_activity(link, session);

    session->state = SESSION_TX_WAIT_ACK;
}

RXTX_Session *__RXTX__get_free_session(RXTX_Link *link)
{
    for (uint8_t i = 0; i < RXTX_MAX_SESSIONS; i++)
    {
        RXTX_Session *s = &link->sessions[i];
        if (s->state == SESSION_IDLE)
            return s;
    }

    return NULL;
}

RXTX_Session *__RXTX__find_session(RXTX_Link *link, const uint16_t id)
{
    for (uint8_t i = 0; i < RXTX_MAX_SESSIONS; i++)
    {
        RXTX_Session *s = &link->sessions[i];
        if (s->state != SESSION_IDLE && s->id == id)
            return s;
    }

    return NULL;
}

uint32_t __RXTX__get_millis(const RXTX_Link *link)
{
    return link->transport.get_millis();
}

void __RXTX__update_session_last_activity(const RXTX_Link *link, RXTX_Session *session)
{
    uint32_t now = __RXTX__get_millis(link);
    session->last_activity_ms = now;
}

uint8_t __RXTX__is_session_timeout(const RXTX_Link *link, const RXTX_Session *session)
{
    return __RXTX__get_millis(link) - session->last_activity_ms > RXTX_SESSION_TIMEOUT_THRESHOLD_MS;
}

uint8_t __RXTX__is_byte_read_timeout(const RXTX_Link *link)
{
    return __RXTX__get_millis(link) - link->last_byte_time > RXTX_BYTE_TIMEOUT_THRESHOLD_MS;
}

uint16_t RXTX_Session__get_chunk_size(const RXTX_Session *session)
{
    uint16_t remaining = session->total_size - session->bytes_proceed;
    return (remaining > MAX_RXTX_PAYLOAD_LEN) ? MAX_RXTX_PAYLOAD_LEN : remaining;
}

uint8_t __RXTX__handle_timeout(const RXTX_Link *link, RXTX_Session *session)
{
    if (session->state == SESSION_IDLE) return 0;
    
    if (session->ack_recieved &&
        (session->state == SESSION_TX_WAIT_ACK ||
         session->state == SESSION_TX_HANDSHAKE_WAIT_ACK))
        return 0;

    if (!__RXTX__is_session_timeout(link, session))
        return 0;

    __RXTX__send_system_response(link, RXTX_TYPE_NACK_TIMEOUT, session->id, session->expected_seq);
    session->state = SESSION_IDLE;
    return 1;
}