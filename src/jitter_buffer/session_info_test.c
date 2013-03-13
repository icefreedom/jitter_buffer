#include <jitter_buffer/session_info.h>
#include <jitter_buffer/packet.h>
#include <pjlib.h>
void printList(int *seqList, int num) {
    printf("\n");
    for(int i = 0; i < num; ++i) {
        printf("%d  ", seqList[i]);
    }
    printf("\n");
}
void makeSeqList(int *seqList, int num, int start) {
    for(int i = 0; i < num; ++i) {
        seqList[i] = start++;
    }
}
void test_session_info() {
    //init pjlib
    pj_init();
    //init pool
    pj_caching_pool cp;
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    pj_pool_t *pool = pj_pool_create( &cp.factory,     /* pool factory     */
               "testSessionInfo",       /* pool name.       */
               1024,        /* init size        */
               512,        /* increment size       */
               NULL         /* callback on error    */
               );
    //init list alloc
    packet_alloc_t packet_alloc;
    list_alloc_init(&packet_alloc, pool);
    //init session_info
    Session_Info session_info;
    session_info_init(&session_info, &packet_alloc);

    //test1: complete frame
    JTPacket *packet;
    char payload[2] = {0x65, 0xab};
    //seq = 5, ts = 3000, isFirst = true, isMarket = false
    jt_packet_create(&packet, &packet_alloc, 5, 3000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    if(session_info_insert_packet(&session_info, packet) != 0)
        printf("failed to insert packet into session_info\n");
    else
        printf("success to insert packet into session_info\n");
    
    //seq = 6, ts = 3000, isFirst = false, isMarket = false
    payload[0] = 0x13; payload[1] = 0xac;
    jt_packet_create(&packet, &packet_alloc, 6, 3000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);
    //seq = 7, ts = 3000, isFirst = false, isMarket = true
    payload[0] = 0x26; payload[1] = 0x13;
    jt_packet_create(&packet, &packet_alloc, 7, 3000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_TRUE, payload, 2);
    session_info_insert_packet(&session_info, packet);
    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count, 
                session_info.isComplete);
    //build nack
    int seq_num_list[10] ;
    makeSeqList(seq_num_list, 10, 4);
    session_info_buildSoftNackList(&session_info, seq_num_list, 10, 100);
    printList(seq_num_list, 10);

    session_info_makeDecodable(&session_info);
    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);


    //test2: miss first packet
    session_info_reset(&session_info);
    //seq = 9, ts = 6000, isFirst = false, isMarket = false
    payload[0] = 0x1c; payload[1] = 0x13; //FU-A
    jt_packet_create(&packet, &packet_alloc, 9, 6000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);    
    //seq = 10, ts = 6000, isFirst = false, isMarket = false
    payload[0] = 0x1c; payload[1] = 0x42; //FU-A, end nal unit
    jt_packet_create(&packet, &packet_alloc, 10, 6000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);

    //seq = 11, ts = 6000, isFirst = false, isMarket = true
    payload[0] = 0x04; payload[1] = 0x11;
    jt_packet_create(&packet, &packet_alloc, 11, 6000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_TRUE, payload, 2);
    session_info_insert_packet(&session_info, packet);
    
    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);
    //build nack
    makeSeqList(seq_num_list, 10, 7);
    session_info_buildSoftNackList(&session_info, seq_num_list, 10, 100);
    printList(seq_num_list, 10);
    //make decodable
    session_info_makeDecodable(&session_info);
    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);

    //test3: miss middle packets
    session_info_reset(&session_info);
    //seq = 12, ts = 9000, isFirst = true, isMarket = false
    payload[0] = 0x1c; payload[1] = 0x83; //FU-A
    jt_packet_create(&packet, &packet_alloc, 12, 9000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);
    //seq = 15, ts = 9000, isFirst = false, isMarket = false
    payload[0] = 0x1c; payload[1] = 0x12; //FU-A
    jt_packet_create(&packet, &packet_alloc, 15, 9000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);

    //seq = 17, ts = 9000, isFirst = false, isMarket = false
    payload[0] = 0x04; payload[1] = 0x11;
    jt_packet_create(&packet, &packet_alloc, 17, 9000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);
    //seq = 18, ts = 9000, isFirst = false, isMarket = true
    payload[0] = 0x04; payload[1] = 0x11;
    jt_packet_create(&packet, &packet_alloc, 18, 9000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_TRUE, payload, 2);
    session_info_insert_packet(&session_info, packet);

    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);
    //build nack
    makeSeqList(seq_num_list, 10, 10);
    session_info_set_previous_frame_loss(&session_info, PJ_FALSE);
    //rtt is large
    session_info_buildSoftNackList(&session_info, seq_num_list, 10, 2000);
    printList(seq_num_list, 10);
    //rtt small
    makeSeqList(seq_num_list, 10, 10);
    session_info_buildSoftNackList(&session_info, seq_num_list, 10, 100);
    printList(seq_num_list, 10);
    //make decodable
    session_info_makeDecodable(&session_info);
    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);
    //test4: miss end packets
    session_info_reset(&session_info);
    session_info_set_previous_frame_loss(&session_info, PJ_FALSE);
    //seq = 19, ts = 12000, isFirst = true, isMarket = false
    payload[0] = 0x1c; payload[1] = 0x83; //FU-A
    jt_packet_create(&packet, &packet_alloc, 19, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_TRUE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);
    //seq = 20, ts = 12000, isFirst = false, isMarket = false
    payload[0] = 0x1c; payload[1] = 0x42; //FU-A, end nal unit
    jt_packet_create(&packet, &packet_alloc, 20, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);

    //seq = 21, ts = 12000, isFirst = false, isMarket = false
    payload[0] = 0x04; payload[1] = 0x11;
    jt_packet_create(&packet, &packet_alloc, 21, 12000, PJMEDIA_FRAME_TYPE_VIDEO,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);

    //empty packet
    //seq = 24, ts = 12000, isFirst = false, isMarket = false 
    payload[0] = 0x04; payload[1] = 0x11;
    jt_packet_create(&packet, &packet_alloc, 24, 12000, PJMEDIA_FRAME_TYPE_EMPTY,
                PJ_FALSE, PJ_FALSE, payload, 2);
    session_info_insert_packet(&session_info, packet);

    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);
    //build nack
    makeSeqList(seq_num_list, 10, 18);
    session_info_buildSoftNackList(&session_info, seq_num_list, 10, 100);
    printList(seq_num_list, 10);
    //make decodable
    session_info_makeDecodable(&session_info);
    
    printf("packets_count = %d, isComplete = %d\n", session_info.packets_count,
                session_info.isComplete);
    //test5: empty packets
       
    //destroy pool
    pj_pool_release(pool);
    //destroy pjlib
    pj_shutdown();    
}

int main() {
    test_session_info();
    return 0;
}
