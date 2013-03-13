#include <jitter_buffer/jitter_utility.h>
#include <pj/os.h>

PJ_DEF(pj_uint16_t) LatestSeqNum(pj_uint16_t first, pj_uint16_t second) {
    pj_bool_t wrap = (first < 0x00ff && second > 0xff00) ||
                    (first > 0xff00 && second < 0x00ff);
    if(first < second && !wrap)
        return second;
    else if(first >= second && !wrap)
        return first;
    else if(first < second && wrap)
        return first;
    else
        return second;
}
//check if 'second' is the continuous number of 'first'
PJ_DEF(pj_bool_t) InSequence(pj_uint16_t first, pj_uint16_t second) {
    if(first == 0xffff && second == 0)
        return PJ_TRUE;
    if(first + 1 == second)
        return PJ_TRUE;
    return PJ_FALSE;
}

//get current time in ms
PJ_DEF(pj_ssize_t) GetCurrentTimeMs() {
    pj_timestamp now_ts, zeroTs;
    pj_set_timestamp32(&zeroTs, 0, 0);
    pj_get_timestamp(&now_ts);
    return (pj_ssize_t)pj_elapsed_msec64(&zeroTs, &now_ts);
}
//transform timestamp to ms
PJ_DEF(pj_ssize_t) TimestampToMs(pj_timestamp *ts) {
    pj_timestamp zeroTs;
    pj_set_timestamp32(&zeroTs, 0, 0);
    return (pj_ssize_t)pj_elapsed_msec64(&zeroTs, ts);
}

