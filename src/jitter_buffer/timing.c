#include <jitter_buffer/timing.h>
#include <jitter_buffer/jitter_utility.h>
#include <pj/log.h>

#define THIS_FILE   "timing.c"

PJ_DEF(int) timing_init(Timing *timing, pj_ssize_t nowMs, pj_pool_t *pool) {
    timestamp_extrapolator_init(&timing->timestamp_extrapolator, nowMs);
    decode_timer_init(&timing->decode_timer);
    timing->_renderDelayMs = kDefaultRenderDelayMs;
    timing->_minTotalDelayMs = 0;
    timing->_requiredDelayMs = 0;
    timing->_currentDelayMs = 0;
    timing->_prevFrameTimestamp = 0;
    if(pj_mutex_create_simple(pool, NULL, &timing->timing_mutex) != PJ_SUCCESS)
        return -1;
    return 0;
}

PJ_DEF(void) timing_reset(Timing *timing) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return;
    timestamp_extrapolator_reset(&timing->timestamp_extrapolator, GetCurrentTimeMs());
    decode_timer_reset(&timing->decode_timer);
    timing->_renderDelayMs = kDefaultRenderDelayMs;
    timing->_minTotalDelayMs = 0;
    timing->_requiredDelayMs = 0;
    timing->_currentDelayMs = 0;
    timing->_prevFrameTimestamp = 0;
    pj_mutex_unlock(timing->timing_mutex);
}

PJ_DEF(void) timing_setRequiredDelay(Timing *timing, 
                            pj_uint32_t requiredDelayMs) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return;
    timing->_requiredDelayMs = requiredDelayMs;
    pj_mutex_unlock(timing->timing_mutex);
}

static pj_uint32_t MaxDecodeTimeMs(Decode_Timer *decode_timer) {
    pj_int32_t decodeTime = decode_timer_getMaxDecodeTime(decode_timer);
    if(decodeTime < 0) 
        return 0;
    return (pj_uint32_t)decodeTime;
}

static pj_uint32_t TargetDelayInternal(Timing *timing) {
    return timing->_requiredDelayMs + MaxDecodeTimeMs(&timing->decode_timer) + timing->_renderDelayMs;
}

PJ_DEF(pj_uint32_t) timing_getTargetDelay(Timing *timing) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return;
    pj_uint32_t ret = TargetDelayInternal(timing);
    pj_mutex_unlock(timing->timing_mutex);
    return ret;
}

PJ_DEF(void) timing_updateCurrentDelay(Timing *timing, pj_uint32_t timestamp) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return;
    //get target delay
    pj_uint32_t targetDelay = TargetDelayInternal(timing);
    if(targetDelay < timing->_minTotalDelayMs)
        targetDelay = timing->_minTotalDelayMs;
    //if current delay not inited 
    if(timing->_currentDelayMs == 0)
        timing->_currentDelayMs = targetDelay;
    else {
    //update delay
        pj_ssize_t diffMs = (pj_ssize_t)(targetDelay) - timing->_currentDelayMs;
        pj_ssize_t maxChange = 0;
        if(timing->_prevFrameTimestamp > 0xFFFF0000 && timestamp < 0x0000FFFF) {
            //wrap
            maxChange = kDelayMaxChangeMsPerS * (timestamp + ((pj_uint32_t)0xFFFFFF - timing->_prevFrameTimestamp + 1) ) / 1000;
        } else if(timing->_prevFrameTimestamp < timestamp) {
            maxChange = kDelayMaxChangeMsPerS * (timestamp - timing->_prevFrameTimestamp) / 1000;
        } else { //inorder frame
            pj_mutex_unlock(timing->timing_mutex);
            return;
        }
        if(diffMs < -maxChange) 
            diffMs = -maxChange;
        else if(diffMs > maxChange)
            diffMs = maxChange;
        timing->_currentDelayMs += diffMs;
    }
    timing->_prevFrameTimestamp = timestamp;
    pj_mutex_unlock(timing->timing_mutex);
}
PJ_DEF(void) timing_stopDecodeTimer(Timing *timing, pj_timestamp *startTime, pj_timestamp *nowTime) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return;
    decode_timer_stopTimer(&timing->decode_timer, startTime, nowTime);
    pj_mutex_unlock(timing->timing_mutex);
}
PJ_DEF(void) timing_incomingTimestamp(Timing *timing, pj_uint32_t timestamp, pj_ssize_t nowMs) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return;
    timestamp_extrapolator_update(&timing->timestamp_extrapolator, nowMs, timestamp, PJ_FALSE);
    pj_mutex_unlock(timing->timing_mutex);
}

PJ_DEF(pj_ssize_t) timing_getRenderTimeMs(Timing *timing, pj_uint32_t timestamp, pj_ssize_t nowMs) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return 0;
    pj_ssize_t localTimestamp = timestamp_extrapolator_extrapolateLocalTime(&timing->timestamp_extrapolator, timestamp);
    if(localTimestamp <= 0)
        localTimestamp = nowMs;
    PJ_LOG(4, (THIS_FILE, "localtime:%ld", localTimestamp));
    PJ_LOG(4, (THIS_FILE, "currentDelay:%d, target delay:%d", timing->_currentDelayMs, TargetDelayInternal(timing)));
    
    pj_ssize_t ret = localTimestamp + timing->_currentDelayMs;
    pj_mutex_unlock(timing->timing_mutex);
    return ret;
}

PJ_DEF(pj_uint32_t) timing_getMaxWaitingTimeMs(Timing *timing, pj_ssize_t renderTimeMs, pj_ssize_t nowMs) {
    if(pj_mutex_lock(timing->timing_mutex) != 0)
        return 0;
    pj_ssize_t maxWaitingTime = renderTimeMs - nowMs - MaxDecodeTimeMs(&timing->decode_timer) - timing->_renderDelayMs;
    pj_mutex_unlock(timing->timing_mutex);
    if(maxWaitingTime < 0)
        return 0;
    return (pj_uint32_t) maxWaitingTime;
}   


