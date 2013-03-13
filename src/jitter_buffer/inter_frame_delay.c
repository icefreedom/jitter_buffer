#include <jitter_buffer/inter_frame_delay.h>
#include <pj/assert.h>
#include <stdio.h>

static void UpdateWrap(pj_int32_t *wrapAround, pj_uint32_t timestamp, 
                            pj_uint32_t prev_timestamp) {
    if(timestamp < prev_timestamp) {
        //wrap forward
        if((pj_int32_t) (timestamp - prev_timestamp) > 0)
            *wrapAround += 1;
    } else if((pj_int32_t) (prev_timestamp - timestamp ) > 0) {
        //wrap backward
        *wrapAround -= 1;
    }
}

PJ_DEF(void) inter_frame_delay_init(Inter_Frame_Delay *inter_frame_delay) {
    inter_frame_delay->prev_frame_timestamp = 0;
    inter_frame_delay->prev_wall_clock = 0;
    inter_frame_delay->wrapAround = 0;
    inter_frame_delay->dT = 0;
}

PJ_DEF(void) inter_frame_delay_reset(Inter_Frame_Delay *inter_frame_delay) {
    inter_frame_delay_init(inter_frame_delay);
}

PJ_DEF(pj_bool_t) inter_frame_delay_calculateDelay(pj_ssize_t *delay, 
                    pj_uint32_t timestamp, pj_ssize_t currentWallClock,
                    Inter_Frame_Delay *inter_frame_delay) {
    pj_assert(inter_frame_delay != NULL && delay != NULL) ;
    if(inter_frame_delay->prev_frame_timestamp == 0 &&
        inter_frame_delay->dT == 0) {
        //at begining
        inter_frame_delay->prev_frame_timestamp = timestamp;
        inter_frame_delay->prev_wall_clock = currentWallClock;
        *delay = 0;
        return PJ_TRUE;
    }
    int prev_wrapAround = inter_frame_delay->wrapAround;
    UpdateWrap(&inter_frame_delay->wrapAround, timestamp, 
                inter_frame_delay->prev_frame_timestamp);

    // it is -1, 0 , or 1
    // is this essential? why not use current 'wrapAround'?
    int wrapAroundSincePrev = inter_frame_delay->wrapAround - prev_wrapAround;
    //frame out of order, ignore
    if((wrapAroundSincePrev == 0 && timestamp < inter_frame_delay->prev_frame_timestamp) 
        || wrapAroundSincePrev < 0) {
        *delay = 0;
        return PJ_FALSE;
    }
    //calculate 
    if(wrapAroundSincePrev < 0) {
        inter_frame_delay->dT = -(((pj_uint32_t)0xFFFFFFFF) - timestamp + 1 + inter_frame_delay->prev_frame_timestamp);
    } else if(wrapAroundSincePrev > 0)
        inter_frame_delay->dT = ((pj_uint32_t)0xFFFFFFFF) - inter_frame_delay->prev_frame_timestamp + 1 + timestamp;
    *delay = currentWallClock - inter_frame_delay->prev_wall_clock - inter_frame_delay->dT;
    
    //save status
    inter_frame_delay->prev_frame_timestamp = timestamp;
    inter_frame_delay->prev_wall_clock = currentWallClock;

    return PJ_TRUE;
}


