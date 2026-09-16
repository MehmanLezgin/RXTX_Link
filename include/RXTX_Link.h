#pragma once

#include "rx_state.h"
#include "transport_interface.h"
#include "rxtx_packet.h"

typedef struct
{
    TransportInterface_t transport;
    RxState state;
    RXTX_Packet packet;
    uint32_t last_byte_time;
    uint16_t bytes_recieved;
    uint16_t expected_seq;
    uint8_t version;
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
void RXTX__handle_system_response(const RXTX_Link *link, const uint8_t id, const uint16_t seq);
void RXTX__on_valid_packet_recieved(const RXTX_Link *link);