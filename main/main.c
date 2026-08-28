#include <stdio.h>
#include <inttypes.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "mbedtls/platform.h"
#include "nvs_config.h"
#include "nvs_device.h"
#include "main.h"
#include "global_state.h"
#include "self_test.h"
#include "serial.h"
#include "asic.h"
#include "system.h"
#include "gpio_input_output.h"
#include "common.h"
#include "device.h"
#include "http_server.h"

#include "stratum_task.h"
#include "stratum_poolb_task.h"
#include "asic_task.h"
#include "create_jobs_task.h"
#include "asic_result_task.h"
#include "protocol_task.h"
#include "socket_api_task.h"
#include "influx_task.h"
#include "ping_task.h"
#include "http_task.h"

#include "rtc_sync.h"
#include "lvgl_porting.h"
#include "lv_input.h"

#include "network.h"


static const char * TAG = "serpentx";

static GlobalState GLOBAL_STATE = {
    .extranonce_str = NULL, 
    .extranonce_2_len = 0, 
    .abandon_work = 0, 
    .version_mask = 0,
    .ASIC_initalized = false,
    .interface_initalized = false
};

void show_mining_screen(void)
{
    SYSTEM_notify_status_change(&GLOBAL_STATE, SYSTEM_NORMAL_MINING);
}

void restart_with_reason(const char *reason)
{
    ESP_LOGI(TAG, "Restarting with reason: %s", reason);
    nvs_config_set_string(NVS_CONFIG_RESTART_REASON, reason);
    power_off_hashboard(&GLOBAL_STATE);
    for (int i = 3; i >= 0; i--) {
        ESP_LOGI(TAG, "Restarting in %d seconds...", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

void restart(void)
{
    restart_with_reason("Normal restart");
}

void init_jobs(GlobalState *GLOBAL_STATE)
{
    for(int chain_num = 0; chain_num < MAX_CHAIN_NUM; chain_num++){
        if(GLOBAL_STATE->chain_pluged[chain_num]){
            //initialize the semaphore
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].semaphore = xSemaphoreCreateBinary();

            //GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs = malloc(sizeof(bm_job *) * 128);
            //GLOBAL_STATE->valid_jobs[chain_num] = malloc(sizeof(uint8_t) * 128);
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs = heap_caps_malloc(sizeof(bm_job *) * 128, MALLOC_CAP_SPIRAM);
            GLOBAL_STATE->valid_jobs[chain_num] = heap_caps_malloc(sizeof(uint8_t) * 128, MALLOC_CAP_SPIRAM);
            
            for (int i = 0; i < 128; i++)
            {
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[i] = NULL;
                GLOBAL_STATE->valid_jobs[chain_num][i] = 0;
            }
        }
    }
}

static void restart_callback(void)
{
    esp_rom_printf("\nSerpentX restarting...\n\n");
}

void analyze_task_memory(void) {
    ESP_LOGI("TASKS", "=== 任务内存分析 ===");
    
    // 获取任务数量
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    ESP_LOGI("TASKS", "总任务数: %d", task_count);
    
    // 分配数组存储任务状态
    TaskStatus_t *task_status = (TaskStatus_t *)pvPortMalloc(task_count * sizeof(TaskStatus_t));
    
    if (task_status != NULL) {
        // 获取任务状态数组
        task_count = uxTaskGetSystemState(task_status, task_count, NULL);
        
        // 打印每个任务的信息
        for (int i = 0; i < task_count; i++) {
            ESP_LOGI("TASKS", "任务: %s", task_status[i].pcTaskName);
            ESP_LOGI("TASKS", "  优先级: %d", task_status[i].uxCurrentPriority);
            ESP_LOGI("TASKS", "  栈高水位线: %d", task_status[i].usStackHighWaterMark);
            ESP_LOGI("TASKS", "  任务编号: %d", task_status[i].xTaskNumber);
        }
        
        vPortFree(task_status);
    }
}

void improved_memory_monitor(void) 
{
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    ESP_LOGI("MEMORY", "=== 改进后内存状态 ===");
    ESP_LOGI("MEMORY", "内部RAM空闲: %d bytes", internal_free);
    ESP_LOGI("MEMORY", "SPIRAM空闲: %d bytes", spiram_free);
    ESP_LOGI("MEMORY", "总堆空闲: %d bytes", esp_get_free_heap_size());
    ESP_LOGI("MEMORY", "最小堆: %d bytes", esp_get_minimum_free_heap_size());
    
    if (internal_free < 20000) { // 如果内部RAM少于20KB
        ESP_LOGW("MEMORY", "警告: 内部RAM仍然紧张!");
    }
}

// 内存监控任务
void memory_monitor_task(void *pvParameters) {
    ESP_LOGI("MONITOR", "内存监控任务启动");
    
    while (1) {
        ESP_LOGI("MONITOR", "=== 实时内存状态 ===");
        
        // 基础内存信息
        size_t free_heap = xPortGetFreeHeapSize();
        size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
        
        ESP_LOGI("MONITOR", "当前堆: %d bytes, 最小堆: %d bytes", 
                 free_heap, min_free_heap);
        
        // 堆碎片分析
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_8BIT);
        
        ESP_LOGI("MONITOR", "最大空闲块: %d bytes", info.largest_free_block);
        ESP_LOGI("MONITOR", "空闲块数量: %d", info.free_blocks);
        
        // 内存使用率计算
        float fragmentation = 0.0;
        if (info.largest_free_block > 0) {
            fragmentation = (1.0 - ((float)info.largest_free_block / info.total_free_bytes)) * 100.0;
        }
        ESP_LOGI("MONITOR", "内存碎片率: %.2f%%", fragmentation);
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒检查一次

        analyze_task_memory();
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒检查一次
        improved_memory_monitor();

        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒检查一次
    }
}

void showLastResetReason(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    static const char* m_lastResetReason;
    switch (reason) {
        case ESP_RST_UNKNOWN: m_lastResetReason = "Unknown"; break;
        case ESP_RST_POWERON: m_lastResetReason = "Power on reset"; break;
        case ESP_RST_EXT: m_lastResetReason = "External reset"; break;
        case ESP_RST_SW: m_lastResetReason = "Software reset"; break;
        case ESP_RST_PANIC: m_lastResetReason = "Software panic reset"; break;
        case ESP_RST_INT_WDT: m_lastResetReason = "Interrupt watchdog reset"; break;
        case ESP_RST_TASK_WDT: m_lastResetReason = "Task watchdog reset"; break;
        case ESP_RST_WDT: m_lastResetReason = "Other watchdog reset"; break;
        case ESP_RST_DEEPSLEEP: m_lastResetReason = "Exiting deep sleep"; break;
        case ESP_RST_BROWNOUT: m_lastResetReason = "Brownout reset"; break;
        case ESP_RST_SDIO: m_lastResetReason = "SDIO reset"; break;
        default: m_lastResetReason = "Not specified"; break;
    }
    ESP_LOGI(TAG, "Reset reason: %s", m_lastResetReason);
}

void showLastResetReasonCustom(void)
{
    char *custom_reason = nvs_config_get_string(NVS_CONFIG_RESTART_REASON, "None");
    if (custom_reason != NULL) {
        ESP_LOGI(TAG, "Custom restart reason: %s", custom_reason);
        nvs_config_set_string(NVS_CONFIG_RESTART_REASON, "None");
    }
}

// Custom calloc function that allocates from PSRAM
void *psram_calloc(size_t num, size_t size) {
    void *ptr = heap_caps_calloc(num, size, MALLOC_CAP_SPIRAM);
    if (!ptr) {
        ESP_LOGE(TAG, "PSRAM allocation failed! Falling back to internal RAM.");
        ptr = heap_caps_calloc(num, size, MALLOC_CAP_DEFAULT);
    }
    return ptr;
}

void free_psram(void *ptr) {
    heap_caps_free(ptr);
}

void app_main(void)
{
    init_logging_system();

    esp_rom_printf("\nSerpentX starting....\n\n");
    ESP_LOGI(TAG, "APP: %s %s", CONFIG_APP_PROJECT_VER, __TIME__);
	
    showLastResetReason();

    // use PSRAM because TLS costs a lot of internal RAM
    mbedtls_platform_set_calloc_free(psram_calloc, free_psram);

    esp_err_t ret = esp_register_shutdown_handler(restart_callback);
    if (ret != ESP_OK) {
        ESP_LOGE("RESTART", "Failed to register restart callback: %s", esp_err_to_name(ret));
    }

    //initialize the ESP32 NVS
    if (NVSDevice_init() != ESP_OK){
        ESP_LOGE(TAG, "Failed to init NVS");
        return;
    }

    showLastResetReasonCustom();

    /*get the nvs config, parse the NVS config into GLOBAL_STATE*/
    if (NVSDevice_parse_config(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse NVS config");
        return;
    }

    GLOBAL_STATE.global_parameter_mutex = xSemaphoreCreateMutex();   
    SYSTEM_init_system(&GLOBAL_STATE);
    #ifdef STATISTIC_SYSTEM_FEATURE
    statistic_init_system_by_device(&GLOBAL_STATE);
    #endif

    /*init the lcd display.*/
    dev_display_init(&GLOBAL_STATE);

	ESP_ERROR_CHECK(hammer_i2c_init());

    #if 1
    if(should_test()){
        self_test(&GLOBAL_STATE);
        power_off_hashboard(&GLOBAL_STATE);
        if(GLOBAL_STATE.SELF_TEST_MODULE.result)
            vTaskDelay(pdMS_TO_TICKS(5000));
        else
            vTaskDelay(pdMS_TO_TICKS(10000));
        restart_with_reason("Self test completed");
        return;
    }
    #else
    {
        nvs_config_set_u16(NVS_CONFIG_SELF_TEST,1);
    }
    #endif

	/*create the health task*/
    xTaskCreate(health_maintenance_task, "health_maintener", 4096, (void*)&GLOBAL_STATE, 12, NULL);

    network_init(&GLOBAL_STATE);

    /*sync the locol time.*/
    vSemaphoreCreateBinary(xSyncTimeSemaphore);
    vSemaphoreCreateBinary(xSyncTimeDoneSemaphore);
    if(NULL == xSyncTimeSemaphore || NULL == xSyncTimeDoneSemaphore){
        ESP_LOGE(TAG, "Could not create xSyncTimeSemaphore or xSyncTimeDoneSemaphore.");
        ESP_ERROR_CHECK(ESP_FAIL);
    }else{
        if (pdFALSE == xSemaphoreTake(xSyncTimeSemaphore, 0)) {
            ESP_LOGI(TAG, "Could not take xSyncTimeSemaphore.");
            ESP_ERROR_CHECK(ESP_FAIL);
        }else{
            xTaskCreate(rtc_sync_task, "rtc-sync", 4096, NULL, 5, NULL);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
   
    xSemaphoreTake(xSyncTimeDoneSemaphore, portMAX_DELAY);

    /*Should enter sleep mode?*/
    if(SLEEP_MODE == GLOBAL_STATE.SYSTEM_MODULE.boot_mode){
        ESP_LOGI(TAG, "Enter sleep mode....");
        GLOBAL_STATE.SYSTEM_MODULE.is_sleep_mode = true;
        return;
    }

    ESP_ERROR_CHECK(init_all_peripherals(&GLOBAL_STATE));
    /*Serial Init and detect the asic.*/
    if(ASIC_detect(&GLOBAL_STATE))
    {
        power_off_hashboard(&GLOBAL_STATE);
        xTaskCreate(system_task, "system task", 4096, (void*)&GLOBAL_STATE, 4, NULL);
        show_mining_screen();
        return;
    }
    else
    {
        if((strlen(GLOBAL_STATE.SYSTEM_MODULE.pool_url) < 5)&&(strlen(GLOBAL_STATE.SYSTEM_MODULE.fallback_pool_url) < 5))
        {
            ESP_LOGI(TAG, "error pool : %s , %s ",GLOBAL_STATE.SYSTEM_MODULE.pool_url, GLOBAL_STATE.SYSTEM_MODULE.fallback_pool_url);
            power_off_hashboard(&GLOBAL_STATE);
        }
    }

    /*create the queue.*/
    queue_init(&GLOBAL_STATE.stratum_queue);

    /* dual mining: pool B's own queue and the locks guarding its session state.
     * Initialised unconditionally so enabling dual mining at runtime does not
     * need a restart to have somewhere to put work. */
    queue_init(&GLOBAL_STATE.stratum_queueB);
    pthread_mutex_init(&GLOBAL_STATE.transportB_lock, NULL);
    pthread_mutex_init(&GLOBAL_STATE.extranonceB_lock, NULL);
    for(int i = 0; i < MAX_CHAIN_NUM; i++){
        if(GLOBAL_STATE.chain_pluged[i])
            queue_init(&GLOBAL_STATE.ASIC_jobs_queue[i]);
    }
    init_jobs(&GLOBAL_STATE);

    /*create the stratum task.*/
    xTaskCreate(stratum_task, "stratum_manager", 8192, (void *)&GLOBAL_STATE, 5, NULL);
    /* pool B idles cheaply when dual mining is off, so it is always started */
    xTaskCreate(stratum_poolb_task, "stratum_poolb", 8192, (void *)&GLOBAL_STATE, 5, NULL);
    /*create the uart task.*/
    xTaskCreate(create_jobs_task, "stratum_worker", 6144, (void *)&GLOBAL_STATE, 10, NULL);
    /*create the socket api task.*/
    #ifdef SOCKET_API_FEAUTRE
    xTaskCreate(socket_api_task, "socket-api", 4096, (void*)&GLOBAL_STATE, 16, NULL);
    #endif
    /*create influx task*/
    //xTaskCreate(influx_task, "influx", 8192, (void*)&GLOBAL_STATE, 23, NULL);
    /*create ping pool task*/
    //xTaskCreate(ping_task, "ping task", 4096, (void*)&GLOBAL_STATE, 20, NULL);
    /*create http task*/
    xTaskCreate(http_task, "http task", 6144, (void*)&GLOBAL_STATE, 3, NULL);
    /*create memory monitor task*/
    //xTaskCreate(memory_monitor_task, "memory monitor task", 4096, (void*)&GLOBAL_STATE, 1, NULL);
    /*create system task*/
    xTaskCreate(system_task, "system task", 4096, (void*)&GLOBAL_STATE, 4, NULL);

    AsicParam chain_param[2] = {{.chain_num = 0, .p_global_state = &GLOBAL_STATE}, {.chain_num = 1, .p_global_state = &GLOBAL_STATE}};
    for(uint32_t i = 0; i < MAX_CHAIN_NUM; i++)
    {
        if(GLOBAL_STATE.chain_pluged[i]){
            ESP_LOGI(TAG, "create tasks for chain %"PRIu32"", i);
            /*create the asic work.*/
            char tmp_name[30];
            BaseType_t xReturn;

            {
                sprintf(tmp_name, "asic_task_%"PRIu32"", i);
                xReturn = xTaskCreate(ASIC_task, tmp_name, 5120, (void *)&(chain_param[i]), 10, NULL);
                if(pdPASS != xReturn){
                    ESP_LOGW(TAG, "Create %s failed %d.", tmp_name, xReturn);
                }
            }
            
            {
                sprintf(tmp_name, "asic_result_%"PRIu32"", i);
                xReturn = xTaskCreate(ASIC_result_task, tmp_name, 6144, (void *)&(chain_param[i]), 15, NULL);
                if(pdPASS != xReturn){
                    ESP_LOGW(TAG, "Create %s failed %d.", tmp_name, xReturn);
                }
            }
            #if 1
            {
                sprintf(tmp_name, "hashrate_task_%"PRIu32"", i);
                xReturn = xTaskCreate(hashrate_monitor_task, tmp_name, 4096, (void *)&(chain_param[i]), 5, NULL);
                if(pdPASS != xReturn){
                    ESP_LOGW(TAG, "Create %s failed %d.", tmp_name, xReturn);
                }
            }
            #endif
        }
    }

    SYSTEM_notify_status_change(&GLOBAL_STATE, SYSTEM_HARDWARE_OK);
    show_mining_screen();
    
    ESP_LOGI(TAG, "heap free heap size: %" PRIu32 " bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
    /*restart();*/
}
