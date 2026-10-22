#include <stdint.h>
#include <stdbool.h>
typedef union { uint32_t as_u32; } eeprom_var;
bool conf_general_read_eeprom_var_hw(eeprom_var *v,int address);
bool conf_general_store_eeprom_var_hw(eeprom_var *v,int address);
