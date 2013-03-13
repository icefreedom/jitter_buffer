#ifndef __JITTER_BUFFER_DECODE_STATE_H__
#define __JITTER_BUFFER_DECODE_STATE_H__
#include <pj/types.h>
#include <pjmedia/frame.h>
#include <jitter_buffer/frame_buffer.h>
#include <jitter_buffer/packet.h>

PJ_BEGIN_DECL

typedef struct Decode_State {
    pj_uint16_t seq; //highest seq in last decoded frame
    pj_uint32_t ts;  //ts in last decoded frame
    pj_bool_t isKeyFrame; //is it key frame
    pjmedia_frame_type frame_type; //frame type
    pj_bool_t inited; //is it inited
} Decode_State;


PJ_DECL(pj_bool_t) decode_state_isOldPacket(JTPacket *packet, Decode_State 
 *decode_state) ;
PJ_DECL(pj_bool_t) decode_state_isOldFrame(Frame_Buffer *frame_buffer, Decode_State *decode_state);
PJ_DECL(pj_bool_t) decode_state_isContinuousFrame(Frame_Buffer *frame_buffer, Decode_State *decode_state) ;
PJ_DECL(void) decode_state_init(Decode_State *decode_state);
PJ_DECL(void) decode_state_reset(Decode_State *decode_state);
//packet belong to last decoded frame
PJ_DECL(void) decode_state_updateOldPacket( JTPacket *packet, Decode_State *decode_state);
//empty frame, set decoded
PJ_DECL(void) decode_state_updateEmptyFrame( Frame_Buffer *frame, Decode_State *decode_state);
//media frame, update state
PJ_DECL(void) decode_state_updateFrame(Frame_Buffer *frame, Decode_State *decode_state);
            
PJ_END_DECL

#endif
