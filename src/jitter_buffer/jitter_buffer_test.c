#include <jitter_buffer/jitter_buffer.h>
#include <jitter_buffer/jitter_utility.h>
#include <pjmedia/frame.h>
#include <stdio.h>

void testJitterBuffer() {
    //init frame buffer
    //init pjlib
    pj_init();
    //init pool
    pj_caching_pool cp;
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    pj_pool_t *pool = pj_pool_create( &cp.factory,     /* pool factory     */
               "testDecodeState",       /* pool name.       */
               1024,        /* init size        */
               512,        /* increment size       */
               NULL         /* callback on error    */
               );
    //init jitter buffer;
    Jitter_Buffer *jitter_buffer;
    if(jitter_buffer_create(&jitter_buffer, pool, 100, NACK_MODE_HYBRID, 0, 500) != 0) {
        printf("failed to create jitter buffer\n");
        return;
    }
    jitter_buffer_start(jitter_buffer);
    //insert a packet
    pj_uint16_t base_seq = 100;
    pj_uint16_t seq = base_seq + 1;
    pj_uint32_t base_ts = (pj_uint32_t)GetCurrentTimeMs();
    pj_uint32_t ts = base_ts - 12000;
    pjmedia_frame_type frame_type = PJMEDIA_FRAME_TYPE_VIDEO;
    char payload[2] = {0x65, 0x11};
    int size = 2;
    pj_bool_t isFirstPacket = PJ_FALSE;
    pj_bool_t isMarketPacket = PJ_FALSE;
    seq = base_seq;
    payload[0] = 0x11;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    //end packet
    seq = base_seq + 1; isMarketPacket = PJ_FALSE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    Frame_Buffer *frame;
    /*frame = jitter_buffer_getCompleteFrameForDecoding(jitter_buffer, 0);
    if(frame == 0) {
        printf("failed to get a complete frame\n");
        frame = jitter_buffer_getFrameForDecoding(jitter_buffer);
        if(frame == NULL) {
            printf("not a decodable frame\n");
        } else
            printf("incomplete frame ts:%u, packets count:%d, isComplete:%d, isKeyFrame:%d\n", frame->ts, frame->session_info.packets_count,
                frame->session_info.isComplete, frame->session_info.isKeyFrame);

    } else {
        printf("complete frame ts:%u, packets count:%d, isComplete:%d, isKeyFrame:%d\n", frame->ts, frame->session_info.packets_count,
                frame->session_info.isComplete, frame->session_info.isKeyFrame);
    }*/
    // a new frame
    payload[0] = 0x65;
    ts = base_ts - 10000;  seq = base_seq + 3; isMarketPacket = PJ_TRUE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    /*frame = jitter_buffer_getCompleteFrameForDecoding(jitter_buffer, 0);
    if(frame == 0) {
        printf("failed to get a complete frame\n");
        frame = jitter_buffer_getFrameForDecoding(jitter_buffer);
        if(frame == NULL) {
            printf("not a decodable frame\n");
        } else
            printf("incomplete frame ts:%u, packets count:%d, isComplete:%d, isKeyFrame:%d\n", frame->ts, frame->session_info.packets_count,
                frame->session_info.isComplete, frame->session_info.isKeyFrame);

    } else {
        printf("complete frame ts:%u, packets count:%d, isComplete:%d, isKeyFrame:%d\n", frame->ts, frame->session_info.packets_count,
                frame->session_info.isComplete, frame->session_info.isKeyFrame);
    }*/
    // a complete frame
    ts = base_ts - 8500; seq = base_seq + 4; isMarketPacket = PJ_FALSE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    ts = base_ts - 8500; seq = base_seq + 5; isMarketPacket = PJ_FALSE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    ts = base_ts - 8500; seq = base_seq + 6; isMarketPacket = PJ_TRUE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    frame = jitter_buffer_getCompleteFrameForDecoding(jitter_buffer, 0);
    if(frame == 0) {
        printf("failed to get a complete frame\n");
        frame = jitter_buffer_getFrameForDecoding(jitter_buffer);
        if(frame == NULL) {
            printf("not a decodable frame\n");
        } else
            printf("incomplete frame ts:%u, packets count:%d, isComplete:%d, isKeyFrame:%d\n", frame->ts, frame->session_info.packets_count,
                frame->session_info.isComplete, frame->session_info.isKeyFrame);

    } else {
        printf("complete frame ts:%u, packets count:%d, isComplete:%d, isKeyFrame:%d\n", frame->ts, frame->session_info.packets_count,
                frame->session_info.isComplete, frame->session_info.isKeyFrame);
    }
    
    //destroy pool, pjlib
    pj_pool_release(pool);
    pj_shutdown();

}

int main() {
    testJitterBuffer();
    return 0;
}
