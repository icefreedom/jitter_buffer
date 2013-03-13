#include <jitter_buffer/jitter_buffer_interface.h>
#include <jitter_buffer/jitter_utility.h>
#include <stdio.h>

void testJitterBufferInterface() {
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
    //init jitter buffer
    Jitter_Buffer_Interface jbi;
    if(jitter_buffer_interface_init(&jbi, pool, 100, NACK_MODE_HYBRID, 0, 500)      != 0) {
        printf("failed to create jitter buffer\n");
        return;
    }
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
    payload[0] = 0x65;
    if(jitter_buffer_interface_insert_packet(&jbi, seq, ts, frame_type,
            payload, size,  isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    //end packet
    seq = base_seq + 1; isMarketPacket = PJ_TRUE;
    if(jitter_buffer_interface_insert_packet(&jbi, seq, ts, frame_type,
            payload, size, isMarketPacket) != 0) 
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    Frame_Buffer *frame;
    #define MAX_PACKET_CNT 100
    pjmedia_frame frames[MAX_PACKET_CNT];
    int cnt ;
    cnt = jitter_buffer_interface_getFrameForDecoding(&jbi, &frame, frames, MAX_PACKET_CNT, frame_type);
    if(cnt == 0) {
        printf("failed to get a frame\n");
    } else 
        printf("get a frame, cnt:%d\n", cnt);
    pj_timestamp start_decode_ts;
    pj_get_timestamp(&start_decode_ts);
    pj_sub_timestamp32(&start_decode_ts, 50);
    jitter_buffer_interface_frameDecoded(&jbi, frame, &start_decode_ts);   
    //destory pool, pjlib
    pj_pool_release(pool);
    pj_shutdown();
}

int main() {
    testJitterBufferInterface();
    return 0;
}
