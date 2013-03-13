#include <jitter_buffer/jitter_buffer_interface.h>
#include <jitter_buffer/jitter_utility.h>
#include <pj/pool.h>
#include <stdio.h>
#include <pj/log.h>
#include <utility/time_parser.h>
#include <pj/compat/string.h>
enum { kMaxVideoDelayMs       = 2000 }; // in ms
#define VCM_MIN(a, b)   (a) < (b) ? (a) :(b)
#define VCM_MAX(a, b)   (a) > (b) ? (a) :(b)

#define THIS_FILE "jbi.c"
PJ_DEF(int) jitter_buffer_interface_init(Jitter_Buffer_Interface *jbi, pj_pool_t *pool,
    int max_number_of_frames, NACK_MODE nack_mode,
    int low_rtt_threshold_ms, int high_rtt_threshold_ms) {
    //init jitter buffer
    if(jitter_buffer_create(&jbi->jitter_buffer, pool, max_number_of_frames, nack_mode, 
            low_rtt_threshold_ms, high_rtt_threshold_ms) != 0)
        return -1;
    jitter_buffer_start(jbi->jitter_buffer);
    //init timing
    if(timing_init(&jbi->timing, GetCurrentTimeMs(), pool) != 0)
        return -1;
    //init mutex
    /*if(pj_mutex_create_simple(pool, NULL, &jbi->jbi_mutex) != PJ_SUCCESS)
        return -1;*/
    return 0;
}
PJ_DEF(void) jitter_buffer_interface_stop(Jitter_Buffer_Interface *jbi) {
    /*if(pj_mutex_lock(jbi->jbi_mutex) != 0) {
        return;
    }*/
    jitter_buffer_stop(jbi->jitter_buffer);
    timing_reset(&jbi->timing);
    //pj_mutex_unlock(jbi->jbi_mutex);
}

PJ_DEF(void) jitter_buffer_interface_reset(Jitter_Buffer_Interface *jbi) {
    /*if(pj_mutex_lock(jbi->jbi_mutex) != 0) {
        return;
    }*/
    jitter_buffer_reset(jbi->jitter_buffer);
    timing_reset(&jbi->timing);
    //pj_mutex_unlock(jbi->jbi_mutex);
}


PJ_DEF(int) jitter_buffer_interface_insert_packet(Jitter_Buffer_Interface *jbi,
       pj_uint16_t seq, pj_uint32_t ts, pjmedia_frame_type frame_type,
        char *payload, int size, pj_bool_t isMarketPacket) {
    int ret = 0;
    Frame_Buffer *frame;
    pj_ssize_t now_ms;
    pj_ssize_t render_time;
    /*if(pj_mutex_lock(jbi->jbi_mutex) != 0) {
        return -1;
    }*/
    now_ms = GetCurrentTimeMs();
    render_time = timing_getRenderTimeMs(&jbi->timing, ts, now_ms);
    //check render time
    if(render_time < 0) {
        jitter_buffer_flush(jbi->jitter_buffer);
        timing_reset(&jbi->timing);
        ret = -1;
        goto ON_RET;
    } else if(render_time < now_ms - kMaxVideoDelayMs) {
        //over 2 seconds delay
        jitter_buffer_flush(jbi->jitter_buffer);
        timing_reset(&jbi->timing);
        ret = -1;
        goto ON_RET;
    } else if(timing_getTargetDelay(&jbi->timing) > kMaxVideoDelayMs) {
        jitter_buffer_flush(jbi->jitter_buffer);
        timing_reset(&jbi->timing);
        ret = -1;
        goto ON_RET;
    }
    //insert packet
    if(jitter_buffer_insert_packet(jbi->jitter_buffer, seq, ts, frame_type, payload,
                                size, PJ_FALSE, isMarketPacket) != 0) {
        ret = -1;
        goto ON_RET;
    }
    jitter_buffer_setRenderTime(jbi->jitter_buffer, ts, render_time);
    ON_RET:
    //pj_mutex_unlock(jbi->jbi_mutex);
    return ret;
}

static Frame_Buffer* FrameForDecoding(Jitter_Buffer_Interface *jbi, 
                                        pj_uint32_t max_wait_time_ms, 
                                        pj_ssize_t next_render_time_ms) {
    pj_ssize_t now_ms = GetCurrentTimeMs();
    pj_uint32_t wait_time_ms = timing_getMaxWaitingTimeMs(&jbi->timing, 
                                next_render_time_ms, now_ms);
    Frame_Buffer *frame = jitter_buffer_getCompleteFrameForDecoding(jbi->jitter_buffer,
                            0);
    if(frame == NULL && max_wait_time_ms == 0 && wait_time_ms > 0) 
        return NULL;
    if(frame == NULL && wait_time_ms == 0)  {
        // no time to wait
        frame = jitter_buffer_getFrameForDecoding(jbi->jitter_buffer);
    }
    if(frame == NULL) {
        //wait for a completed frame
        frame = jitter_buffer_getCompleteFrameForDecoding(jbi->jitter_buffer,
                                max_wait_time_ms);
    }
    if(frame == NULL) {
        now_ms = GetCurrentTimeMs();
        if((wait_time_ms = timing_getMaxWaitingTimeMs(&jbi->timing,
                                next_render_time_ms, now_ms)) > 0) {
            //still have time
            return NULL;
        }
        frame = jitter_buffer_getFrameForDecoding(jbi->jitter_buffer);
            
    }
    return frame;
}

static Frame_Buffer* GetFrameForDecoding(Jitter_Buffer_Interface *jbi,
                                        pj_uint32_t max_wait_time_ms) {
    pj_timestamp startTs;
    pj_get_timestamp(&startTs);
    pjmedia_frame_type frame_type;
    pj_ssize_t next_render_time_ms;
    pj_ssize_t timestamp = jitter_buffer_nextTimestamp(jbi->jitter_buffer, max_wait_time_ms,
                                    &frame_type, &next_render_time_ms);

    if(timestamp == -1)  {
        PJ_LOG(4, (THIS_FILE, "failed to get next timestamp"));
        return NULL;
    }
    pj_uint32_t ts = (pj_uint32_t)timestamp;
    //update timing
    timing_setRequiredDelay(&jbi->timing, jitter_buffer_estimateJitterMS(jbi->jitter_buffer));
    timing_updateCurrentDelay(&jbi->timing, ts);
    pj_timestamp endTs;
    pj_get_timestamp(&endTs);
    pj_int32_t tmp_wait_time_ms = (pj_int32_t)max_wait_time_ms - pj_elapsed_msec(&startTs, &endTs);
    pj_uint32_t new_max_wait_time_ms = (pj_uint32_t)VCM_MAX(tmp_wait_time_ms, 0);
    //get frame
    Frame_Buffer *frame = FrameForDecoding(jbi, new_max_wait_time_ms, next_render_time_ms);
    if(frame != NULL) {
       if(frame->nack_count == 0) {
            timing_incomingTimestamp(&jbi->timing, ts, TimestampToMs(&frame->latest_packet_timestamp));
        }
    } 
    return frame;

}
enum {kMaxWaitTimeMS = 50};
PJ_DEF(int) jitter_buffer_interface_getFrameForDecoding(Jitter_Buffer_Interface *jbi, 
                                Frame_Buffer **ret_frame, pjmedia_frame * frames, 
                                const int max_cnt, pjmedia_frame_type frame_type) {
    Frame_Buffer *frame = NULL;
    int cnt = 0;
    /*if(pj_mutex_lock(jbi->jbi_mutex) != 0) {
        return -1;
    }*/
    //get a frame
    frame = GetFrameForDecoding(jbi, kMaxWaitTimeMS);
    //check frame type
    if(frame == NULL || frame->frame_type != frame_type)
        return 0;
    //transform
    if(frame != NULL) {
        JTPacket *packet = frame->session_info.packetList.next;
        cnt = 0;
        while(packet != &frame->session_info.packetList) {
            frames[cnt].type = frame_type;
            frames[cnt].timestamp.u64 = frame->ts;
            frames[cnt].bit_info = 0;
            frames[cnt].buf = packet->data_ptr;
            frames[cnt].size = packet->size;
            packet = packet->next;
            ++cnt;
            if(cnt >= max_cnt) //too many packets
                break;
        }
    }
    *ret_frame = frame;
    //pj_mutex_unlock(jbi->jbi_mutex);
    return cnt;
}

PJ_DEF(int) jitter_buffer_interface_frameDecoded(Jitter_Buffer_Interface *jbi, 
                                                Frame_Buffer *frame, pj_timestamp *decode_start_ts) {
    pj_timestamp now_ts;
    /*if(pj_mutex_lock(jbi->jbi_mutex) != 0) {
        return -1;
    }*/
    jitter_buffer_changeFrameStatus(jbi->jitter_buffer, frame, FRAME_STATUS_FREE);
    pj_get_timestamp(&now_ts);
    timing_stopDecodeTimer(&jbi->timing, decode_start_ts, &now_ts);
    //pj_mutex_unlock(jbi->jbi_mutex);
}
PJ_DEF(int) jitter_buffer_interface_createNackList(Jitter_Buffer_Interface *jbi,
            pj_uint32_t rtt_ms, pj_uint16_t **nack_seq_list,
            pj_uint32_t *nack_seq_num)  {
    return jitter_buffer_createNackList(jbi->jitter_buffer, rtt_ms, nack_seq_list, nack_seq_num);
}
