/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SPEEDPED_KM5S_PROTOCOL_H
#define SPEEDPED_KM5S_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

enum { KM5S_MAX_INPUT = 18, KM5S_REPLY_SIZE = 15 };
typedef struct { uint8_t command, length, data[10]; } km5s_frame;
typedef struct {
    uint8_t buffer[KM5S_MAX_INPUT], used;
    uint32_t last_byte, frames, bad_frames, bad_checksums, timeouts, invalid_settings;
} km5s_parser;

static inline void km5s_discard(km5s_parser *p, unsigned count) {
    p->used -= count;
    memmove(p->buffer, p->buffer + count, p->used);
}

// Time units are supplied by the caller; unsigned subtraction supports wrap.
// Embedded 0x3A payload/checksum bytes are legal: never reset at every colon.
static inline bool km5s_feed(km5s_parser *p, uint8_t byte, uint32_t now,
        uint32_t timeout, km5s_frame *out) {
    if (p->used && (uint32_t)(now-p->last_byte)>timeout) {
        p->used=0; p->timeouts++;
    }
    p->last_byte=now;
    if(p->used==KM5S_MAX_INPUT) km5s_discard(p,1);
    p->buffer[p->used++]=byte;
    while(p->used) {
        if(p->buffer[0]!=0x3A) { km5s_discard(p,1); continue; }
        if(p->used<4) return false;
        const uint8_t cmd=p->buffer[2], len=p->buffer[3];
        // Actual PC capture has S length 10; preserve its three extension
        // bytes without assuming an undocumented parameter mapping.
        if(p->buffer[1]!=0x1A || !((cmd=='S' && (len==7 || len==10)) || (cmd=='R' && len==2))) {
            p->bad_frames++; km5s_discard(p,1); continue;
        }
        const unsigned total=len+8;
        if(p->used<total) return false;
        if(p->buffer[total-2]!=0x0D || p->buffer[total-1]!=0x0A) {
            p->bad_frames++; km5s_discard(p,1); continue;
        }
        uint16_t sum=0;
        for(unsigned i=1;i<4U+len;i++) sum+=p->buffer[i];
        if(p->buffer[4+len]!=(sum&255) || p->buffer[5+len]!=(sum>>8)) {
            p->bad_checksums++; km5s_discard(p,1); continue;
        }
        if(cmd=='S' && p->buffer[9]>=64) {
            p->invalid_settings++; km5s_discard(p,total); continue;
        }
        *out=(km5s_frame){0};
        out->command=cmd; out->length=len;
        memcpy(out->data,p->buffer+4,len);
        km5s_discard(p,total); p->frames++;
        return true;
    }
    return false;
}

static const uint8_t km5s_password[64]={
    137,159,134,249,88,11,250,61,33,150,3,193,118,141,209,94,
    226,68,146,158,145,127,216,62,116,230,101,211,251,54,229,247,
    20,222,59,63,35,252,142,238,23,197,84,77,147,173,210,57,
    142,223,157,97,36,160,229,237,75,80,37,113,154,88,23,120
};

// Tested display wheel code 6 uses approximately 2277 mm internally.
// Encode the measured road speed against that circumference, independently
// of the physical wheel circumference/pulse count used by SPEEDPED.
#define KM5S_DISPLAY_WHEEL_MM 2277.0f
#define KM5S_STANDSTILL_MS 65535U
static inline uint16_t km5s_wheel_ms(float speed_mps) {
    if (!isfinite(speed_mps) || speed_mps<=0.0f) return KM5S_STANDSTILL_MS;
    float ms=KM5S_DISPLAY_WHEEL_MM/speed_mps;
    if (ms>=65534.0f) return 65534U;
    if (ms<1.0f) return 1U;
    return (uint16_t)(ms+0.5f);
}
static inline uint8_t km5s_current_code(float amps) {
    if (!isfinite(amps) || amps<=0.0f) return 0;
    if (amps>=85.0f) return 255;
    return (uint8_t)(amps*3.0f+0.5f);
}
static inline uint8_t km5s_temperature_code(float temperature) {
    if (!isfinite(temperature) || temperature<0.0f) return 0;
    if (temperature>=255.0f) return 255;
    return (uint8_t)(temperature+0.5f);
}

// Reply current scale follows the PDF example (30 means 10 A), not its
// contradictory "current / 3" text. Exact displayed amperes still need verification.
static inline unsigned km5s_reply(const km5s_frame *request, uint8_t status,
        uint8_t current_x3, uint16_t wheel_ms, uint8_t fault,
        uint8_t controller_temp, uint8_t motor_temp, uint8_t out[KM5S_REPLY_SIZE]) {
    // FFFF is the verified standstill code; preserve whole-wheel periods.
    const bool settings=request->command=='S';
    const unsigned length=settings ? 5U : 7U;
    const uint8_t detail=settings ? km5s_password[request->data[5]&63] : fault;
    // Verified PC test: R data bytes6/7 are controller/motor degrees C.
    // With the PDF's short R5 reply the display reads checksum as temperature.
    const uint8_t data[KM5S_REPLY_SIZE]={0x3A,0x1A,request->command,length,status,current_x3,
        wheel_ms>>8,wheel_ms&255,detail,controller_temp,motor_temp,0,0,0,0};
    memcpy(out,data,sizeof(data));
    uint16_t sum=0;
    for(unsigned i=1;i<4U+length;i++) sum+=out[i];
    out[4+length]=sum&255; out[5+length]=sum>>8;
    out[6+length]=0x0D; out[7+length]=0x0A;
    return length+8;
}
#endif
