#include "rxtx_buffer_session.h"

void RXTX_Session__init(
    RXTX_Session *s,
    const uint16_t id,
    uint8_t *buffer,
    const uint16_t total_size,
    const uint32_t time_ms
)
{
    s->id = id;
    s->buffer = buffer;
    s->ack_recieved = 0;
    s->bytes_proceed = 0;
    s->expected_seq = 0;
    s->total_size = total_size;
    s->last_activity_ms = time_ms;
}