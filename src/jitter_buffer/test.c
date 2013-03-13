#include <pj/types.h>
#include <stdio.h>
int main() {
    pj_ssize_t max32 = (pj_int64_t)(1) << 32;
    printf("max32:%ld, pj_int64_t:%d\n", max32, sizeof(pj_int64_t));
    return 0;
}
