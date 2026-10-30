/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <math.h>
#include "../../hwconf/SPEEDPED75100/hw_SPEEDPED75100.h"
#ifndef HW_HAS_3_SHUNTS
#error Three current measurement channels must remain enabled
#endif
int main(void) {
    const double factor=(V_REG/4095.0)/(CURRENT_SHUNT_RES*CURRENT_AMP_GAIN);
    const double old_factor=(V_REG/4095.0)/((0.0005/3.0)*20.0);
    if(fabs(CURRENT_SHUNT_RES-0.0005)>1e-12 || CURRENT_AMP_GAIN!=20.0 ||
            fabs(factor*3.0-old_factor)>1e-12) return 1;
    // Ten actual amps produces ~120 ADC counts, decoded back as ten amps.
    const double counts=10.0*0.0005*20.0/(V_REG/4095.0);
    if(fabs(counts*factor-10.0)>1e-9 || fabs(counts*old_factor-30.0)>1e-9) return 1;
    puts("Current sensing: 3 channels, single R0005, gain 20, 3:1 scale correction passed.");
    return 0;
}
