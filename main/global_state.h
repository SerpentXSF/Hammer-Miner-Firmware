#ifndef GLOBAL_STATE_H_
#define GLOBAL_STATE_H_

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include <freertos/semphr.h>

#include "common.h"
#include "asic_task.h"
#include "miner.h"
#include "stratum_api.h"
#include "work_queue.h"
#include "health_maintenance.h"
#include "statistic.h"
#include "hashrate_monitor_task.h"

#define STRATUM_USER CONFIG_STRATUM_USER
#define FALLBACK_STRATUM_USER CONFIG_FALLBACK_STRATUM_USER

#define HISTORY_LENGTH      100
#define DIFF_STRING_SIZE    10

/*TODO: replace the MAX_VALID_JOBS*/
#define MAX_VALID_JOBS      128

typedef enum
{
    DEVICE_UNKNOWN = -1,
    DEVICE_MODE1,
    DEVICE_MODE2,
    DEVICE_LOTTO,
    DEVICE_DC02,
    DEVICE_DC04,
    DEVICE_DC06,
    DEVICE_BC04,
    DEVICE_BC08,
    DEVICE_BC01,
    DEVICE_BC02,
    DEVICE_BC06,
    DEVICE_BC01_Pro,
} DeviceModel;

typedef enum
{
    ASIC_UNKNOWN = -1,
    ASIC_LT0051,
} AsicModel;

typedef struct {
    char message[64];
    uint32_t count;
} RejectedReasonStat;

typedef enum{
	SYSTEM_BOOT_UP = 0x0,
	SYSTEM_WIFI_CONNECTED,
	SYSTEM_HARDWARE_OK,
	SYSTEM_POOL_CONNECTED,
	SYSTEM_NORMAL_MINING,
	SYSTEM_ERROR_STATUS,
	SYSTEM_SATUS_MAX_NUM
}SYSTEM_SATUS;

typedef enum{
	SPECIAL_ERROR = 0x0,
	WRONG_ASIC_ERROR,
	FAN_ERROR,
	EEPROM_ERROR,
	RUN_MODE_ERROR,
	CONTROL_SYSTEM_ERROR, /*control borad error.*/
	CONFIG_FILE_ERROR,
	HASHRATE_ERROR,
	HIGH_TEMPERATURE_ERROR,
	WIFI_CONNETION_ERROR,
	NETWORK_ERROR,	/*could not connect to the pool.*/
	SYSTEM_ERROR_MAX_NUM
}SYSTEM_ERROR;

typedef struct
{
    double duration_start;
    int historical_hashrate_rolling_index;
    double historical_hashrate_time_stamps[HISTORY_LENGTH];
    double historical_hashrate[HISTORY_LENGTH];
    int historical_hashrate_init;
    double current_hashrate;
    int64_t start_time;
    uint64_t shares_accepted;
    uint64_t shares_rejected;
    uint64_t work_received;
#ifdef HW_STATISTIC_FEATURE
    uint64_t recveived_nonce;
    uint64_t recveived_hw;
#endif
    RejectedReasonStat rejected_reason_stats[10];
    int rejected_reason_stats_count;
    int screen_page;
    uint64_t best_nonce_diff;
    char best_diff_string[DIFF_STRING_SIZE];
    uint64_t best_session_nonce_diff;
    char best_session_diff_string[DIFF_STRING_SIZE];
    bool FOUND_BLOCK;
    uint16_t BLOCK_NUM;
	
    char ssid[32];
    char wifi_status[20];
    char ip_addr_str[16]; // IP4ADDR_STRLEN_MAX
    char ap_ssid[32];
    bool ap_enabled;
	
    char * pool_url;
    char * fallback_pool_url;
    uint16_t pool_port;
    uint16_t fallback_pool_port;
    char * pool_user;
    char * fallback_pool_user;
    char * pool_pass;
    char * fallback_pool_pass;
    uint16_t pool_difficulty;
    uint16_t fallback_pool_difficulty;
    bool pool_extranonce_subscribe;
    bool fallback_pool_extranonce_subscribe;
    uint16_t pool_tls;
    uint16_t fallback_pool_tls;
    char * pool_cert;
    char * fallback_pool_cert;
    bool is_using_fallback;
    uint16_t overheat_mode;
    uint16_t power_fault;
    uint32_t lastClockSync;
    bool is_screen_active;
    bool is_firmware_update;
    char firmware_update_filename[20];
    char firmware_update_status[20];
    char * asic_status;
    bool is_sleep_mode;
    uint16_t boot_mode;
    char * system_error;
    SYSTEM_SATUS system_status;
    bool is_network_error;      // NETWORK_ERROR 时置 true，跳过 HASHRATE_ERROR 误报
    char *sn[MAX_CHAIN_NUM];
} SystemModule;

typedef struct
{
    bool active;
    char *message;
    bool result;
    bool finished;
} SelfTestModule;

typedef enum{
    SHARE_ACCEPTED,
    SHARE_REJECTED,
    SHARE_DISCARDED,
    SHARE_STALE
}SHARE_STATUS;

typedef struct
{
    DeviceModel device_model;
    char * device_model_str;
    int board_version;
    AsicModel asic_model;
    char * asic_model_str;
    uint16_t asic_count[MAX_CHAIN_NUM];
    uint16_t voltage_domain;
    double asic_job_frequency_ms;
    uint32_t asic_difficulty;
    uint32_t asic_freqency;
#ifdef CONFIG_BC04_INDIVIDUAL_FREQ
    uint16_t bc04_chip_freqs[4];
#endif
    uint16_t asic_vol_max;
    uint16_t asic_vol_min;
	uint16_t asic_vol_default;

    work_queue stratum_queue;
    work_queue ASIC_jobs_queue[MAX_CHAIN_NUM];

    SystemModule SYSTEM_MODULE;
    AsicTaskModule ASIC_TASK_MODULE[MAX_CHAIN_NUM];
    HealthMaintenceModule HEALTH_MODULE;
    SelfTestModule SELF_TEST_MODULE;
#ifdef STATISTIC_SYSTEM_FEATURE    
    StatisticModule STATISTIC_MODULE;
#endif
    HashrateMonitorModule HASHRATE_MONITOR_MODULE;

    SemaphoreHandle_t global_parameter_mutex;
    char *extranonce_str;
    int extranonce_2_len;
    int abandon_work;

    uint8_t * valid_jobs[MAX_CHAIN_NUM];
    pthread_mutex_t valid_jobs_lock[MAX_CHAIN_NUM];

    uint32_t stratum_difficulty;
    bool new_set_mining_difficulty_msg;
    uint32_t version_mask;
    bool new_stratum_version_rolling_msg;

    esp_transport_handle_t transport;

    // A message ID that must be unique per request that expects a response.
    // For requests not expecting a response (called notifications), this is null.
    int send_uid;

    bool ASIC_initalized;
    bool interface_initalized;
    bool chain_pluged[MAX_CHAIN_NUM];
    float real_freq[MAX_CHAIN_NUM];
    uint32_t baud_rate[MAX_CHAIN_NUM];
    uint32_t tmmusk;

    bool screen_flash;

    int block_height;
    char * scriptsig;
    char network_diff_string[DIFF_STRING_SIZE];
    char pd_state;
} GlobalState;

typedef struct{
    uint32_t chain_num; /*uart num*/
    GlobalState *p_global_state;
}AsicParam;

#endif /* GLOBAL_STATE_H_ */
