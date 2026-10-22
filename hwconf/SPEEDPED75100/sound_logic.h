/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SPEEDPED_SOUND_LOGIC_H
#define SPEEDPED_SOUND_LOGIC_H
#include <stdint.h>
#include <stdbool.h>
/* Standalone preparation: no motor calls. Level is normalized, not volts. */
typedef struct { bool startup, ride; uint32_t volume_percent, tempo_percent; } speedped_sound_config;
typedef struct {
    bool initialized, boot_done, boot_playing, riding;
    uint32_t boot_time, start_time, note_time;
    unsigned note;
} speedped_sound_state;
typedef struct { float frequency_hz, level; } speedped_sound_output;
static inline speedped_sound_config speedped_sound_defaults(void) {
    return (speedped_sound_config){false,false,10,100};
}
static inline bool speedped_sound_valid(const speedped_sound_config *c) {
    return c->volume_percent<=100 && c->tempo_percent>=25 && c->tempo_percent<=400;
}
/* Own retro motifs, not Nintendo recordings or transcriptions.
   Caller supplies validated PAS period and fault/measurement exclusion.
   Initialize only on power-up, not on every configuration write. */
static inline speedped_sound_output speedped_sound_tick(speedped_sound_state *s,
        const speedped_sound_config *c,uint32_t now,uint32_t hz,
        bool blocked,bool idle,bool driving,bool pas_valid,uint32_t pas_period) {
    const speedped_sound_output silent={0,0};
    if(!s->initialized) { s->initialized=true; s->boot_time=now; }
    if(hz<100 || hz>1000000 || !speedped_sound_valid(c) || blocked || !c->volume_percent) {
        s->riding=false; s->boot_done=true; s->boot_playing=false; return silent;
    }
    float level=c->volume_percent/100.0f;
    if(!s->boot_done) {
        if(!c->startup || !idle || driving || (uint32_t)(now-s->boot_time)>5U*hz) {
            s->boot_done=true; s->boot_playing=false;
        } else if((uint32_t)(now-s->boot_time)>=2U*hz) {
            if(!s->boot_playing) { s->boot_playing=true; s->start_time=now; }
            uint32_t age=now-s->start_time;
            if(age<hz*80U/1000U) return (speedped_sound_output){1046.5f,level};
            if(age<hz*120U/1000U) return silent;
            if(age<hz*280U/1000U) return (speedped_sound_output){2349.3f,level};
            s->boot_done=true; s->boot_playing=false;
        }
    }
    if(!c->ride || !driving || !pas_valid || !pas_period) { s->riding=false; return silent; }
    uint64_t raw=(uint64_t)pas_period*100U/c->tempo_percent;
    uint32_t interval=raw<hz/20U ? hz/20U : raw>hz ? hz : (uint32_t)raw;
    if(!s->riding) { s->riding=true; s->note=0; s->note_time=now; }
    uint32_t elapsed=now-s->note_time;
    if(elapsed>=interval) {
        s->note=(s->note+(elapsed/interval)%8U)%8U;
        s->note_time+=(elapsed/interval)*interval;
    }
    static const float motif[]={523.25f,783.99f,659.25f,880.0f,783.99f,587.33f,698.46f,659.25f};
    return (uint32_t)(now-s->note_time)<interval*3U/4U ?
        (speedped_sound_output){motif[s->note],level} : silent;
}
#endif
