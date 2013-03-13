#include <jitter_buffer/jitter_buffer.h>
#include <jitter_buffer/jitter_utility.h>
#include <pjmedia/frame.h>
#include <stdio.h>
static void printNackList(pj_uint16_t *seq_num_list, pj_uint32_t seq_num) {
    int i = 0;
    printf("nack list number:%d\n", seq_num);
    for(i = 0; i < seq_num ; ++i) {
        printf("%d ", seq_num_list[i]);
    }
    printf("\n");
}
void testNackList() {
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
    pj_uint32_t base_ts = (pj_uint32_t)GetCurrentTimeMs() -12000;
    pj_uint32_t ts = base_ts ;
    pjmedia_frame_type frame_type = PJMEDIA_FRAME_TYPE_VIDEO;
    char payload[2] = {0x65, 0x11};
    int size = 2;
    pj_bool_t isFirstPacket = PJ_FALSE;
    pj_bool_t isMarketPacket = PJ_FALSE;
    seq = base_seq;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    seq = base_seq + 3;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    seq = base_seq + 7;
    ts = base_ts + 100;
    isFirstPacket = PJ_TRUE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    
    seq = base_seq + 9;
    ts = base_ts + 100;
    isFirstPacket = PJ_FALSE;
    isMarketPacket = PJ_TRUE;
    if(jitter_buffer_insert_packet(jitter_buffer,  seq, ts, frame_type,
            payload, size, isFirstPacket, isMarketPacket) != 0)
        printf("failed to insert packet, seq:%u, ts:%u\n", seq, ts);
    pj_uint16_t *seq_num_list;
    pj_uint32_t seq_num;
    jitter_buffer_createNackList(jitter_buffer, 100, &seq_num_list, &seq_num);
    printNackList(seq_num_list, seq_num);
    //destroy pool, pjlib
    pj_pool_release(pool);
    pj_shutdown();
}
int main() {
    testNackList();
    return 0;
}
