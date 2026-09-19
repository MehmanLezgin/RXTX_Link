#pragma once

#include "packet_rx_state.h"
#include "transport_interface.h"
#include "rxtx_packet.h"
#include "rxtx_buffer_session.h"

#ifndef RXTX_MAX_SESSIONS
#define RXTX_MAX_SESSIONS 4
#endif


// acknowledgement
#define RXTX_TYPE_ACK 0x00FF
#define RXTX_TYPE_BUFFER_START 0x00FE
#define RXTX_TYPE_BUFFER_END_ACK 0x00FD
#define RXTX_TYPE_NACK_CRC 0x00FC
#define RXTX_TYPE_NACK_SEQ 0x00FB
#define RXTX_TYPE_NACK_NO_MEM 0x00FA
#define RXTX_TYPE_NACK_BUSY 0x00F9
#define RXTX_TYPE_NACK_TIMEOUT 0x00F8
#define RXTX_TYPE_NACK_LEN 0x00F7
#define RXTX_TYPE_NACK_OVERFLOW 0x00F6

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

inline uint16_t RXTX__set_u16_low8(uint16_t dest, uint8_t src);
inline uint16_t RXTX__set_u16_high8(uint16_t dest, uint8_t src);
uint8_t RXTX__parse_byte(RXTX_Link *link, uint8_t byte);

void RXTX__send_packet(
    const RXTX_Link *link,
    const uint16_t id,
    const uint16_t seq,
    const uint8_t *payload,
    const uint16_t payload_length);

void RXTX__transmit_packet(
    const RXTX_Link *link,
    const RXTX_Packet *packet);

void RXTX__on_packet_recieved(RXTX_Link *link);
void RXTX__send_system_response(const RXTX_Link *link, const uint8_t id, const uint16_t seq);
void RXTX__handle_system_response(const RXTX_Link *link);
void RXTX__on_valid_packet_recieved(const RXTX_Link *link);

inline uint8_t RXTX__is_system_response_id(const uint16_t id);

uint8_t RXTX__transmitBuffer(RXTX_Link *link, const uint16_t *buffer, const uint16_t size);
void RXTX__Update(RXTX_Link *link);
RXTX_Session* RXTX__get_free_session(const RXTX_Link *link);
RXTX_Session* RXTX__find_session(const RXTX_Link *link, const uint16_t id);
void RXTX__begin_buffer_recv(const RXTX_Link *link);