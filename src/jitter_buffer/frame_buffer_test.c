#include <jitter_buffer/frame_buffer.h>
#include <jitter_buffer/packet.h>
#include <pjlib.h>
#include <stdio.h>

void test_frame_buffer() {
    //init pjlib
    pj_init();
    //init pool
    pj_caching_pool cp;
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    pj_pool_t *pool = pj_pool_create( &cp.factory,     /* pool factory     */
               "testFrameBuffer",       /* pool name.       */
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
    jt_packet_create(&packet, &packet_alloc, 21, 12000, PJMEDIA_FRAME_TYPE_EMPTY,
                PJ_FALSE, PJ_FALSE, NULL, 0);
    if(frame_buffer_insert_packet(&frame_buffer, packet) != 0) {
        printf("failed to insert empty packets\n");
        //if ret > 0, free the packet
    }
    //insert a media packet
    payload[0] = 0x65; payload[1] = 0x11;
    jt_packet_create(&packet, &packet_alloc, 22, 15000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    int ret;
    if((ret = frame_buffer_insert_packet(&frame_buffer, packet)) != 0) {
        printf("failed to insert packet, seq:%d, ts:%d, ret:%d\n", 22, 15000, ret);
    }
    jt_packet_create(&packet, &packet_alloc, 22, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    if((ret = frame_buffer_insert_packet(&frame_buffer, packet)) != 0) {
        printf("failed to insert packet, seq:%d, ts:%d, ret:%d\n", 22, 12000, ret);
    }
    jt_packet_create(&packet, &packet_alloc, 22, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    if((ret = frame_buffer_insert_packet(&frame_buffer, packet)) != 0) {
        printf("duplicat to insert packet, seq:%d, ts:%d, ret:%d\n", 22, 12000, ret);
    }
    printf("packets in frame:%d\n", frame_buffer_get_packets_count(&frame_buffer));
    
    //test2:
    //change status to decoding
    //insert a packet
    frame_buffer_setStatus(&frame_buffer, FRAME_STATUS_DECODING);
    jt_packet_create(&packet, &packet_alloc, 23, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    if((ret = frame_buffer_insert_packet(&frame_buffer, packet)) != 0) {
        printf("failed to insert packet, seq:%d, ts:%d, ret:%d\n", 23, 12000, ret);
    }
    //test3:
    //reset frame_buffer
    //insert a packet
    frame_buffer_reset(&frame_buffer);
    jt_packet_create(&packet, &packet_alloc, 24, 15000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    if((ret = frame_buffer_insert_packet(&frame_buffer, packet)) != 0) {
        printf("failed to insert packet, seq:%d, ts:%d, ret:%d\n", 24, 15000, ret);
    }
    printf("packets in frame:%d\n", frame_buffer_get_packets_count(&frame_buffer));
    

    //destroy pool
    pj_pool_release(pool);
    //destroy pjlib
    pj_shutdown();
}

int main() {
    test_frame_buffer();
    return 0;
}
