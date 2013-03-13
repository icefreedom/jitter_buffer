#include <jitter_buffer/jitter_estimator.h>
#include <pj/assert.h>
#include <math.h>
#define VCM_MAX(a, b)  (a) >= (b)? (a):(b)
PJ_DEF(void) jitter_estimator_init(Jitter_Estimator *jitter_estimator) {
    //init constants
    jitter_estimator->_phi = 0.97;
    jitter_estimator->_psi = 0.9999;
    jitter_estimator->_alphaCountMax = 400;
    jitter_estimator->_thetaLow = 0.000001;
    jitter_estimator->_nackLimit = 3;
    jitter_estimator->_numStdDevDelayOutlier = 15;
    jitter_estimator->_numStdDevFrameSizeOutlier = 3;
    jitter_estimator->_noiseStdDevs = 2.33;
    jitter_estimator->_noiseStdDevOffset = 30.0;
    jitter_estimator_reset(jitter_estimator);
}

PJ_DEF(void) jitter_estimator_reset(Jitter_Estimator *jitter_estimator) {
    jitter_estimator->_theta[0] = 1/(512e3/8);
    jitter_estimator->_theta[1] = 0;
    jitter_estimator->_varNoise = 4.0;

    jitter_estimator->_thetaCov[0][0] = 1e-4;
    jitter_estimator->_thetaCov[1][1] = 1e2;
    jitter_estimator->_thetaCov[0][1] = jitter_estimator->_thetaCov[1][0] = 0;
    jitter_estimator->_Qcov[0][0] = 2.5e-10;
    jitter_estimator->_Qcov[1][1] = 1e-10;
    jitter_estimator->_Qcov[0][1] = jitter_estimator->_Qcov[1][0] = 0;
    jitter_estimator->_avgFrameSize = 500;
    jitter_estimator->_maxFrameSize = 500;
    jitter_estimator->_varFrameSize = 100;
    jitter_estimator->_lastUpdateT = -1;
    jitter_estimator->_prevEstimate = -1.0;
    jitter_estimator->_prevFrameSize = 0;
    jitter_estimator->_avgNoise = 0.0;
    jitter_estimator->_alphaCount = 1;
    jitter_estimator->_filterJitterEstimate = 0.0;
    jitter_estimator->_latestNackTimestamp = 0;
    jitter_estimator->_nackCount = 0;
    jitter_estimator->_fsSum = 0;
    jitter_estimator->_fsCount = 0;
    jitter_estimator->_startupCount = 0;
    jitter_estimator->_rttMS = 0;
}

static double
 NoiseThreshold(Jitter_Estimator *jitter_estimator) 
{
    double noiseThreshold = jitter_estimator->_noiseStdDevs * sqrt(jitter_estimator->_varNoise) - 
                            jitter_estimator->_noiseStdDevOffset;
    if (noiseThreshold < 1.0)
    {
        noiseThreshold = 1.0;
    }
    return noiseThreshold;
}

// Calculates the current jitter estimate from the filtered estimates
static double
 CalculateEstimate(Jitter_Estimator *jitter_estimator)
{
    double ret = jitter_estimator->_theta[0] * (jitter_estimator->_maxFrameSize - jitter_estimator->_avgFrameSize) + 
                NoiseThreshold(jitter_estimator);

    // A very low estimate (or negative) is neglected
    if (ret < 1.0) {
        if (jitter_estimator->_prevEstimate <= 0.01)
        {
            ret = 1.0;
        }
        else
        {
            ret = jitter_estimator->_prevEstimate;
        }
    }
    if (ret > 10000.0) // Sanity
    {
        ret = 10000.0;
    }
    jitter_estimator->_prevEstimate = ret;
    return ret;
}


/*static void
 UpdateMaxFrameSize(pj_uint32_t frameSizeBytes, double &_maxFrameSize)
{
    if (_maxFrameSize < frameSizeBytes)
    {
        _maxFrameSize = frameSizeBytes;
    }
}*/
static void
 PostProcessEstimate(Jitter_Estimator *jitter_estimator)
{
    jitter_estimator->_filterJitterEstimate = CalculateEstimate(jitter_estimator);
}
// Calculate difference in delay between a sample and the
// expected delay estimated by the Kalman filter
static double
 DeviationFromExpectedDelay(pj_ssize_t frameDelayMS, 
                            pj_int32_t deltaFSBytes, double * _theta)
{
    return frameDelayMS - (_theta[0] * deltaFSBytes + _theta[1]);
}

// Estimates the random jitter by calculating the variance of the
// sample distance from the line given by theta.
static void  EstimateRandomJitter(double d_dT, pj_bool_t incompleteFrame, Jitter_Estimator *jitter_estimator)
{
    double alpha;
    pj_assert(jitter_estimator->_alphaCount > 0);
    alpha = (double)(jitter_estimator->_alphaCount - 1) / (double)(jitter_estimator->_alphaCount);
    jitter_estimator->_alphaCount++;
    if (jitter_estimator->_alphaCount > jitter_estimator->_alphaCountMax)
    {
        jitter_estimator->_alphaCount = jitter_estimator->_alphaCountMax;
    }
    double avgNoise = alpha * jitter_estimator->_avgNoise + (1 - alpha) * d_dT;
    double varNoise = alpha * jitter_estimator->_varNoise +
                      (1 - alpha) * (d_dT - jitter_estimator->_avgNoise) * (d_dT - jitter_estimator->_avgNoise);
    if (!incompleteFrame || varNoise > jitter_estimator->_varNoise)
    {
        jitter_estimator->_avgNoise = avgNoise;
        jitter_estimator->_varNoise = varNoise;
    }
    if (jitter_estimator->_varNoise < 1.0)
    {
        // The variance should never be zero, since we might get
        // stuck and consider all samples as outliers.
        jitter_estimator->_varNoise = 1.0;
    }
}

// Updates Kalman estimate of the channel
// The caller is expected to sanity check the inputs.
static void KalmanEstimateChannel(pj_ssize_t frameDelayMS, pj_int32_t deltaFSBytes, 
                                    Jitter_Estimator *jitter_estimator) {
    double Mh[2];
    double hMh_sigma;
    double kalmanGain[2];
    double measureRes;
    double t00, t01;

    // Kalman filtering

    // Prediction
    // M = M + Q
    jitter_estimator->_thetaCov[0][0] += jitter_estimator->_Qcov[0][0];
    jitter_estimator->_thetaCov[0][1] += jitter_estimator->_Qcov[0][1];
    jitter_estimator->_thetaCov[1][0] += jitter_estimator->_Qcov[1][0];
    jitter_estimator->_thetaCov[1][1] += jitter_estimator->_Qcov[1][1];

    // Kalman gain
    // K = M*h'/(sigma2n + h*M*h') = M*h'/(1 + h*M*h')
    // h = [dFS 1]
    // Mh = M*h'
    // hMh_sigma = h*M*h' + R
    Mh[0] = jitter_estimator->_thetaCov[0][0] * deltaFSBytes + jitter_estimator->_thetaCov[0][1];
    Mh[1] = jitter_estimator->_thetaCov[1][0] * deltaFSBytes + jitter_estimator->_thetaCov[1][1];
    // sigma weights measurements with a small deltaFS as noisy and
    // measurements with large deltaFS as good
    if (jitter_estimator->_maxFrameSize < 1.0)
    {
        return;
    }
    double sigma = (300.0 * exp(-fabs((double)(deltaFSBytes)) /
                   (1e0 * jitter_estimator->_maxFrameSize)) + 1) * sqrt(jitter_estimator->_varNoise);
    if (sigma < 1.0)
    {
        sigma = 1.0;
    }
    hMh_sigma = deltaFSBytes * Mh[0] + Mh[1] + sigma;
    if ((hMh_sigma < 1e-9 && hMh_sigma >= 0) || (hMh_sigma > -1e-9 && hMh_sigma <= 0))
    {
        pj_assert(PJ_FALSE);
        return;
    }
    kalmanGain[0] = Mh[0] / hMh_sigma;
    kalmanGain[1] = Mh[1] / hMh_sigma;

    // Correction
    // theta = theta + K*(dT - h*theta)
    measureRes = frameDelayMS - (deltaFSBytes * jitter_estimator->_theta[0] + jitter_estimator->_theta[1]);
    jitter_estimator->_theta[0] += kalmanGain[0] * measureRes;
    jitter_estimator->_theta[1] += kalmanGain[1] * measureRes;

    if (jitter_estimator->_theta[0] < jitter_estimator->_thetaLow)
    {
        jitter_estimator->_theta[0] = jitter_estimator->_thetaLow;
    }

    // M = (I - K*h)*M
    t00 = jitter_estimator->_thetaCov[0][0];
    t01 = jitter_estimator->_thetaCov[0][1];
    jitter_estimator->_thetaCov[0][0] = (1 - kalmanGain[0] * deltaFSBytes) * t00 -
                      kalmanGain[0] * jitter_estimator->_thetaCov[1][0];
    jitter_estimator->_thetaCov[0][1] = (1 - kalmanGain[0] * deltaFSBytes) * t01 -
                      kalmanGain[0] * jitter_estimator->_thetaCov[1][1];
    jitter_estimator->_thetaCov[1][0] = jitter_estimator->_thetaCov[1][0] * (1 - kalmanGain[1]) -
                      kalmanGain[1] * deltaFSBytes * t00;
    jitter_estimator->_thetaCov[1][1] = jitter_estimator->_thetaCov[1][1] * (1 - kalmanGain[1]) -
                      kalmanGain[1] * deltaFSBytes * t01;

    // Covariance matrix, must be positive semi-definite
    pj_assert(jitter_estimator->_thetaCov[0][0] + jitter_estimator->_thetaCov[1][1] >= 0 &&
           jitter_estimator->_thetaCov[0][0] * jitter_estimator->_thetaCov[1][1] - jitter_estimator->_thetaCov[0][1] * jitter_estimator->_thetaCov[1][0] >= 0 &&
           jitter_estimator->_thetaCov[0][0] >= 0);
}
// Updates the estimates with the new measurements
PJ_DEF(void) jitter_estimator_updateEstimate(pj_ssize_t frameDelayMS, pj_uint32_t frameSizeBytes,
                                            pj_bool_t incompleteFrame, Jitter_Estimator *jitter_estimator)
{
    if (frameSizeBytes == 0)
    {
        return;
    }
    int deltaFS = frameSizeBytes - jitter_estimator->_prevFrameSize;
    if (jitter_estimator->_fsCount < kFsAccuStartupSamples)
    {
        jitter_estimator->_fsSum += frameSizeBytes;
        jitter_estimator->_fsCount++;
    }
    else if (jitter_estimator->_fsCount == kFsAccuStartupSamples)
    {
        // Give the frame size filter
        jitter_estimator->_avgFrameSize = (double)(jitter_estimator->_fsSum) /
                        (double)(jitter_estimator->_fsCount);
        jitter_estimator->_fsCount++;
    }
    if (!incompleteFrame || frameSizeBytes > jitter_estimator->_avgFrameSize)
    {
        double avgFrameSize = jitter_estimator->_phi * jitter_estimator->_avgFrameSize +
                              (1 - jitter_estimator->_phi) * frameSizeBytes;
        if (frameSizeBytes < jitter_estimator->_avgFrameSize + 2 * sqrt(jitter_estimator->_varFrameSize))
        {
            // Only update the average frame size if this sample wasn't a
            // key frame
            jitter_estimator->_avgFrameSize = avgFrameSize;
        }
        // Update the variance anyway since we want to capture cases where we only get
        // key frames.
        jitter_estimator->_varFrameSize = VCM_MAX(jitter_estimator->_phi * jitter_estimator->_varFrameSize + (1 - jitter_estimator->_phi) *
                                (frameSizeBytes - avgFrameSize) *
                                (frameSizeBytes - avgFrameSize), 1.0);
    }

    // Update max frameSize estimate
    jitter_estimator->_maxFrameSize = VCM_MAX(jitter_estimator->_psi * jitter_estimator->_maxFrameSize, (double)(frameSizeBytes));

    if (jitter_estimator->_prevFrameSize == 0)
    {
        jitter_estimator->_prevFrameSize = frameSizeBytes;
        return;
    }
    jitter_estimator->_prevFrameSize = frameSizeBytes;

    // Only update the Kalman filter if the sample is not considered
    // an extreme outlier. Even if it is an extreme outlier from a
    // delay point of view, if the frame size also is large the
    // deviation is probably due to an incorrect line slope.
    double deviation = DeviationFromExpectedDelay(frameDelayMS, deltaFS, jitter_estimator->_theta);

    if (fabs(deviation) < jitter_estimator->_numStdDevDelayOutlier * sqrt(jitter_estimator->_varNoise) ||
        frameSizeBytes > jitter_estimator->_avgFrameSize + jitter_estimator->_numStdDevFrameSizeOutlier * sqrt(jitter_estimator->_varFrameSize))
    {
        // Update the variance of the deviation from the
        // line given by the Kalman filter
        EstimateRandomJitter(deviation, incompleteFrame, jitter_estimator);
        // Prevent updating with frames which have been congested by a large
        // frame, and therefore arrives almost at the same time as that frame.
        // This can occur when we receive a large frame (key frame) which
        // has been delayed. The next frame is of normal size (delta frame),
        // and thus deltaFS will be << 0. This removes all frame samples
        // which arrives after a key frame.
        if ((!incompleteFrame || deviation >= 0.0) &&
            (double)(deltaFS) > - 0.25 * jitter_estimator->_maxFrameSize)
        {
            // Update the Kalman filter with the new data
            KalmanEstimateChannel(frameDelayMS, deltaFS, jitter_estimator);
        }
    }
    else
    {
        int nStdDev = (deviation >= 0) ? jitter_estimator->_numStdDevDelayOutlier : -jitter_estimator->_numStdDevDelayOutlier;
        EstimateRandomJitter(nStdDev * sqrt(jitter_estimator->_varNoise), incompleteFrame, jitter_estimator);
    }
    // Post process the total estimated jitter
    if (jitter_estimator->_startupCount >= kStartupDelaySamples)
    {
        PostProcessEstimate(jitter_estimator);
    }
    else
    {
        jitter_estimator->_startupCount++;
    }
}




PJ_DEF(void) jitter_estimator_updateRtt(pj_uint32_t rttMS, Jitter_Estimator *jitter_estimator)
{
    jitter_estimator->_rttMS = rttMS;
}


// Returns the current filtered estimate if available,
// otherwise tries to calculate an estimate.
PJ_DEF(double)
 jitter_estimator_getJitterEstimate(Jitter_Estimator *jitter_estimator, double rtt_mult)
{
    double jitterMS = CalculateEstimate(jitter_estimator);
    if (jitter_estimator->_filterJitterEstimate > jitterMS)
    {
        jitterMS = jitter_estimator->_filterJitterEstimate;
    }
    if (jitter_estimator->_nackCount >= jitter_estimator->_nackLimit)
    {
        return jitterMS +  jitter_estimator->_rttMS * rtt_mult;
    }
    return jitterMS;
}

// Updates the nack/packet ratio
PJ_DEF(void) jitter_estimator_frameNacked(Jitter_Estimator *jitter_estimator)
{
    // Wait until _nackLimit retransmissions has been received,
    // then always add ~1 RTT delay.
    // TODO(holmer): Should we ever remove the additional delay if the
    // the packet losses seem to have stopped? We could for instance scale
    // the number of RTTs to add with the amount of retransmissions in a given
    // time interval, or similar.
    if (jitter_estimator->_nackCount < jitter_estimator->_nackLimit)
    {
       jitter_estimator->_nackCount++;
    }
}

PJ_DEF(void) jitter_estimator_resetNackCount(Jitter_Estimator *jitter_estimator)
{
    jitter_estimator->_nackCount = 0;
}
