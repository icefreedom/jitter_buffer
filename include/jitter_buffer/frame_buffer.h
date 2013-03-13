#ifndef __FRAME_BUFFER_H__
#define __FRAME_BUFFER_H__
#include <pj/types.h>
#include <jitter_buffer/session_info.h>
#include <utility/list_alloc.h>
#include <pjmedia/frame.h>
#include <pj/os.h>
#include <pj/list.h>
PJ_BEGIN_DECL

typedef enum FrameStatus {
    FRAME_STATUS_FREE, //no packets
    FRAME_STATUS_EMPTY,  //has empty packets
    FRAME_STATUS_COMPLETE,  //a complete frame
    FRAME_STATUS_INCOMPLETE, //incomplete frame
    FRAME_STATUS_DECODING   //decoding this frame, can't modify
} FrameStatus;

typedef struct Frame_Buffer {
    PJ_DECL_LIST_MEMBER(struct Frame_Buffer);
    FrameStatus frame_status;
    Session_Info session_info; //include packets list, isComplete, decodable 
    pj_bool_t counted; //is this frame counted by jitter buffer
    pj_timestamp latest_packet_timestamp;
    pj_uint32_t ts; //frame timestamp, in rtp
    pjmedia_frame_type frame_type;
    pj_ssize_t renderTimeMs; //the time to render
    pj_int32_t frame_size; //frame length
    pj_int32_t nack_count;
}Frame_Buffer;

typedef list_alloc(Frame_Buffer) frame_alloc_t;
//prepare packet_alloc for session_info, not a good way
PJ_DECL(int) frame_buffer_init(Frame_Buffer *frame_buffer, packet_alloc_t *packet_alloc);
//rest frame_buffer status to free and free all packets
PJ_DECL(int) frame_buffer_reset(Frame_Buffer *frame_buffer);
//if status transform illegally,  this function cause program stop
PJ_DECL(void) frame_buffer_setStatus(Frame_Buffer *frame_buffer, FrameStatus  frame_status);
//insert a packet into frame
PJ_DECL(int) frame_buffer_insert_packet(Frame_Buffer *frame_buffer, JTPacket *packet);
//count of packets in this frame, exclude empty packets
PJ_DECL(int) frame_buffer_get_packets_count(Frame_Buffer *frame_buffer);

//get the begining sequence number
PJ_DECL(int) frame_buffer_getFirstSeq(Frame_Buffer *frame_buffer);
//get the end sequence number
PJ_DECL(int) frame_buffer_getHighSeq(Frame_Buffer *frame_buffer);

PJ_DECL(void) frame_buffer_IncrementNackCount(Frame_Buffer *frame_buffer) ;
PJ_DECL(int) frame_buffer_buildSoftNackList(Frame_Buffer *frame_buffer, 
            int *seq_num_list, int seq_num_list_len, pj_uint32_t rtt_ms) ;
PJ_DECL(int) frame_buffer_buildHardNackList(Frame_Buffer* frame_buffer, 
            int *seq_num_list, int seq_num_list_len) ;

PJ_END_DECL
#endif
