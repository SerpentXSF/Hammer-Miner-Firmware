#ifndef _PROTOCOL_TASK_H
#define _PROTOCOL_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t xIpReporterSemaphore;
extern SemaphoreHandle_t xSyncTimeSemaphore, xSyncTimeDoneSemaphore;

void ip_reporter_task(void *pvParameters);
void rtc_sync_task(void *pvParameters);

bool check_time_is_synced();
bool is_valid_ip(const char *ip_str);
#endif

