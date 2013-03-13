#ifndef __NAL_TYPE_H__
#define __NAL_TYPE_H__
#include <pj/types.h>
#include <jitter_buffer/packet.h>
PJ_BEGIN_DECL
PJ_DECL(NAL_TYPE) distinguish_packet_nal_type(JTPacket *packet);
PJ_DECL(NAL_TYPE) distinguish_nal_type(const char *data_ptr, int size);
PJ_END_DECL

#endif
