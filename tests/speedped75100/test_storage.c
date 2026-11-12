/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#undef assert
#define assert(x) do { if (!(x)) { fprintf(stderr,"CHECK FAILED line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
#include "../../hwconf/SPEEDPED75100/bike_logic.h"
static void chSysLock(void) {}
static void chSysUnlock(void) {}
static void speedped_pedal_configure(void) {}
#include "../../hwconf/SPEEDPED75100/bike_storage.h"
static uint32_t ee[32];
static bool exists[32];
static int fail_after=-1, writes;
bool conf_general_read_eeprom_var_hw(eeprom_var *v,int a) {
    assert(a>=0 && a<32); v->as_u32=ee[a]; return exists[a];
}
bool conf_general_store_eeprom_var_hw(eeprom_var *v,int a) {
    assert(a>=0 && a<32);
    if (fail_after>=0 && writes++>=fail_after) return false;
    ee[a]=v->as_u32; exists[a]=true; return true;
}
static void reload(void) { bike_sequence=0; bike_slot=0; bike_saving=false; bike_load(); }
int main(void) {
    reload(); assert(bike_conf.v[BIKE_TIMEOUT]==500);
    uint8_t wire[BIKE_WIRE_SIZE]; assert(bike_get_cfg(wire,true)==BIKE_WIRE_SIZE);
    uint8_t *xml; assert(bike_get_xml(&xml)>100 && xml);
    bike_conf.v[BIKE_PEDAL_KMH]=25;
    bike_get_cfg(wire,false); assert(bike_set_cfg(wire)); reload();
    assert(bike_conf.v[BIKE_PEDAL_KMH]==25);
    uint32_t backup[32]; memcpy(backup,ee,sizeof(ee));
    bool present[32]; memcpy(present,exists,sizeof(exists));
    bike_conf.v[BIKE_PEDAL_KMH]=30; bike_get_cfg(wire,false);
    // Interrupt each individual write of the alternate slot. Previous slot survives.
    for (int i=0;i<14;i++) {
        memcpy(ee,backup,sizeof(ee)); memcpy(exists,present,sizeof(exists)); reload();
        fail_after=i; writes=0; assert(!bike_set_cfg(wire)); reload();
        assert(bike_conf.v[BIKE_PEDAL_KMH]==25);
    }
    fail_after=-1; assert(bike_set_cfg(wire)); reload();
    assert(bike_conf.v[BIKE_PEDAL_KMH]==30);
    ee[bike_slot*BIKE_SLOT_WORDS+3]^=1; reload(); assert(bike_conf.v[BIKE_PEDAL_KMH]==25);
    wire[0]^=1; assert(!bike_set_cfg(wire)); wire[0]^=1;
    wire[32]=2; assert(!bike_set_cfg(wire));
    assert(!bike_saving);
    // Both legacy slots migrate without losing settings at any interrupted write.
    for(unsigned legacy_slot=0;legacy_slot<2;legacy_slot++) {
        memset(ee,0,sizeof(ee)); memset(exists,0,sizeof(exists));
        bike_config old=bike_defaults(); old.v[BIKE_PEDAL_KMH]=27;
        uint32_t record[12]={BIKE_LEGACY_SIGNATURE,42};
        for(unsigned j=0;j<9;j++) record[j+2]=j==4?1800:old.v[j<4?j:j-1];
        record[11]=bike_checksum(record,11);
        fail_after=-1; assert(bike_write_record(legacy_slot*12,record,12)); reload();
        assert(bike_legacy && bike_conf.v[BIKE_PEDAL_KMH]==27);
        assert(bike_conf.v[BIKE_NO_WHEEL_SECONDS]==100 && bike_conf.v[BIKE_NO_WHEEL_ERPM]==10000);
        memcpy(backup,ee,sizeof(ee)); memcpy(present,exists,sizeof(exists));
        bike_conf.v[BIKE_NO_WHEEL_SECONDS]=7; bike_conf.v[BIKE_NO_WHEEL_WATTS]=250;
        bike_conf.v[BIKE_NO_WHEEL_ERPM]=1200; bike_get_cfg(wire,false);
        for(int i=0;i<(legacy_slot==1?27:14);i++) {
            memcpy(ee,backup,sizeof(ee)); memcpy(exists,present,sizeof(exists)); reload();
            fail_after=i; writes=0; assert(!bike_set_cfg(wire)); reload();
            assert(bike_conf.v[BIKE_PEDAL_KMH]==27 && bike_conf.v[BIKE_NO_WHEEL_ERPM]==10000);
        }
        fail_after=-1; assert(bike_set_cfg(wire)); reload(); assert(!bike_legacy);
        assert(bike_conf.v[BIKE_PEDAL_KMH]==27 && bike_conf.v[BIKE_NO_WHEEL_SECONDS]==7);
        assert(bike_conf.v[BIKE_NO_WHEEL_WATTS]==250 && bike_conf.v[BIKE_NO_WHEEL_ERPM]==1200);
    }
    // Upgrade either pre-duty slot; preserve all twelve fields and default duty to off.
    for(unsigned slot=0;slot<2;slot++) {
        memset(ee,0,sizeof(ee)); memset(exists,0,sizeof(exists));
        bike_config old=bike_defaults(); old.v[BIKE_NO_WHEEL_WATTS]=250;
        uint32_t record[15]={BIKE_PRE_DUTY_SIGNATURE,99};
        for(unsigned j=0;j<12;j++) record[j+2]=j==4?1800:old.v[j<4?j:j-1];
        record[14]=bike_checksum(record,14);
        fail_after=-1; assert(bike_write_record(slot*16,record,15)); reload();
        assert(bike_conf.v[BIKE_SH_DUTY]==15 && bike_conf.v[BIKE_NO_WHEEL_WATTS]==250);
        memcpy(backup,ee,sizeof(ee)); memcpy(present,exists,sizeof(exists));
        bike_conf.v[BIKE_SH_DUTY]=10; bike_get_cfg(wire,false);
        for(int i=0;i<14;i++) {
            memcpy(ee,backup,sizeof(ee)); memcpy(exists,present,sizeof(exists)); reload();
            fail_after=i; writes=0; assert(!bike_set_cfg(wire)); reload();
            assert(bike_conf.v[BIKE_SH_DUTY]==15 && bike_conf.v[BIKE_NO_WHEEL_WATTS]==250);
        }
        fail_after=-1; assert(bike_set_cfg(wire)); reload();
        assert(bike_conf.v[BIKE_SH_DUTY]==10 && bike_conf.v[BIKE_NO_WHEEL_WATTS]==250);
    }
    // Pre-sound 13-field records survive every interruption during migration.
    for(unsigned slot=0;slot<2;slot++) {
        memset(ee,0,sizeof(ee)); memset(exists,0,sizeof(exists));
        bike_config old=bike_defaults(); old.v[BIKE_SH_DUTY]=12;
        uint32_t record[16]={BIKE_PRE_SOUND_SIGNATURE,123};
        for(unsigned j=0;j<13;j++) record[j+2]=j==4?1800:old.v[j<4?j:j-1];
        record[15]=bike_checksum(record,15);
        fail_after=-1; assert(bike_write_record(slot*16,record,16)); reload();
        assert(bike_conf.v[BIKE_SH_DUTY]==12 && bike_conf.v[BIKE_SOUND_START]==0);
        memcpy(backup,ee,sizeof(ee)); memcpy(present,exists,sizeof(exists));
        bike_conf.v[BIKE_SOUND_START]=1; bike_conf.v[BIKE_SOUND_RIDE]=1;
        bike_conf.v[BIKE_SOUND_TEMPO]=400; bike_get_cfg(wire,false);
        for(int i=0;i<14;i++) {
            memcpy(ee,backup,sizeof(ee)); memcpy(exists,present,sizeof(exists)); reload();
            fail_after=i; writes=0; assert(!bike_set_cfg(wire)); reload();
            assert(bike_conf.v[BIKE_SH_DUTY]==12 && bike_conf.v[BIKE_SOUND_START]==0);
        }
        fail_after=-1; assert(bike_set_cfg(wire)); reload();
        assert(bike_conf.v[BIKE_SOUND_START]==1 && bike_conf.v[BIKE_SOUND_RIDE]==1);
        assert(bike_conf.v[BIKE_SOUND_TEMPO]==400 && bike_conf.v[BIKE_SH_DUTY]==12);
    }
    // Previous packed sound records: discard old SH eRPM, preserve every other value.
    for(unsigned slot=0;slot<2;slot++) {
        memset(ee,0,sizeof(ee)); memset(exists,0,sizeof(exists));
        bike_conf=bike_defaults(); bike_conf.v[BIKE_SH_DUTY]=17;
        bike_conf.v[BIKE_SOUND_VOLUME]=34; bike_conf.v[BIKE_TIMEOUT]=1234;
        bike_get_cfg(wire,false); bike_slot=1-slot; fail_after=-1;
        assert(bike_set_cfg(wire));
        ee[slot*16]=BIKE_PRE_REMOVE_SH_SIGNATURE;
        ee[slot*16+4]=99999;
        ee[slot*16+10]=bike_checksum(ee+slot*16,10);
        reload();
        assert(bike_conf.v[BIKE_TIMEOUT]==1234 && bike_conf.v[BIKE_SH_DUTY]==17);
        assert(bike_conf.v[BIKE_SOUND_VOLUME]==34);
        memcpy(backup,ee,sizeof(ee)); memcpy(present,exists,sizeof(exists));
        bike_conf.v[BIKE_SH_DUTY]=18; bike_get_cfg(wire,false);
        for(int i=0;i<14;i++) {
            memcpy(ee,backup,sizeof(ee)); memcpy(exists,present,sizeof(exists)); reload();
            fail_after=i; writes=0; assert(!bike_set_cfg(wire)); reload();
            assert(bike_conf.v[BIKE_SH_DUTY]==17 && bike_conf.v[BIKE_SOUND_VOLUME]==34);
        }
        fail_after=-1; assert(bike_set_cfg(wire)); reload();
        assert(bike_conf.v[BIKE_SH_DUTY]==18 && bike_conf.v[BIKE_TIMEOUT]==1234);
    }
    for(unsigned slot=0;slot<2;slot++) {
        memset(ee,0,sizeof(ee)); memset(exists,0,sizeof(exists));
        bike_conf=bike_defaults(); bike_conf.v[BIKE_PEDAL_KMH]=29;
        bike_slot=1-slot; fail_after=-1; assert(bike_store_config(bike_conf,false));
        ee[slot*16]=BIKE_PRE_ADC2_SIGNATURE;
        ee[slot*16+10]=bike_checksum(ee+slot*16,10);
        exists[slot*16+11]=false; exists[slot*16+12]=false;
        reload(); assert(bike_conf.v[BIKE_PEDAL_KMH]==29 && bike_conf.v[BIKE_ADC2_V1]==1340);
        memcpy(backup,ee,sizeof(ee)); memcpy(present,exists,sizeof(exists));
        bike_conf.v[BIKE_ADC2_V1]=1300; bike_conf.v[BIKE_ADC2_I1]=0;
        bike_conf.v[BIKE_ADC2_V2]=1500; bike_conf.v[BIKE_ADC2_I2]=10000;
        bike_get_cfg(wire,false);
        for(int i=0;i<14;i++) {
            memcpy(ee,backup,sizeof(ee)); memcpy(exists,present,sizeof(exists)); reload();
            fail_after=i; writes=0; assert(!bike_set_cfg(wire)); reload();
            assert(bike_conf.v[BIKE_PEDAL_KMH]==29 && bike_conf.v[BIKE_ADC2_V1]==1340);
        }
        fail_after=-1; assert(bike_set_cfg(wire)); reload();
        assert(bike_conf.v[BIKE_ADC2_V1]==1300 && bike_conf.v[BIKE_ADC2_I1]==0);
        assert(bike_conf.v[BIKE_ADC2_V2]==1500 && bike_conf.v[BIKE_ADC2_I2]==10000);
        bike_config invalid=bike_conf; invalid.v[BIKE_ADC2_V2]=1300;
        assert(!bike_store_config(invalid,false));
    }
    bike_config maximum={{5000,64,150,30,5000,64,32,1,120,100000,100000,95,1,1,100,400,100,0,3000,60000}};
    assert(bike_valid(&maximum)); bike_conf=maximum; bike_get_cfg(wire,false);
    assert(bike_set_cfg(wire)); reload(); assert(!memcmp(&bike_conf,&maximum,sizeof(maximum)));
    puts("Bike storage: wire format, validation, reload, corruption and all interrupted writes passed.");
}
