#ifndef __SESSION_INFO_H__
#define __SESSION_INFO_H__
#include <pj/types.h>
#include <jitter_buffer/packet.h>
#include <utility/list_alloc.h>
#include <pjmedia/frame.h>
#include <pj/os.h> //timestamp
PJ_BEGIN_DECL
typedef struct Session_Info {
    pj_bool_t session_nacked; //has nacked or not
    pj_bool_t isComplete; //is it a completed frame
    pj_bool_t isDecodable; //incompleted frame may be decodable
    pj_bool_t previous_frame_loss;
    pj_bool_t isKeyFrame; //key frame
    pjmedia_frame_type frame_type;
    JTPacket packetList; //packet list
    int packets_count; //number of media packets, not include empty packets
    int empty_low_seq_num; /*empty packets from 'empty_low_seq_num' to
                            * 'empty_high_seq_num', continuous */
    int empty_high_seq_num;
    pj_uint16_t packets_not_decodable; /** the number of packets that can't be decoded */
    packet_alloc_t *packet_alloc; /** alloc and free packets, this should be point 
                                          * to the same one in jitter buffer */
    pj_timestamp oldest_media_packet; /** timestamp that the first media packet was received */
}Session_Info;

PJ_DECL(int) session_info_init(Session_Info *session_info, packet_alloc_t *packet_alloc) ;
PJ_DECL(int) session_info_reset(Session_Info *session_info);
PJ_DECL(void) session_info_set_previous_frame_loss(Session_Info *session_info, 
                                            pj_bool_t previous_frame_loss);
/**
  * insert a packet into session info 
  */
PJ_DECL(int) session_info_insert_packet(Session_Info *session_info, JTPacket *packet);

/**
  * retransmit in hard mode 
  */
PJ_DECL(int) session_info_buildHardNackList(Session_Info *session_info, int *seq_num_list,
                                            int seq_num_list_len);
/**
  * retransmit in hybrid mode
  */
PJ_DECL(int) session_info_buildSoftNackList(Session_Info *session_info, int *seq_num_list,
                                            int seq_num_list_len, pj_uint32_t rtt_ms);
/**
  * make this session decodable, delete packet with incomplete nal 
  */
PJ_DECL(int) session_info_makeDecodable(Session_Info *session_info);



PJ_END_DECL
#endif
