/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SPEEDPED_BIKE_LOGIC_H
#define SPEEDPED_BIKE_LOGIC_H
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "bike_defaults.h"

enum { BIKE_MM, BIKE_PULSES, BIKE_PEDAL_KMH, BIKE_AH_KMH,
    BIKE_TIMEOUT, BIKE_MAGNETS, BIKE_START, BIKE_DUTY,
    BIKE_NO_WHEEL_SECONDS, BIKE_NO_WHEEL_WATTS, BIKE_NO_WHEEL_ERPM, BIKE_SH_DUTY,
    BIKE_SOUND_START, BIKE_SOUND_RIDE, BIKE_SOUND_VOLUME, BIKE_SOUND_TEMPO, BIKE_FIELDS };
typedef struct { uint32_t v[BIKE_FIELDS]; } bike_config;
static inline bike_config bike_defaults(void) {
    // Zero speed limits disable that limiter; PAS/button interlocks still apply.
    return (bike_config){{SPEEDPED_BIKE_DEFAULT_VALUES}};
}
static inline bool bike_valid(const bike_config *c) {
    const uint32_t lo[] = {100,1,0,0,50,1,1,0,0,0,0,0,0,0,0,25};
    const uint32_t hi[] = {5000,64,150,30,5000,64,32,1,120,100000,100000,95,1,1,100,400};
    for (unsigned i=0; i<BIKE_FIELDS; i++)
        if (c->v[i]<lo[i] || c->v[i]>hi[i]) return false;
    return true;
}
typedef struct { bool stable, candidate; uint32_t since; } bike_input;
static inline void bike_input_init(bike_input *s, bool active, uint32_t now) {
    *s = (bike_input){active, active, now};
}
static inline bool bike_input_sample(bike_input *s, bool active, uint32_t now, uint32_t debounce) {
    if (active != s->candidate) { s->candidate=active; s->since=now; }
    if (s->stable != active && (uint32_t)(now-s->since)>=debounce) {
        s->stable=active;
        return true;
    }
    return false;
}
typedef struct {
    bike_input sh, ah, wheel;
    uint32_t last_pulse, period, inhibit_since;
    bool have_pulse, have_period, inhibit, fallback_active, fallback_limited;
    bool buttons_armed, limited;
} bike_state;
static inline void bike_init(bike_state *s, uint32_t now, bool sh, bool ah, bool wheel) {
    *s=(bike_state){0};
    s->last_pulse=now;
    bike_input_init(&s->sh, sh, now); bike_input_init(&s->ah, ah, now);
    bike_input_init(&s->wheel, wheel, now);
    // Held buttons at boot/configuration do not grant assist.
    s->buttons_armed = !sh && !ah;
}
static inline bool bike_sample(bike_state *s, uint32_t now, bool sh, bool ah, bool wheel,
        uint32_t debounce, uint32_t second) {
    bike_input_sample(&s->ah, ah, now, debounce);
    bool sh_changed = bike_input_sample(&s->sh, sh, now, debounce);
    if (!s->sh.stable && !s->ah.stable) s->buttons_armed=true;
    if (sh_changed && !s->sh.stable && !s->ah.stable) {
        s->inhibit=true; s->inhibit_since=now;
    }
    if (s->inhibit && (uint32_t)(now-s->inhibit_since)>=second) s->inhibit=false;
    if (bike_input_sample(&s->wheel, wheel, now, debounce) && s->wheel.stable) {
        uint32_t elapsed=now-s->last_pulse;
        if (s->have_pulse && elapsed && elapsed<=10U*second) {
            s->period=elapsed; s->have_period=true;
            s->fallback_active=false; s->fallback_limited=false;
        } else {
            s->have_period=false;
        }
        s->have_pulse=true; s->last_pulse=now;
    }
    return s->inhibit;
}
// 0 = pedal, 1 = walking assist, 2 = starting assist.
static inline unsigned bike_mode(const bike_state *s) {
    return s->buttons_armed && s->sh.stable ? (s->ah.stable ? 2U : 1U) : 0U;
}
static inline bool bike_wheel_fresh(const bike_state *s, uint32_t now, uint32_t hz) {
    return s->have_period && s->period && (uint32_t)(now-s->last_pulse)<=10U*hz;
}
// Shared by drive limits, terminal diagnostics and standard speed telemetry.
static inline float bike_kmh(const bike_state *s, const bike_config *c, uint32_t hz, uint32_t now) {
    if (!bike_wheel_fresh(s,now,hz)) return 0.0f;
    // Once the next pulse is overdue, the old speed is no longer justified.
    // This also avoids holding a high value for two seconds, then dropping to
    // zero precisely while a speed limiter is slowing the wheel down.
    uint32_t age=now-s->last_pulse;
    uint32_t interval=age>s->period ? age : s->period;
    return (float)c->v[BIKE_MM]*3.6f*hz/(1000.0f*c->v[BIKE_PULSES]*interval);
}
static inline float bike_limit_scale(float speed,float limit) {
    if(!isfinite(speed) || !isfinite(limit) || limit<0) return 0;
    if(limit==0) return 1;
    return fminf(1.0f,fmaxf(0.0f,(limit-fabsf(speed))/(limit*0.2f)));
}
static inline bool bike_drive_allowed(bike_state *s, const bike_config *c, uint32_t now,
        uint32_t hz, bool pedal, bool request, float erpm, float watts, float *sh_duty_cap,
        float *speed_scale) {
    if(speed_scale) *speed_scale=0;
    if (sh_duty_cap) *sh_duty_cap=-1.0f; // Negative: not in SH duty mode.
    unsigned mode=bike_mode(s);
    // Wheel acquisition must not depend on throttle or limiter state.
    // Positive battery-input power only; regeneration cannot trigger this.
    if (c->v[BIKE_NO_WHEEL_ERPM]==0 || c->v[BIKE_NO_WHEEL_SECONDS]==0) {
        s->fallback_active=false; s->fallback_limited=false;
    } else if (request && (uint32_t)(now-s->last_pulse)>=c->v[BIKE_NO_WHEEL_SECONDS]*hz &&
            watts>(float)c->v[BIKE_NO_WHEEL_WATTS]) {
        s->fallback_active=true;
    }
    if (mode==0 && (!pedal || s->inhibit)) return false;
    if (mode==1 && sh_duty_cap) {
        *sh_duty_cap=(float)c->v[BIKE_SH_DUTY]/100.0f;
        if (*sh_duty_cap<=0.0f) return false;
        // Duty SH replaces its eRPM mode limit. The independent missing-wheel
        // fallback remains in effect if the user explicitly enabled it.
        if(speed_scale) {
            *speed_scale=s->fallback_active ? bike_limit_scale(erpm,c->v[BIKE_NO_WHEEL_ERPM]) : 1.0f;
            s->limited=false; s->fallback_limited=false;
            return true;
        }
        if (s->fallback_active) {
            float rpm=fabsf(erpm), cap=(float)c->v[BIKE_NO_WHEEL_ERPM];
            if (rpm>=cap) s->fallback_limited=true;
            if (rpm<cap*0.95f) s->fallback_limited=false;
            if (s->fallback_limited) return false;
        }
        s->limited=false;
        return true;
    }
    float limit=mode==1 ? 0.0f : (float)c->v[mode==2 ? BIKE_AH_KMH : BIKE_PEDAL_KMH];
    float speed=mode==1 ? fabsf(erpm) : bike_kmh(s,c,hz,now);
    if(speed_scale) {
        *speed_scale=bike_limit_scale(speed,limit);
        if(s->fallback_active) *speed_scale=fminf(*speed_scale,bike_limit_scale(erpm,c->v[BIKE_NO_WHEEL_ERPM]));
        s->limited=false; s->fallback_limited=false;
        return true; // Limit tapering must not reset the permission/ADC ramp.
    }
    if (s->fallback_active) {
        float rpm=fabsf(erpm), cap=(float)c->v[BIKE_NO_WHEEL_ERPM];
        if (rpm>=cap) s->fallback_limited=true;
        if (rpm<cap*0.95f) s->fallback_limited=false;
        if (s->fallback_limited) return false;
    }
    if (limit==0.0f) s->limited=false;
    else {
        if (speed>=limit) s->limited=true;
        if (speed<limit*0.95f) s->limited=false;
    }
    return !s->limited;
}

// SH duty command ramp: nominal 0..cap in 0.5 s; above 80% progressively
// reduce the rise rate to 10% at the cap. Decreases are never delayed here.
static inline float bike_sh_duty_step(float previous,float request,float cap,float dt) {
    if (!isfinite(request) || !isfinite(previous) || !isfinite(cap) || cap<=0 ||
            !isfinite(dt) || dt<=0) return 0;
    if (dt>0.02f) dt=0.02f; // No catch-up jump after a scheduling gap.
    float target=fminf(fabsf(request),cap);
    float old=previous*request<0 ? 0 : fminf(fabsf(previous),cap);
    if (target<=old) return copysignf(target,request);
    float ratio=old/cap;
    float factor=ratio<=0.8f ? 1.0f : fmaxf(0.1f,1.0f-4.5f*(ratio-0.8f));
    return copysignf(fminf(target,old+2.0f*cap*dt*factor),request);
}
// Slew increases of the final duty command using the configured ADC ramp time.
// Tracks the actual output across SH->AH and recovery from a speed limit.
// Limit reductions and throttle reductions are never delayed by this helper.
static inline float bike_duty_rise(float previous,float request,float max_duty,float ramp,float dt) {
    if(!isfinite(previous) || !isfinite(request) || !isfinite(max_duty) || max_duty<=0 ||
            !isfinite(ramp) || !isfinite(dt) || dt<=0) return 0;
    float target=fminf(fabsf(request),max_duty);
    float old=previous*request<0 ? 0 : fminf(fabsf(previous),max_duty);
    if(target<=old || ramp<=0.01f) return copysignf(target,request);
    return copysignf(fminf(target,old+max_duty*fminf(dt,0.02f)/ramp),request);
}
#endif
