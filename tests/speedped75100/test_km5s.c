#include <stdio.h>
#include <stdlib.h>
#include "../../hwconf/SPEEDPED75100/km5s_protocol.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"KM5S check failed at %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static const uint8_t example_s[]={0x3A,0x1A,0x53,7,2,0x40,1,0x0A,0x8F,0x32,0x7D,0xFF,1,0x0D,0x0A};
static const uint8_t example_r[]={0x3A,0x1A,0x52,2,0xAB,0x80,0x99,1,0x0D,0x0A};
static unsigned feed(km5s_parser *p,const uint8_t *data,unsigned n,uint32_t start,km5s_frame *f) {
    unsigned valid=0;
    for(unsigned i=0;i<n;i++) valid+=km5s_feed(p,data[i],start+i,100,f);
    return valid;
}
static void checksum(uint8_t *packet,unsigned n) {
    unsigned sum=0;
    for(unsigned i=1;i<n-4;i++) sum+=packet[i];
    packet[n-4]=sum&255; packet[n-3]=sum>>8;
}
int main(void) {
    CHECK(km5s_wheel_ms(0)==65535 && km5s_wheel_ms(NAN)==65535);
    CHECK(km5s_wheel_ms(-1)==65535 && km5s_wheel_ms(INFINITY)==65535);
    CHECK(km5s_wheel_ms(30.0f/3.6f)==273);
    CHECK(km5s_wheel_ms(1)==2277 && km5s_wheel_ms(0.5f)==4554);
    CHECK(km5s_wheel_ms(0.001f)==65534 && km5s_wheel_ms(10000)==1);
    CHECK(km5s_current_code(10)==30 && km5s_current_code(100)==255);
    CHECK(km5s_current_code(-10)==0 && km5s_current_code(NAN)==0);
    CHECK(km5s_temperature_code(25)==25 && km5s_temperature_code(35)==35);
    CHECK(km5s_temperature_code(-1)==0 && km5s_temperature_code(NAN)==0);
    CHECK(km5s_temperature_code(300)==255);
    km5s_parser p={0}; km5s_frame f;
    CHECK(feed(&p,example_s,sizeof(example_s),0,&f)==1);
    CHECK(f.command=='S' && f.length==7 && f.data[5]==50 && p.frames==1);
    const uint8_t extended[]={0x3A,0x1A,0x53,10,2,0x99,1,10,0xDE,30,0xDE,3,1,0xEC,0xE7,3,13,10};
    CHECK(feed(&p,extended,sizeof(extended),20,&f)==1);
    CHECK(f.length==10 && f.data[7]==3 && f.data[8]==1 && f.data[9]==236);
    // Restore the PDF request used by the subsequent reply assertions.
    CHECK(feed(&p,example_s,sizeof(example_s),40,&f)==1);
    uint8_t reply[KM5S_REPLY_SIZE];
    CHECK(km5s_reply(&f,0,30,1000,0,25,35,reply)==13);
    CHECK(reply[8]==157 && reply[5]==30 && reply[6]==3 && reply[7]==232);
    uint8_t copy[KM5S_REPLY_SIZE]; memcpy(copy,reply,13); checksum(copy,13);
    CHECK(!memcmp(copy,reply,13));
    CHECK(feed(&p,example_r,sizeof(example_r),30,&f)==1);
    CHECK(km5s_reply(&f,0x80,255,65535,0x22,25,35,reply)==15);
    CHECK(reply[2]=='R' && reply[4]==0x80 && reply[8]==0x22);
    CHECK(reply[6]==0xFF && reply[7]==0xFF);
    CHECK(reply[3]==7 && reply[9]==25 && reply[10]==35);
    memcpy(copy,reply,15); checksum(copy,15); CHECK(!memcmp(copy,reply,15));
    CHECK(km5s_reply(&f,0,0,82,0,25,35,reply)==15);
    const uint8_t captured_reply[]={0x3A,0x1A,0x52,7,0,0,0,0x52,0,25,35,1,1,13,10};
    CHECK(!memcmp(reply,captured_reply,sizeof(captured_reply)));
    // A dropped/interrupted frame must never create an output or poison
    // the next valid request, including timer wrap and embedded delimiters.
    for(unsigned cut=1;cut<sizeof(example_s);cut++) {
        p=(km5s_parser){0};
        CHECK(!feed(&p,example_s,cut,0,&f));
        CHECK(feed(&p,example_r,sizeof(example_r),500,&f)==1 && p.timeouts==1);
    }
    p=(km5s_parser){0};
    CHECK(feed(&p,example_s,sizeof(example_s),UINT32_MAX-7,&f)==1 && p.timeouts==0);
    uint8_t altered[15]; memcpy(altered,example_s,15);
    altered[4]=0x3A; altered[5]=0x0D; altered[6]=0x0A; checksum(altered,15);
    CHECK(feed(&p,altered,15,30,&f)==1 && f.data[0]==0x3A);
    // Every single-byte bit error must invalidate the example before a
    // subsequent correct S frame can be accepted.
    for(unsigned pos=0;pos<15;pos++) for(unsigned bit=0;bit<8;bit++) {
        p=(km5s_parser){0}; memcpy(altered,example_s,15); altered[pos]^=1U<<bit;
        CHECK(!feed(&p,altered,15,0,&f));
        CHECK(feed(&p,example_s,15,30,&f)==1);
    }
    p=(km5s_parser){0}; memcpy(altered,example_s,15); altered[9]=64; checksum(altered,15);
    CHECK(!feed(&p,altered,15,0,&f) && p.invalid_settings==1);
    CHECK(feed(&p,example_r,10,20,&f)==1);
    // Bad lengths / random prefix bytes cannot overflow the fixed buffer.
    p=(km5s_parser){0}; uint8_t noise[]={0,0x3A,0x1A,'S',255,0x3A,0x3A,0x1A,'R',7};
    CHECK(!feed(&p,noise,sizeof(noise),0,&f));
    CHECK(feed(&p,example_s,15,20,&f)==1);
    for(unsigned i=0;i<100000;i++) km5s_feed(&p,(uint8_t)(i*13U),100+i,100,&f);
    CHECK(p.used<=KM5S_MAX_INPUT);
    puts("KM5S: PDF frames, checksums, replies, handshake, embedded markers, all bit errors, truncation, wrap and resync passed.");
    return 0;
}
