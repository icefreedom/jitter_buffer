#ifndef __JITTER_UTILITY_H__
#define __JITTER_UTILITY_H__
#include <pj/types.h>
PJ_BEGIN_DECL

PJ_DECL(pj_uint16_t) LatestSeqNum(pj_uint16_t first, pj_uint16_t second);
PJ_DECL(pj_bool_t) InSequence(pj_uint16_t first, pj_uint16_t second);
PJ_DECL(pj_ssize_t) GetCurrentTimeMs() ;
PJ_DECL(pj_ssize_t) TimestampToMs(pj_timestamp *ts) ;

PJ_END_DECL

#endif
