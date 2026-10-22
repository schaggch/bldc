#include <stdint.h>
static inline void buffer_append_uint32(uint8_t *d,uint32_t n,int32_t *i) {
    for(int shift=24;shift>=0;shift-=8) d[(*i)++]=(uint8_t)(n>>shift);
}
static inline uint32_t buffer_get_uint32(const uint8_t *d,int32_t *i) {
    uint32_t n=0;
    for(int j=0;j<4;j++) n=(n<<8)|d[(*i)++];
    return n;
}
