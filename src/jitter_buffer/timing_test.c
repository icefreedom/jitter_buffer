#include <jitter_buffer/timing.h>
#include <jitter_buffer/jitter_utility.h>
#include <stdio.h>

void testTiming() {
    //init
    Timing timing;
    timing_init(&timing, GetCurrentTimeMs());
    //frames
    pj_uint32_t base_timestamp = (pj_uint32_t)GetCurrentTimeMs();
    pj_ssize_t base_nowMs = GetCurrentTimeMs();
   
    //frame 1
    pj_uint32_t timestamp = base_timestamp - 8000;
    pj_ssize_t nowMs = base_nowMs - 7800;
    timing_incomingTimestamp(&timing, timestamp, nowMs);
    timing_updateCurrentDelay(&timing, timestamp);
    //decode
    pj_timestamp startTime, completedTime, baseTime;
    pj_get_timestamp(&baseTime);
    startTime = baseTime;
    completedTime = baseTime;
    pj_sub_timestamp32(&completedTime, 7760);
    pj_sub_timestamp32(&startTime, 7800);
    timing_stopDecodeTimer(&timing, &startTime, &completedTime); //40 ms
    //jitter
    timing_setRequiredDelay(&timing, 20); //20 ms

    //frame 2
    timestamp = base_timestamp - 7500;
    nowMs = base_nowMs - 7300;
    timing_incomingTimestamp(&timing, timestamp, nowMs);
    timing_updateCurrentDelay(&timing, timestamp);
    //decode
    startTime = baseTime;
    completedTime = baseTime;
    pj_sub_timestamp32(&completedTime, 7370);
    pj_sub_timestamp32(&startTime, 7350);
    timing_stopDecodeTimer(&timing, &startTime, &completedTime); //40 ms
    //jitter
    timing_setRequiredDelay(&timing, 30); //20 ms
    //output
    pj_ssize_t renderTime = timing_getRenderTimeMs(&timing, timestamp, nowMs);
    pj_uint32_t max_wait_time_ms = timing_getMaxWaitingTimeMs(&timing, renderTime, nowMs);
    printf("nowMs: %ld, renderTime:%ld, max_wait_time_ms:%u, target delay:%u\n", nowMs, renderTime, max_wait_time_ms, timing_getTargetDelay(&timing));
}

int main() {
    testTiming();
    return 0;
}
