#ifndef NETWORK_H_
#define NETWORK_H_

#include "connect.h"

typedef struct
{
	char wifi_on;
    char wifi_ssid[32];
    char wifi_status[20];
    char wifi_ip_str[16]; // IP4ADDR_STRLEN_MAX
    bool wifi_ap_enabled;
	bool wifi_get_ip;

	char eth_on;
	char eth_status[20];
	char eth_ip_str[16];
	bool eth_get_ip;
}NetWorkInfo;

void network_set_wifi_status(wifi_status_t status, int retry_count, int reason);
void network_set_wifi_ap_status(bool enabled);

void network_eth_init_test(void);

void network_init(void * globalState);
NetWorkInfo  network_get_info(void);
bool network_is_connected(void);

#endif /* NETWORK_H_ */
