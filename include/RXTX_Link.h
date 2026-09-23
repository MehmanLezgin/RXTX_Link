#pragma once

#include "packet_rx_state.h"
#include "transport_interface.h"
#include "rxtx_packet.h"
#include "rxtx_buffer_session.h"

#ifndef RXTX_MAX_SESSIONS
#define RXTX_MAX_SESSIONS 4
#endif

#ifndef RXTX_SESSION_TIMEOUT_THRESHOLD_MS
#define RXTX_SESSION_TIMEOUT_THRESHOLD_MS 100
#endif

#ifndef RXTX_BYTE_TIMEOUT_THRESHOLD_MS
#define RXTX_BYTE_TIMEOUT_THRESHOLD_MS 100
#endif

// acknowledgement
#define RXTX_TYPE_ACK (uint16_t)0xFF00u
#define RXTX_TYPE_BUFFER_START (uint16_t)0xFE00u
// #define RXTX_TYPE_BUFFER_END_ACK (uint16_t)0xFD00u
#define RXTX_TYPE_NACK_CRC (uint16_t)0xFC00u
#define RXTX_TYPE_NACK_SEQ (uint16_t)0xFB00u
#define RXTX_TYPE_NACK_NO_MEM (uint16_t)0xFA00u
#define RXTX_TYPE_NACK_BUSY (uint16_t)0xF900u
#define RXTX_TYPE_NACK_TIMEOUT (uint16_t)0xF800u
#define RXTX_TYPE_NACK_LEN (uint16_t)0xF700u
#define RXTX_TYPE_NACK_OVERFLOW (uint16_t)0xF600u

#define __RXTX_SYSTEM_RESPONSE_MIN RXTX_TYPE_NACK_OVERFLOW

typedef struct
{
    TransportInterface_t transport;
    PacketRxState state;
    RXTX_Packet packet;
    uint32_t last_byte_time;
    uint16_t packet_bytes_recieved;
    uint8_t version;
    RXTX_Session sessions[RXTX_MAX_SESSIONS];
} RXTX_Link;

static inline uint16_t __RXTX__set_u16_low8(uint16_t dest, uint8_t src)
{
    return (dest & 0xFF00u) | (uint16_t)src;
}

static inline uint16_t __RXTX__set_u16_high8(uint16_t dest, uint8_t src)
{
    return (dest & 0x00FFu) | ((uint16_t)src << 8);
}

static inline uint16_t __read_uint16_be(const uint8_t *buffer)
{
    return ((uint16_t)buffer[0] << 8) | (uint16_t) buffer[1];
}

static inline uint8_t __RXTX__is_system_response_id(const uint16_t id)
{
    return id >= __RXTX_SYSTEM_RESPONSE_MIN;
}

void __RXTX__parse_byte(RXTX_Link *link, uint8_t byte);

RXTX_Link RXTX_Link__create(TransportInterface_t transport, uint8_t version);

void __RXTX__send_packet(
    const RXTX_Link *link,
    const uint16_t id,
    const uint16_t seq,
    const uint8_t *payload,
    const uint16_t payload_length);

void __RXTX__transmit_packet(
    const RXTX_Link *link,
    const RXTX_Packet *packet);

void __RXTX__send_system_response(const RXTX_Link *link, const uint16_t sys_res_id, const uint16_t id, const uint16_t seq);

void __RXTX__on_packet_recieved(RXTX_Link *link);
void __RXTX__handle_system_response(RXTX_Link *link);
void __RXTX__on_valid_packet_recieved(RXTX_Link *link);

uint8_t RXTX__transmit_data(
    RXTX_Link *link,
    const uint16_t id,
    uint8_t *payload,
    const uint16_t len);

void RXTX__update(RXTX_Link *link);
RXTX_Session *__RXTX__get_free_session(RXTX_Link *link);
RXTX_Session *__RXTX__find_session(RXTX_Link *link, const uint16_t id);
void __RXTX__begin_buffer_recv(RXTX_Link *link);
void __RXTX__process_transmit_session(
    const RXTX_Link *link,
    RXTX_Session *session,
    const uint8_t advance_bytes);

uint32_t __RXTX__get_millis(const RXTX_Link *link);
void __RXTX__update_session_last_activity(const RXTX_Link *link, RXTX_Session *session);
uint8_t __RXTX__is_session_timeout(const RXTX_Link *link, const RXTX_Session *session);
uint8_t __RXTX__is_byte_read_timeout(const RXTX_Link *link);
uint8_t __RXTX__handle_timeout(const RXTX_Link *link, RXTX_Session *session);