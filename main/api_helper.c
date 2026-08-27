#include "esp_log.h"
#include <string.h>

#include "api_helper.h"
#include "nvs_config.h"
#include "ip_reporter.h"
#include "protocol_task.h"

const char *TAG = "api-helper";

cJSON *get_network_info_json()
{
    char *host_name = nvs_config_get_string(NVS_CONFIG_HOSTNAME, "");
    char *wifi_ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, "");
    char *wifi_pass = nvs_config_get_string(NVS_CONFIG_WIFI_PASS, "");

    uint16_t isStatic = nvs_config_get_u16(NVS_CONFIG_IS_STATIC_IP, 0);
    char *staticIp = nvs_config_get_string(NVS_CONFIG_STATIC_IP, "");
    char *subnetMask = nvs_config_get_string(NVS_CONFIG_SUBNET_MASK, "");
    char *gateWay = nvs_config_get_string(NVS_CONFIG_GATEWAY, "");
    char *dns = nvs_config_get_string(NVS_CONFIG_DNS, "");

    uint16_t isStatic_ETH = nvs_config_get_u16(NVS_CONFIG_ETH_IS_STATIC_IP, 0);
    char *staticIp_ETH = nvs_config_get_string(NVS_CONFIG_ETH_STATIC_IP, "");
    char *subnetMask_ETH = nvs_config_get_string(NVS_CONFIG_ETH_SUBNET_MASK, "");
    char *gateWay_ETH = nvs_config_get_string(NVS_CONFIG_ETH_GATEWAY, "");
    char *dns_ETH = nvs_config_get_string(NVS_CONFIG_ETH_DNS, "");

    uint16_t network_mode = (char)nvs_config_get_u16(NVS_CONFIG_WIFI_ON, 1);
    if(nvs_config_get_u16(NVS_CONFIG_ETH_ON, 1))
    {
        network_mode += 2;
    }

    //char formattedMac[18] = "\0";
    //char str_ip[20] = "\0";
    //char str_netmask[20] = "\0";

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "code", "200");
    cJSON_AddStringToObject(root, "msg", "");

    cJSON *data = cJSON_CreateObject();

    cJSON_AddStringToObject(data, "hostname", host_name);
    cJSON_AddStringToObject(data, "ssid", wifi_ssid);
    cJSON_AddStringToObject(data, "wifiPass", wifi_pass);
    cJSON_AddNumberToObject(data, "networkMode", network_mode);

    if(!isStatic){
        cJSON_AddStringToObject(data, "wifi_conf_nettype", "DHCP");
        //cJSON_AddStringToObject(data, "wifi_conf_ipaddress", "");
        //cJSON_AddStringToObject(data, "wifi_conf_netmask", "");
        //cJSON_AddStringToObject(data, "wifi_conf_gateway", "");
        //cJSON_AddStringToObject(data, "wifi_conf_dnsservers", "");
    }else{
        cJSON_AddStringToObject(data, "wifi_conf_nettype", "Static");
    }
    cJSON_AddStringToObject(data, "wifi_conf_ipaddress", staticIp);
    cJSON_AddStringToObject(data, "wifi_conf_netmask", subnetMask);
    cJSON_AddStringToObject(data, "wifi_conf_gateway", gateWay);
    cJSON_AddStringToObject(data, "wifi_conf_dnsservers", dns);
    
    if(!isStatic_ETH){
        cJSON_AddStringToObject(data, "eth_conf_nettype", "DHCP");
        //cJSON_AddStringToObject(data, "eth_conf_ipaddress", "");
        //cJSON_AddStringToObject(data, "eth_conf_netmask", "");
        //cJSON_AddStringToObject(data, "eth_conf_gateway", "");
        //cJSON_AddStringToObject(data, "eth_conf_dnsservers", "");
    }else{
        cJSON_AddStringToObject(data, "eth_conf_nettype", "Static");
    }
    cJSON_AddStringToObject(data, "eth_conf_ipaddress", staticIp_ETH);
    cJSON_AddStringToObject(data, "eth_conf_netmask", subnetMask_ETH);
    cJSON_AddStringToObject(data, "eth_conf_gateway", gateWay_ETH);
    cJSON_AddStringToObject(data, "eth_conf_dnsservers", dns_ETH);

    char *data_str = cJSON_PrintUnformatted(data);
    cJSON_AddStringToObject(root, "data", data_str);

    if(NULL != data_str)
        free((void*)data_str);

    if(NULL != staticIp)
        free(staticIp);
    if(NULL != subnetMask)
        free(subnetMask);
    if(NULL != gateWay)
        free(gateWay);
    if(NULL != dns)
        free(dns);

    if(NULL != staticIp_ETH)
        free(staticIp_ETH);
    if(NULL != subnetMask_ETH)
        free(subnetMask_ETH);
    if(NULL != gateWay_ETH)
        free(gateWay_ETH);
    if(NULL != dns_ETH)
        free(dns_ETH);

    if(NULL != host_name)
        free(host_name);
    if(NULL != wifi_ssid)
        free(wifi_ssid);
    if(NULL != wifi_pass)
        free(wifi_pass);

    cJSON_Delete(data);

    return root;
}

esp_err_t set_network_conf_json(cJSON *network_conf)
{
    esp_err_t ret = ESP_OK;
    cJSON * item;

    if ((item = cJSON_GetObjectItem(network_conf, "hostname")) != NULL){
        nvs_config_set_string(NVS_CONFIG_HOSTNAME, item->valuestring);
    }else{
        //ESP_LOGW(TAG, "Failed to get hostname from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "ssid")) != NULL){
        nvs_config_set_string(NVS_CONFIG_WIFI_SSID, item->valuestring);
    }else{
        //ESP_LOGW(TAG, "Failed to get SSID from network config.");
    }   

    if ((item = cJSON_GetObjectItem(network_conf, "wifiPass")) != NULL){
        nvs_config_set_string(NVS_CONFIG_WIFI_PASS, item->valuestring);
    }else{
        //ESP_LOGW(TAG, "Failed to get wifi_pass from network config.");
    } 

    if ((item = cJSON_GetObjectItem(network_conf, "networkMode")) != NULL){
        nvs_config_set_u16(NVS_CONFIG_WIFI_ON, (item->valueint & 1));
        nvs_config_set_u16(NVS_CONFIG_ETH_ON, (item->valueint & 2));
    }else{
        //ESP_LOGW(TAG, "Failed to get networkMode from network config.");
    } 

    // wifi settings
    if((item = cJSON_GetObjectItem(network_conf, "wifi_conf_nettype")) != NULL){
        if(0 == strcmp(item->valuestring, "DHCP"))
            nvs_config_set_u16(NVS_CONFIG_IS_STATIC_IP, 0);
        else if(0 == strcmp(item->valuestring, "Static"))
            nvs_config_set_u16(NVS_CONFIG_IS_STATIC_IP, 1);
        else
            ESP_LOGW(TAG, "unknow nettype %s", item->valuestring);
    }else{
        //ESP_LOGW(TAG, "Failed to get wifi_conf_nettype from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "wifi_conf_ipaddress")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_STATIC_IP, item->valuestring);
        }else{
            ESP_LOGW(TAG, "staticIP %s is not a valid IP", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get wifi_conf_ipaddress from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "wifi_conf_netmask")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_SUBNET_MASK, item->valuestring);
        }else{
            ESP_LOGW(TAG, "subnetMask %s is not valid.", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get wifi_conf_netmask from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "wifi_conf_gateway")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_GATEWAY, item->valuestring);
        }else{
            ESP_LOGW(TAG, "gateway %s is not valid.", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get wifi_conf_gateway from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "wifi_conf_dnsservers")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_DNS, item->valuestring);
        }else{
            ESP_LOGW(TAG, "DNS %s is not valid.", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get wifi_conf_dnsservers from network config.");
    }

    //eth settings 
    if((item = cJSON_GetObjectItem(network_conf, "eth_conf_nettype")) != NULL){
        if(0 == strcmp(item->valuestring, "DHCP"))
            nvs_config_set_u16(NVS_CONFIG_ETH_IS_STATIC_IP, 0);
        else if(0 == strcmp(item->valuestring, "Static"))
            nvs_config_set_u16(NVS_CONFIG_ETH_IS_STATIC_IP, 1);
        else
            ESP_LOGW(TAG, "eth unknow nettype %s", item->valuestring);
    }else{
        //ESP_LOGW(TAG, "Failed to get eth_conf_nettype from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "eth_conf_ipaddress")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_ETH_STATIC_IP, item->valuestring);
        }else{
            ESP_LOGW(TAG, "eth staticIP %s is not a valid IP", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get eth_conf_ipaddress from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "eth_conf_netmask")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_ETH_SUBNET_MASK, item->valuestring);
        }else{
            ESP_LOGW(TAG, "subnetMask %s is not valid.", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get eth_conf_netmask from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "eth_conf_gateway")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_ETH_GATEWAY, item->valuestring);
        }else{
            ESP_LOGW(TAG, "eth gateway %s is not valid.", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get eth_conf_gateway from network config.");
    }

    if ((item = cJSON_GetObjectItem(network_conf, "eth_conf_dnsservers")) != NULL){
        if(is_valid_ip(item->valuestring) || 0 == strlen(item->valuestring)){
            nvs_config_set_string(NVS_CONFIG_ETH_DNS, item->valuestring);
        }else{
            ESP_LOGW(TAG, "eth DNS %s is not valid.", item->valuestring);
        }
    }else{
        //ESP_LOGW(TAG, "Failed to get eth_conf_dnsservers from network config.");
    }

    return ret;
}
