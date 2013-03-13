#ifndef __JITTER_BUFFER_PACKET_H__
#define __JITTER_BUFFER_PACKET_H__
#include <pj/types.h>
#include <pj/list.h>
#include <pjmedia/frame.h>
#include <utility/list_alloc.h>

PJ_BEGIN_DECL
#define MAX_PAYLOAD_SIZE 2000
typedef enum NAL_TYPE {
    NAL_TYPE_COMPLETE,
    NAL_TYPE_START,
    NAL_TYPE_INCOMPLETE,
    NAL_TYPE_END
    } NAL_TYPE;

typedef struct JTPacket{
    PJ_DECL_LIST_MEMBER(struct JTPacket);
    //attributes from rtp header
    pj_uint16_t seq;
    pj_uint32_t ts;
    pjmedia_frame_type frame_type;
    //payload
    char data_ptr[MAX_PAYLOAD_SIZE];
    int size;
    //retransmit
    pj_bool_t isRetrans; //is this a retrans-packet
    pj_bool_t isFirst; //first packet in a frame
    pj_bool_t isMarket; //end packet in a frame
    NAL_TYPE    nal_type; //h264 packetizer
    char time_in_jb[60]; //timestamp when the packet be put into the jitter buffer ,for test
}JTPacket;
typedef list_alloc(JTPacket) packet_alloc_t;
PJ_DECL(int) jt_packet_create(JTPacket **packet, packet_alloc_t *packet_alloc, pj_uint16_t seq,
                        pj_uint32_t ts, pjmedia_frame_type frame_type, pj_bool_t isFirst,
                        pj_bool_t isMarket, const char *data_ptr, int size);
PJ_END_DECL

#endif
