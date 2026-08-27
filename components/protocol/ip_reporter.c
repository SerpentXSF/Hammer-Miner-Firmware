#include "ip_reporter.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"

int udp_sock = -1;
struct sockaddr_in addrto;
struct sockaddr_in from;
char mac_tmp[30] = {0};

static const char *TAG = "ip_reporter";

int ip_udp_init()
{
  if(-1 != udp_sock)
    return 1;
  
  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("socket error");
		return -1;
	}

	const int opt = 1;
	//set socker broadcast type
	int nb = 0;
	nb = setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
	if(nb == -1)
	{
	 printf("set socket error...");
	 return -1;
	}
	bzero(&addrto, sizeof(struct sockaddr_in));
	addrto.sin_family=AF_INET;
	addrto.sin_addr.s_addr=htonl(INADDR_BROADCAST);
	addrto.sin_port=htons(14235);

	bzero(&from, sizeof(struct sockaddr_in));
	from.sin_family = AF_INET;
	from.sin_addr.s_addr = htonl(INADDR_ANY);
	from.sin_port = htons(14236);
	if(bind(udp_sock,(struct sockaddr *)&(from), sizeof(struct sockaddr_in)) == -1)
	{
		printf("bind error...");
		return 0;
	}

    return 1;
}

int get_ip(char *ipaddr)
{
    // 获取默认WiFi STA接口的netif结构体
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to get WIFI_STA_DEF");
        return 0;
    }

    // Retrieve IP info
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGD(TAG, "Subnet Mask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGD(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    } else {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
    }

    // 打印IP地址
    if(NULL != ipaddr)
        sprintf(ipaddr, IPSTR, IP2STR(&ip_info.ip));

    return 1;
}

int get_eth_ip(char *ipaddr)
{
    // 1. 获取默认ETH网络接口对象（系统级接口，无需手动保存句柄）
    esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    
    if (eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed to get ETH_DEF");
        return 0;
    }

    // 2. 直接获取IP信息
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(eth_netif, &ip_info);
    if (ret == ESP_OK) {
        char ip_str[16];
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "ETH IP: %s", ip_str);
        if(ipaddr)
        {
            sprintf(ipaddr, IPSTR, IP2STR(&ip_info.ip));
        }
        return 1;
    } else {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
        return 0;
    }
}

esp_err_t get_ip_netmask_gw(char *str_ip, char *str_netmask, char *str_gw)
{
    if(NULL == str_ip || NULL == str_netmask || NULL == str_gw){
        ESP_LOGE(TAG, "ip %p, netmask %p, gateway %p", str_ip, str_netmask, str_gw);
        return ESP_FAIL;
    }

    // 获取默认WiFi STA接口的netif结构体
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to get WIFI_STA_DEF");
        return ESP_FAIL;
    }

    // Retrieve IP info
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Subnet Mask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    } else {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
    }

    sprintf(str_ip, IPSTR, IP2STR(&ip_info.ip));
    sprintf(str_netmask, IPSTR, IP2STR(&ip_info.netmask));
    sprintf(str_gw, IPSTR, IP2STR(&ip_info.gw));

    return ret;
}

esp_err_t get_mac(char *mac)
{
    uint8_t mac_addr[6]; 

    // 1. get mac address of WiFi Station Mode.
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac_addr);

    if(ret == ESP_OK){
        ESP_LOGD(TAG, "WiFi Station MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
    }else{
        ESP_LOGE(TAG, "Fail to get mac: %s", esp_err_to_name(ret));
    }

    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;    
}

esp_err_t get_hostname(char *hostname)
{
    const char *tmp_hostname = NULL;

    esp_err_t ret = esp_netif_get_hostname(esp_netif_get_default_netif(), &tmp_hostname);
    strcpy(hostname, tmp_hostname);

    return ret;
}

esp_err_t get_dns_server(char* dns_server)
{
    esp_netif_t *netif = esp_netif_get_default_netif();
    esp_err_t ret = ESP_FAIL;

    if (!netif) {
        ESP_LOGE(TAG, "No default network interface");
        return ESP_FAIL;
    }
    
    // 获取主DNS服务器
    esp_netif_dns_info_t dns_info;
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        char dns_str[16];
        esp_ip4addr_ntoa(&dns_info.ip.u_addr.ip4, dns_str, sizeof(dns_str));
        ESP_LOGI(TAG, "Primary DNS: %s", dns_str);

        strcpy(dns_server, dns_str);
        ret = ESP_OK;    
    }

    return ret;
}


bool wait_for_mac_response(int udp_sock, struct sockaddr *addrto, socklen_t addrto_len) {
    char recv_buffer[64]; // 假设接收缓冲区足够大
    
    // 接收数据
    int ret = recvfrom(udp_sock, recv_buffer, sizeof(recv_buffer) - 1, 0, addrto, &addrto_len);
    if (ret > 0) {
        recv_buffer[ret] = '\0'; // 确保字符串以空字符结尾
        ESP_LOGI(TAG, "Received MAC: %s", recv_buffer);
        return true; // 成功接收到数据
    } else if (ret == 0) {
        ESP_LOGI(TAG, "Connection closed by peer");
        return false;
    } else {
        ESP_LOGI(TAG, "Receive timeout or error");
        return false; // 超时或错误
    }
}

bool send_ipmac()
{
    char sendmsg[100];
    char mac_data[30];
    char ipaddr[40];
    bool ret = false;
    struct timeval timeout;

    // 设置接收超时为 1 秒
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG, "Failed to set socket timeout");
        return false;
    }

    memset(mac_data,0,sizeof(char)*30);
    memset(ipaddr,0,sizeof(char)*40);
    memset(sendmsg,0,sizeof(char)*100);

    if(!get_ip(ipaddr))
    {
        ESP_LOGI(TAG, "get IP error");
    }

    get_mac(mac_data);
    snprintf(mac_tmp, sizeof(mac_tmp), "%s", mac_data);
    strcpy(sendmsg,ipaddr);
    strcat(sendmsg,",");
    strcat(sendmsg,mac_data);
    ESP_LOGI(TAG, "send ipmac: %s",sendmsg);
    int ret_len = sendto(udp_sock, sendmsg, strlen(sendmsg), 0, (struct sockaddr*)&addrto, sizeof(addrto));
    if(ret_len<0)
    {
        ESP_LOGI(TAG, "send error....");
    }

    //wait response
    // 等待对端返回 MAC 地址，超时 1 秒
    if (wait_for_mac_response(udp_sock, (struct sockaddr*)&addrto, sizeof(addrto))) {
        ESP_LOGI(TAG, "MAC response received successfully");
        sendto(udp_sock, "OK", strlen("OK"), 0, (struct sockaddr*)&addrto, sizeof(addrto));
        ret = true;
    } else {
        ESP_LOGI(TAG, "Failed to receive MAC response within 1 seconds");
    }

    return ret;
}

esp_err_t ip_udp_close()
{
    if(-1 != udp_sock){
        close(udp_sock);
        udp_sock = -1;
        return ESP_OK;
    }else{
        return ESP_FAIL;
    }

}

esp_err_t send_ip_reporter()
{
    int ret = ip_udp_init();

    if(ret < 0)
        return ESP_FAIL;

    for(int i = 0; i < 5; i++){
        if(!send_ipmac()){
            vTaskDelay(1000/portTICK_PERIOD_MS);
        }else{
            return ESP_FAIL;
        }
    }

    return ret;
}


/*
int send_ack(char *ack)
{
    int ret=sendto(udp_sock, ack, (size_t)10, 0, (struct sockaddr*)&addrto, sizeof(addrto));
    
    if(ret<0)
    {
        DEBUG_printf("send error....");
    }
    else
    {
        DEBUG_printf("send ack OK!\n ");
    }
    return ret;
}
*/