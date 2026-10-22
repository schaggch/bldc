/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../hwconf/SPEEDPED75100/bike_logic.h"
#include "../../hwconf/SPEEDPED75100/pedal_gate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
// Fail on stderr instead of opening the Windows CRT assertion dialog.
#undef assert
#define assert(x) do { if (!(x)) { fprintf(stderr,"CHECK FAILED line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static bike_state b;
static bike_config c;
static uint32_t t;
static float watts;
static void sample(bool sh,bool ah,bool wheel,unsigned ms) {
    while(ms--) bike_sample(&b,++t,sh,ah,wheel,2,1000);
}
static bool drive(bool pedal,bool throttle,float rpm) {
    return bike_drive_allowed(&b,&c,t,1000,pedal,throttle,rpm,watts,NULL,NULL);
}
static void init(void) {
    t=0; watts=0; bike_init(&b,t,false,false,false); c=bike_defaults();
    c.v[BIKE_PEDAL_KMH]=25; c.v[BIKE_AH_KMH]=6;
    c.v[BIKE_MM]=2100; c.v[BIKE_SH_DUTY]=0; c.v[BIKE_NO_WHEEL_ERPM]=0; c.v[BIKE_NO_WHEEL_WATTS]=0; c.v[BIKE_NO_WHEEL_SECONDS]=5;
}
int main(void) {
    c=bike_defaults(); assert(bike_valid(&c));
    c.v[BIKE_START]=0; assert(!bike_valid(&c));
    c=bike_defaults(); c.v[BIKE_DUTY]=2; assert(!bike_valid(&c));
    init(); assert(!drive(false,true,0)); assert(drive(true,true,0));
    sample(false,true,false,3); assert(bike_mode(&b)==0); assert(!drive(false,true,0));
    sample(true,true,false,3); assert(bike_mode(&b)==2); assert(drive(false,true,0));
    sample(true,false,false,3); assert(bike_mode(&b)==1); assert(drive(false,true,1000));
    assert(drive(false,true,1500)); assert(drive(false,true,-1500));
    assert(drive(false,true,1450)); assert(drive(false,true,1400));
    // SH release with AH inactive inhibits PAS for exactly one second.
    sample(false,false,false,3); assert(b.inhibit); uint32_t released=t;
    assert(!drive(true,true,0)); sample(false,false,false,999); assert(b.inhibit);
    sample(false,false,false,1); assert(!b.inhibit && t-released==1000);
    assert(!drive(false,true,0));
    // Held AH exempts the release from PAS inhibition, but cannot drive alone.
    init(); sample(true,true,false,3); assert(bike_mode(&b)==2);
    sample(false,true,false,3); assert(!b.inhibit && bike_mode(&b)==0);
    assert(!drive(false,true,0)); assert(drive(true,true,0));
    // A held SH at startup must be released before assistance becomes available.
    init(); bike_init(&b,0,true,false,false); assert(bike_mode(&b)==0);
    sample(false,false,false,3); sample(true,false,false,3); assert(bike_mode(&b)==1);
    // Wheel timing: 2100 mm at one pulse/second = 7.56 km/h.
    init(); sample(false,false,true,3); sample(false,false,false,997);
    sample(false,false,true,3); assert(fabsf(bike_kmh(&b,&c,1000,t)-7.56f)<0.01f);
    sample(true,true,true,3); assert(!drive(false,true,0)); // AH 6 km/h
    sample(false,true,true,3); assert(drive(true,true,0)); // PAS 25 km/h
    // Telemetry uses the same value, independent of motor RPM and throttle.
    assert(fabsf(bike_kmh(&b,&c,1000,t)/3.6f-2.1f)<0.0001f);
    c.v[BIKE_PULSES]=2;
    assert(fabsf(bike_kmh(&b,&c,1000,t)-3.78f)<0.001f);
    c.v[BIKE_MM]=1000;
    assert(fabsf(bike_kmh(&b,&c,1000,t)-1.8f)<0.001f);
    t=b.last_pulse+10000; assert(bike_kmh(&b,&c,1000,t)>0.0f);
    t++; assert(bike_kmh(&b,&c,1000,t)==0.0f);
    assert(drive(true,true,0)); // Fallback cap zero disables the replacement limit.
    // After a long gap, require two new wheel edges, not an old/new interval.
    sample(false,true,false,3); sample(false,true,true,3);
    assert(bike_kmh(&b,&c,1000,t)==0.0f);
    sample(false,true,false,997); sample(false,true,true,3);
    assert(fabsf(bike_kmh(&b,&c,1000,t)-1.8f)<0.001f);
    // Startup/single edge and unsigned tick wrap.
    init(); assert(bike_kmh(&b,&c,1000,t)==0.0f);
    t=UINT32_MAX-500; bike_init(&b,t,false,false,false);
    sample(false,false,true,3); assert(bike_kmh(&b,&c,1000,t)==0.0f);
    sample(false,false,false,997); sample(false,false,true,3);
    assert(fabsf(bike_kmh(&b,&c,1000,t)-7.56f)<0.001f);
    // Configurable missing-wheel timeout and strict positive power threshold.
    init(); c.v[BIKE_NO_WHEEL_SECONDS]=5; c.v[BIKE_NO_WHEEL_WATTS]=200;
    c.v[BIKE_NO_WHEEL_ERPM]=1000; watts=201;
    t=4999; assert(drive(true,true,1200)); assert(!b.fallback_active);
    t=5000; watts=200; assert(drive(true,true,1200));
    watts=-500; assert(drive(true,true,1200)); assert(!b.fallback_active);
    watts=201; drive(true,false,1200); assert(!b.fallback_active);
    assert(!drive(true,true,1200)); assert(b.fallback_active);
    watts=0; assert(!drive(true,true,1000)); // Dropped power must not release latch.
    assert(!drive(true,true,950)); assert(drive(true,true,949));
    assert(drive(true,true,970)); assert(!drive(true,true,-1000));
    sample(false,false,true,3); assert(!drive(true,true,1200)); // One edge insufficient.
    sample(false,false,false,997); sample(false,false,true,3);
    assert(drive(true,true,1200)); assert(!b.fallback_active);
    // Configured timeout also works below the two-second display freshness.
    c.v[BIKE_NO_WHEEL_SECONDS]=1; t+=1000; watts=201;
    assert(bike_wheel_fresh(&b,t,1000)); assert(!drive(true,true,1200));
    t+=5000; watts=201; assert(!drive(true,true,1200));
    c.v[BIKE_NO_WHEEL_ERPM]=0; assert(drive(true,true,1200)); assert(!b.fallback_active);
    // Timeout zero disables fallback, including an already latched limit.
    init(); c.v[BIKE_NO_WHEEL_ERPM]=1000; c.v[BIKE_NO_WHEEL_SECONDS]=0;
    watts=1; t=100000; assert(drive(true,true,1000));
    assert(!b.fallback_active && !b.fallback_limited);
    b.fallback_active=true; b.fallback_limited=true;
    assert(drive(true,true,1000));
    assert(!b.fallback_active && !b.fallback_limited);
    // Unsigned tick wrap preserves an enabled timeout.
    c.v[BIKE_NO_WHEEL_SECONDS]=2; t=UINT32_MAX-500; bike_init(&b,t,false,false,false);
    t+=1999; assert(drive(true,true,1000)); t++; assert(!drive(true,true,1000));
    // SH has no separate eRPM limit and remains usable without a wheel signal.
    init();
    sample(true,false,false,3); t+=5000; assert(drive(false,true,100));
    assert(drive(false,true,5000));
    // SH duty cap and progressive command ramp.
    init(); sample(true,false,false,3);
    float duty_cap=-1;
    assert(!bike_drive_allowed(&b,&c,t,1000,false,true,5000,0,&duty_cap,NULL));
    c.v[BIKE_SH_DUTY]=10;
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,5000,0,&duty_cap,NULL));
    assert(fabsf(duty_cap-0.1f)<0.0001f); // No SH eRPM gate in duty mode.
    float duty=0;
    for(unsigned i=0;i<2000;i++) {
        float next=bike_sh_duty_step(duty,0.9f,duty_cap,0.001f);
        assert(next>=duty && next<=duty_cap); duty=next;
    }
    assert(fabsf(duty-duty_cap)<0.0001f);
    float early=bike_sh_duty_step(0.07f,0.9f,duty_cap,0.001f)-0.07f;
    float late=bike_sh_duty_step(0.095f,0.9f,duty_cap,0.001f)-0.095f;
    assert(late>0 && late<early);
    assert(bike_sh_duty_step(duty,0,duty_cap,0.001f)==0);
    assert(bike_sh_duty_step(duty,0.04f,duty_cap,0.001f)==0.04f);
    assert(bike_sh_duty_step(-0.07f,-0.9f,duty_cap,0.001f)<-0.07f);
    assert(bike_sh_duty_step(0.09f,0.9f,0.05f,0.001f)<=0.05f);
    assert(bike_sh_duty_step(0,0.9f,0,0.001f)==0);
    assert(bike_sh_duty_step(0,0.9f,duty_cap,10.0f)<=0.0041f);
    sample(false,false,false,3);
    assert(!bike_drive_allowed(&b,&c,t,1000,true,true,0,0,&duty_cap,NULL));
    init(); sample(true,true,false,3);
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,0,0,&duty_cap,NULL) && duty_cap<0);
    init(); sample(true,true,true,1); sample(false,false,false,3);
    assert(bike_mode(&b)==0 && !b.have_pulse);
    // Duty speed limits taper at 80%, 90%, 100%; permission stays armed.
    assert(bike_limit_scale(20,25)==1);
    assert(fabsf(bike_limit_scale(22.5f,25)-0.5f)<0.00001f);
    assert(bike_limit_scale(25,25)==0 && bike_limit_scale(30,25)==0);
    assert(bike_limit_scale(NAN,25)==0 && bike_limit_scale(0,0)==1);
    init(); c.v[BIKE_MM]=2500; b.have_period=true; b.have_pulse=true;
    b.last_pulse=t; b.period=400; // 22.5 km/h = 90% of PAS limit.
    float factor=-1;
    assert(bike_drive_allowed(&b,&c,t,1000,true,true,0,0,&duty_cap,&factor));
    assert(fabsf(factor-0.5f)<0.00001f && duty_cap<0);
    b.period=360;
    assert(bike_drive_allowed(&b,&c,t,1000,true,true,0,0,&duty_cap,&factor) && factor==0);
    b.period=400;
    assert(bike_drive_allowed(&b,&c,t,1000,true,true,0,0,&duty_cap,&factor) && factor>0);
    assert(!bike_drive_allowed(&b,&c,t,1000,false,true,0,0,&duty_cap,&factor) && factor==0);
    init(); sample(true,true,false,3); c.v[BIKE_MM]=1500;
    b.have_period=true; b.have_pulse=true; b.last_pulse=t; b.period=1000; // AH 5.4 / 6.
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,0,0,&duty_cap,&factor));
    assert(fabsf(factor-0.5f)<0.00001f);
    c.v[BIKE_NO_WHEEL_ERPM]=1000; c.v[BIKE_NO_WHEEL_SECONDS]=1; t=b.last_pulse+1000;
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,950,1,&duty_cap,&factor));
    assert(fabsf(factor-0.25f)<0.00001f); // Strictest limit wins.
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,1000,0,&duty_cap,&factor) && factor==0);
    sample(true,false,false,3); c.v[BIKE_SH_DUTY]=10;
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,900,0,&duty_cap,&factor));
    assert(fabsf(factor-0.5f)<0.00001f && fabsf(duty_cap-0.1f)<0.00001f);
    // SH->AH follows normal ADC positive ramp from actual SH output, not
    // the uncapped throttle ramp. Recovery from a limit is slew-limited too.
    assert(fabsf(bike_duty_rise(0.1f,0.8f,0.95f,1.0f,0.002f)-0.1019f)<0.00001f);
    assert(bike_duty_rise(0,0.8f,0.95f,1,0.002f)<0.002f);
    assert(bike_duty_rise(0.8f,0.1f,0.95f,1,0.002f)==0.1f);
    assert(bike_duty_rise(0.8f,0,0.95f,1,0.002f)==0);
    assert(bike_duty_rise(0.1f,0.8f,0.95f,0,0.002f)==0.8f);
    assert(bike_duty_rise(0.1f,-0.8f,0.95f,1,0.002f)>-0.002f);
    // Zero disables speed limits, not the PAS/button interlocks or fallback.
    init(); c.v[BIKE_PEDAL_KMH]=0;
    b.have_period=true; b.have_pulse=true; b.period=100; b.last_pulse=t;
    b.limited=true;
    assert(drive(true,true,5000) && !b.limited);
    assert(!drive(false,true,5000));
    assert(bike_drive_allowed(&b,&c,t,1000,true,true,5000,0,&duty_cap,&factor) && factor==1);
    sample(true,true,false,3); c.v[BIKE_AH_KMH]=0;
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,5000,0,&duty_cap,&factor) && factor==1);
    c.v[BIKE_NO_WHEEL_ERPM]=1000; c.v[BIKE_NO_WHEEL_SECONDS]=1; t=b.last_pulse+1000;
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,1000,1,&duty_cap,&factor) && factor==0);
    c.v[BIKE_NO_WHEEL_SECONDS]=0;
    assert(bike_drive_allowed(&b,&c,t,1000,false,true,1000,1,&duty_cap,&factor) && factor==1);
    assert(!b.fallback_active && !b.fallback_limited);
    // Slow riding must remain measurable (one magnet, 2.1 m circumference).
    init(); sample(false,false,true,3); sample(false,false,false,2997);
    sample(false,false,true,3);
    assert(fabsf(bike_kmh(&b,&c,1000,t)-2.52f)<0.001f);
    // Deceleration: overdue edges reduce the estimate without a jump to zero.
    t=b.last_pulse+4000;
    assert(fabsf(bike_kmh(&b,&c,1000,t)-1.89f)<0.001f);
    uint32_t saved_period=b.period, saved_pulse=b.last_pulse;
    drive(true,false,0); drive(true,true,0);
    assert(b.period==saved_period && b.last_pulse==saved_pulse && b.have_period);
    // A <2 ms disturbance never becomes a wheel pulse.
    init(); sample(false,false,true,1); sample(false,false,false,5);
    assert(!b.have_pulse && !b.have_period);
    // Timer wrap does not bypass the SH-release inhibition.
    init(); t=UINT32_MAX-5; bike_init(&b,t,false,false,false);
    sample(true,false,false,3); sample(false,false,false,3);
    assert(b.inhibit); sample(false,false,false,1000); assert(!b.inhibit);
    // PAS direction, start count and timeout are runtime settings.
    speedped_gate p; speedped_gate_init(&p,0,false,500,2);
    p.low_majority=false; p.start_periods=3;
    unsigned now=0;
    for (unsigned cycle=0; cycle<4; cycle++) {
        for (unsigned j=0;j<50;j++) speedped_gate_sample(&p,++now,true);
        for (unsigned j=0;j<150;j++) speedped_gate_sample(&p,++now,false);
        assert(speedped_gate_allowed(&p,now)==(cycle>=3));
    }
    now=p.last_valid+501; assert(!speedped_gate_allowed(&p,now));
    puts("Bike: modes, interlock, PAS inhibit, speed, missing sensor, debounce and wrap passed.");
}
