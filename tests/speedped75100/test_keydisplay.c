/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"adapter check failed line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
typedef enum { FAULT_CODE_NONE, FAULT_CODE_UNDER_VOLTAGE, FAULT_CODE_ABS_OVER_CURRENT } mc_fault_code;
static uint32_t clock_ms;
static float speed_mps, battery_amps, fet_temp, motor_temp;
static mc_fault_code fault;
static bool adc_ok=true;
static unsigned writes, tx_size;
static uint8_t tx[15];
static int uart1;
#define HW_UART_DEV uart1
#define MS2ST(x) (x)
#define ST2MS(x) (x)
static void chSysLock(void) {}
static void chSysUnlock(void) {}
static uint32_t chVTGetSystemTimeX(void) { return clock_ms; }
static void commands_printf(const char *format,...) { (void)format; }
static void terminal_register_command_callback(const char *name,const char *help,void *args,void (*fn)(int,const char **)) {
    CHECK(!strcmp(name,"keydisplay")); (void)help; (void)args; (void)fn;
}
static float speedped_get_speed(void) { return speed_mps; }
static float mc_interface_get_tot_current_in_filtered(void) { return battery_amps; }
static float mc_interface_temp_fet_filtered(void) { return fet_temp; }
static float mc_interface_temp_motor_filtered(void) { return motor_temp; }
static mc_fault_code mc_interface_get_fault(void) { return fault; }
static const char *mc_interface_fault_to_string(mc_fault_code f) { (void)f; return "test"; }
static bool app_adc_range_ok(void) { return adc_ok; }
static void sdWrite(int *port,const uint8_t *data,unsigned size) {
    CHECK(port==&uart1 && size<=sizeof(tx)); memcpy(tx,data,size); tx_size=size; writes++;
}
#include "../../hwconf/SPEEDPED75100/keydisplay_adapter.h"
static void send_frame(uint8_t command,const uint8_t *data,unsigned length) {
    uint8_t bytes[18]={0x3A,0x1A,command,length}; unsigned sum=0;
    memcpy(bytes+4,data,length);
    for(unsigned i=1;i<length+4;i++) sum+=bytes[i];
    bytes[length+4]=sum&255; bytes[length+5]=sum>>8;
    bytes[length+6]=13; bytes[length+7]=10;
    for(unsigned i=0;i<length+8;i++) { clock_ms++; speedped_keydisplay_byte(bytes[i]); }
}
int main(void) {
    speedped_keydisplay_init();
    const uint8_t settings[]={2,153,1,10,222,30,222,3,1,236};
    speed_mps=30.0f/3.6f; battery_amps=10; fet_temp=25; motor_temp=35;
    send_frame('S',settings,10);
    CHECK(writes==1 && tx_size==13 && tx[8]==229);
    CHECK(keydisplay_have_s && !memcmp(keydisplay_s.data,settings,10));
    // Every display request is observed only: even zero PWM, walk and light.
    const uint8_t request[]={0,0x90};
    send_frame('R',request,2);
    CHECK(writes==2 && tx_size==15 && tx[5]==30);
    CHECK(tx[6]==1 && tx[7]==17 && tx[9]==25 && tx[10]==35);
    CHECK(keydisplay_r.data[0]==0 && keydisplay_r.data[1]==0x90);
    CHECK(speed_mps==30.0f/3.6f && battery_amps==10);
    speed_mps=0; battery_amps=-5; fault=FAULT_CODE_UNDER_VOLTAGE;
    send_frame('R',request,2);
    CHECK(tx[4]==0x80 && tx[5]==0 && tx[6]==255 && tx[7]==255);
    adc_ok=false; send_frame('R',request,2); CHECK(tx[8]==0x22);
    // Invalid packets produce no reply, including after a partial-frame timeout.
    unsigned before=writes;
    for(unsigned i=0;i<20;i++) speedped_keydisplay_byte(0);
    CHECK(writes==before);
    clock_ms+=200; send_frame('S',settings,10); CHECK(writes==before+1);
    puts("Keydisplay adapter: real telemetry, standstill, handshake, read-only requests and UART1 replies passed.");
    return 0;
}
