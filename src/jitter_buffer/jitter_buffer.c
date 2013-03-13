#include <jitter_buffer/jitter_buffer.h>
#include <jitter_buffer/jitter_utility.h>
#include <pj/pool.h>
#include <pj/list.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/compat/string.h>
#include <utility/time_parser.h>
#define DEFAULT_RTT_MS 100
#define MIN_NUM_OF_FRAMES 30
//delay on receving side
#define OPERATING_SYSTEM_JITTER 10    

#define THIS_FILE "jitter_buffer.c"
PJ_DEF(int) jitter_buffer_create(Jitter_Buffer ** jitter_buffer, pj_pool_t *pool,
    int max_number_of_frames, NACK_MODE nack_mode, 
    int low_rtt_threshold_ms, int high_rtt_threshold_ms) {
    pj_assert(low_rtt_threshold_ms <= high_rtt_threshold_ms);
    //alloc jitter buffer
    Jitter_Buffer *jitter_buffer_internal = (Jitter_Buffer*)pj_pool_zalloc(pool, sizeof(Jitter_Buffer));
    //init list alloc
    list_alloc_init(&jitter_buffer_internal->frame_alloc, pool);
    list_alloc_init(&jitter_buffer_internal->packet_alloc, pool);
    //init frame list, don't call frame_buffer_init, 
    //since the header is not used as a real frame
    pj_list_init(&jitter_buffer_internal->frameList);
    pj_list_init(&jitter_buffer_internal->decodingFrameList);
    //init frame number
    jitter_buffer_internal->max_number_of_frames = max_number_of_frames 
            < MIN_NUM_OF_FRAMES? MIN_NUM_OF_FRAMES : max_number_of_frames;
    jitter_buffer_internal->number_of_frames = 0;
    //init decode state
    decode_state_init(&jitter_buffer_internal->decode_state);
    //init jitter estimator
    jitter_estimator_init(&jitter_buffer_internal->jitter_estimator);
    //init inter_frame_delay
    inter_frame_delay_init(&jitter_buffer_internal->inter_frame_delay);
    //init rtt
    jitter_buffer_internal->rttMs = DEFAULT_RTT_MS;
    //init nack
    if(high_rtt_threshold_ms != -1) 
        jitter_buffer_internal->rttMs = 0;
    jitter_buffer_internal->nack_mode = nack_mode;
    jitter_buffer_internal->low_rtt_threshold_ms = low_rtt_threshold_ms;
    jitter_buffer_internal->high_rtt_threshold_ms = high_rtt_threshold_ms;
    jitter_buffer_internal->nack_seq_num = 0;
    //init event
    if(event_create(&jitter_buffer_internal->frame_event) != 0) {
        return -1;
    }
    if(event_create(&jitter_buffer_internal->packet_event) != 0)
        return -1;
    
    //not running
    jitter_buffer_internal->running = PJ_FALSE;
    //first packet 
    jitter_buffer_internal->first_packet = PJ_FALSE;
    //the first frame must be key frame
    jitter_buffer_internal->waiting_for_key_frame = PJ_TRUE;
    //waiting for completed frame
    jitter_buffer_internal->waiting_for_completed_frame.timestamp = 0;
    jitter_buffer_internal->waiting_for_completed_frame.frame_size = 0;
    jitter_buffer_internal->waiting_for_completed_frame.latest_packet_timestamp = -1;
    //init mutex
    if(pj_mutex_create_simple(pool, NULL, &jitter_buffer_internal->jb_mutex) != PJ_SUCCESS)
        return -1;
    //ret
    *jitter_buffer = jitter_buffer_internal;
    return 0;
    
}
static void Reset(Jitter_Buffer *jitter_buffer) {
    
    //free frame list
    Frame_Buffer * frame = jitter_buffer->frameList.next;
    Frame_Buffer * next_frame;
    while(frame != &jitter_buffer->frameList) {
        next_frame = frame->next;
        pj_list_erase(frame);
        list_alloc_insert(frame, &jitter_buffer->frame_alloc);
        frame = next_frame;
    }
    frame = jitter_buffer->decodingFrameList.next;
    while(frame != &jitter_buffer->decodingFrameList) {
        next_frame = frame->next;
        if(frame->frame_status != FRAME_STATUS_DECODING) {
            //not decoding ,release it
            pj_list_erase(frame);
            list_alloc_insert(frame, &jitter_buffer->frame_alloc);
        }
        frame = next_frame;
    }
    jitter_buffer->number_of_frames = 0;
    //reset decode state
    decode_state_reset(&jitter_buffer->decode_state);
    //reset jitter estimator
    jitter_estimator_reset(&jitter_buffer->jitter_estimator);
    //reset inter_frame_delay
    inter_frame_delay_reset(&jitter_buffer->inter_frame_delay);
    //reset rtt
    if(jitter_buffer->high_rtt_threshold_ms == -1)
        jitter_buffer->rttMs = DEFAULT_RTT_MS;
    else 
        jitter_buffer->rttMs = 0;
    jitter_buffer->nack_seq_num = 0;
    jitter_buffer->waiting_for_key_frame = PJ_TRUE;
    //reset event
    event_reset(jitter_buffer->frame_event);
    event_reset(jitter_buffer->packet_event);
    jitter_buffer->first_packet = PJ_FALSE;
    //waiting for completed frame
    jitter_buffer->waiting_for_completed_frame.timestamp = 0;
    jitter_buffer->waiting_for_completed_frame.frame_size = 0;
    jitter_buffer->waiting_for_completed_frame.latest_packet_timestamp = -1;
}
PJ_DEF(void) jitter_buffer_reset(Jitter_Buffer *jitter_buffer) {
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return;
    }
    Reset(jitter_buffer);
    pj_mutex_unlock(jitter_buffer->jb_mutex);
}

PJ_DEF(void) jitter_buffer_stop(Jitter_Buffer *jitter_buffer) {
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return;
    }
    jitter_buffer->running = PJ_FALSE;
    Reset(jitter_buffer);

    pj_mutex_unlock(jitter_buffer->jb_mutex);
    
}
PJ_DEF(void) jitter_buffer_flush(Jitter_Buffer *jitter_buffer) {
    jitter_buffer_reset(jitter_buffer);
}
PJ_DEF(void) jitter_buffer_start(Jitter_Buffer *jitter_buffer) {
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return;
    }
    if(jitter_buffer->running) //already start
        return;
    //reset 
    Reset(jitter_buffer);
    //running
    jitter_buffer->running = PJ_TRUE;
    pj_mutex_unlock(jitter_buffer->jb_mutex);
}
static int FrameCount(Frame_Buffer *frameList) {
    int count = 0; 
    Frame_Buffer *frame = frameList->next;
    while(frame != frameList) {
        ++count;
        frame = frame->next;
    }
    return count;
}
static void getFrameInfo(Frame_Buffer *frameList, Decode_State *decode_state, char * buf, int size) {
    Frame_Buffer *frame = frameList->next;
    int len = 0;
    len =  snprintf(buf, size, "\nlast decode status, seq:%u, ts:%u",decode_state->seq, decode_state->ts);
    if(len < 0) goto ON_RET;
    size -= len;
    if(size < 2) goto ON_RET;
    buf += len;
    while(frame != frameList) {
        if(frame->frame_type != PJMEDIA_FRAME_TYPE_EMPTY) {
            len =  snprintf(buf, size, "\nts=%u key=%d isComplete=%d marked=%d seqs=",frame->ts, frame->session_info.isKeyFrame,
                        frame->session_info.isComplete, frame->session_info.packetList.prev->isMarket);
            if(len < 0) goto ON_RET;
            size -= len;
            if(size < 2) goto ON_RET;
            buf += len;
            JTPacket *packet = frame->session_info.packetList.next;
            while(packet != &frame->session_info.packetList) {
                len = snprintf(buf, size, "%u ", packet->seq);
                if(len < 0) goto ON_RET;
                size -= len;
                if(size < 2) goto ON_RET;
                buf += len;
                packet = packet->next;
            }
        }
        frame = frame->next;
    }
    ON_RET:
        buf[0] = '\0';
}

/**
  * TODO wrap
  * find a frame with ts >, =, < the given 'ts'
  * @param op must be '>', '=', or '<'
  */
static Frame_Buffer* findFrame(Frame_Buffer *frameList, pj_uint32_t ts, char op) {
    if(frameList == NULL) return NULL;
    Frame_Buffer *frame = frameList->next;
    Frame_Buffer *prev_frame = NULL;
    while(frame != frameList) {
        if(frame->ts == ts) {
            if(op == '<' || op == '=')
                break;
        } else if(frame->ts > ts) {
            break;
        }
        prev_frame = frame;
        frame = frame->next;
    }
    // ret
    if(op == '<') {
        return prev_frame;
    } else if (op == '=') {
        if(frame != frameList && frame->ts == ts) 
            return frame;
        else
            return NULL;
    } else if( op == '>') {
        if(frame != frameList && frame->ts > ts)
            return frame;
        else
            return NULL;
    }
    return NULL;
}
static pj_bool_t first_packet_in_frame(Jitter_Buffer *jitter_buffer, JTPacket *packet) {
    //find prev frame
    Frame_Buffer *prev_frame = findFrame(&jitter_buffer->frameList, packet->ts, '<');
    //check
    if(prev_frame != NULL) {
        if(prev_frame->session_info.packetList.next == &prev_frame->session_info.packetList) //empty
            return PJ_FALSE;
        else {
            int lastHighSeq = frame_buffer_getHighSeq(prev_frame);
            if(lastHighSeq == -1) {
                pj_assert(PJ_FALSE); //bad frame
                return PJ_FALSE;
            } 
            if(InSequence(lastHighSeq, packet->seq))
                return PJ_TRUE;
        }
    } else {
        if(InSequence(jitter_buffer->decode_state.seq, packet->seq))
            return PJ_TRUE;
    }
    return PJ_FALSE;
}

PJ_DEF(pj_uint32_t) jitter_buffer_estimateJitterMS(Jitter_Buffer *jitter_buffer) {
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return 0;
    }
    pj_uint32_t jitter = OPERATING_SYSTEM_JITTER;
    double rtt_mult = 1.0f;
    if(jitter_buffer->nack_mode == NACK_MODE_HYBRID && 
        (jitter_buffer->low_rtt_threshold_ms != -1 && jitter_buffer->rttMs > jitter_buffer->low_rtt_threshold_ms)) {
        rtt_mult = 0.0f; //why?
    }
    jitter += jitter_estimator_getJitterEstimate(&jitter_buffer->jitter_estimator, rtt_mult);
    pj_mutex_unlock(jitter_buffer->jb_mutex);
    return jitter;
}

static void RealseFrame(Jitter_Buffer *jitter_buffer, Frame_Buffer *frame) {
    if(frame->frame_status == FRAME_STATUS_DECODING)
        return;
    frame_buffer_reset(frame);
    pj_list_erase(frame);
    list_alloc_insert(frame, &jitter_buffer->frame_alloc);
    jitter_buffer->number_of_frames--;
}
static void CleanAllFramesExceptLatestOne(Jitter_Buffer *jitter_buffer) {
    Frame_Buffer *frameList = &jitter_buffer->frameList;
    Frame_Buffer *frame = frameList->next;
    Frame_Buffer *next_frame ;
    while(frame != frameList && frame != frameList->prev) {
        next_frame = frame->next;
        RealseFrame(jitter_buffer, frame);
        frame = next_frame;
    }

}
static void CleanOldFrames(Jitter_Buffer *jitter_buffer) {
    if(!jitter_buffer->decode_state.inited)
        return;
    Frame_Buffer *frameList = &jitter_buffer->frameList;
    while(frameList->next != frameList) {
        Frame_Buffer *firstFrame = frameList->next;
        if(firstFrame->frame_status == FRAME_STATUS_DECODING) {
            jitter_buffer->number_of_frames--;
            pj_list_erase(firstFrame);
            pj_list_insert_before(&jitter_buffer->decodingFrameList, firstFrame);
        } else if(decode_state_isOldFrame(firstFrame, &jitter_buffer->decode_state)) {
            RealseFrame(jitter_buffer, firstFrame);
        } else if(firstFrame->frame_type == PJMEDIA_FRAME_TYPE_EMPTY) {
            decode_state_updateEmptyFrame(firstFrame, &jitter_buffer->decode_state);
            RealseFrame(jitter_buffer, firstFrame);
        } else
            break;
    }
       
}

PJ_DEF(pj_ssize_t) jitter_buffer_nextTimestamp(Jitter_Buffer *jitter_buffer, pj_uint32_t max_wait_time_ms, 
                                    pjmedia_frame_type *frame_type, pj_ssize_t *next_render_time) {
    //running
    if(!jitter_buffer->running)
            return -1;
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return -1;
    }
    //clean old frames
    CleanOldFrames(jitter_buffer);
    pj_ssize_t timestamp = -1;
    //get first frame
    Frame_Buffer *frame = jitter_buffer->frameList.next;
    //if no first frame , wait 'max_wait_time_ms'
    if(frame == &jitter_buffer->frameList) {
        if(max_wait_time_ms > 0) {
            event_reset(jitter_buffer->frame_event); //reset event for next waiting
            pj_mutex_unlock(jitter_buffer->jb_mutex); //realse lock
            if(event_wait(jitter_buffer->frame_event, max_wait_time_ms) == WAIT_RET_SIGNAL) {
                CleanOldFrames(jitter_buffer); //jitter buffer status may be changed in other threads
                frame = jitter_buffer->frameList.next;
            }
            pj_mutex_lock(jitter_buffer->jb_mutex);
        }
    }
    //no first frame ,return
    if(frame == &jitter_buffer->frameList) {
        pj_mutex_unlock(jitter_buffer->jb_mutex);
        return timestamp;
    }
    //get values
    *next_render_time = frame->renderTimeMs;
    *frame_type = frame->frame_type;
    timestamp = (pj_ssize_t)frame->ts;
    pj_mutex_unlock(jitter_buffer->jb_mutex);
    return timestamp;
}   

static pj_bool_t WaitingForRetransmit(Jitter_Buffer *jitter_buffer) {
    if(jitter_buffer->nack_mode == NACK_MODE_NO)
        return PJ_FALSE;
    if(jitter_buffer->nack_mode == NACK_MODE_HARD)
        return PJ_TRUE;
    //hybrid mode, rtt is too large 
    if(jitter_buffer->nack_mode == NACK_MODE_HYBRID && jitter_buffer->high_rtt_threshold_ms > 0
        && jitter_buffer->rttMs > jitter_buffer->high_rtt_threshold_ms)
        return PJ_FALSE;
    return PJ_TRUE;
}

PJ_DEF(void) jitter_buffer_changeFrameStatus(Jitter_Buffer *jitter_buffer, Frame_Buffer *frame, FrameStatus frame_status) {
    //running
    if(!jitter_buffer->running)
            return ;
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return ;
    }
    if(frame_status == FRAME_STATUS_FREE) { //release the frame
        pj_list_erase(frame);
        list_alloc_insert(frame, &jitter_buffer->frame_alloc);
    } else
        frame_buffer_setStatus(frame, frame_status);
    pj_mutex_unlock(jitter_buffer->jb_mutex);
}

static Frame_Buffer * findOldestCompleteContinuousFrame(Jitter_Buffer *jitter_buffer) {
    Frame_Buffer * frame = jitter_buffer->frameList.next;
    if(frame == &jitter_buffer->frameList) //no frame
        return NULL;
    if(frame->frame_type == PJMEDIA_FRAME_TYPE_NONE || frame->frame_type == PJMEDIA_FRAME_TYPE_EMPTY)
        return NULL;
    if(frame->frame_status != FRAME_STATUS_COMPLETE || !decode_state_isContinuousFrame(frame, &jitter_buffer->decode_state))
        return NULL;
    if(jitter_buffer->waiting_for_key_frame && !frame->session_info.isKeyFrame) //not key frame
        return NULL;
    return frame;
}
/**
  * @param latest_packet_time the arrived time of latest packet in the frame
  */
static void UpdateJitterEstimator(Jitter_Buffer *jitter_buffer, pj_uint32_t timestamp, 
                                    pj_int32_t frame_size, pj_ssize_t latest_packet_time, 
                                    pj_bool_t isCompleted) {
    pj_ssize_t delay;
    pj_bool_t ordered = inter_frame_delay_calculateDelay(&delay, timestamp, latest_packet_time,
                                &jitter_buffer->inter_frame_delay);
    if(ordered) {
        jitter_estimator_updateEstimate(delay, frame_size, isCompleted, &jitter_buffer->jitter_estimator);
    }
}

static void UpdateJitterEstimatorForWaitingFrame(Jitter_Buffer *jitter_buffer, Waiting_For_Completed_Frame *frame) {
    UpdateJitterEstimator(jitter_buffer, frame->timestamp, frame->frame_size, frame->latest_packet_timestamp,
            PJ_FALSE);
}

static void UpdateJitterEstimatorForFrame(Jitter_Buffer *jitter_buffer, Frame_Buffer *frame) {
    if(frame->frame_type == PJMEDIA_FRAME_TYPE_NONE || frame->frame_type == PJMEDIA_FRAME_TYPE_EMPTY)
        return;
    UpdateJitterEstimator(jitter_buffer, frame->ts, frame->frame_size, TimestampToMs(&frame->latest_packet_timestamp),
            frame->frame_status == FRAME_STATUS_COMPLETE ? PJ_TRUE: PJ_FALSE);
}

PJ_DEF(Frame_Buffer *) jitter_buffer_getCompleteFrameForDecoding(Jitter_Buffer *jitter_buffer, pj_uint32_t max_wait_time_ms) {
    //running
    if(!jitter_buffer->running)
            return NULL;
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) { //error
        return NULL;
    }
    /*{
        char buf[1024];
        getFrameInfo(&jitter_buffer->frameList, &jitter_buffer->decode_state, buf, 1024);
        PJ_LOG(4, (THIS_FILE, "jb status:\n%s\n", buf));
    }*/
    //the first frame must be a key frame
    if(jitter_buffer->decode_state.inited == PJ_FALSE)
        jitter_buffer->waiting_for_key_frame == PJ_TRUE;
    //clean old frames
    CleanOldFrames(jitter_buffer);
    //get complete , continuous frame
    Frame_Buffer * frame = findOldestCompleteContinuousFrame(jitter_buffer);
    //if not got, try to wait a frame
    if(frame == NULL) {
        if(max_wait_time_ms == 0)
            goto ON_RET;
        pj_timestamp current ;
        pj_get_timestamp(&current);
        pj_timestamp end = current;
        pj_add_timestamp32(&end, max_wait_time_ms);
        pj_int32_t wait_time_ms = max_wait_time_ms;
        event_reset(jitter_buffer->packet_event);
        while(wait_time_ms > 0) {
            pj_mutex_unlock(jitter_buffer->jb_mutex);
            if(event_wait(jitter_buffer->packet_event, wait_time_ms) == WAIT_RET_SIGNAL) {
                //incoming a packet
                pj_mutex_lock(jitter_buffer->jb_mutex);
                if(!jitter_buffer->running) //jitter buffer stoped
                    goto ON_RET;
                CleanOldFrames(jitter_buffer);
                frame = findOldestCompleteContinuousFrame(jitter_buffer);
                if(frame != NULL)
                    break;
                pj_get_timestamp(&current);
                int elapsed_msec = pj_elapsed_msec(&current, &end);
                wait_time_ms = elapsed_msec;
            } else {
                pj_mutex_lock(jitter_buffer->jb_mutex); //error or timeout
                break;
            }
        }
    } else
        event_reset(jitter_buffer->packet_event);
    if(frame == NULL)
        goto ON_RET;
    /*if(jitter_buffer->waiting_for_key_frame && !frame->session_info.isKeyFrame) {
        frame = NULL; //not a key frame
        goto ON_RET;
    }*/
    //got one, update jitter
    if(frame->nack_count > 0) {
        jitter_estimator_frameNacked(&jitter_buffer->jitter_estimator); //nacked
    } else {
        //update jitter estimator
        UpdateJitterEstimatorForFrame(jitter_buffer, frame);
    }
    //set frame status
    frame_buffer_setStatus(frame, FRAME_STATUS_DECODING);
    //update decode state
    decode_state_updateFrame(frame, &jitter_buffer->decode_state);
    //waiting for key frame
    if(frame->session_info.isKeyFrame)
        jitter_buffer->waiting_for_key_frame = PJ_FALSE;
    //clean old frames
    CleanOldFrames(jitter_buffer);
    //return frame
    ON_RET:
        pj_mutex_unlock(jitter_buffer->jb_mutex);
        return frame;
}

static void VerifyDecodableAndSetPreviousLoss(Frame_Buffer *frame) {
    if(frame->frame_type == PJMEDIA_FRAME_TYPE_NONE || frame->frame_type ==
        PJMEDIA_FRAME_TYPE_EMPTY)
        return;
    //make decodable
    session_info_makeDecodable(&frame->session_info);
    //if not key frame ,set previous frame loss
    if(!frame->session_info.isKeyFrame)
        session_info_set_previous_frame_loss(&frame->session_info, PJ_TRUE);
}
static Frame_Buffer * FindKeyFrame(Frame_Buffer * frameList, Decode_State *decode_state, pj_bool_t complete) {
    Frame_Buffer *frame = frameList->next;
    while(frame != frameList) {
        if(decode_state_isOldFrame(frame, decode_state)) {
            frame = frame->next;
            continue;
        }
        if(frame->session_info.isKeyFrame && (!complete || frame->session_info.isComplete ))
            break;
        frame = frame->next;
    }
    if(frame == frameList)
        return NULL;
    return frame;
}
static Frame_Buffer * GetFrameForDecodingNACK(Jitter_Buffer *jitter_buffer) {
    if(!jitter_buffer->decode_state.inited)
        jitter_buffer->waiting_for_key_frame = PJ_TRUE;
    CleanOldFrames(jitter_buffer);
    //get a complete continuous frame
    Frame_Buffer *frame = findOldestCompleteContinuousFrame(jitter_buffer);
    if(frame == NULL) {
        //get a decodable key frame
        frame = FindKeyFrame(&jitter_buffer->frameList, &jitter_buffer->decode_state, PJ_TRUE);
        if(frame == NULL) {
            CleanAllFramesExceptLatestOne(jitter_buffer);
            return NULL;
        }
    }
    if(frame == NULL || frame->next == &jitter_buffer->frameList) {
        return NULL;
    }
    /*if(jitter_buffer->waiting_for_key_frame && !frame->session_info.isKeyFrame) {
        frame = NULL; //not a key frame
        return NULL;
    }*/
    //verify decodable
    VerifyDecodableAndSetPreviousLoss(frame);
    //got one, update jitter
    if(frame->nack_count > 0) { //retransmit
        jitter_estimator_frameNacked(&jitter_buffer->jitter_estimator); //nacked
    } else {
        //update jitter estimator
        UpdateJitterEstimatorForFrame(jitter_buffer, frame);
    }
    //set frame status
    frame_buffer_setStatus(frame, FRAME_STATUS_DECODING);
    //update decode state
    decode_state_updateFrame(frame, &jitter_buffer->decode_state);
    //waiting for key frame
    if(frame->session_info.isKeyFrame)
        jitter_buffer->waiting_for_key_frame = PJ_FALSE;
    //clean old frames
    CleanOldFrames(jitter_buffer);

    //return
    return frame;
}
PJ_DEF(Frame_Buffer*) jitter_buffer_getFrameForDecoding(Jitter_Buffer *jitter_buffer) {
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return NULL;
    }
    Frame_Buffer *frame = NULL;
    if(!jitter_buffer->running)
        goto ON_RET;
    if(WaitingForRetransmit(jitter_buffer)) {
        frame = GetFrameForDecodingNACK(jitter_buffer);
        goto ON_RET;
    }
    /*{
        char buf[1024];
        getFrameInfo(&jitter_buffer->frameList, &jitter_buffer->decode_state, buf, 1024);
        PJ_LOG(4, (THIS_FILE, "2, jb status:\n%s\n", buf));
    }*/
    frame = jitter_buffer->frameList.next;
    //empty or only one frame and the frame is incomplete
    if(frame == &jitter_buffer->frameList || 
        (frame->next == &jitter_buffer->frameList && !frame->session_info.isComplete)) {
        frame = NULL;
        goto ON_RET;
    }
    if(frame->frame_type == PJMEDIA_FRAME_TYPE_NONE) {
        PJ_LOG(2, (THIS_FILE, "wrong frame type"));
        pj_assert(PJ_FALSE);
    }
    //empty frame
    if(frame->frame_type == PJMEDIA_FRAME_TYPE_EMPTY) {
        frame = NULL;
        decode_state_updateEmptyFrame(frame, &jitter_buffer->decode_state);
        goto ON_RET;
    }
    if(!decode_state_isContinuousFrame(frame, &jitter_buffer->decode_state)) {
        frame = FindKeyFrame(&jitter_buffer->frameList, &jitter_buffer->decode_state, PJ_TRUE);
        if(frame == NULL) {
            CleanAllFramesExceptLatestOne(jitter_buffer);
            goto ON_RET;
        } 
    }
    /*if(jitter_buffer->waiting_for_key_frame && !frame->session_info.isKeyFrame) {
        frame = NULL;
        goto ON_RET;
    }*/
    //update jitter
    if(frame->nack_count > 0) //retransmit
        jitter_estimator_frameNacked(&jitter_buffer->jitter_estimator); //nacked
    else {
        Waiting_For_Completed_Frame * wfcf = &jitter_buffer->waiting_for_completed_frame;
        if(wfcf->latest_packet_timestamp >= 0) {
            UpdateJitterEstimatorForWaitingFrame(jitter_buffer, wfcf);
        }
        //reset waiting frame
        wfcf->latest_packet_timestamp = TimestampToMs(&frame->latest_packet_timestamp) ;
        wfcf->frame_size = frame->frame_size;
        wfcf->timestamp = frame->ts;
    }
    //update frame status
    frame->frame_status = FRAME_STATUS_DECODING;
    //update decode status
    decode_state_updateFrame(frame, &jitter_buffer->decode_state);
    //waiting for key frame
    if(frame->session_info.isKeyFrame)
        jitter_buffer->waiting_for_key_frame = PJ_FALSE;
    //clean old frames
    CleanOldFrames(jitter_buffer);
    ON_RET:
    pj_mutex_unlock(jitter_buffer->jb_mutex);
    return frame;      
}

/**
  * @isFirstPacket first packet in a frame
  * @isMarketPacket last packet in a frame
  */
PJ_DEF(int) jitter_buffer_insert_packet(Jitter_Buffer *jitter_buffer, 
pj_uint16_t seq, pj_uint32_t ts, pjmedia_frame_type frame_type, 
char *payload, int size, 
pj_bool_t isFirstPacket, pj_bool_t isMarketPacket) {
    int ret = 0;
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return -1;
    }
    pj_ssize_t current;
    JTPacket *packet = NULL;
    Frame_Buffer *frame;
    // is running
    if(!jitter_buffer->running) {
        ret = -2;
        goto ON_RET;
    }
    // sanity check
    if(ts == 0 || (frame_type != PJMEDIA_FRAME_TYPE_EMPTY && size == 0)) {
        ret = -3;
        goto ON_RET;
    }
    if(!jitter_buffer->first_packet) {
        isFirstPacket = PJ_TRUE;
        jitter_buffer->first_packet = PJ_TRUE;
    }
    //clean old frames
    //CleanOldFrames(jitter_buffer);
    //update jitter
    current = GetCurrentTimeMs();
    if(frame_type != PJMEDIA_FRAME_TYPE_EMPTY && frame_type != PJMEDIA_FRAME_TYPE_NONE) {
        if(jitter_buffer->waiting_for_completed_frame.timestamp == ts) {
            jitter_buffer->waiting_for_completed_frame.frame_size += size;
            jitter_buffer->waiting_for_completed_frame.latest_packet_timestamp = current;
        } else if(jitter_buffer->waiting_for_completed_frame.latest_packet_timestamp > 0 && 
            current - jitter_buffer->waiting_for_completed_frame.latest_packet_timestamp  > 2000) {
                //too old
                UpdateJitterEstimatorForWaitingFrame(jitter_buffer, &jitter_buffer->waiting_for_completed_frame);
                jitter_buffer->waiting_for_completed_frame.frame_size = 0;
                jitter_buffer->waiting_for_completed_frame.latest_packet_timestamp = -1;
                jitter_buffer->waiting_for_completed_frame.timestamp = 0;
        }
    }
    //create packet
    packet = NULL;
    if(jt_packet_create(&packet, &jitter_buffer->packet_alloc, seq, ts, frame_type, 
                    isFirstPacket, isMarketPacket, payload, size) != 0) {
        if(packet)
            list_alloc_insert(packet, &jitter_buffer->packet_alloc);
        ret = -1;
        goto ON_RET;
    }
    //GetCurrentTimeInLocal(packet->time_in_jb, 60);
    if(!isFirstPacket) {
        //is first packet in frame
        isFirstPacket = first_packet_in_frame(jitter_buffer, packet);
        packet->isFirst = isFirstPacket;
    }
    if(isMarketPacket) {
        //check if next packet is first packet
        Frame_Buffer *next_frame = findFrame(&jitter_buffer->frameList, packet->ts, '>');
        if(next_frame != NULL) {
            JTPacket * first_packet = next_frame->session_info.packetList.next;
            if(packet != &next_frame->session_info.packetList) {
                if(InSequence(packet->seq, first_packet->seq)) {
                    first_packet->isFirst = PJ_TRUE;
                }
            }
        }
    }
    //clean old frames
    CleanOldFrames(jitter_buffer);   
    // is old packet 
    if(decode_state_isOldPacket(packet, &jitter_buffer->decode_state)) {
            decode_state_updateOldPacket(packet, &jitter_buffer->decode_state);
            list_alloc_insert(packet, &jitter_buffer->packet_alloc);
            ret = -1;
            goto ON_RET;
    }
    
    // find or alloc a frame
    frame = findFrame(&jitter_buffer->frameList, packet->ts, '=');
    if(frame == NULL) { //alloc one
        //if(jitter_buffer->number_of_frames > jitter_buffer->max_number_of_frames) {
        if(jitter_buffer->number_of_frames > jitter_buffer->max_number_of_frames) {
            //clean old frames at least one
            Frame_Buffer *oldestFrame = jitter_buffer->frameList.next;
            if(oldestFrame != &jitter_buffer->frameList)
                RealseFrame(jitter_buffer, oldestFrame);
        }
        list_alloc_alloc(Frame_Buffer, &frame, &jitter_buffer->frame_alloc);
        //init
        frame_buffer_init(frame, &jitter_buffer->packet_alloc);
    }
    //insert packet into the frame
    ret = frame_buffer_insert_packet(frame, packet);
    if(ret > 0) {
        list_alloc_insert(packet, &jitter_buffer->packet_alloc);
        frame_buffer_reset(frame);
        list_alloc_insert(frame, &jitter_buffer->frame_alloc);
        ret = -1;
        goto ON_RET;
    } else if (ret < 0) {
        frame_buffer_reset(frame);
        list_alloc_insert(frame, &jitter_buffer->frame_alloc);
        ret = -1;
        goto ON_RET;
    } else {
        event_set(jitter_buffer->packet_event);
        if(packet->isRetrans)
            frame_buffer_IncrementNackCount(frame);
    }
    
    //insert frame to frame list
    if(findFrame(&jitter_buffer->frameList, frame->ts, '=') == NULL) {
        Frame_Buffer *prev_frame = findFrame(&jitter_buffer->frameList, frame->ts, '<');
        prev_frame = (prev_frame == NULL ? &jitter_buffer->frameList : prev_frame);
        pj_list_insert_after(prev_frame, frame);
        event_set(jitter_buffer->frame_event);
        jitter_buffer->number_of_frames++;
    }
    ON_RET:
    pj_mutex_unlock(jitter_buffer->jb_mutex);
    return ret;
    
}
static Frame_Buffer* findFrameByTs(Jitter_Buffer *jitter_buffer, pj_uint32_t ts) {
    return findFrame(&jitter_buffer->frameList, ts, '=');
}
PJ_DEF(void) jitter_buffer_setRenderTime(Jitter_Buffer *jitter_buffer, pj_uint32_t ts, 
                                        pj_ssize_t render_time_ms) {
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return ;
    }   
    Frame_Buffer* frame = findFrameByTs(jitter_buffer, ts);
    if(frame != NULL && frame->session_info.packets_count == 1) {
        //first media packet in the frame
        frame->renderTimeMs = render_time_ms;
    }
    pj_mutex_unlock(jitter_buffer->jb_mutex);
}
/**
  * create nack sequence number list
  * @param nack_seq_list out parameter
  */
PJ_DEF(int) jitter_buffer_createNackList(Jitter_Buffer *jitter_buffer, 
            pj_uint32_t rtt_ms, pj_uint16_t **nack_seq_list, 
            pj_uint32_t *nack_seq_num) {
    int ret = 0;
    if(pj_mutex_lock(jitter_buffer->jb_mutex) != PJ_SUCCESS) {
        return -1;
    }   
    *nack_seq_num = 0;
    //retrans or not
    if(!WaitingForRetransmit(jitter_buffer)) {
        goto ON_RET;   
    }
    //init nack list
    Frame_Buffer * frameList = &jitter_buffer->frameList;
    Frame_Buffer * first_frame = frameList->next;
    if(first_frame == frameList) {
        goto ON_RET;
    }
    int first_seq = frame_buffer_getFirstSeq(first_frame);
    if(first_seq == -1) //error
    {
        ret = -1;
        goto ON_RET;
    }
    {
        //no first packet in the frame
        JTPacket *packet = first_frame->session_info.packetList.next;
        if(packet != &first_frame->session_info.packetList && 
            !packet->isFirst) {
            if(first_seq == 0) first_seq = 0xFFFF;
            else first_seq--;
        }
    }
    Frame_Buffer *last_frame = frameList->prev;
    if(last_frame == frameList) {
        ret = 0;
        goto ON_RET;
    }
    int last_seq = frame_buffer_getHighSeq(last_frame);
    if(last_seq == -1) //error
    {
        ret = -1;
        goto ON_RET;
    }
    int i;
    jitter_buffer->nack_seq_num = 0;
    for(i = first_seq; i <= last_seq && jitter_buffer->nack_seq_num < 
        MAX_NACK_SEQ_NUM; ++i) {
        jitter_buffer->nack_seq_internal[jitter_buffer->nack_seq_num++] = i; 
    }
    if(jitter_buffer->nack_seq_num == 0) {
        goto ON_RET;
    }
    //create nack list frame by frame
    Frame_Buffer *frame = frameList->next;
    while(frame != frameList) {
        if(jitter_buffer->nack_mode == NACK_MODE_HYBRID) {
            frame_buffer_buildSoftNackList(frame,  jitter_buffer->nack_seq_internal, jitter_buffer->nack_seq_num, rtt_ms); 
        } else if(jitter_buffer->nack_mode == NACK_MODE_HARD) {
            frame_buffer_buildHardNackList(frame, jitter_buffer->nack_seq_internal, jitter_buffer->nack_seq_num);
        }
        frame = frame->next;
    }
    //build external nack list
    int j = 0;
    for(i = 0; i < jitter_buffer->nack_seq_num; ++i) {
        if(jitter_buffer->nack_seq_internal[i] >= 0) {
            jitter_buffer->nack_seq[j++] = (pj_uint16_t)jitter_buffer->nack_seq_internal[i];
        }
    }
    jitter_buffer->nack_seq_num = j;
    *nack_seq_list = jitter_buffer->nack_seq;
    *nack_seq_num = jitter_buffer->nack_seq_num;
    ON_RET:
    pj_mutex_unlock(jitter_buffer->jb_mutex);
    return ret;
    
}
