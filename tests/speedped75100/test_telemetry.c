/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "buffer.h"
#include "bike_logic.h"
#include "battery_current.h"
static float battery_adc_voltage=1.55f;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"telemetry check failed line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
enum { COMM_GET_VALUES=4, COMM_GET_VALUES_SELECTIVE=50, COMM_GET_VALUES_SETUP=47, COMM_GET_VALUES_SETUP_SELECTIVE=51 };
static uint8_t packet[256], response[256];
static unsigned response_size;
static uint8_t *mempools_get_packet_buffer(void) { return packet; }
static void mempools_free_packet_buffer(uint8_t *p) { (void)p; }
static void reply_func(uint8_t *p,int32_t n) { CHECK(n<=256); memcpy(response,p,n); response_size=n; }
static bool timeout_has_timeout(void) { return false; }
static bool timeout_kill_sw_active(void) { return false; }
static struct { uint8_t controller_id; } appconf={9};
static typeof(appconf) *app_get_configuration(void) { return &appconf; }
#define FLOAT_GETTER(name) static float name(void) { return 12.0f; }
FLOAT_GETTER(mc_interface_temp_fet_filtered)
FLOAT_GETTER(mc_interface_temp_motor_filtered)
FLOAT_GETTER(mc_interface_read_reset_avg_motor_current)
static float mc_interface_read_reset_avg_input_current(void) { return speedped_battery_current_from_voltage(battery_adc_voltage); }
FLOAT_GETTER(mc_interface_read_reset_avg_id)
FLOAT_GETTER(mc_interface_read_reset_avg_iq)
FLOAT_GETTER(mc_interface_get_duty_cycle_now)
FLOAT_GETTER(mc_interface_get_input_voltage_filtered)
FLOAT_GETTER(mc_interface_get_pid_pos_now)
FLOAT_GETTER(mc_interface_read_reset_avg_vd)
FLOAT_GETTER(mc_interface_read_reset_avg_vq)
FLOAT_GETTER(mc_interface_get_distance)
FLOAT_GETTER(mc_interface_get_distance_abs)
static float mc_interface_get_rpm(void) { return 12345.0f; }
static int mc_interface_get_fault(void) { return 0; }
static float mc_interface_get_amp_hours(bool x) { (void)x; return 1; }
static float mc_interface_get_amp_hours_charged(bool x) { (void)x; return 2; }
static float mc_interface_get_watt_hours(bool x) { (void)x; return 3; }
static float mc_interface_get_watt_hours_charged(bool x) { (void)x; return 4; }
static int32_t mc_interface_get_tachometer_value(bool x) { (void)x; return 123; }
static int32_t mc_interface_get_tachometer_abs_value(bool x) { (void)x; return 234; }
static int mc_interface_get_motor_thread(void) { return 1; }
#define NTC_TEMP_MOS1_M2() 25.0f
#define NTC_TEMP_MOS2_M2() 26.0f
#define NTC_TEMP_MOS3_M2() 27.0f
#define NTC_TEMP_MOS1() 22.0f
#define NTC_TEMP_MOS2() 23.0f
#define NTC_TEMP_MOS3() 24.0f
static float mc_interface_get_battery_level(float *wh) { *wh=42; return 0.75f; }
static uint32_t mc_interface_get_odometer(void) { return 987; }
typedef struct { float current_tot,current_in_tot,ah_tot,ah_charge_tot,wh_tot,wh_charge_tot; uint8_t num_vescs; } setup_values;
static setup_values mc_interface_get_setup_values(void) { return (setup_values){1,speedped_battery_current_from_voltage(battery_adc_voltage),3,4,5,6,1}; }
typedef unsigned syssts_t;
static uint32_t now=1000;
#define CH_CFG_ST_FREQUENCY 1000U
#define MS2ST(x) (x)
static uint32_t chVTGetSystemTimeX(void) { return now; }
static syssts_t chSysGetStatusAndLockX(void) { return 0; }
static void chSysRestoreStatusX(syssts_t s) { (void)s; }
static bool pedal_started=true,bike_saving;
static struct { uint32_t last_sample; } pedal={1000};
static bike_state bike;
static bike_config bike_conf;
#include "telemetry_speed.inc"
static float mc_interface_get_speed(void) { return speedped_get_speed(); }
#include "telemetry_handlers.inc"
static int32_t read_int(unsigned pos) { int32_t i=pos; return buffer_get_int32(response,&i); }
static void request_mask(unsigned command,uint32_t mask) {
    uint8_t bytes[4]; int32_t i=0; buffer_append_uint32(bytes,mask,&i);
    telemetry(command,bytes);
}
int main(void) {
    bike_conf=bike_defaults(); bike_conf.v[BIKE_MM]=2250; bike_conf.v[BIKE_PULSES]=1;
    bike_init(&bike,now,false,false,false);
    bike.have_period=true; bike.have_pulse=true; bike.last_pulse=now; bike.period=225;
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<3);
    CHECK(response_size==9 && abs(read_int(5)-1000)<=1);
    request_mask(COMM_GET_VALUES_SETUP_SELECTIVE,1U<<3);
    CHECK(response_size==9 && abs(read_int(5)-1000)<=1);
    battery_adc_voltage=1.34f;
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<3); CHECK(abs(read_int(5)-120)<=1);
    battery_adc_voltage=1.55f;
    // Exact production speed getter and command serializers, with motor RPM unrelated to speed.
    request_mask(COMM_GET_VALUES_SETUP_SELECTIVE,1U<<6);
    CHECK(response_size==9 && response[0]==51 && read_int(5)==10000);
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<22);
    CHECK(response_size==9 && response[0]==50 && read_int(1)==0x00400000 && read_int(5)==10000);
    telemetry(COMM_GET_VALUES,NULL);
    CHECK(response_size==78 && response[0]==4 && read_int(23)==12345 && read_int(74)==10000);
    uint8_t full[256]; unsigned full_size=response_size; memcpy(full,response,full_size);
    telemetry_standard(COMM_GET_VALUES,NULL);
    CHECK(response_size==74 && !memcmp(full,response,74));
    // Mixed selection retains motor RPM first and appends measured wheel speed.
    request_mask(COMM_GET_VALUES_SELECTIVE,(1U<<7)|(1U<<22));
    CHECK(response_size==13 && read_int(5)==12345 && read_int(9)==10000);
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<7);
    CHECK(response_size==9 && read_int(5)==12345);
    telemetry(COMM_GET_VALUES_SETUP,NULL);
    CHECK(read_int(19)==10000);
    request_mask(COMM_GET_VALUES_SELECTIVE,0); CHECK(response_size==5);
    // More than one wheel magnet gives the same road speed with shorter pulse periods.
    bike_conf.v[BIKE_PULSES]=3; bike.period=75;
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<22); CHECK(read_int(5)==10000);
    // Missing wheel signal must never silently fall back to motor eRPM.
    bike.have_period=false;
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<22); CHECK(read_int(5)==0);
    bike.have_period=true; bike.period=75; now+=10001; pedal.last_sample=now;
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<22); CHECK(read_int(5)==0);
    // A stalled sampler also returns zero on both telemetry paths.
    bike.last_pulse=now; pedal.last_sample=now-21;
    request_mask(COMM_GET_VALUES_SETUP_SELECTIVE,1U<<6); CHECK(read_int(5)==0);
    request_mask(COMM_GET_VALUES_SELECTIVE,1U<<22); CHECK(read_int(5)==0);
    puts("Telemetry: real command handlers, wheel speed, legacy prefix, RPM, masks, multiple magnets and stale sensors passed.");
    return 0;
}
