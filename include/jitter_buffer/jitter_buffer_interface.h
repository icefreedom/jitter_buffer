#ifndef __JITTER_BUFFER_INTERFACE_H__
#define __JITTER_BUFFER_INTERFACE_H__
#include <pj/types.h>
#include <pj/os.h>
#include <jitter_buffer/jitter_buffer.h>
#include <jitter_buffer/timing.h>
#include <pjmedia/frame.h>

PJ_BEGIN_DECL
typedef struct Jitter_Buffer_Interface {
    Jitter_Buffer *jitter_buffer;
    Timing timing;
    pj_mutex_t * jbi_mutex;
} Jitter_Buffer_Interface;
//init
PJ_DECL(int) jitter_buffer_interface_init(Jitter_Buffer_Interface *jbi, pj_pool_t *pool,
    int max_number_of_frames, NACK_MODE nack_mode,
    int low_rtt_threshold_ms, int high_rtt_threshold_ms) ;
//insert a packet into jitter buffer
PJ_DECL(int) jitter_buffer_interface_insert_packet(Jitter_Buffer_Interface *jbi,
       pj_uint16_t seq, pj_uint32_t ts, pjmedia_frame_type frame_type,
        char *payload, int size, pj_bool_t isMarketPacket) ;
//get a frame
PJ_DECL(int) jitter_buffer_interface_getFrameForDecoding(Jitter_Buffer_Interface *jbi, 
                                Frame_Buffer **ret_frame, pjmedia_frame * frames, 
                                const int max_cnt, pjmedia_frame_type frame_type) ;
//change frame status
PJ_DECL(int) jitter_buffer_interface_frameDecoded(Jitter_Buffer_Interface *jbi, 
                                                Frame_Buffer *frame, pj_timestamp *decode_start_ts) ;
PJ_DECL(void) jitter_buffer_interface_stop(Jitter_Buffer_Interface *jbi);
PJ_DECL(void) jitter_buffer_interface_reset(Jitter_Buffer_Interface *jbi);
PJ_DECL(int) jitter_buffer_interface_createNackList(Jitter_Buffer_Interface *jbi,
            pj_uint32_t rtt_ms, pj_uint16_t **nack_seq_list,
            pj_uint32_t *nack_seq_num);
PJ_END_DECL

#endif
