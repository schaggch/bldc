/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SPEEDPED_BATTERY_CURRENT_H
#define SPEEDPED_BATTERY_CURRENT_H
#include <stdbool.h>
// ACS758LCB-050B-PFF-T, 5 V supply, 3k + 3k divider to ADC2/PA6.
// User measurements at the ADC pin: 1.34 V = 1.2 A, 1.55 V = 10 A.
// Signed output preserves charging current; never auto-zero an idle load.
static inline float speedped_battery_current_from_voltage(float voltage) {
    return 1.2f + (voltage - 1.34f) * (8.8f / 0.21f);
}
static inline float speedped_battery_current_from_points(float voltage,
        unsigned zero_uv, unsigned mv1, unsigned ma1, unsigned mv2, unsigned ma2) {
    return (voltage-(float)zero_uv/1000000.0f)*
        ((float)ma2-(float)ma1)/((float)mv2-(float)mv1);
}
static inline bool speedped_battery_voltage_valid(float voltage) {
    // Rail readings cannot represent the specified sensor/divider output.
    // This plausibility check is not a complete sensor-disconnect detector.
    return voltage >= 0.1f && voltage <= 3.2f;
}
#endif
