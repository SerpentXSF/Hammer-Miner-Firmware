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
#include "http_server.h"

#include "system.h"
#include "gpio_input_output.h"
#include "common.h"
#include "device.h"

#include "lv_input.h"
#include "network.h"
#include "ethernet_init.h"
#include "global_state.h"

static const char * TAG = "NETWORK";

static GlobalState * GLOBAL_STATE = NULL;

static NetWorkInfo netWork_Info;


void network_set_wifi_status(wifi_status_t status, int retry_count, int reason)
{
    switch(status) {
        case WIFI_CONNECTING:
            snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "Connecting...");
            return;
        case WIFI_CONNECTED:
            snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "Connected!");
			netWork_Info.wifi_get_ip = 1;
            if(netWork_Info.eth_on)
			{
				wifi_softap_off();
				ESP_LOGI(TAG, "Connected to SSID: %s", netWork_Info.wifi_ssid);
			}
            return;
        case WIFI_RETRYING:
            // See https://github.com/espressif/esp-idf/blob/master/components/esp_wifi/include/esp_wifi_types_generic.h for codes
            switch(reason) {
                case 201:
                    snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "No AP found (%d)", retry_count);
                    return;
                case 15:
                case 205:
                    snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "Password error (%d)", retry_count);
                    return;
                default:
                    snprintf(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, 20, "Error %d (%d)", reason, retry_count);
                    return;
            }
		case WIFI_DISCONNECTED:
			netWork_Info.wifi_get_ip = 0;
            if(retry_count)
            {
                if(netWork_Info.eth_get_ip == 0)
                {
                    ESP_LOGE(TAG, "Failed to connect to SSID: %s", netWork_Info.wifi_ssid);

                    strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "Failed to connect", 20);
                    SYSTEM_notify_error_info(GLOBAL_STATE, WIFI_CONNETION_ERROR, NULL);
                    ESP_LOGI(TAG, "Finished, waiting for user input:");
                }
                else
                {
                    wifi_softap_off();
                    ESP_LOGI(TAG, "ap off , retry_count = %d", retry_count);
                    esp_log_level_set("wifi", CONFIG_LOG_DEFAULT_LEVEL_INFO);
                }
            }
			return;
    }
    ESP_LOGW(TAG, "Unknown status: %d", status);
}

void network_set_wifi_ap_status(bool enabled) 
{
    GLOBAL_STATE->SYSTEM_MODULE.ap_enabled = enabled;
}

int network_wifi_connect(void)
{
    int ret = -1;

    // pull the wifi credentials and hostname out of NVS
    char * wifi_ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, WIFI_SSID);
    char * wifi_pass = nvs_config_get_string(NVS_CONFIG_WIFI_PASS, WIFI_PASS);
    char * hostname  = nvs_config_get_string(NVS_CONFIG_HOSTNAME, HOSTNAME);

    strncpy(netWork_Info.wifi_ssid, wifi_ssid, sizeof(netWork_Info.wifi_ssid));
    netWork_Info.wifi_ssid[sizeof(netWork_Info.wifi_ssid)-1] = 0;

    int time_out = 0;
    if(!gpio_get_level(PIN_BUTTON_NEXT))
    {
        wifi_ssid[1] = 0;
        wifi_pass[1] = 0;
        time_out = 1;
    }

    // init and connect to wifi
    wifi_init(wifi_ssid, wifi_pass, hostname, netWork_Info.wifi_ip_str);

    if(time_out)
    {
        SYSTEM_notify_error_info(GLOBAL_STATE, WIFI_CONNETION_ERROR, NULL);
    }

	if(netWork_Info.eth_on == 0)
	{
	    // if the wifi is connected to ap, whether turn off the softap?
	    EventBits_t result_bits = wifi_connect();
	    wifi_sta_list_t sta_list;

	    if (result_bits & WIFI_CONNECTED_BIT) {
	        ESP_LOGI(TAG, "Connected to SSID: %s", wifi_ssid);
	        strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "Connected!", 20);
	        SYSTEM_notify_status_change(GLOBAL_STATE, SYSTEM_WIFI_CONNECTED);

	        wifi_softap_off();
	    } else if (result_bits & WIFI_FAIL_BIT) {
	        ESP_LOGE(TAG, "Failed to connect to SSID: %s", wifi_ssid);

	        strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "Failed to connect", 20);
	        SYSTEM_notify_error_info(GLOBAL_STATE, WIFI_CONNETION_ERROR, NULL);
	        /*set ap mode*/
	        /*wifi_only_softap();
	        ESP_LOGI(TAG, "wifi switch to soft ap mode for configuration.");*/
	        
	        // User might be trying to configure with AP, just chill here
	        ESP_LOGI(TAG, "Finished, waiting for user input:");
	        while (1) {
	            vTaskDelay(1000 / portTICK_PERIOD_MS);
	            if(esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK)
	            {
	                if (sta_list.num > 0)
	                {
	                    time_out = 1;
	                    ESP_LOGI(TAG, "AP Connect num: %d", sta_list.num);
	                }
	            }

	            //if(time_out)
	            {
	                time_out ++;
	                if(time_out > 90)
	                {
	                    restart_with_reason("WiFi connection timeout");
	                }
	            }
	        }
	    } else {
	        ESP_LOGE(TAG, "UNEXPECTED EVENT : %X",result_bits);
	        strncpy(GLOBAL_STATE->SYSTEM_MODULE.wifi_status, "unexpected error", 20);
	        // User might be trying to configure with AP, just chill here
	        SYSTEM_notify_error_info(GLOBAL_STATE, WIFI_CONNETION_ERROR, NULL);        
	        ESP_LOGI(TAG, "Finished, waiting for user input::");
	        while (1) {
	            vTaskDelay(1000 / portTICK_PERIOD_MS);
	            if(esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK)
	            {
	                if (sta_list.num > 0)
	                {
	                    time_out = 1;
	                    ESP_LOGI(TAG, "AP Connect num: %d", sta_list.num);
	                }
	            }
	            //if(time_out)
	            {
	                time_out ++;
	                if(time_out > 90)
	                {
	                    restart_with_reason("WiFi connection timeout");
	                }
	            }
	        }
	    }
	}

    if (NULL != wifi_ssid)
        free(wifi_ssid);
    if (NULL != wifi_pass)
        free(wifi_pass);
    if (NULL != hostname)
        free(hostname);
    
    return ret;
}

esp_err_t network_config_wifi_static_ip(void)
{
    esp_err_t ret = ESP_OK;
    uint16_t is_static_ip = nvs_config_get_u16(NVS_CONFIG_IS_STATIC_IP, 0);

    if(1 == is_static_ip){
        char* str_static_ip = nvs_config_get_string(NVS_CONFIG_STATIC_IP, "192.168.11.211");
        char* str_subnet_mask = nvs_config_get_string(NVS_CONFIG_SUBNET_MASK, "255.255.255.0");
        char* str_gateway = nvs_config_get_string(NVS_CONFIG_GATEWAY, "192.168.11.1");
        char* str_dns = nvs_config_get_string(NVS_CONFIG_DNS, "192.168.11.1");
    
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_dhcpc_stop(sta_netif);

        esp_netif_ip_info_t ip_info;
        ip4addr_aton(str_static_ip, (ip4_addr_t *)&ip_info.ip);
        ip4addr_aton(str_subnet_mask, (ip4_addr_t *)&ip_info.netmask);
        ip4addr_aton(str_gateway, (ip4_addr_t *)&ip_info.gw);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

        esp_netif_dns_info_t dns_info;
        ip4addr_aton(str_dns, (ip4_addr_t *)&dns_info.ip.u_addr.ip4);
        dns_info.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info));

        ESP_LOGI(TAG, "WIFI Static IP %s, mask %s, gw %s, dns %s", str_static_ip, str_subnet_mask, str_gateway, str_dns);

        if(NULL != str_static_ip)
            free(str_static_ip);
        if(NULL != str_subnet_mask)
            free(str_subnet_mask);
        if(NULL != str_gateway)
            free(str_gateway);
        if(NULL != str_dns)
            free(str_dns);
    }else{
        ESP_LOGI(TAG, "WIFI Dynamic IP");
    }

    return ret;
}

esp_err_t network_config_eth_static_ip(void)
{
    esp_err_t ret = ESP_OK;
    uint16_t is_static_ip = nvs_config_get_u16(NVS_CONFIG_ETH_IS_STATIC_IP, 0);

    if(1 == is_static_ip){
        char* str_static_ip = nvs_config_get_string(NVS_CONFIG_ETH_STATIC_IP, "192.168.2.222");
        char* str_subnet_mask = nvs_config_get_string(NVS_CONFIG_ETH_SUBNET_MASK, "255.255.255.0");
        char* str_gateway = nvs_config_get_string(NVS_CONFIG_ETH_GATEWAY, "192.168.2.1");
        char* str_dns = nvs_config_get_string(NVS_CONFIG_ETH_DNS, "192.168.2.1");
    
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
        esp_netif_dhcpc_stop(sta_netif);

        esp_netif_ip_info_t ip_info;
        ip4addr_aton(str_static_ip, (ip4_addr_t *)&ip_info.ip);
        ip4addr_aton(str_subnet_mask, (ip4_addr_t *)&ip_info.netmask);
        ip4addr_aton(str_gateway, (ip4_addr_t *)&ip_info.gw);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

        esp_netif_dns_info_t dns_info;
        ip4addr_aton(str_dns, (ip4_addr_t *)&dns_info.ip.u_addr.ip4);
        dns_info.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info));

        ESP_LOGI(TAG, "ETH Static IP %s, mask %s, gw %s, dns %s", str_static_ip, str_subnet_mask, str_gateway, str_dns);

        if(NULL != str_static_ip)
            free(str_static_ip);
        if(NULL != str_subnet_mask)
            free(str_subnet_mask);
        if(NULL != str_gateway)
            free(str_gateway);
        if(NULL != str_dns)
            free(str_dns);
    }else{
        ESP_LOGI(TAG, "ETH Dynamic IP");
    }

    return ret;
}


/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		snprintf(netWork_Info.eth_status, 20, "ETH Link Up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
		snprintf(netWork_Info.eth_status, 20, "ETH Link Down");
		netWork_Info.eth_get_ip = 0;
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
		snprintf(netWork_Info.eth_status, 20, "ETH Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
		snprintf(netWork_Info.eth_status, 20, "ETH Stopped");
		netWork_Info.eth_get_ip = 0;
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void eht_got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    #if 0
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    #endif
	snprintf(netWork_Info.eth_ip_str, IP4ADDR_STRLEN_MAX, IPSTR, IP2STR(&ip_info->ip));
	snprintf(netWork_Info.eth_status, 20, "ETH GetIP");
	netWork_Info.eth_get_ip = 1;
}

void network_eth_init_test(void)
{
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
    // default esp-netif configuration parameters.
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eht_got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }
}

void network_eth_init(void)
{
	// Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    //ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    //ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
    // default esp-netif configuration parameters.
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eht_got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }
}

void network_init(void * globalState)
{
	GLOBAL_STATE = (GlobalState *)globalState;
    wifi_sta_list_t sta_list;
    int time_out = 0;

	netWork_Info.wifi_on = (char)nvs_config_get_u16(NVS_CONFIG_WIFI_ON, 1);
	netWork_Info.eth_on = (char)nvs_config_get_u16(NVS_CONFIG_ETH_ON, 1);

	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    start_rest_server((void *) globalState);

	if(netWork_Info.eth_on)
	{
		network_eth_init();
		network_config_eth_static_ip();
		vTaskDelay(pdMS_TO_TICKS(200));
	}

	if(netWork_Info.wifi_on)
	{
		network_wifi_connect();
		network_config_wifi_static_ip();
		vTaskDelay(pdMS_TO_TICKS(200));
	}

	if((netWork_Info.wifi_on == 0) && (netWork_Info.eth_on == 0))
	{
		network_eth_init();
		network_config_eth_static_ip();
		vTaskDelay(pdMS_TO_TICKS(200));
	}

	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(200));
		if(netWork_Info.eth_get_ip || netWork_Info.wifi_get_ip)
		{
            if((netWork_Info.eth_get_ip == 1)&&(netWork_Info.wifi_get_ip == 0))
            {
                wifi_softap_off();
                ESP_LOGI(TAG, "eth get ip, wifi ap off");
                esp_log_level_set("wifi", CONFIG_LOG_DEFAULT_LEVEL_INFO);
            }
			SYSTEM_notify_status_change(GLOBAL_STATE, SYSTEM_WIFI_CONNECTED);
			break;
		}

        if(esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK)
        {
            if (sta_list.num > 0)
            {
                time_out = 1;
            }
        }

        {
            time_out ++;
            if(time_out > 5*60*5)
            {
                restart_with_reason("Network configuration timeout");
            }
        }
    }
}

NetWorkInfo  network_get_info(void)
{
	return netWork_Info;
}

bool network_is_connected(void)
{
	return (netWork_Info.eth_get_ip | netWork_Info.wifi_get_ip);
}
