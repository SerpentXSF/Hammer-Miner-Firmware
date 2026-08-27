#ifndef LT0051_H_
#define LT0051_H_

#include <stdint.h>
#include "common.h"
#include "mining.h"
#include "global_state.h"

// Register description
#define REG_CHIP_ADDRESS									0x0
#define REG_UART_BAND										0x1
#define REG_UART_GAP										0x2
#define REG_PLL0_CONFIG									    0x3
#define REG_PLL1_CONFIG									    0x4

#define REG_PLL_STATUS										0x10

#define REG_SOFT_RESET										0x15

#define REG_HASH_CLK_CTRL								    0x22

#define COREREG_TICKMASK						            0x2

#define COREREG_SRAMCTRL0						            0x4
#define COREREG_SRAMCTRL1						            0x5

#define BASE_25M
//#define BASE_50M

#if defined(BASE_25M)
#define BASE_CLK_M 25.0
#define MIN_DIVR 0
#define MAX_DIVR 2
#define MIN_DIVF 79
#define MAX_DIVF 479
#define DEFAULT_Pll1_FREQ 300

#elif defined(BASE_50M)
#define BASE_CLK_M 50.0
#define MIN_DIVR 0
#define MAX_DIVR 6
#define MIN_DIVF 39
#define MAX_DIVF 558
#define DEFAULT_Pll1_FREQ 600
#else
	#error BASE_CLOCK Undefined
#endif

typedef struct _pll_config_data{
	uint32_t pll_range:3;
	uint32_t pll_divq:8;
	uint32_t pll_divf:10;
	uint32_t pll_divr:6;
	uint32_t pll_bypass:1;
	uint32_t pll_resetn:1;
	uint32_t rsv:3;
}pll_config_data;

esp_err_t LT0051_init(GlobalState *state, uint64_t frequency, uint16_t asic_count);
esp_err_t LT0051_init_by_chain(GlobalState *state, int chain_num, uint64_t frequency, uint16_t asic_count);

int LT0051_dummy_init(GlobalState *state, uint64_t frequency, uint16_t asic_count);

void LT0051_send_work(GlobalState *state, bm_job * next_bm_job, uint32_t chain_num, uint8_t word_id);
void LT0051_send_work_by_chip(GlobalState * GLOBAL_STATE, bm_job *next_bm_job, uint32_t chain_num, uint8_t workid, uint16_t asic_count);
void LT0051_send_work_by_cores_group(GlobalState * GLOBAL_STATE, bm_job *next_bm_job, uint32_t chain_num, uint8_t workid, uint8_t core_groups, uint8_t core_id_mask);

void LT0051_set_job_difficulty_mask(GlobalState *, int);
void LT0051_set_version_mask(GlobalState *state, uint32_t version_mask);
int LT0051_set_max_baud_by_chain(GlobalState *state, int chain_num);
int LT0051_set_default_baud(GlobalState *state);
bool LT0051_send_hash_frequency(GlobalState *state, float frequency);
task_result* LT0051_process_work(GlobalState *state, uint32_t chain_num);

int switch_by_chain(GlobalState *state, uint32_t chain_num);
/*
int bypass_asic_freq_by_chain(GlobalState *state, int chain_num);
*/
#endif