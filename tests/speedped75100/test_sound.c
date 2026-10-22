#include "../../hwconf/SPEEDPED75100/sound_logic.h"
#include <assert.h>
#include <stdio.h>
static speedped_sound_state s;
static speedped_sound_config c;
static speedped_sound_output tick(uint32_t t,bool blocked,bool idle,bool drive,bool pas,uint32_t period) {
    return speedped_sound_tick(&s,&c,t,1000,blocked,idle,drive,pas,period);
}
static void reset(void) { s=(speedped_sound_state){0}; c=speedped_sound_defaults(); }
int main(void) {
    reset(); assert(tick(0,false,true,false,false,0).frequency_hz==0);
    assert(tick(2100,false,true,false,false,0).frequency_hz==0);
    reset(); c.startup=true;
    assert(tick(0,false,true,false,false,0).frequency_hz==0);
    assert(tick(1999,false,true,false,false,0).frequency_hz==0);
    assert(tick(2000,false,true,false,false,0).frequency_hz==1046.5f);
    assert(tick(2080,false,true,false,false,0).frequency_hz==0);
    assert(tick(2120,false,true,false,false,0).frequency_hz==2349.3f);
    assert(tick(2280,false,true,false,false,0).frequency_hz==0);
    assert(s.boot_done && !s.boot_playing);
    assert(tick(4000,false,true,false,false,0).frequency_hz==0);
    reset(); c.startup=true;
    tick(0,false,true,false,false,0); tick(2000,false,true,false,false,0);
    assert(tick(2001,true,true,false,false,0).frequency_hz==0);
    assert(tick(2002,false,true,false,false,0).frequency_hz==0);
    reset(); c.startup=true; tick(0,false,true,false,false,0);
    assert(tick(2100,false,false,true,false,0).frequency_hz==0 && s.boot_done);
    reset(); c.ride=true;
    assert(tick(0,false,false,true,true,200).frequency_hz==523.25f);
    assert(tick(149,false,false,true,true,200).frequency_hz==523.25f);
    assert(tick(150,false,false,true,true,200).frequency_hz==0);
    assert(tick(200,false,false,true,true,200).frequency_hz==783.99f);
    assert(tick(201,false,false,true,false,200).frequency_hz==0);
    assert(tick(202,false,false,true,true,200).frequency_hz==523.25f);
    assert(tick(203,false,true,false,true,200).frequency_hz==0);
    assert(tick(204,true,false,true,true,200).frequency_hz==0);
    c.tempo_percent=0; assert(tick(205,false,false,true,true,200).frequency_hz==0);
    c.tempo_percent=200;
    assert(tick(300,false,false,true,true,200).frequency_hz==523.25f);
    assert(tick(400,false,false,true,true,200).frequency_hz==783.99f);
    reset(); c.ride=true; tick(UINT32_MAX-99,false,false,true,true,200);
    assert(tick(100,false,false,true,true,200).frequency_hz==783.99f);
    reset(); c.ride=true; tick(0,false,false,true,true,UINT32_MAX);
    assert(tick(1000,false,false,true,true,UINT32_MAX).frequency_hz==783.99f);
    reset(); c.ride=true; tick(0,false,false,true,true,1);
    assert(tick(50,false,false,true,true,1).frequency_hz==783.99f);
    c.volume_percent=101; assert(tick(51,false,false,true,true,1).frequency_hz==0);
    puts("Sound: startup, cancellation, PAS tempo, mute, bounds and timer wrap passed.");
}

