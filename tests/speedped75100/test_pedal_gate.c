/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../hwconf/SPEEDPED75100/pedal_gate.h"
#include <assert.h>
#include <stdio.h>

static speedped_gate gate;
static uint32_t now;
static void init(uint32_t time, bool low) {
    now = time;
    speedped_gate_init(&gate, now, low, 1000, 2);
}
static void hold(bool low, unsigned duration) {
    for (unsigned i = 0; i < duration; i++) speedped_gate_sample(&gate, ++now, low);
}
static bool allowed(void) { return speedped_gate_allowed(&gate, now); }
static void forward_start(void) {
    hold(false, 10);
    hold(true, 150); hold(false, 50);
    assert(!allowed());
    hold(true, 150); hold(false, 50);
    assert(gate.valid_periods == 1 && !allowed());
    hold(true, 3);
    assert(gate.valid_periods == 2 && allowed());
    assert(gate.period == 200);
}
int main(void) {
    // Startup in either electrical state cannot grant permission by itself.
    init(0, false); hold(false, 2000); assert(!allowed());
    init(0, true); hold(true, 2000); assert(!allowed());
    hold(false, 50); assert(!allowed());
    forward_start();

    // Two full forward periods, and no counting both edges as two magnets.
    init(0, false); forward_start();
    const uint32_t last = gate.last_valid;
    hold(true, (unsigned)(last + 1000 - now)); assert(allowed());
    hold(true, 1); assert(!allowed());
    hold(true, 2000); assert(!allowed());
    // Timeout requires a new safe start, not one pulse resuming old permission.
    hold(false, 30); forward_start();

    // Stuck high also expires, despite the preceding forward periods.
    init(0, false); forward_start(); hold(false, 1001); assert(!allowed());

    // Reverse and 50:50 duty never qualify.
    for (unsigned low = 50; low <= 100; low += 50) {
        init(0, false);
        for (unsigned i = 0; i < 20; i++) {
            hold(true, low); hold(false, 200 - low);
            assert(!allowed());
        }
    }

    // A reverse period revokes an existing permission and resets the count.
    init(0, false); forward_start();
    hold(true, 47); hold(false, 150); hold(true, 3);
    assert(!allowed() && gate.valid_periods == 0);
    hold(true, 147); hold(false, 50); hold(true, 3);
    assert(!allowed() && gate.valid_periods == 1);
    hold(true, 147); hold(false, 50); hold(true, 3); assert(allowed());

    // Noise shorter than the debounce time cannot fake pedalling.
    init(0, false);
    for (unsigned i = 0; i < 500; i++) { hold(true, 1); hold(false, 3); }
    assert(!allowed() && gate.valid_periods == 0);

    // Unsigned tick wrap must not create or permanently revoke permission.
    init(UINT32_MAX - 250, false); forward_start(); assert(allowed());
    hold(true, 1001); assert(!allowed());

    // Pedalling with intervals over one second cannot accumulate safe start.
    init(0, false);
    for (unsigned i = 0; i < 4; i++) { hold(true, 800); hold(false, 300); }
    assert(!allowed());

    // Exactly one second periods remain valid with the debounce delay.
    init(0, false);
    hold(true, 750); hold(false, 250);
    hold(true, 750); hold(false, 250);
    hold(true, 3); assert(allowed());

    // Configuration/fault reset discards previous permission and partial cycles.
    speedped_gate_init(&gate, now, true, 1000, 2);
    assert(!allowed());
    puts("SPEEDPED75100 pedal gate: all scenarios passed");
    return 0;
}
