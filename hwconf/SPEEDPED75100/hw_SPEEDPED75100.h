/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef HW_SPEEDPED75100_H_
#define HW_SPEEDPED75100_H_

#include <stdbool.h>
#include "stock_defaults.h"
#include "../flipsky/hw_75_100_V2.h"

#undef HW_NAME
#define HW_NAME "SPEEDPED75100"
#define HW_SPEEDPED75100
bool speedped_install_stock_once(void);

// PPM/PB6 is reserved for the single-wire, active-low pedal sensor.
void speedped_pedal_configure(void);
bool speedped_pedal_allowed(void);
bool speedped_bike_allowed(bool throttle_requested, float *sh_duty_cap, float *speed_scale);
float speedped_sh_duty_step(float previous, float request, float cap, float dt);
float speedped_duty_rise(float previous, float request, float max_duty, float ramp, float dt);
bool speedped_sound_watchdog_ok(void);
void speedped_sound_stop(void);
float speedped_get_speed(void); // Hall C wheel speed in m/s, unsigned.
unsigned char speedped_pin_levels(void); // Bits 0..3: SH, AH, wheel, PAS; 1 = GND.

#endif
