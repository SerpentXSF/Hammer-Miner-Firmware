#ifndef _SOCKET_API_TASK_H
#define _SOCKET_API_TASK_H

#include "global_state.h"

#include <cJSON.h>

char* process_command(const char *json_str, GlobalState *GLOBAL_STATE);


void socket_api_task(void *pvParameters);

#endif