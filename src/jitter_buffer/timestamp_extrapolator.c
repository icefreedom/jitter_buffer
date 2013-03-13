#include <jitter_buffer/timestamp_extrapolator.h>
#include <jitter_buffer/jitter_utility.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <math.h>
#define VCM_MAX(a, b)   (a) > (b) ? (a) : (b)
#define VCM_MIN(a, b)   (a) < (b) ? (a) : (b)
PJ_DEF(void) timestamp_extrapolator_init(Timestamp_Extrapolator * timestamp_extrapolator, 
            pj_ssize_t nowMs) {
    timestamp_extrapolator->_startMs = 0;
    timestamp_extrapolator->_firstTimestamp = 0;
    timestamp_extrapolator->_wrapArounds = 0;
    timestamp_extrapolator->_prevTs90khz = 0;
    timestamp_extrapolator->_lambda = 1;
    timestamp_extrapolator->_firstAfterReset = PJ_TRUE;
    timestamp_extrapolator->_packetCount = 0;
    timestamp_extrapolator->_startUpFilterDelayInPackets = 2;
    timestamp_extrapolator->_detectorAccumulatorPos = 0;
    timestamp_extrapolator->_detectorAccumulatorNeg = 0;
    timestamp_extrapolator->_alarmThreshold = 60e3;
    timestamp_extrapolator->_accDrift = 6600; // in timestamp ticks, i.e. 15 ms
    timestamp_extrapolator->_accMaxError = 7000;
    timestamp_extrapolator->_P11 = 1e10;
    timestamp_extrapolator_reset(timestamp_extrapolator, nowMs);
}


PJ_DEF(void) timestamp_extrapolator_reset(Timestamp_Extrapolator * timestamp_extrapolator, 
            pj_ssize_t nowMs) {
    pj_assert(nowMs > -1);
    timestamp_extrapolator->_startMs = nowMs;
    timestamp_extrapolator->_prevMs = timestamp_extrapolator->_startMs;
    timestamp_extrapolator->_firstTimestamp = 0;
    timestamp_extrapolator->_w[0] = 1.0;
    timestamp_extrapolator->_w[1] = 0;
    timestamp_extrapolator->_P[0][0] = 1;
    timestamp_extrapolator->_P[1][1] = timestamp_extrapolator->_P11;
    timestamp_extrapolator->_P[0][1] = timestamp_extrapolator->_P[1][0] = 0;
    timestamp_extrapolator->_firstAfterReset = PJ_TRUE;
    timestamp_extrapolator->_prevTs90khz = 0;
    timestamp_extrapolator->_wrapArounds = 0;
    timestamp_extrapolator->_packetCount = 0;
    timestamp_extrapolator->_detectorAccumulatorPos = 0;
    timestamp_extrapolator->_detectorAccumulatorNeg = 0;
}

// Investigates if the timestamp clock has overflowed since the last timestamp and
// keeps track of the number of wrap arounds since reset.
void static CheckForWrapArounds(Timestamp_Extrapolator * timestamp_extrapolator, pj_uint32_t ts90khz)
{
    if (timestamp_extrapolator->_prevTs90khz == 0)
    {
        timestamp_extrapolator->_prevTs90khz = ts90khz;
        return;
    }
    if (ts90khz < timestamp_extrapolator->_prevTs90khz)
    {
        // This difference will probably be less than -2^31 if we have had a wrap around
        // (e.g. timestamp = 1, timestamp_extrapolator->_previousTimestamp = 2^32 - 1). Since it is casted to a Word32,
        // it should be positive.
        if ((pj_int32_t)(ts90khz - timestamp_extrapolator->_prevTs90khz) > 0)
        {
            // Forward wrap around
            timestamp_extrapolator->_wrapArounds++;
        }
    }
    // This difference will probably be less than -2^31 if we have had a backward wrap around.
    // Since it is casted to a Word32, it should be positive.
    else if ((pj_int32_t)(timestamp_extrapolator->_prevTs90khz - ts90khz) > 0)
    {
        // Backward wrap around
        timestamp_extrapolator->_wrapArounds--;
    }
    timestamp_extrapolator->_prevTs90khz = ts90khz;
}

static pj_bool_t DelayChangeDetection(Timestamp_Extrapolator * timestamp_extrapolator, double error, pj_bool_t trace)
{
    // CUSUM detection of sudden delay changes
    error = (error > 0) ? VCM_MIN(error, timestamp_extrapolator->_accMaxError) : VCM_MAX(error, -timestamp_extrapolator->_accMaxError);
    timestamp_extrapolator->_detectorAccumulatorPos = VCM_MAX(timestamp_extrapolator->_detectorAccumulatorPos + error - timestamp_extrapolator->_accDrift, (double)0);
    timestamp_extrapolator->_detectorAccumulatorNeg = VCM_MIN(timestamp_extrapolator->_detectorAccumulatorNeg + error + timestamp_extrapolator->_accDrift, (double)0);
    if (timestamp_extrapolator->_detectorAccumulatorPos > timestamp_extrapolator->_alarmThreshold || timestamp_extrapolator->_detectorAccumulatorNeg < -timestamp_extrapolator->_alarmThreshold)
    {
        timestamp_extrapolator->_detectorAccumulatorPos = timestamp_extrapolator->_detectorAccumulatorNeg = 0;
        return PJ_TRUE;
    }
    return PJ_FALSE;
}
PJ_DEF(void) timestamp_extrapolator_update(Timestamp_Extrapolator * timestamp_extrapolator, 
                                        pj_ssize_t tMs, pj_uint32_t ts90khz, pj_bool_t trace)
{

    if (tMs - timestamp_extrapolator->_prevMs > 10e3)
    {
        // Ten seconds without a complete frame.
        // Reset the extrapolator
        timestamp_extrapolator_reset(timestamp_extrapolator, GetCurrentTimeMs());
    }
    else
    {
        timestamp_extrapolator->_prevMs = tMs;
    }

    // Remove offset to prevent badly scaled matrices
    tMs -= timestamp_extrapolator->_startMs;

    pj_int32_t prevWrapArounds = timestamp_extrapolator->_wrapArounds;
    CheckForWrapArounds(timestamp_extrapolator, ts90khz);
    pj_int32_t wrapAroundsSincePrev = timestamp_extrapolator->_wrapArounds - prevWrapArounds;

    if (wrapAroundsSincePrev == 0 && ts90khz < timestamp_extrapolator->_prevTs90khz)
    {
        return;
    }

    if (timestamp_extrapolator->_firstAfterReset)
    {
        // Make an initial guess of the offset,
        // should be almost correct since tMs - timestamp_extrapolator->_startMs
        // should about zero at this time.
        timestamp_extrapolator->_w[1] = -timestamp_extrapolator->_w[0] * tMs;
        timestamp_extrapolator->_firstTimestamp = ts90khz;
        timestamp_extrapolator->_firstAfterReset = PJ_FALSE;
    }

    // Compensate for wraparounds by changing the line offset
    timestamp_extrapolator->_w[1] = timestamp_extrapolator->_w[1] - wrapAroundsSincePrev * ((pj_uint32_t)(0xFFFFFFFF) );

    double residual = ((double)(ts90khz) - timestamp_extrapolator->_firstTimestamp) - (double)(tMs) * timestamp_extrapolator->_w[0] - timestamp_extrapolator->_w[1];
    if (DelayChangeDetection(timestamp_extrapolator, residual, trace) &&
        timestamp_extrapolator->_packetCount >= timestamp_extrapolator->_startUpFilterDelayInPackets)
    {
        // A sudden change of average network delay has been detected.
        // Force the filter to adjust its offset parameter by changing
        // the offset uncertainty. Don't do this during startup.
        timestamp_extrapolator->_P[1][1] = timestamp_extrapolator->_P11;
    }
    //T = [t(k) 1]';
    //that = T'*w;
    //K = P*T/(lambda + T'*P*T);
    double K[2];
    K[0] = timestamp_extrapolator->_P[0][0] * tMs + timestamp_extrapolator->_P[0][1];
    K[1] = timestamp_extrapolator->_P[1][0] * tMs + timestamp_extrapolator->_P[1][1];
    double TPT = timestamp_extrapolator->_lambda + tMs * K[0] + K[1];
    K[0] /= TPT;
    K[1] /= TPT;
    //w = w + K*(ts(k) - that);
    timestamp_extrapolator->_w[0] = timestamp_extrapolator->_w[0] + K[0] * residual;
    timestamp_extrapolator->_w[1] = timestamp_extrapolator->_w[1] + K[1] * residual;
    //P = 1/lambda*(P - K*T'*P);
    double p00 = 1 / timestamp_extrapolator->_lambda * (timestamp_extrapolator->_P[0][0] - (K[0] * tMs * timestamp_extrapolator->_P[0][0] + K[0] * timestamp_extrapolator->_P[1][0]));
    double p01 = 1 / timestamp_extrapolator->_lambda * (timestamp_extrapolator->_P[0][1] - (K[0] * tMs * timestamp_extrapolator->_P[0][1] + K[0] * timestamp_extrapolator->_P[1][1]));
    timestamp_extrapolator->_P[1][0] = 1 / timestamp_extrapolator->_lambda * (timestamp_extrapolator->_P[1][0] - (K[1] * tMs * timestamp_extrapolator->_P[0][0] + K[1] * timestamp_extrapolator->_P[1][0]));
    timestamp_extrapolator->_P[1][1] = 1 / timestamp_extrapolator->_lambda * (timestamp_extrapolator->_P[1][1] - (K[1] * tMs * timestamp_extrapolator->_P[0][1] + K[1] * timestamp_extrapolator->_P[1][1]));
    timestamp_extrapolator->_P[0][0] = p00;
    timestamp_extrapolator->_P[0][1] = p01;
    if (timestamp_extrapolator->_packetCount < timestamp_extrapolator->_startUpFilterDelayInPackets)
    {
        timestamp_extrapolator->_packetCount++;
    }
}

PJ_DEF(pj_ssize_t) timestamp_extrapolator_extrapolateLocalTime(Timestamp_Extrapolator * timestamp_extrapolator, 
                pj_uint32_t timestamp90khz)
{
    pj_ssize_t localTimeMs = 0;
    if (timestamp_extrapolator->_packetCount == 0)
    {
        localTimeMs = -1;
    }
    else if (timestamp_extrapolator->_packetCount < timestamp_extrapolator->_startUpFilterDelayInPackets)
    {
        localTimeMs = timestamp_extrapolator->_prevMs + (pj_ssize_t)((double)(timestamp90khz - timestamp_extrapolator->_prevTs90khz)  + 0.5);
    }
    else
    {
        if (timestamp_extrapolator->_w[0] < 1e-3)
        {
            localTimeMs = timestamp_extrapolator->_startMs;
        }
        else
        {
            double timestampDiff = (double)(timestamp90khz) - (double)(timestamp_extrapolator->_firstTimestamp);
            localTimeMs = (pj_ssize_t)((double)(timestamp_extrapolator->_startMs) + (timestampDiff - timestamp_extrapolator->_w[1]) / timestamp_extrapolator->_w[0] + 0.5);
        }
    }
    return localTimeMs;
}
PJ_DEF(pj_uint32_t) timestamp_extrapolator_extrapolateTimestamp(Timestamp_Extrapolator * timestamp_extrapolator, 
                                                                pj_ssize_t tMs)
{
    pj_uint32_t timestamp = 0;
    if (timestamp_extrapolator->_packetCount == 0)
    {
        timestamp = 0;
    }
    else if (timestamp_extrapolator->_packetCount < timestamp_extrapolator->_startUpFilterDelayInPackets)
    {
        timestamp = (pj_uint32_t)(tMs - timestamp_extrapolator->_prevMs + timestamp_extrapolator->_prevTs90khz + 0.5);
    }
    else
    {
        timestamp = (pj_uint32_t)(timestamp_extrapolator->_w[0] * (tMs - timestamp_extrapolator->_startMs) + timestamp_extrapolator->_w[1] + timestamp_extrapolator->_firstTimestamp + 0.5);
    }
    return timestamp;
}
