#ifndef eeprom_h__
#define eeprom_h__
/* Copyright (c) 2019 Matthew Madison */

#include <inttypes.h>

typedef enum {
	module_type_cvm,
	module_type_normal,
} eeprom_module_type_t;

typedef enum {
	partnum_type_nvidia,
	partnum_type_customer,
} eeprom_partnum_type_t;

struct eeprom_context_s;
typedef struct eeprom_context_s *eeprom_context_t;

struct module_eeprom_s {
	eeprom_partnum_type_t partnumber_type;
	char partnumber[22];
	uint8_t  factory_default_wifi_mac[6];
	uint8_t  factory_default_bt_mac[6];
	uint8_t  factory_default_wifi_alt_mac[6];
	uint8_t  factory_default_ether_mac[6];
	char     asset_id[15];
	uint8_t  vendor_wifi_mac[6];
	uint8_t  vendor_bt_mac[6];
	uint8_t  vendor_ether_mac[6];
};
typedef struct module_eeprom_s module_eeprom_t;

eeprom_context_t eeprom_open_i2c(unsigned int bus, unsigned int addr, eeprom_module_type_t mtype);
eeprom_context_t eeprom_open(const char *pathname, eeprom_module_type_t mtype);
int eeprom_data_valid(eeprom_context_t ctx);
int eeprom_read(eeprom_context_t ctx, module_eeprom_t *data);
int eeprom_write(eeprom_context_t ctx, module_eeprom_t *data);
void eeprom_close(eeprom_context_t ctx);
int eeprom_readonly(eeprom_context_t ctx);

#endif /* eeprom_h__ */
