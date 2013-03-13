#include <jitter_buffer/decode_timer.h>
PJ_DEF(void) decode_timer_init(Decode_Timer *decode_timer) {
    decode_timer_reset(decode_timer);
}

PJ_DEF(void) decode_timer_reset(Decode_Timer *decode_timer) {
    decode_timer->_filteredMax = 0;
    decode_timer->_firstDecodeTime = PJ_TRUE;
    decode_timer->_shortMax = 0;
    int i ;
    for(i = 0; i < MAX_HISTORY_SIZE; ++i) {
        decode_timer->_history[i].shortMax = 0;
        decode_timer->_history[i].timeMs = -1;
    }
}

static void UpdateHistory(Decode_Timer *decode_timer, pj_int32_t decodeTime, 
                pj_ssize_t timeMs) {
    ShortMaxSample * history = decode_timer->_history;
    if(history[0].timeMs != -1 && (timeMs - history[0].timeMs) < SHORT_FILTER_MS) {
        if(decodeTime > decode_timer->_shortMax) {
            decode_timer->_shortMax = decodeTime;
        }
        return;
    } else {
        //shift one to right
        int i;
        for(i = MAX_HISTORY_SIZE - 2; i >= 0; --i) {
            history[i+1].shortMax = history[i].shortMax;
            history[i+1].timeMs = history[i].timeMs;
        }
        if(decode_timer->_shortMax == 0)
            decode_timer->_shortMax = decodeTime;
        
    }
    history[0].shortMax = decode_timer->_shortMax;
    history[0].timeMs = timeMs;
    decode_timer->_shortMax = 0;
}

static void PostHistory(Decode_Timer *decode_timer) {
    decode_timer->_filteredMax = decode_timer->_shortMax;
    ShortMaxSample * history = decode_timer->_history;
    //find the max decode time
    int i;
    for(i = 0; i < MAX_HISTORY_SIZE && history[i].timeMs != -1; ++i) {
        if(history[i].shortMax > decode_timer->_filteredMax)
            decode_timer->_filteredMax = history[i].shortMax;
    }
}

static void MaxFilter(Decode_Timer *decode_timer, pj_int32_t decodeTime, pj_ssize_t timeMs) {
    if(decode_timer->_firstDecodeTime) {
        decode_timer->_firstDecodeTime = PJ_FALSE; //ignore first frame
    } else {
        UpdateHistory(decode_timer, decodeTime, timeMs);
        PostHistory(decode_timer);
    }
}

PJ_DEF(void) decode_timer_stopTimer(Decode_Timer *decode_timer, pj_timestamp *startTime, pj_timestamp *nowTime) {
    pj_timestamp current;
    pj_get_timestamp(&current);
    pj_int32_t diffMs = (pj_int32_t)pj_elapsed_msec(startTime, &current);
    MaxFilter(decode_timer, diffMs, nowTime->u64 / 1000);
}

PJ_DEF(pj_int32_t) decode_timer_getMaxDecodeTime(Decode_Timer *decode_timer) {
    return decode_timer->_filteredMax;
}


