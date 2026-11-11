/* SPDX-License-Identifier: GPL-3.0-or-later */
// Keep the original power-stage, ADC and shunt configuration in one place.
#include "../flipsky/hw_75_100_V2.c"
#include "pedal_gate.h"
#include "terminal.h"
#include "commands.h"
#include "bike_logic.h"
#include "bike_storage.h"
#include "app.h"
#include "sound_logic.h"
#include "mcpwm_foc.h"
#include "confgenerator.h"
#include "mempools.h"

// Word 31 is outside both current packed records (0..10 and 16..26).
// Old linear records are migrated before this marker is written.
#define SPEEDPED_STOCK_MARKER_ADDRESS 31
#define SPEEDPED_STOCK_REVISION 0x53503101U
static bool speedped_stock_installed(void) {
    eeprom_var v;
    return conf_general_read_eeprom_var_hw(&v,SPEEDPED_STOCK_MARKER_ADDRESS) &&
        v.as_u32==SPEEDPED_STOCK_REVISION;
}
static bool speedped_stock_mark_installed(void) {
    eeprom_var v; v.as_u32=SPEEDPED_STOCK_REVISION;
    return conf_general_store_eeprom_var_hw(&v,SPEEDPED_STOCK_MARKER_ADDRESS) &&
        speedped_stock_installed();
}
static bool speedped_stock_write_mc(void) {
    mc_configuration *c=mempools_alloc_mcconf();
    memset(c,0,sizeof(*c));
    confgenerator_set_defaults_mcconf(c);
    bool ok=conf_general_store_mc_configuration(c,false);
    if(ok) mc_interface_set_configuration(c);
    mempools_free_mcconf(c);
    return ok;
}
static bool speedped_stock_write_app(void) {
    app_configuration *c=mempools_alloc_appconf();
    memset(c,0,sizeof(*c));
    confgenerator_set_defaults_appconf(c);
    bool ok=conf_general_store_app_configuration(c);
    mempools_free_appconf(c);
    return ok;
}
static bool speedped_stock_write_bike(void) {
    bike_load();
    return bike_store_config(bike_defaults(),false);
}
#include "stock_install.h"

static speedped_gate pedal;
static bool pedal_started;
static bike_state bike;
static THD_WORKING_AREA(pedal_wa, 1024);
static bool drive_requested, drive_permitted;
static uint32_t drive_checked;
static speedped_sound_state sound;
static volatile bool sound_owned;
static volatile uint32_t sound_checked;

void speedped_sound_stop(void) {
    if(sound_owned) {
        mcpwm_foc_stop_audio(false);
        sound_owned=false;
    }
}

// Called by the FOC interrupt: a stalled PAS/sound task cannot leave a tone on.
bool speedped_sound_watchdog_ok(void) {
    return !sound_owned || (uint32_t)(chVTGetSystemTimeX()-sound_checked)<=MS2ST(20);
}

static void sound_update(uint32_t now,bool sample_gap) {
    const volatile mc_configuration *mc=mc_interface_get_configuration();
    float rpm=fabsf(mc_interface_get_rpm());
    chSysLock();
    speedped_sound_config cfg={bike_conf.v[BIKE_SOUND_START]!=0,bike_conf.v[BIKE_SOUND_RIDE]!=0,
        bike_conf.v[BIKE_SOUND_VOLUME],bike_conf.v[BIKE_SOUND_TEMPO]};
    bool blocked=mc->motor_type!=MOTOR_TYPE_FOC || mc->foc_sensor_mode!=FOC_SENSOR_MODE_SENSORLESS ||
        mc_interface_get_fault()!=FAULT_CODE_NONE || mc_interface_is_locked() ||
        app_is_output_disabled() || bike_saving || sample_gap;
    bool idle=!drive_requested && rpm<50.0f && (sound_owned || mc_interface_get_state()!=MC_STATE_RUNNING);
    bool driving=drive_requested && drive_permitted &&
        (uint32_t)(now-drive_checked)<=MS2ST(20) && rpm>100.0f;
    speedped_sound_output output=speedped_sound_tick(&sound,&cfg,now,CH_CFG_ST_FREQUENCY,
        blocked,idle,driving,speedped_gate_allowed(&pedal,now),pedal.period);
    sound_checked=now;
    if(output.frequency_hz>0) {
        // Experimental, user-enabled overlay: 0.2 V maximum. At standstill,
        // VESC energizes zero-current FOC to generate the startup tone.
        // Set ownership before enabling audio so the ISR watchdog also covers it.
        sound_owned=true;
        if(!mcpwm_foc_play_tone(0,output.frequency_hz,0.01f+0.19f*output.level))
            speedped_sound_stop();
    } else {
        speedped_sound_stop();
    }
    chSysUnlock();
}

static void pedal_reset(uint32_t now, bool low) {
    speedped_gate_init(&pedal, now, low, MS2ST(bike_conf.v[BIKE_TIMEOUT]), MS2ST(2));
    pedal.start_periods=bike_conf.v[BIKE_START];
    pedal.low_majority=bike_conf.v[BIKE_DUTY]!=0;
}

static THD_FUNCTION(pedal_thread, arg) {
    (void)arg;
    chRegSetThreadName("SPEEDPED_PAS");
    uint32_t last_sound=chVTGetSystemTimeX();
    bool sound_gap=false;
    for (;;) {
        const bool fault = mc_interface_get_fault() != FAULT_CODE_NONE;
        chSysLock();
        const uint32_t now = chVTGetSystemTimeX();
        const bool low = !palReadPad(HW_ICU_GPIO, HW_ICU_PIN);
        const bool gap=(uint32_t)(now-pedal.last_sample)>MS2ST(20);
        sound_gap|=gap;
        if (fault || gap || bike_saving) {
            bike_init(&bike,now,!READ_HALL1(),!READ_HALL2(),!READ_HALL3());
            pedal_reset(now, low);
        } else {
            bool inhibit=bike_sample(&bike,now,!READ_HALL1(),!READ_HALL2(),!READ_HALL3(),MS2ST(2),MS2ST(1000));
            if (inhibit) pedal_reset(now,low);
            else speedped_gate_sample(&pedal, now, low);
        }
        chSysUnlock();
        if((uint32_t)(now-last_sound)>=MS2ST(5)) {
            sound_update(now,sound_gap); last_sound=now; sound_gap=false;
        }
        chThdSleepMilliseconds(1);
    }
}

float speedped_get_speed(void) {
    syssts_t status=chSysGetStatusAndLockX();
    uint32_t now=chVTGetSystemTimeX();
    float speed=pedal_started && !bike_saving &&
        (uint32_t)(now-pedal.last_sample)<=MS2ST(20) ?
        bike_kmh(&bike,&bike_conf,CH_CFG_ST_FREQUENCY,now)/3.6f : 0.0f;
    chSysRestoreStatusX(status);
    return speed;
}

unsigned char speedped_pin_levels(void) {
    chSysLock();
    unsigned char pins=(!READ_HALL1() ? 1U : 0U) |
        (!READ_HALL2() ? 2U : 0U) | (!READ_HALL3() ? 4U : 0U) |
        (!palReadPad(HW_ICU_GPIO,HW_ICU_PIN) ? 8U : 0U);
    chSysUnlock();
    return pins;
}

float speedped_sh_duty_step(float previous,float request,float cap,float dt) {
    return bike_sh_duty_step(previous,request,cap,dt);
}

float speedped_duty_rise(float previous,float request,float max_duty,float ramp,float dt) {
    return bike_duty_rise(previous,request,max_duty,ramp,dt);
}

bool speedped_bike_allowed(bool throttle_requested, float *sh_duty_cap, float *speed_scale) {
    if (sh_duty_cap) *sh_duty_cap=0.0f;
    if (speed_scale) *speed_scale=0.0f;
    const volatile mc_configuration *mc=mc_interface_get_configuration();
    bool sensorless=mc->m_sensor_port_mode==SENSOR_PORT_MODE_HALL &&
        (mc->motor_type==MOTOR_TYPE_FOC ? mc->foc_sensor_mode==FOC_SENSOR_MODE_SENSORLESS :
         mc->sensor_mode==SENSOR_MODE_SENSORLESS);
    float rpm=mc_interface_get_rpm();
    float watts=mc_interface_get_input_voltage_filtered()*mc_interface_get_tot_current_in_filtered();
    chSysLock();
    uint32_t now=chVTGetSystemTimeX();
    bool allowed=pedal_started && sensorless && !bike_saving &&
        speedped_battery_voltage_valid(ADC_VOLTS(ADC_IND_EXT2)) &&
        (uint32_t)(now-pedal.last_sample)<=MS2ST(20) &&
        bike_drive_allowed(&bike,&bike_conf,now,CH_CFG_ST_FREQUENCY,
            speedped_gate_allowed(&pedal,now),throttle_requested,rpm,watts,sh_duty_cap,speed_scale);
    drive_requested=throttle_requested; drive_permitted=allowed && (!speed_scale || *speed_scale>0);
    drive_checked=now;
    chSysUnlock();
    return allowed;
}

bool speedped_pedal_allowed(void) {
    chSysLock();
    const uint32_t now = chVTGetSystemTimeX();
    bool allowed = pedal_started &&
        (uint32_t)(now - pedal.last_sample) <= MS2ST(20) &&
        speedped_gate_allowed(&pedal, now);
    chSysUnlock();
    return allowed;
}

static void pedal_status(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    commands_printf("Current sensing: one R0005 per phase, shunt=%.3f mOhm, gain=%.1f",
        (double)(CURRENT_SHUNT_RES*1000.0),(double)CURRENT_AMP_GAIN);
    commands_printf("Battery sensor: ADC2=%.4f V current=%.3f A valid=%d (ACS758 3k/3k)",
        (double)ADC_VOLTS(ADC_IND_EXT2),(double)GET_INPUT_CURRENT(),
        speedped_battery_voltage_valid(ADC_VOLTS(ADC_IND_EXT2)));
    const bool allowed = speedped_pedal_allowed();
    chSysLock();
    speedped_gate snapshot = pedal;
    bike_state bs=bike;
    bike_config bc=bike_conf;
    const uint32_t now = chVTGetSystemTimeX();
    const uint32_t age = now - snapshot.last_valid;
    chSysUnlock();
    const float rpm = snapshot.period ?
        60.0f * CH_CFG_ST_FREQUENCY / (bc.v[BIKE_MAGNETS] * (float)snapshot.period) : 0.0f;
    commands_printf("SPEEDPED75100: low=%d periods=%u/%u enabled=%d age_ms=%u pedal_rpm=%.1f",
        snapshot.stable_low, snapshot.valid_periods, snapshot.start_periods,
        allowed, (unsigned)ST2MS(age), (double)rpm);
    commands_printf("SPEEDPED: SH=%d AH=%d mode=%u speed=%.2f km/h fallback=%d PAS_block=%d",
        bs.sh.stable,bs.ah.stable,bike_mode(&bs),(double)bike_kmh(&bs,&bc,CH_CFG_ST_FREQUENCY,now),
        bs.fallback_active,bs.inhibit);
    commands_printf("WHEEL: raw_low=%d period_ms=%u age_ms=%u valid=%d GPIO_modes=%u pullups=%u",
        !READ_HALL3(),(unsigned)ST2MS(bs.period),(unsigned)ST2MS(now-bs.last_pulse),
        bike_wheel_fresh(&bs,now,CH_CFG_ST_FREQUENCY),
        (unsigned)((GPIOC->MODER>>12)&63U),(unsigned)((GPIOC->PUPDR>>12)&63U));
    const volatile mc_configuration *mc=mc_interface_get_configuration();
    commands_printf("DIAG: fault=%s motor=%d foc_sensor=%d sensor_port=%d ADC_type=%d ADC=%.3f V=%.3f request=%d allowed=%d",
        mc_interface_fault_to_string(mc_interface_get_fault()),mc->motor_type,mc->foc_sensor_mode,
        mc->m_sensor_port_mode,app_get_configuration()->app_adc_conf.ctrl_type,
        (double)app_adc_get_decoded_level(),(double)app_adc_get_voltage(),drive_requested,drive_permitted);
}

void speedped_pedal_configure(void) {
    if (!pedal_started) bike_load();
    chSysLock();
    palSetPadMode(HW_ICU_GPIO, HW_ICU_PIN, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(HW_HALL_ENC_GPIO1,HW_HALL_ENC_PIN1,PAL_MODE_INPUT_PULLUP);
    palSetPadMode(HW_HALL_ENC_GPIO2,HW_HALL_ENC_PIN2,PAL_MODE_INPUT_PULLUP);
    palSetPadMode(HW_HALL_ENC_GPIO3,HW_HALL_ENC_PIN3,PAL_MODE_INPUT_PULLUP);
    bike_init(&bike,chVTGetSystemTimeX(),!READ_HALL1(),!READ_HALL2(),!READ_HALL3());
    pedal_reset(chVTGetSystemTimeX(), !palReadPad(HW_ICU_GPIO, HW_ICU_PIN));
    chSysUnlock();
    if (!pedal_started) {
        pedal_started = true;
        conf_custom_add_config(bike_get_cfg,bike_set_cfg,bike_get_xml);
        terminal_register_command_callback("speedped", "Pedal sensor and drive permission", 0, pedal_status);
        chThdCreateStatic(pedal_wa, sizeof(pedal_wa), NORMALPRIO + 1, pedal_thread, 0);
    }
}

float speedped_adc_limit(float request,float cap,bool braking) {
    return bike_adc_limit(request,cap,braking);
}

#include "keydisplay_adapter.h"
