/* SPDX-License-Identifier: GPL-3.0-or-later */
// Included by the SPEEDPED hardware target. Stage 1 is
// communication/diagnostics: no display request is applied to motor settings.
#include "km5s_protocol.h"
static km5s_parser keydisplay_parser;
static km5s_frame keydisplay_s, keydisplay_r;
static bool keydisplay_have_s, keydisplay_have_r;
static uint32_t keydisplay_last_s, keydisplay_last_r, keydisplay_replies;

static void keydisplay_diagnostics(int argc, const char **argv) {
    (void)argc; (void)argv;
    chSysLock();
    km5s_parser parser=keydisplay_parser;
    km5s_frame s=keydisplay_s, r=keydisplay_r;
    const bool have_s=keydisplay_have_s, have_r=keydisplay_have_r;
    const uint32_t now=chVTGetSystemTimeX();
    const uint32_t age_s=now-keydisplay_last_s, age_r=now-keydisplay_last_r;
    const uint32_t replies=keydisplay_replies;
    chSysUnlock();
    commands_printf("KM5S UART1 9600 8N1; live telemetry; settings read-only; UART2 Bluetooth unchanged");
    commands_printf("KM5S frames=%u replies=%u bad_frame=%u bad_sum=%u partial_timeout=%u bad_handshake=%u",
        (unsigned)parser.frames,(unsigned)replies,(unsigned)parser.bad_frames,
        (unsigned)parser.bad_checksums,(unsigned)parser.timeouts,(unsigned)parser.invalid_settings);
    commands_printf("KM5S S=%d age_ms=%u R=%d age_ms=%u live=%d",have_s,(unsigned)ST2MS(age_s),
        have_r,(unsigned)ST2MS(age_r),have_s && have_r && age_r<MS2ST(1500));
    if(have_s) {
        commands_printf("S RAW: %02X %02X %02X %02X %02X %02X %02X",
            s.data[0],s.data[1],s.data[2],s.data[3],s.data[4],s.data[5],s.data[6]);
        if(s.length==10) commands_printf("S extension RAW bytes 8..10: %02X %02X %02X (layout unverified)",
            s.data[7],s.data[8],s.data[9]);
        commands_printf("S start=%u PAS_follow_raw=%u wheel_pulses=%u softstart_2bit=%u current_raw=%u",
            s.data[0]&63,s.data[1],s.data[2]&15,(s.data[2]>>4)&3,s.data[4]&63);
        commands_printf("S direction_bit=%u voltage_code=%u wheel_code=%u speed_limit=%u km/h; NOT APPLIED",
            (s.data[0]>>6)&1,s.data[4]>>6,s.data[6]&7,(s.data[6]>>3)+20);
    }
    if(have_r) commands_printf("R RAW: %02X %02X; PWM_max=%u walk=%u light=%u; NOT APPLIED",
        r.data[0],r.data[1],r.data[0],(r.data[1]>>4)&1,r.data[1]>>7);
    commands_printf("SPEEDPED speed=%.2f km/h battery_current=%.2f A; display wheel code 6 expected (2277 mm)",
        (double)(speedped_get_speed()*3.6f),(double)mc_interface_get_tot_current_in_filtered());
    commands_printf("Controller fault: %s (KM5S has no general fault code)",
        mc_interface_fault_to_string(mc_interface_get_fault()));
    commands_printf("VESC temperatures: controller=%.1f C motor=%.1f C; negative/invalid encoded0",
        (double)mc_interface_temp_fet_filtered(),(double)mc_interface_temp_motor_filtered());
}

void speedped_keydisplay_init(void) {
    terminal_register_command_callback("keydisplay","KM5S raw settings and link diagnostics",0,keydisplay_diagnostics);
}

void speedped_keydisplay_byte(unsigned char byte) {
    km5s_frame frame;
    chSysLock();
    const uint32_t now=chVTGetSystemTimeX();
    const bool valid=km5s_feed(&keydisplay_parser,byte,now,MS2ST(100),&frame);
    if(valid) {
        if(frame.command=='S') { keydisplay_s=frame; keydisplay_have_s=true; keydisplay_last_s=now; }
        else { keydisplay_r=frame; keydisplay_have_r=true; keydisplay_last_r=now; }
    }
    chSysUnlock();
    if(!valid) return;

    const uint8_t current=km5s_current_code(mc_interface_get_tot_current_in_filtered());
    const uint16_t period=km5s_wheel_ms(speedped_get_speed());
    const mc_fault_code fault=mc_interface_get_fault();
    uint8_t fault_code=0;
    if(fault==FAULT_CODE_ABS_OVER_CURRENT) fault_code=0x21;
    if(!app_adc_range_ok()) fault_code=0x22;
    uint8_t reply[KM5S_REPLY_SIZE];
    const unsigned reply_length=km5s_reply(&frame,fault==FAULT_CODE_UNDER_VOLTAGE ? 0x80 : 0,
        current,period,fault_code,
        km5s_temperature_code(mc_interface_temp_fet_filtered()),
        km5s_temperature_code(mc_interface_temp_motor_filtered()),reply);
    // This is the exclusive COMM-header UART. Never route this through VESC
    // packet_send_packet, and never touch UART4 / RX2-TX2.
    sdWrite(&HW_UART_DEV,reply,reply_length);
    chSysLock(); keydisplay_replies++; chSysUnlock();
}
