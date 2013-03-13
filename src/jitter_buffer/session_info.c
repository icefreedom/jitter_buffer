#include <jitter_buffer/session_info.h>
#include <jitter_buffer/jitter_utility.h>
#include <pj/assert.h>
#include <pjmedia/key_frame.h>

#define SESSION_MAX_PACKETS     200
#define FRAME_MAX_DELAY 2000    /* ms */

PJ_DEF(int) session_info_init(Session_Info *session_info, packet_alloc_t *packet_alloc) {
    pj_assert(session_info != NULL && packet_alloc != NULL);
    session_info->session_nacked = PJ_FALSE;
    session_info->isComplete = PJ_FALSE;
    session_info->isDecodable = PJ_FALSE;
    session_info->previous_frame_loss = PJ_TRUE;
    session_info->frame_type = PJMEDIA_FRAME_TYPE_NONE;
    session_info->isKeyFrame = PJ_FALSE;
    //init list
    pj_list_init(&session_info->packetList);
    
    session_info->empty_low_seq_num = -1;
    session_info->empty_high_seq_num = -1;
    session_info->packets_not_decodable = 0;
    session_info->packets_count = 0;

    session_info->packet_alloc = packet_alloc;
    return 0;
}

PJ_DEF(int) session_info_reset(Session_Info *session_info) {
    //release all packets
    while(session_info->packetList.next != &session_info->packetList) {
        JTPacket * del_packet = session_info->packetList.next;
        pj_list_erase(del_packet);
        list_alloc_insert(del_packet, session_info->packet_alloc);
    }
    session_info->packets_count = 0;
    //reset the variables
    session_info->session_nacked = PJ_FALSE;
    session_info->isComplete = PJ_FALSE;
    session_info->isDecodable = PJ_FALSE;
    session_info->previous_frame_loss = PJ_TRUE;
    session_info->empty_low_seq_num = -1;
    session_info->empty_high_seq_num = -1;
    session_info->packets_not_decodable = 0;
    session_info->frame_type = PJMEDIA_FRAME_TYPE_NONE;
    session_info->isKeyFrame = PJ_FALSE;
    return 0;
}

PJ_DEF(void) session_info_set_previous_frame_loss(Session_Info *session_info, 
                                            pj_bool_t previous_frame_loss) {
        session_info->previous_frame_loss = previous_frame_loss;
}

static void InformEmptyPacket(Session_Info *session_info, JTPacket *packet) {
    pj_uint16_t seq = packet->seq;
    if(packet->frame_type != PJMEDIA_FRAME_TYPE_EMPTY) return;
    if(session_info->empty_low_seq_num == -1 || 
       LatestSeqNum((pj_uint16_t)session_info->empty_low_seq_num, seq) ==
       (pj_uint16_t)session_info->empty_low_seq_num) {
        session_info->empty_low_seq_num = seq;
    }
        
    if(session_info->empty_high_seq_num == -1 ||
        LatestSeqNum((pj_uint16_t)session_info->empty_high_seq_num, seq) ==
        seq)
        session_info->empty_high_seq_num = seq;
    //free packet
    list_alloc_insert(packet, session_info->packet_alloc);
    
}
static void UpdateComplete(Session_Info *session_info) {
    if(session_info->packetList.next == &session_info->packetList){ //empty
        session_info->isComplete = PJ_FALSE;
        return;
    }
    //no start or end
    if(session_info->packetList.next->isFirst != PJ_TRUE || 
        session_info->packetList.prev->isMarket != PJ_TRUE) {
        session_info->isComplete = PJ_FALSE;
        return;
    }
    //in sequence
    {
        JTPacket *prev = session_info->packetList.next;
        JTPacket *next = prev->next;
        while(next != &session_info->packetList) {
            if(!InSequence(prev->seq, next->seq)) {
                session_info->isComplete = PJ_FALSE;
                return;
            }
            prev = next;
            next = next->next;
        }
        session_info->isComplete = PJ_TRUE;
        session_info->isDecodable = PJ_TRUE;
    }
}
static int session_info_insert_packet_internal(Session_Info *session_info, JTPacket *packet) {
    if(packet->frame_type == PJMEDIA_FRAME_TYPE_NONE)
        return -1; //error
    //first media packet
    if((session_info->frame_type == PJMEDIA_FRAME_TYPE_NONE ||
        session_info->frame_type == PJMEDIA_FRAME_TYPE_EMPTY) &&
        packet->frame_type != PJMEDIA_FRAME_TYPE_EMPTY ) {
        pj_get_timestamp(&session_info->oldest_media_packet);
    }   
    //update frame type
    if(session_info->frame_type == PJMEDIA_FRAME_TYPE_NONE)
        session_info->frame_type = packet->frame_type;
    else if(session_info->frame_type == PJMEDIA_FRAME_TYPE_EMPTY
            && packet->frame_type != PJMEDIA_FRAME_TYPE_EMPTY) {
        session_info->frame_type = packet->frame_type;
    }
        
    //update empty seq numbers
    if(packet->frame_type == PJMEDIA_FRAME_TYPE_EMPTY) {
        InformEmptyPacket(session_info, packet);
        return 0;
    }
    if(session_info->packets_count >= SESSION_MAX_PACKETS) //too many
        return 3;
    
    //insert into packetList
    {
        JTPacket *packet_itr = session_info->packetList.next;
        //find packet with seq bigger than the input one
        while(packet_itr != &session_info->packetList) {
            //duplicate
            if(packet_itr->seq == packet->seq)
                return 4;
            if(LatestSeqNum(packet_itr->seq, packet->seq) == packet_itr->seq)
                break;
            packet_itr = packet_itr->next;
        }
        pj_list_insert_before(packet_itr, packet);
        //check if key frame
        if(!session_info->isKeyFrame && check_if_key_frame(packet->data_ptr, packet->size))
            session_info->isKeyFrame = PJ_TRUE;
    }
    //increase count
    session_info->packets_count += 1;
    //check if completed
    UpdateComplete(session_info);
    return 0;
}
PJ_DEF(int) session_info_insert_packet(Session_Info *session_info, JTPacket *packet) {
    
    //failed, free the packet
    int ret = 0;
    if((ret = session_info_insert_packet_internal(session_info, packet)) != 0) {
        list_alloc_insert(packet, session_info->packet_alloc);
        ret = ret < 0 ? ret : -1;
        return ret;
    }
    
    return 0;
}
static void ClearPackets(int *seq_num_list, int seq_num_list_len,
                        int seq_low, int seq_high, int value) {
    int i;
    if(seq_low != -1 && seq_high != -1) {
        for(i = 0; i < seq_num_list_len; ++i) { 
            if(seq_low == seq_num_list[i])
                break;
        }
        if(i == seq_num_list_len)
            return ;
        for(; i < seq_num_list_len && seq_num_list[i] != seq_high; ++i)
            seq_num_list[i] = value;
        if(seq_num_list[i] == seq_high)
            seq_num_list[i] = value;
    }
}
static void ClearEmptyPackets(Session_Info *session_info, int *seq_num_list,
                                            int seq_num_list_len) {
    int i;
    int empty_low_seq_num = session_info->empty_low_seq_num;
    int empty_high_seq_num = session_info->empty_high_seq_num;
    if(empty_low_seq_num != -1 && empty_high_seq_num != -1) {
        ClearPackets(seq_num_list, seq_num_list_len, empty_low_seq_num,
                        empty_high_seq_num, -2);
    }
}

PJ_DEF(int) session_info_buildHardNackList(Session_Info *session_info, int *seq_num_list,
                                            int seq_num_list_len) {
    if(seq_num_list_len == 0) return 0;
    //find the start position in seq_num_list
    int i;
    int empty_low_seq_num = session_info->empty_low_seq_num;
    int empty_high_seq_num = session_info->empty_high_seq_num;
    //set the value to -2 if the empty packet in seq_num_list
    //don't retransmit empty packets
    if(empty_low_seq_num != -1)
        ClearEmptyPackets(session_info, seq_num_list, seq_num_list_len);
        
    JTPacket *packet = session_info->packetList.next;
    if(packet == &session_info->packetList ) //empty
        return 0;
    for(i = 0; i < seq_num_list_len; ++i) {
        if(packet->seq == seq_num_list[i]) 
            break;
    }
    if(i == seq_num_list_len) //not found
        return 0;
    //set the value to -1 if the packet in seq_num_list
    for(; i < seq_num_list_len; ++i) {
       if(packet->seq == seq_num_list[i]) {
            printf("found seq:%d\n", packet->seq);
            seq_num_list[i] = -1;
            packet = packet->next;
            if(packet == &session_info->packetList)
                break;
       }
    }
    //nacked
    if(!session_info->isComplete)
        session_info->session_nacked = PJ_TRUE;
    return 0;
}

PJ_DEF(int) session_info_buildSoftNackList(Session_Info *session_info, int *seq_num_list,
                                            int seq_num_list_len, pj_uint32_t rtt_ms) {
    if(seq_num_list_len == 0) return 0;
    int empty_low_seq_num = session_info->empty_low_seq_num;
    int empty_high_seq_num = session_info->empty_high_seq_num;
    //clear empty packets
    if(empty_low_seq_num != -1)
        ClearEmptyPackets(session_info, seq_num_list, seq_num_list_len);
    JTPacket *packet = session_info->packetList.next;
    printf("check if has packets in session\n");
    if(packet == &session_info->packetList ) //empty
        return 0;
    int media_seq_num_low, media_seq_num_high;
    pj_bool_t session_nacked = PJ_TRUE;
    //if not key frame and previous frame lost, don't retransmit
    if(session_info->previous_frame_loss && !session_info->isKeyFrame) {
        session_nacked = PJ_FALSE;
    }
    //if (now + rtt) > (ts + FRAME_MAX_DELAY), don't retransmit
    //use ntp time, but ts in rtp is not ntp time in current, so substitute received time for ts
    //if (now + rtt * 1.5) > (received + FRAME_MAX_DELAY), don't retransmit
    if(session_info->frame_type != PJMEDIA_FRAME_TYPE_NONE && session_info->frame_type != 
            PJMEDIA_FRAME_TYPE_EMPTY) {
        //has media packets
        pj_timestamp now, received;
        pj_get_timestamp(&now);
        pj_add_timestamp32(&now, rtt_ms * 1000 * 3 / 2);
        memcpy(&received, &session_info->oldest_media_packet, sizeof(pj_timestamp));
        pj_add_timestamp32(&received, FRAME_MAX_DELAY * 1000);
        if(pj_cmp_timestamp(&now, &received) > 0) {
            session_nacked = PJ_FALSE;
        }     
    }
    if(!session_nacked) {
        media_seq_num_low = session_info->packetList.next->isFirst? session_info->packetList.next->seq:
                                    session_info->packetList.next->seq - 1;
        media_seq_num_high = session_info->packetList.prev->isMarket? session_info->packetList.prev->seq:
                                    session_info->packetList.prev->seq + 1;
        ClearPackets(seq_num_list, seq_num_list_len, media_seq_num_low, media_seq_num_high, -1);
        return 0;
    }
    
    return session_info_buildHardNackList(session_info, seq_num_list, seq_num_list_len);   
}
//delete [start, end)
static void DeletePackets(Session_Info *session_info , JTPacket *start, JTPacket *end) {
    JTPacket *packet = start, *del_packet;
    while(packet != end) {
        del_packet = packet;
        packet = packet->next;
        pj_list_erase(del_packet);
        list_alloc_insert(del_packet, session_info->packet_alloc);
        session_info->packets_count -= 1;
    }
}
PJ_DECL(int) session_info_makeDecodable(Session_Info *session_info) {
    if(session_info->frame_type == PJMEDIA_FRAME_TYPE_NONE || 
        session_info->frame_type == PJMEDIA_FRAME_TYPE_EMPTY)
        return 0;
    JTPacket *packet, *packet_prev;
    packet = session_info->packetList.next;
    while(packet != &session_info->packetList) {
        if(packet->nal_type != NAL_TYPE_COMPLETE) {
            JTPacket *packet_start = packet;
            packet_prev = packet;
            pj_bool_t bad_nal = PJ_FALSE;
            if(packet->nal_type != NAL_TYPE_START)
                bad_nal = PJ_TRUE;
            packet = packet->next;
            while(packet != &session_info->packetList) {
                if(packet->nal_type == NAL_TYPE_START ||
                    packet->nal_type == NAL_TYPE_COMPLETE) //next nal
                    break;
                if(!bad_nal && !InSequence(packet_prev->seq, packet->seq))
                    bad_nal = PJ_TRUE;
                packet_prev = packet;
                packet = packet->next;
            }
            if(bad_nal) { //not a complete nal unit, delete these packets
                DeletePackets(session_info, packet_start, packet);
            }
        }else 
            packet = packet->next;
    }
    if(session_info->packets_count > 0) 
        session_info->isDecodable = PJ_TRUE;   
    return 0;
}

