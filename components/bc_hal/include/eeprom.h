#ifndef _EEPROM_H
#define _EEPROM_H

#include <stdint.h>
#include <miner.h>

#define EEPROM_0_WRITE_PROTECT_IO	CONFIG_GPIO_EEPROM_WRITE_EN_0
#define EEPROM_1_WRITE_PROTECT_IO	CONFIG_GPIO_EEPROM_WRITE_EN_1

typedef struct{
	//write fix data
	uint8_t version;
	uint8_t algorithm;//encode type
	uint8_t chip_bin;// chip_bin= 4bit pacakge type + 4bit bin type
	uint8_t pcb_version;
	uint32_t sn;

	/*board mining param*/
	uint32_t voltage_normal_init;//version 0
	uint32_t voltage_normal_target; //version 0
	uint32_t freq_normal;//version 0 freq_normal;
	uint32_t voltage_over_init; //version 0
	uint32_t voltage_over_target;//version 0
	uint32_t freq_over; //version 0
	uint8_t rsv0[4];
	uint8_t rsv1[4];
	uint8_t rsv2[4];
	uint8_t rsv3[4];
	uint8_t rsv4[4];
	uint8_t rsv5[4];
	uint8_t rsv6[4];;
	uint8_t rsv7_1;
	uint8_t rsv7_2;
	uint8_t rsv7_3;
	uint8_t crc;
}eprom_data;

extern const uint8_t i2c_slave_addr_eprom[MAX_CHAIN_NUM];
extern eprom_data eeprom_tmp_data[MAX_CHAIN_NUM];

esp_err_t eeprom_init(uint8_t i2c_master_index, uint8_t slave_addr, uint8_t eeprom_index, const char* tag);

esp_err_t eeprom_page_write(uint8_t eeprom_index, uint8_t* write_buf);
esp_err_t eeprom_page_read(uint8_t eeprom_index, uint8_t* read_buf);

esp_err_t eeprom_write_data(uint8_t eeprom_index, 
    uint32_t voltage_normal_init, uint32_t voltage_normal_target, uint32_t freq_normal,
	uint32_t voltage_over_init, uint32_t voltage_over_target, uint32_t freq_over);
esp_err_t eeprom_read_data(uint8_t eeprom_index);

int32_t get_voltage_from_eeprom(uint8_t eeprom_index);
int32_t get_freq_from_eeprom(uint8_t eeprom_index);
int32_t get_bin_from_eeprom(uint8_t eeprom_index);
int32_t get_sn_from_eeprom(uint8_t eeprom_index, uint8_t* sn);
int32_t update_sn_to_eeprom(uint8_t eeprom_index, uint8_t* sn);

void factory_set_bin_type(uint8_t bin_class);
uint8_t factory_get_bin_type();

int testWriteEpromData(int chain);
/* CRC5() came from the components/a binary blob; eeprom.c now uses
 * crc5_bits() from components/asic/crc.c instead. */

#endif