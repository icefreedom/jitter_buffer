#include <jitter_buffer/inter_frame_delay.h>
#include "gtest/gtest.h"
using ::testing::InitGoogleTest;
TEST(Inter_Frame_Delay_Test, FirstFrame) {
    //init
    Inter_Frame_Delay inter_frame_delay;
    inter_frame_delay_init(&inter_frame_delay);
    pj_ssize_t delay = -1;
    EXPECT_EQ(PJ_TRUE, inter_frame_delay_calculateDelay(&delay, 
            1000, 1050, &inter_frame_delay));
    EXPECT_EQ(0, delay);
}

TEST(Inter_Frame_Delay_Test, ManyFrames) {
    //init
    Inter_Frame_Delay inter_frame_delay;
    inter_frame_delay_init(&inter_frame_delay);

    //normal
    pj_ssize_t delay = -1;
    EXPECT_EQ(PJ_TRUE, inter_frame_delay_calculateDelay(&delay,
            1000, 1050, &inter_frame_delay));
    EXPECT_EQ(0, delay);
    EXPECT_EQ(PJ_TRUE, inter_frame_delay_calculateDelay(&delay,
            1100, 1170, &inter_frame_delay));
    EXPECT_EQ(20, delay);
    //frame inorder
    EXPECT_EQ(PJ_FALSE, inter_frame_delay_calculateDelay(&delay,
            500, 1250, &inter_frame_delay));
    EXPECT_EQ(0, delay);
    //backward wrap
    EXPECT_EQ(PJ_FALSE, inter_frame_delay_calculateDelay(&delay,
            (pj_uint32_t)0xFFFFFFFF - 200, ((pj_ssize_t)(1) << 32) - 101, &inter_frame_delay));
    EXPECT_EQ(0, delay);
    //forward wrap
    inter_frame_delay_reset(&inter_frame_delay);
    inter_frame_delay_calculateDelay(&delay,
            (pj_uint32_t)0xFFFFFFFF - 200, ((pj_ssize_t)(1) << 32) - 100, &inter_frame_delay);
    EXPECT_EQ(PJ_TRUE, inter_frame_delay_calculateDelay(&delay,
            1000, ((pj_ssize_t)(1) << 32) + 1251, &inter_frame_delay));
    EXPECT_EQ(150, delay);
}

int main(int argc, char**argv) {
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
