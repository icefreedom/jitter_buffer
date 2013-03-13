#ifndef __DECODE_TIMER_H__
#define __DECODE_TIMER_H__
#include <pj/types.h>
#include <pj/os.h>

PJ_BEGIN_DECL
#define MAX_HISTORY_SIZE 20
#define SHORT_FILTER_MS 1000
typedef struct ShortMaxSample {
    pj_int32_t shortMax;
    pj_ssize_t timeMs;
} ShortMaxSample;
typedef struct Decode_Timer {
    pj_int32_t                     _filteredMax;
    pj_int32_t                     _shortMax; //max decoded time in 1 second
    pj_bool_t                      _firstDecodeTime;
    ShortMaxSample                 _history[MAX_HISTORY_SIZE];

} Decode_Timer;
/**
  * it should be called each time when decoded a frame
  * @param startTime deocode start time
  * @param nowTime  it is time that this is called
  */
PJ_DECL(void) decode_timer_stopTimer(Decode_Timer *decode_timer, pj_timestamp *startTime, pj_timestamp *nowTime) ;

/**
  * return the max decode time
  */
PJ_DECL(pj_int32_t) decode_timer_getMaxDecodeTime(Decode_Timer *decode_Timer) ;

PJ_DECL(void) decode_timer_init(Decode_Timer *decode_timer) ;
PJ_DECL(void) decode_timer_reset(Decode_Timer *decode_timer) ;
PJ_END_DECL

#endif
