#include <jitter_buffer/decode_state.h>
#include <jitter_buffer/packet.h>
#include <jitter_buffer/jitter_utility.h>
#include <jitter_buffer/frame_buffer.h>
#include <pj/assert.h>

PJ_DEF(pj_bool_t) decode_state_isOldPacket(JTPacket *packet, Decode_State 
 *decode_state) {
    if(decode_state->ts == 0) //not inited
        return PJ_FALSE;
    if(packet->ts <= decode_state->ts)
        return PJ_TRUE;
    else if(LatestSeqNum(packet->seq, decode_state->seq) == decode_state->seq) 
        return PJ_TRUE;
    return PJ_FALSE;

}

PJ_DEF(pj_bool_t) decode_state_isOldFrame(Frame_Buffer *frame_buffer, Decode_State *decode_state) {
    if(decode_state->ts == 0) //not inited
        return PJ_FALSE;
    if(frame_buffer->ts <= decode_state->ts) 
        return PJ_TRUE;
    return PJ_FALSE;
}

PJ_DEF(pj_bool_t) decode_state_isContinuousFrame(Frame_Buffer *frame_buffer, Decode_State *decode_state) {
    if(decode_state->ts == 0) //not inited
        return PJ_TRUE;
    int seq = frame_buffer_getFirstSeq(frame_buffer);
    if(seq == -1) //no media packets in the frame
        return PJ_FALSE;
    if(InSequence(decode_state->seq, (pj_uint16_t)seq))
        return PJ_TRUE;
    return PJ_FALSE;
}

PJ_DEF(void) decode_state_init(Decode_State *decode_state) {
    decode_state->seq = 0;
    decode_state->ts = 0;
    decode_state->isKeyFrame = PJ_FALSE;
    decode_state->frame_type = PJMEDIA_FRAME_TYPE_NONE;
    decode_state->inited = PJ_FALSE;
}

PJ_DEF(void) decode_state_reset(Decode_State *decode_state) {
    decode_state_init(decode_state);
    decode_state->inited = PJ_FALSE;
}

PJ_DEF(void) decode_state_updateOldPacket(JTPacket *packet, Decode_State *decode_state) {
    pj_assert(decode_state != NULL && packet != NULL);
    if(packet->ts == decode_state->ts) {
        if(LatestSeqNum(packet->seq, decode_state->seq) == packet->seq)
            decode_state->seq = packet->seq;
    }
}

//media frame, update state
PJ_DEF(void) decode_state_updateFrame(Frame_Buffer *frame, Decode_State *decode_state) {
        decode_state->seq = frame_buffer_getHighSeq(frame);
        decode_state->ts = frame->ts;
        decode_state->inited = PJ_TRUE;
}
//empty frame, set decoded
PJ_DEF(void) decode_state_updateEmptyFrame( Frame_Buffer *frame, Decode_State *decode_state) {
    if(frame->frame_type == PJMEDIA_FRAME_TYPE_EMPTY && 
        decode_state_isContinuousFrame(frame, decode_state)) {
        decode_state_updateFrame(frame, decode_state);
    }
}


