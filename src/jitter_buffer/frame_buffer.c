#include <jitter_buffer/frame_buffer.h>
#include <jitter_buffer/packet.h>
#include <pj/assert.h>
PJ_DEF(int) frame_buffer_init(Frame_Buffer *frame_buffer, packet_alloc_t *packet_alloc) {
    //init session_info
    session_info_init(&frame_buffer->session_info, packet_alloc);
    //init other attributs
    frame_buffer->frame_status = FRAME_STATUS_FREE;
    frame_buffer->counted = PJ_FALSE;
    pj_get_timestamp(&frame_buffer->latest_packet_timestamp);
    frame_buffer->ts = 0;
    frame_buffer->frame_type = PJMEDIA_FRAME_TYPE_NONE;
    frame_buffer->renderTimeMs = 0;
    frame_buffer->frame_size = 0;
    frame_buffer->nack_count = 0;
    return 0;
}
PJ_DEF(int) frame_buffer_reset(Frame_Buffer *frame_buffer) {
    session_info_reset(&frame_buffer->session_info);
    frame_buffer->frame_status = FRAME_STATUS_FREE;
    frame_buffer->counted = PJ_FALSE;
    pj_get_timestamp(&frame_buffer->latest_packet_timestamp);
    frame_buffer->ts = 0;
    frame_buffer->frame_type = PJMEDIA_FRAME_TYPE_NONE;
    frame_buffer->renderTimeMs = 0;
    frame_buffer->frame_size = 0;
    frame_buffer->nack_count = 0;
    return 0;
}

PJ_DEF(void) frame_buffer_setStatus(Frame_Buffer *frame_buffer, FrameStatus  frame_status) {
    pj_assert(frame_buffer != NULL);
    //sanity check
    switch(frame_status) {
        case FRAME_STATUS_EMPTY: pj_assert(frame_buffer->frame_status == FRAME_STATUS_FREE); 
                                    break;
        case FRAME_STATUS_INCOMPLETE: pj_assert(frame_buffer->frame_status == FRAME_STATUS_FREE
                                    || frame_buffer->frame_status == FRAME_STATUS_EMPTY || 
                                    frame_buffer->frame_status == FRAME_STATUS_INCOMPLETE);
                                    break;
        case FRAME_STATUS_COMPLETE: pj_assert(frame_buffer->frame_status == FRAME_STATUS_FREE
                                    || frame_buffer->frame_status == FRAME_STATUS_EMPTY ||
                                    frame_buffer->frame_status == FRAME_STATUS_INCOMPLETE);
                                    break;
        case FRAME_STATUS_DECODING: pj_assert(frame_buffer->frame_status == FRAME_STATUS_INCOMPLETE
                                    || frame_buffer->frame_status == FRAME_STATUS_COMPLETE);
                                    break;
    }
    frame_buffer->frame_status = frame_status;
}
//if ret > 0, jitter buffer should retrieve this packet
PJ_DEF(int) frame_buffer_insert_packet(Frame_Buffer *frame_buffer, JTPacket *packet) {
    pj_assert(frame_buffer != NULL && packet != NULL);
    if(frame_buffer->frame_status == FRAME_STATUS_DECODING)
        return 1; //can't insert when decoding, jitter buffer should retrieve this packet
    if(packet->frame_type == PJMEDIA_FRAME_TYPE_NONE)
        return 2;
    if(frame_buffer->frame_type == PJMEDIA_FRAME_TYPE_NONE ) {
        frame_buffer->frame_type = packet->frame_type;
        frame_buffer->ts = packet->ts;
    } else if(frame_buffer->frame_type == PJMEDIA_FRAME_TYPE_EMPTY &&
        packet->frame_type != PJMEDIA_FRAME_TYPE_EMPTY) {
        //the first media packet
        frame_buffer->frame_type = packet->frame_type;
    }
    if(frame_buffer->ts != packet->ts) {//the packet not in this frame
        return 3;
    }
    int result = session_info_insert_packet(&frame_buffer->session_info, packet);
    if(result == 0) { //update status
        if(frame_buffer->session_info.isComplete)
            frame_buffer_setStatus(frame_buffer, FRAME_STATUS_COMPLETE);
        else
            frame_buffer_setStatus(frame_buffer, FRAME_STATUS_INCOMPLETE);
        //update timestamp
        pj_get_timestamp(&frame_buffer->latest_packet_timestamp);
        frame_buffer->frame_size += packet->size;
        
    }
    return result;
        
}

PJ_DEF(int) frame_buffer_get_packets_count(Frame_Buffer *frame_buffer) {
    pj_assert(frame_buffer != NULL);
    return frame_buffer->session_info.packets_count;
}

PJ_DEF(int) frame_buffer_getFirstSeq(Frame_Buffer *frame_buffer) {
    pj_assert(frame_buffer != NULL);
    JTPacket *packetList = &frame_buffer->session_info.packetList;
    if(packetList->next != packetList) { // not empty
        return packetList->next->seq;
    } else if(frame_buffer->session_info.empty_low_seq_num != -1) 
        return frame_buffer->session_info.empty_low_seq_num;
    return -1;
}
//get the end sequence number
PJ_DEF(int) frame_buffer_getHighSeq(Frame_Buffer *frame_buffer) {
    pj_assert(frame_buffer != NULL);
    JTPacket *packetList = &frame_buffer->session_info.packetList;
    if(packetList->prev != packetList) {
        int highSeq = (int)packetList->prev->seq;
        if(frame_buffer->session_info.empty_high_seq_num > highSeq)
            highSeq = frame_buffer->session_info.empty_high_seq_num;
        return highSeq;
    } else{
        if(frame_buffer->session_info.empty_high_seq_num != -1) //empty frame
            return frame_buffer->session_info.empty_high_seq_num;
    }
    return -1;
}
PJ_DEF(void) frame_buffer_IncrementNackCount(Frame_Buffer *frame_buffer) {
    pj_assert(frame_buffer != NULL);
    frame_buffer->nack_count++;
}

PJ_DEF(int) frame_buffer_buildSoftNackList(Frame_Buffer *frame_buffer, 
            int *seq_num_list, int seq_num_list_len, pj_uint32_t rtt_ms) {
    if(frame_buffer->frame_status == FRAME_STATUS_DECODING || frame_buffer->frame_status == FRAME_STATUS_FREE)
        return 0;
    return session_info_buildSoftNackList(&frame_buffer->session_info, seq_num_list, seq_num_list_len, rtt_ms);
}

PJ_DEF(int) frame_buffer_buildHardNackList(Frame_Buffer* frame_buffer, 
            int *seq_num_list, int seq_num_list_len) {
    
    if(frame_buffer->frame_status == FRAME_STATUS_DECODING || frame_buffer->frame_status == FRAME_STATUS_FREE)
        return 0;
    return session_info_buildHardNackList(&frame_buffer->session_info, seq_num_list, seq_num_list_len);
}

