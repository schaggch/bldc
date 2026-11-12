/* SPDX-License-Identifier: GPL-3.0-or-later */
// Included by the hardware source; two alternating EEPROM slots retain the
// previous configuration if power is lost during a save.
#include "conf_custom.h"
#include "conf_general.h"
#include "buffer.h"
#include "bike_schema.h"
#include <string.h>
static bike_config bike_conf;
static unsigned bike_slot;
static uint32_t bike_sequence;
static bool bike_saving;
static bool bike_legacy;
static unsigned bike_legacy_slot;
#define BIKE_SLOT_WORDS 16U
#define BIKE_RECORD_WORDS 13U
#define BIKE_PRE_ZERO_SIGNATURE 0x79b111c3U
#define BIKE_PRE_ADC2_SIGNATURE 0x2ea07d4cU
#define BIKE_LEGACY_SIGNATURE 0xd31eefe0U
#define BIKE_PRE_DUTY_SIGNATURE 0x1b98fcefU
#define BIKE_PRE_REMOVE_SH_SIGNATURE 0x856d82beU
#define BIKE_PRE_SOUND_SIGNATURE 0x8f4f713fU
static uint32_t bike_checksum(const uint32_t *words, unsigned count) {
    uint32_t crc=0xffffffffU;
    for (unsigned i=0; i<count; i++) {
        uint32_t w=words[i];
        for (unsigned b=0; b<4; b++, w>>=8) {
            crc^=w&255U;
            for (unsigned j=0; j<BIKE_DUTY; j++) crc=(crc>>1)^((crc&1)?0x82f63b78U:0U);
        }
    }
    return ~crc;
}
static bool bike_read_record(unsigned base, unsigned fields, uint32_t signature,
        bike_config *cfg, uint32_t *sequence) {
    uint32_t words[BIKE_SLOT_WORDS];
    unsigned count=fields+3U;
    if(count>BIKE_SLOT_WORDS || fields>BIKE_FIELDS) return false;
    for (unsigned i=0; i<count; i++) {
        eeprom_var v;
        if (!conf_general_read_eeprom_var_hw(&v,base+i)) return false;
        words[i]=v.as_u32;
    }
    if (words[0]!=signature || words[count-1]!=bike_checksum(words,count-1)) return false;
    *cfg=bike_defaults();
    // Old linear records had the removed field at index 4.
    for(unsigned j=0;j<fields;j++) if(j!=4) cfg->v[j<4?j:j-1]=words[j+2];
    *sequence=words[1];
    return bike_valid(cfg);
}
static bool bike_read_slot(unsigned slot,bike_config *cfg,uint32_t *sequence) {
    uint32_t w[BIKE_RECORD_WORDS];
    for(unsigned i=0;i<11U;i++) {
        eeprom_var v; if(!conf_general_read_eeprom_var_hw(&v,slot*BIKE_SLOT_WORDS+i)) return false;
        w[i]=v.as_u32;
    }
    if (w[0]==BIKE_SIGNATURE || w[0]==BIKE_PRE_ZERO_SIGNATURE) {
        for(unsigned i=11;i<BIKE_RECORD_WORDS;i++) {
            eeprom_var v; if(!conf_general_read_eeprom_var_hw(&v,slot*BIKE_SLOT_WORDS+i)) return false;
            w[i]=v.as_u32;
        }
        if(w[12]!=bike_checksum(w,12)) return false;
    } else if ((w[0]!=BIKE_PRE_ADC2_SIGNATURE && w[0]!=BIKE_PRE_REMOVE_SH_SIGNATURE) ||
            w[10]!=bike_checksum(w,10)) return false;
    *cfg=bike_defaults(); *sequence=w[1];
    cfg->v[BIKE_MM]=w[2]&65535; cfg->v[BIKE_PULSES]=w[2]>>16;
    cfg->v[BIKE_PEDAL_KMH]=w[3]&255; cfg->v[BIKE_AH_KMH]=(w[3]>>8)&255;
    cfg->v[BIKE_START]=(w[3]>>16)&255; cfg->v[BIKE_MAGNETS]=w[3]>>24;
    cfg->v[BIKE_TIMEOUT]=w[5]&65535;
    cfg->v[BIKE_DUTY]=(w[5]>>16)&1; cfg->v[BIKE_NO_WHEEL_SECONDS]=w[5]>>17;
    cfg->v[BIKE_NO_WHEEL_WATTS]=w[6]; cfg->v[BIKE_NO_WHEEL_ERPM]=w[7];
    cfg->v[BIKE_SH_DUTY]=w[8]&255; cfg->v[BIKE_SOUND_START]=(w[8]>>8)&1;
    cfg->v[BIKE_SOUND_RIDE]=(w[8]>>9)&1; cfg->v[BIKE_SOUND_VOLUME]=w[8]>>16;
    cfg->v[BIKE_SOUND_TEMPO]=w[9];
    if(w[0]==BIKE_SIGNATURE || w[0]==BIKE_PRE_ZERO_SIGNATURE) {
        cfg->v[BIKE_ADC2_V1]=w[10]&65535; cfg->v[BIKE_ADC2_I1]=w[10]>>16;
        cfg->v[BIKE_ADC2_V2]=w[11]&65535; cfg->v[BIKE_ADC2_I2]=w[11]>>16;
    }
    if(w[0]==BIKE_SIGNATURE) cfg->v[BIKE_ADC2_ZERO]=w[4];
    if(w[0]==BIKE_PRE_ZERO_SIGNATURE && cfg->v[BIKE_ADC2_I2]>cfg->v[BIKE_ADC2_I1]) {
        double zero=(double)cfg->v[BIKE_ADC2_V1]*1000.0-
            (double)cfg->v[BIKE_ADC2_I1]*((double)cfg->v[BIKE_ADC2_V2]-cfg->v[BIKE_ADC2_V1])*1000.0/
            ((double)cfg->v[BIKE_ADC2_I2]-cfg->v[BIKE_ADC2_I1]);
        if(zero<100000.0 || zero>3200000.0) return false;
        cfg->v[BIKE_ADC2_ZERO]=(uint32_t)(zero+0.5);
    }
    return bike_valid(cfg);
}
static bool bike_write_record(unsigned base,const uint32_t *words,unsigned count) {
    eeprom_var v; v.as_u32=0;
    bool ok=conf_general_store_eeprom_var_hw(&v,base);
    for(unsigned j=1;ok && j<count;j++) {
        v.as_u32=words[j]; ok=conf_general_store_eeprom_var_hw(&v,base+j);
    }
    if(ok) { v.as_u32=words[0]; ok=conf_general_store_eeprom_var_hw(&v,base); }
    return ok;
}
static void bike_load(void) {
    bike_conf=bike_defaults();
    bike_sequence=0; bike_slot=0; bike_legacy=false;
    bike_config other; uint32_t seq;
    bool found=bike_read_slot(0,&other,&seq);
    if (found) { bike_conf=other; bike_sequence=seq; bike_slot=0; }
    if (bike_read_slot(1,&other,&seq) && (!found || (int32_t)(seq-bike_sequence)>0)) {
        bike_conf=other; bike_sequence=seq; bike_slot=1; found=true;
    }
    if (!found) {
        for(unsigned slot=0;slot<2;slot++) {
            if(bike_read_record(slot*BIKE_SLOT_WORDS,13,BIKE_PRE_SOUND_SIGNATURE,&other,&seq) &&
                    (!found || (int32_t)(seq-bike_sequence)>0)) {
                bike_conf=other; bike_sequence=seq; bike_slot=slot; found=true;
            }
        }
    }
    if (!found) {
        for(unsigned slot=0;slot<2;slot++) {
            if(bike_read_record(slot*BIKE_SLOT_WORDS,12,BIKE_PRE_DUTY_SIGNATURE,&other,&seq) &&
                    (!found || (int32_t)(seq-bike_sequence)>0)) {
                bike_conf=other; bike_sequence=seq; bike_slot=slot; found=true;
            }
        }
    }
    if (!found) {
        for(unsigned slot=0;slot<2;slot++) {
            if(bike_read_record(slot*12,9,BIKE_LEGACY_SIGNATURE,&other,&seq) &&
                    (!bike_legacy || (int32_t)(seq-bike_sequence)>0)) {
                bike_conf=other; bike_sequence=seq; bike_legacy=true; bike_legacy_slot=slot;
            }
        }
    }
}
static int bike_get_cfg(uint8_t *data, bool defaults) {
    chSysLock(); bike_config c=defaults?bike_defaults():bike_conf; chSysUnlock();
    int32_t i=0; buffer_append_uint32(data,BIKE_SIGNATURE,&i);
    for (unsigned j=0; j<BIKE_DUTY; j++) buffer_append_uint32(data,c.v[j],&i);
    data[i++]=(uint8_t)c.v[BIKE_DUTY];
    for(unsigned j=BIKE_DUTY+1;j<BIKE_FIELDS;j++) buffer_append_uint32(data,c.v[j],&i);
    return i;
}
static bool bike_store_config(bike_config c, bool configure) {
    if (!bike_valid(&c)) return false;
    chSysLock();
    if (bike_saving) { chSysUnlock(); return false; }
    bike_saving=true;
    chSysUnlock();
    uint32_t words[BIKE_RECORD_WORDS]={BIKE_SIGNATURE,bike_sequence+1};
    words[2]=c.v[BIKE_MM] | (c.v[BIKE_PULSES]<<16);
    words[3]=c.v[BIKE_PEDAL_KMH] | (c.v[BIKE_AH_KMH]<<8) | (c.v[BIKE_START]<<16) | (c.v[BIKE_MAGNETS]<<24);
    words[4]=c.v[BIKE_ADC2_ZERO]; // Microvolts; legacy readers ignored this word.
    words[5]=c.v[BIKE_TIMEOUT] | (c.v[BIKE_DUTY]<<16) | (c.v[BIKE_NO_WHEEL_SECONDS]<<17);
    words[6]=c.v[BIKE_NO_WHEEL_WATTS]; words[7]=c.v[BIKE_NO_WHEEL_ERPM];
    words[8]=c.v[BIKE_SH_DUTY] | (c.v[BIKE_SOUND_START]<<8) | (c.v[BIKE_SOUND_RIDE]<<9) | (c.v[BIKE_SOUND_VOLUME]<<16);
    words[9]=c.v[BIKE_SOUND_TEMPO];
    words[10]=c.v[BIKE_ADC2_V1] | (c.v[BIKE_ADC2_I1]<<16);
    words[11]=c.v[BIKE_ADC2_V2] | (c.v[BIKE_ADC2_I2]<<16);
    words[12]=bike_checksum(words,12);
    unsigned slot=1-bike_slot;
    bool ok=true;
    // Legacy slot 1 overlaps both new slots. First retain it in legacy slot 0,
    // which is disjoint from the first new destination (addresses 16..30).
    if(bike_legacy && bike_legacy_slot==1) {
        uint32_t old[12]={BIKE_LEGACY_SIGNATURE,bike_sequence};
        for(unsigned j=0;j<9;j++) old[j+2]=j==4?0:bike_conf.v[j<4?j:j-1];
        old[11]=bike_checksum(old,11);
        ok=bike_write_record(0,old,12);
        bike_config old_verify; uint32_t old_seq;
        ok=ok && bike_read_record(0,9,BIKE_LEGACY_SIGNATURE,&old_verify,&old_seq) &&
            old_seq==bike_sequence && !memcmp(old_verify.v,bike_conf.v,8*sizeof(uint32_t));
        if(ok) bike_legacy_slot=0;
    }
    if(ok) ok=bike_write_record(slot*BIKE_SLOT_WORDS,words,BIKE_RECORD_WORDS);
    // Read-back catches a failed/partial flash write as well.
    bike_config verify; uint32_t seq;
    ok=ok && bike_read_slot(slot,&verify,&seq) && seq==words[1] && !memcmp(&verify,&c,sizeof(c));
    chSysLock();
    if (ok) { bike_conf=c; bike_slot=slot; bike_sequence=seq; bike_legacy=false; }
    chSysUnlock();
    if (configure) speedped_pedal_configure();
    chSysLock(); bike_saving=false; chSysUnlock();
    return ok;
}
static bool bike_set_cfg(uint8_t *data) {
    int32_t i=0;
    if (buffer_get_uint32(data,&i)!=BIKE_SIGNATURE) return false;
    bike_config c;
    for (unsigned j=0; j<BIKE_DUTY; j++) c.v[j]=buffer_get_uint32(data,&i);
    c.v[BIKE_DUTY]=data[i++];
    for(unsigned j=BIKE_DUTY+1;j<BIKE_FIELDS;j++) c.v[j]=buffer_get_uint32(data,&i);
    return bike_store_config(c,true);
}
static int bike_get_xml(uint8_t **data) {
    *data=(uint8_t*)bike_xml; return sizeof(bike_xml);
}
