#ifndef __JITTER_BUFFER_H__
#define __JITTER_BUFFER_H__
#include <pj/types.h>
#include <jitter_buffer/packet.h>
#include <jitter_buffer/frame_buffer.h>
#include <jitter_buffer/decode_state.h>
#include <jitter_buffer/jitter_estimator.h>
#include <jitter_buffer/inter_frame_delay.h>
#include <utility/list_alloc.h>
#include <utility/event.h>

PJ_BEGIN_DECL
#define MAX_NACK_SEQ_NUM    128
typedef enum NACK_MODE {
    NACK_MODE_NO,
    NACK_MODE_HARD,
    NACK_MODE_HYBRID
} NACK_MODE;
typedef struct Waiting_For_Completed_Frame {
    pj_int32_t frame_size;
    pj_uint32_t timestamp;
    pj_ssize_t latest_packet_timestamp;
}Waiting_For_Completed_Frame;

typedef struct Jitter_Buffer {
    frame_alloc_t frame_alloc; //frame manager
    packet_alloc_t packet_alloc; //packet mamager
    Frame_Buffer frameList; //frame list;
    int max_number_of_frames; 
    int number_of_frames; //actual number of frames
    Frame_Buffer decodingFrameList; //status of these frames is Decoding
    Decode_State decode_state; //recode decode state
    Jitter_Estimator jitter_estimator; //estimate jitter
    Inter_Frame_Delay inter_frame_delay;  
    pj_uint32_t rttMs; //rtt in ms
    //nack and retransmission
    NACK_MODE nack_mode;
    int low_rtt_threshold_ms;
    int high_rtt_threshold_ms;
    pj_int32_t nack_seq_internal[MAX_NACK_SEQ_NUM];
    pj_uint16_t nack_seq[MAX_NACK_SEQ_NUM];
    pj_uint32_t nack_seq_num;
    pj_bool_t waiting_for_key_frame;
    //lock
    pj_mutex_t *jb_mutex;
    //event
    pj_thread_event_t *frame_event;
    pj_thread_event_t *packet_event;
    //is running
    pj_bool_t running;
    //first packet in jitter buffer
    pj_bool_t first_packet;
    Waiting_For_Completed_Frame waiting_for_completed_frame;
} Jitter_Buffer;
//create jitter buffer
PJ_DECL(int) jitter_buffer_create(Jitter_Buffer ** jitter_buffer, pj_pool_t *pool,
    int max_number_of_frames, NACK_MODE nack_mode, 
    int low_rtt_threshold_ms, int high_rtt_threshold_ms) ;
//reset jitter buffer
PJ_DECL(void) jitter_buffer_reset(Jitter_Buffer *jitter_buffer) ;
PJ_DECL(void) jitter_buffer_flush(Jitter_Buffer *jitter_buffer);
//start jitter buffer
PJ_DECL(void) jitter_buffer_start(Jitter_Buffer *jitter_buffer) ;
//estimate jitter
PJ_DECL(pj_uint32_t) jitter_buffer_estimateJitterMS(Jitter_Buffer *jitter_buffer) ;
//get next timestamp and reander time
PJ_DECL(pj_ssize_t) jitter_buffer_nextTimestamp(Jitter_Buffer *jitter_buffer, pj_uint32_t max_wait_time_ms, 
                                    pjmedia_frame_type *frame_type, pj_ssize_t *next_render_time) ;
//get a complete frame
PJ_DECL(Frame_Buffer *) jitter_buffer_getCompleteFrameForDecoding(Jitter_Buffer *jitter_buffer, pj_uint32_t max_wait_time_ms) ;
//get an incomplete frame
PJ_DECL(Frame_Buffer*) jitter_buffer_getFrameForDecoding(Jitter_Buffer *jitter_buffer) ;
/**
  * insert a packet into jitter buffer
  * @isFirstPacket first packet in a frame
  * @isMarketPacket last packet in a frame
  */
PJ_DECL(int) jitter_buffer_insert_packet(Jitter_Buffer *jitter_buffer, 
pj_uint16_t seq, pj_uint32_t ts, pjmedia_frame_type frame_type, 
char *payload, int size, 
pj_bool_t isFirstPacket, pj_bool_t isMarketPacket) ;
PJ_DECL(void) jitter_buffer_setRenderTime(Jitter_Buffer *jitter_buffer, pj_uint32_t ts,
                                        pj_ssize_t render_time_ms);
PJ_DECL(void) jitter_buffer_changeFrameStatus(Jitter_Buffer *jitter_buffer, Frame_Buffer *frame, FrameStatus frame_status);
PJ_DECL(void) jitter_buffer_stop(Jitter_Buffer *jitter_buffer) ;
/**
  * create nack sequence number list
  * @param nack_seq_list out parameter
  */
PJ_DECL(int) jitter_buffer_createNackList(Jitter_Buffer *jitter_buffer, 
            pj_uint32_t rtt_ms, pj_uint16_t **nack_seq_list, 
            pj_uint32_t *nack_seq_num) ;
PJ_END_DECL

#endif
