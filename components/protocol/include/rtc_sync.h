#ifndef _RTC_SYNC_H
#define _RTC_SYNC_H

#include "esp_err.h"

void sntp_init_sync(const char* str_ntp_server, const char* str_ntp_server_backup);
void print_time();

#endif