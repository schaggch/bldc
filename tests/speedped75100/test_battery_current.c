/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "hw_SPEEDPED75100.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"battery check failed line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static uint16_t ADC_Value[18];
enum { MOTOR_TYPE_BLDC, MOTOR_TYPE_DC, MOTOR_TYPE_FOC };
static struct { struct { int motor_type; } m_conf; } fixture_motor;
#define motor_now() (&fixture_motor)
#include "battery_getters.inc"
static void set_voltage(float voltage) { ADC_Value[ADC_IND_EXT2]=(uint16_t)lroundf(voltage*4096.0f/V_REG); }
int main(void) {
    CHECK(fabsf(speedped_battery_current_from_voltage(1.34f)-1.2f)<0.0001f);
    CHECK(fabsf(speedped_battery_current_from_voltage(1.55f)-10.0f)<0.0001f);
    const float zero=1.34f-1.2f*0.21f/8.8f;
    CHECK(fabsf(speedped_battery_current_from_voltage(zero))<0.0001f);
    CHECK(speedped_battery_current_from_voltage(1.0f)<0);
    CHECK(speedped_battery_current_from_voltage(3.0f)>50);
    CHECK(!speedped_battery_voltage_valid(0) && !speedped_battery_voltage_valid(NAN));
    CHECK(!speedped_battery_voltage_valid(3.3f) && speedped_battery_voltage_valid(1.55f));
    for(int type=MOTOR_TYPE_BLDC;type<=MOTOR_TYPE_FOC;type++) {
        fixture_motor.m_conf.motor_type=type;
        for(int point=0;point<3;point++) {
            float volts=point==0?1.34f:point==1?1.55f:1.0f;
            float expected=speedped_battery_current_from_voltage(volts);
            set_voltage(volts);
            CHECK(fabsf(mc_interface_get_tot_current_in()-expected)<0.025f);
            CHECK(fabsf(mc_interface_get_tot_current_in_filtered()-expected)<0.025f);
            CHECK(fabsf(mcpwm_foc_get_tot_current_in_motor(false)-expected)<0.025f);
            CHECK(fabsf(mcpwm_foc_get_tot_current_in_filtered_motor(false)-expected)<0.025f);
        }
    }
    // Repeated idle offset calls may not erase a real peripheral load.
    set_voltage(1.34f); float before=GET_INPUT_CURRENT();
    for(int i=0;i<100;i++) { GET_INPUT_CURRENT_OFFSET(); MEASURE_INPUT_CURRENT_OFFSET(); }
    CHECK(GET_INPUT_CURRENT()==before && fabsf(before-1.2f)<0.025f);
    float brake=0.8f,decoded_level2=-1,read_voltage2=1.55f;
#include "battery_adc2.inc"
    CHECK(brake==0 && decoded_level2==0 && read_voltage2==1.55f);
    puts("ACS758: calibration, signed readings, all production current getters, idle-load preservation and ADC2 brake reservation passed.");
    return 0;
}
