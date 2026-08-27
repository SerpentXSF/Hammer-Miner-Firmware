#ifndef ASIC_ABSTRACTION_H_
#define ASIC_ABSTRACTION_H_

#include <stdint.h>
#include <string.h>

#define SERIALTX_DEBUG true
#define SERIALRX_DEBUG false
#define DEBUG_WORK false //causes insane amount of debug output
#define DEBUG_JOBS false //causes insane amount of debug output


#define MS_SYNC_TAG 					0xCDAB

#define MS_MODE_CMD						0
#define MS_MODE_WORK					1

#define MS_SYNC_MODE_LEN				4

#define MS_CMD_LEN						12
#define MS_RESP_LEN						11

#define MS_CORE_SET_CMD_LEN				16
#define MS_CORE_GET_CMD_LEN				12
#define MS_CORE_SYNC_MODE_LEN			5

#define MS_HASHJOB1_LEN					93
#define MS_HASHJOB_SYNC_MODE_LEN	    5
#define MS_HASHJOB_DATA_LEN     76


struct cmd_setReg
{
	uint16_t	sync;
	uint8_t		mode;
	uint8_t 	len;
	uint8_t		cmd_type:4;
	uint8_t		cmd_rsvd:3;
	uint8_t 	cmd_all:1;	
	uint8_t		chip_addr;
	uint8_t		reg_addr;
	uint32_t	reg_data;
	uint8_t crc:5;
	uint8_t crc_rsvd:3;
}__attribute__((packed));


struct cmd_getReg
{
	uint16_t	sync;
	uint8_t		mode;
	uint8_t 	len;
	uint8_t		cmd_type:4;
	uint8_t		cmd_rsvd:3;
	uint8_t 	cmd_all:1;	
	uint8_t		chip_addr;
	uint8_t		reg_addr;
	uint8_t		rsvd[4];
	uint8_t crc:5;
	uint8_t crc_rsvd:3;
}__attribute__((packed));

struct cmd_Inactive
{
	uint16_t	sync;
	uint8_t		mode;
	uint8_t 	len;
	uint8_t		cmd_type:4;
	uint8_t		cmd_rsvd:4;	
	uint8_t		rsvd[6];
	uint8_t 	crc:5;
	uint8_t 	crc_rsvd:3;
}__attribute__((packed));

struct cmd_setAddress
{
	uint16_t	sync;
	uint8_t		mode;
	uint8_t 	len;
	uint8_t		cmd_type:4;
	uint8_t		cmd_rsvd:3;	
	uint8_t 	cmd_all:1;	
	uint8_t		chip_addr;
	uint8_t		rsvd[5];
	uint8_t 	crc:5;
	uint8_t 	crc_rsvd:3;
}__attribute__((packed));

struct job_coreSetReg
{
	uint16_t	sync;
	uint8_t		mode;
	uint16_t 	len;
	uint8_t		job_type:4;
	uint8_t		job_rsvd:2;	
	uint8_t 	job_no_coreid_check:1;
	uint8_t 	job_no_chipid_check:1;

	uint8_t		chip_id;
	uint8_t 	core_id;
	uint8_t 	core_id_mask;
	uint8_t 	reg_addr;
	uint32_t 	reg_data;
	uint16_t 	crc16;
}__attribute__((packed));

struct job_coreGetReg
{
	uint16_t	sync;
	uint8_t		mode;
	uint16_t 	len;
	uint8_t		job_type:4;
	uint8_t		job_rsvd:2;	
	uint8_t 	job_no_coreid_check:1;
	uint8_t 	job_no_chipid_check:1;

	uint8_t		chip_id;
	uint8_t 	core_id;
	uint8_t 	core_id_mask;
	uint8_t 	reg_addr;
	uint16_t 	crc16;
}__attribute__((packed));

struct job_HashJob1
{
	uint16_t sync;
	uint8_t mode;
	uint16_t len;
	uint8_t job_type:4;
	uint8_t job_rsvd:2;
	uint8_t job_no_coreid_check:1;
	uint8_t job_no_chipid_check:1;
	uint8_t chip_id;
	uint8_t core_id;
	uint8_t core_id_mask;
	uint8_t msn;
	uint8_t wc;
	uint32_t initNonce;
	uint8_t data[76];
	uint16_t crc;
}__attribute__((packed));

struct nonce_response
{
  uint16_t sync;
  uint8_t type;
  uint32_t nonce;
  uint8_t wc;
  uint8_t chip_id;
  uint8_t core_id;
  uint8_t 	crc:5;
	uint8_t 	crc_rsvd:3;
}__attribute__((packed));


void chain_inactive(uint32_t chain_num);
void set_address(uint32_t chain_num, unsigned char chip_addr);
void set_top_reg(uint32_t chain_num, bool is_all, uint8_t chip_addr, 
                    uint8_t reg_addr, uint32_t reg_value, bool should_check);
void get_top_reg(uint32_t chain_num, bool is_all, uint8_t chip_addr, 
                    uint8_t reg_addr, uint32_t reg_value);
int check_top_reg(uint32_t chain_num, bool is_all, uint8_t chip_addr, 
                    uint8_t reg_addr, uint32_t reg_value);
void set_core_reg(uint32_t chain_num, bool is_all, uint8_t chip_addr,
                    uint8_t is_all_core, uint8_t core_id,
                    uint8_t reg_addr, uint32_t reg_value, bool should_check);
void get_core_reg(uint32_t chain_num, bool is_all, uint8_t chip_addr, 
                    uint8_t is_all_core, uint8_t core_id, uint8_t reg_addr);
int check_core_reg(uint32_t chain_num, bool is_all, uint8_t chip_addr, 
                    uint8_t is_all_core, uint8_t core_id,
                    uint8_t reg_addr, uint32_t reg_value);

void config_all_top_reg(bool is_all, uint8_t chip_addr, 
                    uint8_t reg_addr, uint32_t reg_value, bool should_check);
void config_all_core_reg(bool is_all, uint8_t chip_addr, uint8_t is_all_core, uint8_t core_id,
                    uint8_t reg_addr, uint32_t reg_value, bool should_check);

int pack_ms_cmd_getreg(uint8_t* str, uint8_t all, uint8_t chip_addr, uint8_t reg_addr);
int pack_ms_cmd_setreg(uint8_t* str, uint8_t all, uint8_t chip_addr, uint8_t reg_addr,uint32_t reg_data);
int pack_ms_cmd_inactive(uint8_t* str);
int pack_ms_cmd_setaddr(uint8_t* str, uint8_t all, uint8_t chip_addr);
int pack_ms_job_setCoreReg(uint8_t* str, uint8_t no_chipid_check, uint8_t chip_id,
	    uint8_t is_all_core, uint8_t core_id, uint8_t reg_addr, uint32_t reg_data);
int pack_ms_job_getCoreReg(uint8_t* str, uint8_t no_chipid_check,uint8_t chip_id,
	    uint8_t is_all_core, uint8_t core_id, uint8_t reg_addr);
int pack_ms_job_hashJob1(uint8_t* buf, uint8_t no_chipid_check,uint8_t chip_id,
	    uint8_t is_all_core, uint8_t core_id, uint8_t workid, uint8_t* data, uint32_t init_nonce);

int pack_ms_core_job_hashJob1(uint8_t* buf, uint8_t no_chipid_check,uint8_t chip_id,
	    uint8_t is_all_core, uint8_t core_id, uint8_t core_id_mask,
        uint8_t workid, uint8_t* data, uint32_t init_nonce);

unsigned char CRC5(unsigned char *ptr, unsigned char len);
#endif