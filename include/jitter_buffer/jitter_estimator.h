#ifndef __JITTER_ESTIMATOR_H__
#define __JITTER_ESTIMATOR_H__
#include <pj/types.h>

PJ_BEGIN_DECL

//constants
/*const double PHI = 0.97;
const double PSI = 0.9999;
const pj_uint32_t ALPHA_COUNT_MAX = 400;
const double THETA_LOW = 0.000001;
const pj_uint32_t NACL_LIMIT = 3;
const pj_int32_t NUM_STD_DEV_DELAY_OUTLIER = 15;
const pj_int32_t NUM_STD_DEV_FRAME_SIZE_OUTLIER = 3;
const double NOISE_STD_DEVS = 2.33;
const double NOISE_STD_DEV_OFFSET = 30.0;*/
enum { kFsAccuStartupSamples = 5, kStartupDelaySamples = 30 };

typedef struct Jitter_Estimator {
    //constants
    double          _phi;
    double          _psi;
    pj_uint32_t  _alphaCountMax;
    double          _thetaLow;
    pj_uint32_t  _nackLimit;
    pj_int32_t   _numStdDevDelayOutlier;
    pj_int32_t   _numStdDevFrameSizeOutlier;
    double          _noiseStdDevs;
    double          _noiseStdDevOffset;
    //constants end

    double                _thetaCov[2][2]; // Estimate covariance
    double                _Qcov[2][2];     // Process noise covariance
    double                _avgFrameSize;   // Average frame size
    double                _varFrameSize;   // Frame size variance
    double                _maxFrameSize;   // Largest frame size received (descending
                                           // with a factor _psi)
    pj_uint32_t        _fsSum;
    pj_uint32_t        _fsCount;

    pj_ssize_t         _lastUpdateT;
    double                _prevEstimate;         // The previously returned jitter estimate
    pj_uint32_t        _prevFrameSize;        // Frame size of the previous frame
    double                _avgNoise;             // Average of the random jitter
    pj_uint32_t        _alphaCount;
    double                _filterJitterEstimate; // The filtered sum of jitter estimates
    
    pj_uint32_t        _startupCount;
        
    pj_ssize_t         _latestNackTimestamp;  // Timestamp in ms when the latest nack was seen
    pj_uint32_t        _nackCount;            // Keeps track of the number of nacks received,
                                                 // but never goes above _nackLimit
    pj_uint32_t        _rttMS;                  // Rtt in ms
    double              _theta[2]; // Estimated line parameters (slope, offset)
    double              _varNoise; // Variance of the time-deviation from the line
} Jitter_Estimator;
PJ_DECL(void) jitter_estimator_init(Jitter_Estimator *jitter_estimator) ;
PJ_DECL(void) jitter_estimator_reset(Jitter_Estimator *jitter_estimator) ;
PJ_DECL(void) jitter_estimator_updateEstimate(pj_ssize_t frameDelayMS, pj_uint32_t frameSizeBytes,
                                            pj_bool_t incompleteFrame, Jitter_Estimator *jitter_estimator);
PJ_DECL(double) jitter_estimator_getJitterEstimate(Jitter_Estimator *jitter_estimator, double rtt_mult);
PJ_DECL(void) jitter_estimator_updateRtt(pj_uint32_t rttMS, Jitter_Estimator *jitter_estimator);
PJ_DECL(void) jitter_estimator_frameNacked(Jitter_Estimator *jitter_estimator);
PJ_DECL(void) jitter_estimator_resetNackCount(Jitter_Estimator *jitter_estimator);
PJ_END_DECL

#endif
