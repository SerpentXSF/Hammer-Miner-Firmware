#ifndef _API_HELPER_H
#define _API_HELPER_H

#include "cJSON.h"
#include "global_state.h"

cJSON *get_network_info_json();
esp_err_t set_network_conf_json(cJSON *network_conf);




#endif