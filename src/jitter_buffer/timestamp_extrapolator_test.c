#include <jitter_buffer/timestamp_extrapolator.h>
#include <pj/os.h>
#include <stdio.h>
void test_timestamp_extrapolator() {
    //init
    Timestamp_Extrapolator timestamp_extrapolator;
    pj_timestamp current;
    pj_get_timestamp(&current);
    timestamp_extrapolator_init(&timestamp_extrapolator, current.u64);
    //incoming 3 frames
    timestamp_extrapolator_update(&timestamp_extrapolator, (pj_ssize_t)current.u64 / 1000 , (pj_uint32_t)current.u64 /1000 - 900, PJ_FALSE);
    timestamp_extrapolator_update(&timestamp_extrapolator, (pj_ssize_t)current.u64 / 1000 + 100, (pj_uint32_t)current.u64 /1000 - 799, PJ_FALSE);
    timestamp_extrapolator_update(&timestamp_extrapolator, (pj_ssize_t)current.u64 / 1000 + 500, (pj_uint32_t)current.u64 /1000 - 402, PJ_FALSE);
    pj_ssize_t diff = (pj_ssize_t)current.u64 / 1000 + 700 -
            timestamp_extrapolator_extrapolateLocalTime(&timestamp_extrapolator,
            (pj_uint32_t)current.u64 / 1000 - 200);
    if(diff > 5 || diff < -5)
        printf("failed\n");
    else
        printf("success\n");
}

int main() {
    test_timestamp_extrapolator();
    return 0;
}
