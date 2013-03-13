#ifndef __INTER_FRAME_DELAY_H__
#define __INTER_FRAME_DELAY_H__
#include <pj/types.h>
PJ_BEGIN_DECL

typedef struct Inter_Frame_Delay {
    pj_uint32_t prev_frame_timestamp; //timestamp in rtp
    pj_ssize_t prev_wall_clock; //timestamp when the frame arrived
    pj_int32_t wrapAround; //wrap of timestamp
    pj_ssize_t dT;
} Inter_Frame_Delay;

PJ_DECL(void) inter_frame_delay_init(Inter_Frame_Delay *inter_frame_delay);
PJ_DECL(void) inter_frame_delay_reset(Inter_Frame_Delay *inter_frame_delay);
//calculate frame delay
PJ_DECL(pj_bool_t) inter_frame_delay_calculateDelay(pj_ssize_t *delay, 
                    pj_uint32_t timestamp, pj_ssize_t currentWallClock,
                    Inter_Frame_Delay *inter_frame_delay);
PJ_END_DECL

#endif
