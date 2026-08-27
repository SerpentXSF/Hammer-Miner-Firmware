#ifndef _IP_REPORTER_H
#define _IP_REPORTER_H

#include "esp_err.h"

esp_err_t send_ip_reporter();

int get_ip(char *ipaddr);
int get_eth_ip(char *ipaddr);
esp_err_t get_mac(char *mac);

esp_err_t get_ip_netmask_gw(char *str_ip, char *str_netmask, char *str_gw);
esp_err_t get_hostname(char *hostname);
esp_err_t get_dns_server(char* dns_server);

#endif