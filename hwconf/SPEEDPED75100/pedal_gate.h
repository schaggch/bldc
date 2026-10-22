/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SPEEDPED_PEDAL_GATE_H_
#define SPEEDPED_PEDAL_GATE_H_

#include <stdbool.h>
#include <stdint.h>

#define SPEEDPED_MAGNETS 5U
#define SPEEDPED_START_PERIODS 2U

// Time units are unsigned OS ticks (milliseconds in the host tests).
typedef struct {
    uint32_t timeout, debounce;
    uint32_t candidate_since, low_since, high_since, last_valid, last_sample;
    uint32_t low_duration, period;
    unsigned valid_periods;
    unsigned start_periods;
    bool low_majority;
    bool stable_low, candidate_low, have_low_start, have_low_duration;
} speedped_gate;

static inline void speedped_gate_init(speedped_gate *s, uint32_t now,
        bool low, uint32_t timeout, uint32_t debounce) {
    *s = (speedped_gate){.timeout = timeout, .debounce = debounce,
        .candidate_since = now, .last_sample = now,
        .stable_low = low, .candidate_low = low,
        .start_periods = SPEEDPED_START_PERIODS, .low_majority = true};
}

static inline void speedped_gate_expire(speedped_gate *s, uint32_t now) {
    if ((s->valid_periods && (uint32_t)(now - s->last_valid) > s->timeout) ||
            (s->have_low_start && (uint32_t)(now - s->low_since) > s->timeout + s->debounce)) {
        s->valid_periods = 0;
        s->have_low_start = false;
        s->have_low_duration = false;
        s->period = 0;
    }
}

static inline void speedped_gate_sample(speedped_gate *s, uint32_t now, bool low) {
    speedped_gate_expire(s, now);
    s->last_sample = now;
    if (low != s->candidate_low) {
        s->candidate_low = low;
        s->candidate_since = now;
    }
    if (low == s->stable_low || (uint32_t)(now - s->candidate_since) < s->debounce) {
        return;
    }

    s->stable_low = low;
    const uint32_t edge = s->candidate_since;
    if (!low) {
        // A complete low phase needs a falling edge observed after startup/reset.
        if (s->have_low_start) {
            s->low_duration = edge - s->low_since;
            s->high_since = edge;
            s->have_low_duration = true;
        }
        return;
    }

    if (s->have_low_start && s->have_low_duration) {
        const uint32_t high_duration = edge - s->high_since;
        const uint32_t period = edge - s->low_since;
        const bool direction = s->low_majority ? s->low_duration > high_duration :
            s->low_duration < high_duration;
        if (direction && period <= s->timeout) {
            if (s->valid_periods < s->start_periods) s->valid_periods++;
            s->last_valid = now;
            s->period = period;
        } else {
            // Reverse/50:50/invalid periods revoke permission immediately.
            s->valid_periods = 0;
            s->period = 0;
        }
    }
    s->low_since = edge;
    s->have_low_start = true;
    s->have_low_duration = false;
}

static inline bool speedped_gate_allowed(speedped_gate *s, uint32_t now) {
    speedped_gate_expire(s, now);
    return s->valid_periods >= s->start_periods &&
        (uint32_t)(now - s->last_valid) <= s->timeout;
}

#endif
