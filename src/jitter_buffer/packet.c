#include <jitter_buffer/packet.h>
#include <jitter_buffer/nal_type.h>
#include <utility/list_alloc.h>
#include <pj/assert.h>

/**
  * create a packet and init it
  */
PJ_DEF(int) jt_packet_create(JTPacket **packet, packet_alloc_t *packet_alloc, pj_uint16_t seq,
                        pj_uint32_t ts, pjmedia_frame_type frame_type, pj_bool_t isFirst,
                        pj_bool_t isMarket, const char *data_ptr, int size) {
    pj_assert(packet_alloc != NULL && size <= MAX_PAYLOAD_SIZE);
    //alloc memory
    JTPacket *packet_internal ;
    list_alloc_alloc(JTPacket, &packet_internal, packet_alloc);
    
    //set initial values
    packet_internal->seq = seq;
    packet_internal->ts = ts;
    packet_internal->frame_type = frame_type;
    packet_internal->isRetrans = PJ_FALSE;
    packet_internal->isFirst = isFirst;
    packet_internal->isMarket = isMarket;
    //nal type
    packet_internal->nal_type = distinguish_packet_nal_type(packet_internal);
    //data
    memcpy(packet_internal->data_ptr, data_ptr, size);
    packet_internal->size = size;
    //return
    *packet = packet_internal;
    return 0;
}
