#include <jitter_buffer/decode_state.h>
#include <jitter_buffer/frame_buffer.h>
#include <pj/pool.h>
#define EXPECT_EQ(a, b)  if((a) != (b) ) { printf("not equal, "#a" = "#b"\n"); } else {  \
                            printf("success, "#a" = "#b"\n");}
                        
void testDecodeState() {
    //setup
    Decode_State decode_state;
    decode_state_init(&decode_state);
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
    //init list alloc
    packet_alloc_t packet_alloc;
    list_alloc_init(&packet_alloc, pool);

    //init frame_buffer
    Frame_Buffer frame_buffer;
    frame_buffer_init(&frame_buffer, &packet_alloc);
    //test1:
    //insert an empty packet
    JTPacket *packet;
    char payload[2];
    jt_packet_create(&packet, &packet_alloc, 21, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, NULL, 0);
    if(frame_buffer_insert_packet(&frame_buffer, packet) != 0) {
        printf("failed to insert empty packets\n");
        //if ret > 0, free the packet
    }
    decode_state.seq = 21;
    decode_state.ts = 12000;
    jt_packet_create(&packet, &packet_alloc, 20, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, NULL, 0);
    EXPECT_EQ(PJ_TRUE, decode_state_isOldPacket(packet, &decode_state));
    jt_packet_create(&packet, &packet_alloc, 22, 15000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, NULL, 0);
    EXPECT_EQ(PJ_FALSE, decode_state_isOldPacket(packet, &decode_state));
    EXPECT_EQ(PJ_TRUE, decode_state_isOldFrame(&frame_buffer, &decode_state));
    EXPECT_EQ(PJ_FALSE, decode_state_isContinuousFrame(&frame_buffer, &decode_state));
    frame_buffer_reset(&frame_buffer);
    frame_buffer_insert_packet(&frame_buffer, packet);
    EXPECT_EQ(PJ_FALSE, decode_state_isOldFrame(&frame_buffer, &decode_state));
    EXPECT_EQ(PJ_TRUE, decode_state_isContinuousFrame(&frame_buffer, &decode_state));
    //destroy pool, pjlib
    pj_pool_release(pool);
    pj_shutdown();
    
}

int main(int argc, char **argv) {
    testDecodeState();
    return 0;
}
