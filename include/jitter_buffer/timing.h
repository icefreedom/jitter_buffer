#ifndef __JITTER_BUFFER_TIMING_H__
#define __JITTER_BUFFER_TIMING_H__

#include <pj/types.h>
#include <jitter_buffer/timestamp_extrapolator.h>
#include <jitter_buffer/decode_timer.h>
#include <pj/os.h>
#include <pj/pool.h>

PJ_BEGIN_DECL
enum { kDefaultRenderDelayMs = 10, kDelayMaxChangeMsPerS = 100 };
typedef struct Timing {
	//estimate the local timestamp
	Timestamp_Extrapolator timestamp_extrapolator;
	//calculate max decoded time
	Decode_Timer decode_timer;
	pj_uint32_t                _renderDelayMs;
    pj_uint32_t                _minTotalDelayMs;
    pj_uint32_t                _requiredDelayMs;//jitter buffer delay
    pj_uint32_t                _currentDelayMs;
    pj_uint32_t                _prevFrameTimestamp;
    pj_mutex_t                  *timing_mutex;
}Timing;

PJ_DECL(int) timing_init(Timing *timing, pj_ssize_t nowMs, pj_pool_t *pool);
PJ_DECL(void) timing_reset(Timing *timing) ;
PJ_DECL(void) timing_setRequiredDelay(Timing *timing, 
                            pj_uint32_t requiredDelayMs) ;
PJ_DECL(void) timing_updateCurrentDelay(Timing *timing, pj_uint32_t timestamp) ;
//update decoded time
PJ_DECL(void) timing_stopDecodeTimer(Timing *timing, pj_timestamp *startTime, pj_timestamp *nowTime) ;
PJ_DECL(void) timing_incomingTimestamp(Timing *timing, pj_uint32_t timestamp, pj_ssize_t nowMs) ;
//get the render starting timestamp
PJ_DECL(pj_ssize_t) timing_getRenderTimeMs(Timing *timing, pj_uint32_t timestamp, pj_ssize_t nowMs) ;
//waiting a completed frame in jitter buffer
PJ_DECL(pj_uint32_t) timing_getMaxWaitingTimeMs(Timing *timing, pj_ssize_t renderTimeMs, pj_ssize_t nowMs) ;
PJ_DECL(pj_uint32_t) timing_getTargetDelay(Timing *timing);
PJ_END_DECL

#endif

