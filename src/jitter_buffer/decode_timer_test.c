#include <jitter_buffer/decode_timer.h>
#include <stdio.h>
#define EXPECT_EQ(a, b)  if((a) != (b) ) { printf("not equal, "#a" = "#b"\n"); } else {  \
                            printf("success, "#a" = "#b"\n");}
#define EXPECT_GT(a, b)  if((a) <= (b) ) { printf("not greate than, "#a" <= "#b"\n"); } else {  \
                            printf("success, "#a" > "#b"\n");}
void test_decode_timer() {
    //init
    Decode_Timer decode_timer;
    decode_timer_init(&decode_timer);
    //frame 1
    pj_timestamp ts;
    pj_get_timestamp(&ts);
    pj_timestamp startTs = ts;
    pj_sub_timestamp32(&startTs, 100 * 1000);
    decode_timer_stopTimer(&decode_timer, &startTs, &ts);
    EXPECT_EQ(decode_timer_getMaxDecodeTime(&decode_timer), 0);
    pj_get_timestamp(&startTs);
    pj_get_timestamp(&ts);
    pj_add_timestamp32(&ts, 100 * 1000);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    pj_get_timestamp(&startTs);
    pj_get_timestamp(&ts);
    pj_sub_timestamp32(&startTs, 400 * 1000);
    pj_add_timestamp32(&ts, 200 * 1000);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    pj_get_timestamp(&startTs);
    pj_get_timestamp(&ts);
    pj_sub_timestamp32(&startTs, 50 * 1000);
    pj_add_timestamp32(&ts, 130 * 1000);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    pj_get_timestamp(&startTs);
    pj_get_timestamp(&ts);
    pj_add_timestamp32(&ts, 1300 * 1000);
    decode_timer_stopTimer(&decode_timer,  &startTs, &ts);
    pj_int32_t max_decode_time = decode_timer_getMaxDecodeTime(&decode_timer);
    EXPECT_EQ(max_decode_time, 400);

    
}

int main(int argc, char **argv) {
    test_decode_timer();
    return 0;
}
