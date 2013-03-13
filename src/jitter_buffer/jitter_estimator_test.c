#include <jitter_buffer/jitter_estimator.h>
#include <gtest/gtest.h>

TEST(JitterEstimator, EstimatorJitter) {
    //init
    Jitter_Estimator jitter_estimator;
    jitter_estimator_init(&jitter_estimator);
    //6 frames
    jitter_estimator_updateEstimate(50, 3000, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(30, 2600, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(20, 3000, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateRtt(50, &jitter_estimator);
    jitter_estimator_updateEstimate(10, 3000, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(10, 3500, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(20, 1500, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateRtt(100, &jitter_estimator);
    jitter_estimator_updateEstimate(0, 1500, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(5, 1500, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(5, 1500, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(5, 1500, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(10, 3000, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(10, 3000, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(10, 3000, PJ_TRUE, &jitter_estimator);
    jitter_estimator_updateEstimate(10, 3000, PJ_TRUE, &jitter_estimator);
    double jitter = jitter_estimator_getJitterEstimate(&jitter_estimator, 1.0);
    printf("jitter:%f\n", jitter);
    EXPECT_GT(jitter, 0);
    
}

int main(int argc, char**argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();    
}
