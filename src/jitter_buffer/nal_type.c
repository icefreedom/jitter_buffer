#include <jitter_buffer/packet.h>
#include <jitter_buffer/nal_type.h>
enum
{
    NAL_TYPE_SINGLE_NAL_TYPE_MIN = 1,
    NAL_TYPE_SINGLE_NAL_TYPE_MAX = 23,
    NAL_TYPE_STAP_A     = 24,
    NAL_TYPE_FU_A       = 28,
};
enum { MIN_PAYLOAD_SIZE = 2 };
PJ_DEF(NAL_TYPE) distinguish_packet_nal_type(JTPacket *packet) {
    if(packet == NULL || packet->frame_type == PJMEDIA_FRAME_TYPE_EMPTY) 
        return NAL_TYPE_COMPLETE;
    return distinguish_nal_type(packet->data_ptr, packet->size);
    
}

PJ_DEF(NAL_TYPE) distinguish_nal_type(const char *data_ptr, int size) {
    if(size < MIN_PAYLOAD_SIZE) return NAL_TYPE_COMPLETE;
    const pj_uint8_t *payload = (const pj_uint8_t *)data_ptr;
    int nal_type = payload[0] & 0x1f;
    if(nal_type >= NAL_TYPE_SINGLE_NAL_TYPE_MIN && nal_type <=
        NAL_TYPE_SINGLE_NAL_TYPE_MAX) {
        //single nal unit packet
        return NAL_TYPE_COMPLETE;
    } else if(nal_type == NAL_TYPE_STAP_A) {
        //STAP-A
        return NAL_TYPE_COMPLETE;
    } else if(nal_type == NAL_TYPE_FU_A) {
        //FU-A
        int S, E;
        S = (payload[1] & 0x80) >> 7;
        E = (payload[1] & 0x40) >> 6;
        if(S == 1 && E == 1)
            return NAL_TYPE_COMPLETE;
        else if(S == 1)
            return NAL_TYPE_START;
        else if(E == 1)
            return NAL_TYPE_END;
        else
            return NAL_TYPE_INCOMPLETE;
    }
    return NAL_TYPE_COMPLETE;
}
