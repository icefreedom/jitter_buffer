#include <jitter_buffer/nal_type.h>
#include <stdio.h>
void print_nal_type(NAL_TYPE nal_type) {
    switch(nal_type) {
        case NAL_TYPE_COMPLETE: printf("nal complete\n"); break;
        case NAL_TYPE_START: printf("nal start\n"); break;
        case NAL_TYPE_INCOMPLETE: printf("nal incomplete\n"); break;
        case NAL_TYPE_END: printf("nal end\n"); break;
    }
}
void test_nal_type() {
    //testA
    char payloadA[2] = {0x01, 0x01};
    NAL_TYPE nal_type = distinguish_nal_type(payloadA, 2);
    print_nal_type(nal_type); //NAL_TYPE_COMPLETE
    //testB
    char payloadB[2] = {0x24, 0x01};
    nal_type = distinguish_nal_type(payloadB, 2); 
    print_nal_type(nal_type); //NAL_TYPE_COMPLETE
    //testC
    char payloadC[2] = {0x1C, 0x81};
    nal_type = distinguish_nal_type(payloadC, 2); //NAL_TYPE_START
    print_nal_type(nal_type);
    //testD
    char payloadD[2] = {0x1C, 0x41}; 
    nal_type = distinguish_nal_type(payloadD, 2); 
    print_nal_type(nal_type); //NAL_TYPE_END
    //testE
    char payloadE[2] = {0x1c, 0x21};
    nal_type = distinguish_nal_type(payloadE, 2);
    print_nal_type(nal_type); //NAL_TYPE_INCOMPLETE
}

int main() {
    test_nal_type();
    return 0;
}
