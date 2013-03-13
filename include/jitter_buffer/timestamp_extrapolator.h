#ifndef __TIMESTAMP_EXTRAPOLATOR_H__
#define __TIMESTAMP_EXTRAPOLATOR_H__
#include <pj/types.h>

typedef struct Timestamp_Extrapolator {
    double                _w[2];
    double                _P[2][2];
    pj_ssize_t         _startMs;
    pj_ssize_t         _prevMs;
    pj_uint32_t        _firstTimestamp;
    pj_int32_t         _wrapArounds;
    pj_uint32_t        _prevTs90khz;
    double          _lambda;
    pj_bool_t          _firstAfterReset;
    pj_uint32_t        _packetCount;
    pj_uint32_t  _startUpFilterDelayInPackets;

    double              _detectorAccumulatorPos;
    double              _detectorAccumulatorNeg;
    double        _alarmThreshold;
    double        _accDrift;
    double        _accMaxError;
    double        _P11;
} Timestamp_Extrapolator;

//init
PJ_DECL(void) timestamp_extrapolator_init(Timestamp_Extrapolator * timestamp_extrapolator, 
            pj_ssize_t nowMs) ; 

PJ_DECL(void) timestamp_extrapolator_reset(Timestamp_Extrapolator * timestamp_extrapolator, 
            pj_ssize_t nowMs) ;

//incoming a frame
PJ_DECL(void) timestamp_extrapolator_update(Timestamp_Extrapolator * timestamp_extrapolator, 
                                        pj_ssize_t tMs, pj_uint32_t ts90khz, pj_bool_t trace);

//get local timestamp according remote timestamp
PJ_DECL(pj_ssize_t) timestamp_extrapolator_extrapolateLocalTime(Timestamp_Extrapolator * timestamp_extrapolator, 
                pj_uint32_t timestamp90khz);

PJ_DECL(pj_uint32_t) timestamp_extrapolator_extrapolateTimestamp(Timestamp_Extrapolator * timestamp_extrapolator, 
                                                                pj_ssize_t tMs);
#endif
