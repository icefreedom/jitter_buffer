#include <jitter_buffer/decode_timer.h>
#include "gtest/gtest.h"

TEST(DECODETIMERTEST, MAXDECODTIME) {
    //init
    Decode_Timer decode_timer;
    decode_timer_init(&decode_timer);
    //frame 1
    pj_timestamp ts;
    pj_get_timestamp(&ts);
    pj_timestamp startTs = ts;
    pj_sub_timestamp32(&startTs, 100);
    decode_timer_stopTimer(&decode_timer, &startTs, &ts);
    EXPECT_EQ(decode_timer_getMaxDecodeTime(&decode_timer), 0);
    pj_add_timestamp32(&ts, 100);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    pj_sub_timestamp32(&startTs, 120);
    pj_add_timestamp32(&ts, 120);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    pj_sub_timestamp32(&startTs, 50);
    pj_add_timestamp32(&ts, 130);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    EXPECT_GT(decode_timer_getMaxDecodeTime(&decode_timer), 100);

    
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
