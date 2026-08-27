#include "eeprom.h"
#include <string.h>

#include "esp_log.h"
#include "esp_check.h"

#include "common.h"
#include "miner.h"
#include "hal_i2c.h"
#include "gpio_input_output.h"
#include "crc.h"

static const char *TAG = "eeprom";

const uint8_t i2c_slave_addr_eprom[MAX_CHAIN_NUM] = {0x50, 0x50};
uint32_t const x_key[4] = {0x656c6970, 0x6d696e72, 0x616e6764, 0x75616e68};

static i2c_master_dev_handle_t eeprom_dev_handles[MAX_CHAIN_NUM];
eprom_data eeprom_tmp_data[MAX_CHAIN_NUM];
bool eeprom_tmp_data_read[MAX_CHAIN_NUM] = {false, false};

uint8_t eprom_version=0;
uint8_t eprom_algorithm=0;
uint8_t eprom_chip_bin=0x01;
uint8_t eprom_pcb_version = 0x01;//pcb_version: 1
uint32_t eprom_sn=0;
uint8_t eprom_sn_string[32];

uint8_t factory_bin_class = 0;

#define DELTA 0x9e3779b9  
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))  

void x_encode(uint32_t *v, int n, uint32_t const key[4])
{  
    uint32_t y, z, sum;  
    unsigned p, rounds, e;

    if (n <= 1)
    {
        ESP_LOGD(TAG, "X encode failes, N <= 1 found.");
        return;
    }

    rounds = 6 + 52 / n;
    sum = 0;
    z = v[n - 1];
    do
    {
        sum += DELTA;
        e = (sum >> 2) & 3;
        for (p = 0; p < n - 1; p++)
        {
            y = v[p + 1];
            z = v[p] += MX;
        }
        y = v[0];
        z = v[n - 1] += MX;
    } while (--rounds);
}

void x_decode(uint32_t *v, int n, uint32_t const key[4])
{
    uint32_t y, z, sum;
    unsigned p, rounds, e;

    if (n <= 1)
    {
        ESP_LOGD(TAG, "X decode failes, N <= 1 found.");
        return;
    }

    rounds = 6 + 52 / n;
    sum = rounds * DELTA;
    y = v[0];
    do
    {
        e = (sum >> 2) & 3;
        for (p = n - 1; p > 0; p--)
        {
            z = v[p - 1];
            y = v[p] -= MX;
        }
        z = v[n - 1];
        y = v[0] -= MX;
        sum -= DELTA;
    } while (--rounds);

    return;
}

/*eeprom_index = chain_index.*/
esp_err_t eeprom_init(uint8_t i2c_master_index, uint8_t slave_addr, uint8_t eeprom_index, const char* tag)
{
    memset(eprom_sn_string, 0, 32);

    if(0 == eeprom_index){
        gpio_output_init(EEPROM_0_WRITE_PROTECT_IO);
    }else if(1 == eeprom_index){
        gpio_output_init(EEPROM_1_WRITE_PROTECT_IO);
    }
    return volc_i2c_add_device(i2c_master_index, slave_addr, eeprom_dev_handles+eeprom_index, tag);
}

static void eeprom_write_protect_enable(uint8_t eeprom_index)
{
    if(0 == eeprom_index)
        gpio_output(EEPROM_0_WRITE_PROTECT_IO, 1);
    else if(1 == eeprom_index)
        gpio_output(EEPROM_1_WRITE_PROTECT_IO, 1);
}

static void eeprom_write_protect_disable(uint8_t eeprom_index)
{
    if(0 == eeprom_index)
        gpio_output(EEPROM_0_WRITE_PROTECT_IO, 0);
    else if(1 == eeprom_index)
        gpio_output(EEPROM_1_WRITE_PROTECT_IO, 0);
}

esp_err_t eeprom_page_write(uint8_t eeprom_index, uint8_t* write_buf)
{
    uint8_t merge[66] = {0, 0};
    esp_err_t ret = ESP_FAIL;

    eeprom_write_protect_disable(eeprom_index);
    merge[0] = merge[1] = 0;
    memcpy(merge+2, write_buf, 64);
    ret = volc_i2c_write(eeprom_dev_handles[eeprom_index], merge, 66);
    eeprom_write_protect_enable(eeprom_index);

    return ret;
}

esp_err_t eeprom_page_read(uint8_t eeprom_index, uint8_t* read_buf)
{
    uint8_t data_addr[2] = {0, 0};

    return volc_i2c_write_read(
        eeprom_dev_handles[eeprom_index], data_addr, 2, read_buf, 64
    );
}

int crc_check(eprom_data* data)
{
	return data->crc==crc5_bits((unsigned char *) data, 63);
}

/*update the local eeprom data.*/
esp_err_t eeprom_read_data(uint8_t eeprom_index)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t read_buf[64];

    memset(read_buf, 0xff, sizeof(read_buf));
    ret = eeprom_page_read(eeprom_index, read_buf);
    if(ESP_OK != ret){
        ESP_LOGW(TAG, "eeprom_page_read failed.");
        goto exit;
    }else{
        ESP_LOGI(TAG, "eeprom_page read 64 bytes.");
    }

    x_decode((uint32_t *)read_buf, 64/4, x_key);
    /*ESP_LOG_BUFFER_HEX(TAG, read_buf, 64);*/

    if(crc_check((eprom_data *)read_buf)){
        memcpy(eeprom_tmp_data + eeprom_index, read_buf, 64);
        if(false)
            ESP_LOGI(TAG, "version:%"PRIx8",algorithm:%"PRIx8",chip_bin:%"PRIx8",pcb_version:%"PRIx8",sn:%"PRIx32",voltage:%"PRIu32",freq:%"PRIu32"",
                eeprom_tmp_data[eeprom_index].version, eeprom_tmp_data[eeprom_index].algorithm,
                eeprom_tmp_data[eeprom_index].chip_bin, eeprom_tmp_data[eeprom_index].pcb_version, eeprom_tmp_data[eeprom_index].sn,
                eeprom_tmp_data[eeprom_index].voltage_normal_init, eeprom_tmp_data[eeprom_index].freq_normal
        );
        ret = ESP_OK;
    }else{
        ESP_LOGW(TAG, "read eprom crc error!!!!!");
        ret = ESP_FAIL;
    }

exit:
    return ret;
}

int32_t get_voltage_from_eeprom(uint8_t eeprom_index)
{
    int32_t ret = -1;

    if(!eeprom_tmp_data_read[eeprom_index]){
        if(ESP_OK == eeprom_read_data(eeprom_index)){
            eeprom_tmp_data_read[eeprom_index] = true;
        }else{
            ESP_LOGW(TAG, "Failed to read EEPROM.");
            return ret;
        }
    }

    ret = eeprom_tmp_data[eeprom_index].voltage_normal_init;
    return ret;
}

int32_t get_freq_from_eeprom(uint8_t eeprom_index)
{
    int32_t ret = -1;

    if(!eeprom_tmp_data_read[eeprom_index]){
        if(ESP_OK == eeprom_read_data(eeprom_index)){
            eeprom_tmp_data_read[eeprom_index] = true;
        }else{
            ESP_LOGW(TAG, "Failed to read EEPROM.");
            return ret;
        }
    }

    ret = eeprom_tmp_data[eeprom_index].freq_normal;
    return ret;
}

int32_t get_bin_from_eeprom(uint8_t eeprom_index)
{
    int32_t ret = -1;

    if(!eeprom_tmp_data_read[eeprom_index]){
        if(ESP_OK == eeprom_read_data(eeprom_index)){
            eeprom_tmp_data_read[eeprom_index] = true;
        }else{
            ESP_LOGW(TAG, "Failed to read EEPROM.");
            return ret;
        }
    }

    ret = eeprom_tmp_data[eeprom_index].chip_bin;

    return ret;
}

int32_t get_sn_from_eeprom(uint8_t eeprom_index, uint8_t* sn)
{
    int32_t ret = -1;

    if(!eeprom_tmp_data_read[eeprom_index]){
        if(ESP_OK == eeprom_read_data(eeprom_index)){
            eeprom_tmp_data_read[eeprom_index] = true;
        }else{
            ESP_LOGW(TAG, "Failed to read EEPROM.");
            return ret;
        }
    }

    ret = 0;
    memcpy(sn, (void *)(eeprom_tmp_data[eeprom_index].rsv0), 24);

    return ret;
}

int32_t update_sn_to_eeprom(uint8_t eeprom_index, uint8_t* sn)
{
    int32_t ret = -1;
    uint8_t write_buf[64];

    if(!eeprom_tmp_data_read[eeprom_index]){
        if(ESP_OK == eeprom_read_data(eeprom_index)){
            eeprom_tmp_data_read[eeprom_index] = true;
        }else{
            ESP_LOGW(TAG, "Failed to read EEPROM.");
            return ret;
        }
    }

    /*update the sn*/
    memcpy((void *)(eeprom_tmp_data[eeprom_index].rsv0), sn, 24);
    ESP_LOGI(TAG, "before write.");
    ESP_LOG_BUFFER_HEX(TAG, eeprom_tmp_data+eeprom_index, 64);

    memcpy(write_buf, eeprom_tmp_data+eeprom_index, 64);
    write_buf[63] = crc5_bits(write_buf, 63);
    x_encode((uint32_t *)write_buf, 64/4, x_key);
    eeprom_page_write(eeprom_index, write_buf);
    
    return (ret = 0);
}

esp_err_t eeprom_write_data(uint8_t eeprom_index, 
    uint32_t voltage_normal_init, uint32_t voltage_normal_target, uint32_t freq_normal,
	uint32_t voltage_over_init, uint32_t voltage_over_target, uint32_t freq_over)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t buf[64];

    /*write fixed data*/
    eeprom_tmp_data[eeprom_index].version = eprom_version;
    eeprom_tmp_data[eeprom_index].algorithm = eprom_algorithm;
    eeprom_tmp_data[eeprom_index].chip_bin = eprom_chip_bin;
    eeprom_tmp_data[eeprom_index].pcb_version = eprom_pcb_version;
    eeprom_tmp_data[eeprom_index].sn = eprom_sn;
    
    /*board mining param*/
    eeprom_tmp_data[eeprom_index].voltage_normal_init=voltage_normal_init;
    eeprom_tmp_data[eeprom_index].voltage_normal_target=voltage_normal_target;
    eeprom_tmp_data[eeprom_index].freq_normal=freq_normal;
    eeprom_tmp_data[eeprom_index].voltage_over_init=voltage_over_init;
    eeprom_tmp_data[eeprom_index].voltage_over_target=voltage_over_target;
    eeprom_tmp_data[eeprom_index].freq_over=freq_over;

    eeprom_tmp_data[eeprom_index].rsv0[0]=eprom_sn_string[0];
    eeprom_tmp_data[eeprom_index].rsv0[1]=eprom_sn_string[1];
    eeprom_tmp_data[eeprom_index].rsv0[2]=eprom_sn_string[2];
    eeprom_tmp_data[eeprom_index].rsv0[3]=eprom_sn_string[3];

    eeprom_tmp_data[eeprom_index].rsv1[0]=eprom_sn_string[4];
    eeprom_tmp_data[eeprom_index].rsv1[1]=eprom_sn_string[5];
    eeprom_tmp_data[eeprom_index].rsv1[2]=eprom_sn_string[6];
    eeprom_tmp_data[eeprom_index].rsv1[3]=eprom_sn_string[7];

    eeprom_tmp_data[eeprom_index].rsv2[0]=eprom_sn_string[8];
    eeprom_tmp_data[eeprom_index].rsv2[1]=eprom_sn_string[9];
    eeprom_tmp_data[eeprom_index].rsv2[2]=eprom_sn_string[10];
    eeprom_tmp_data[eeprom_index].rsv2[3]=eprom_sn_string[11];

    eeprom_tmp_data[eeprom_index].rsv3[0]=eprom_sn_string[12];
    eeprom_tmp_data[eeprom_index].rsv3[1]=eprom_sn_string[13];
    eeprom_tmp_data[eeprom_index].rsv3[2]=eprom_sn_string[14];
    eeprom_tmp_data[eeprom_index].rsv3[3]=eprom_sn_string[15];

    eeprom_tmp_data[eeprom_index].rsv4[0]=eprom_sn_string[16];
    eeprom_tmp_data[eeprom_index].rsv4[1]=eprom_sn_string[17];
    eeprom_tmp_data[eeprom_index].rsv4[2]=eprom_sn_string[18];
    eeprom_tmp_data[eeprom_index].rsv4[3]=eprom_sn_string[19];

    eeprom_tmp_data[eeprom_index].rsv5[0]=eprom_sn_string[20];
    eeprom_tmp_data[eeprom_index].rsv5[1]=eprom_sn_string[21];
    eeprom_tmp_data[eeprom_index].rsv5[2]=eprom_sn_string[22];
    eeprom_tmp_data[eeprom_index].rsv5[3]=eprom_sn_string[23];

    eeprom_tmp_data[eeprom_index].rsv6[0]=0;
    eeprom_tmp_data[eeprom_index].rsv6[1]=0;
    eeprom_tmp_data[eeprom_index].rsv6[2]=0;
    eeprom_tmp_data[eeprom_index].rsv6[3]=0;

    eeprom_tmp_data[eeprom_index].rsv7_1=0;
    eeprom_tmp_data[eeprom_index].rsv7_2=0;
    eeprom_tmp_data[eeprom_index].rsv7_3=0;
    eeprom_tmp_data[eeprom_index].crc=crc5_bits((unsigned char *)(eeprom_tmp_data+eeprom_index), 63);
    
    ESP_LOG_BUFFER_HEX(TAG, (uint8_t*)(eeprom_tmp_data+eeprom_index), 64);
    ESP_LOGI(TAG,  "Encode.");
    x_encode((uint32_t *)(eeprom_tmp_data+eeprom_index), 64/4, x_key);
    ESP_LOG_BUFFER_HEX(TAG, (uint8_t*)(eeprom_tmp_data+eeprom_index), 64);

	ret = eeprom_page_write(eeprom_index, (uint8_t*)(eeprom_tmp_data+eeprom_index));
    if(ESP_OK != ret){
        ESP_LOGW(TAG, "eprom_write_data: eprom write page data fail.");
        goto exit;        
    }
	volc_delay(100);

	memset(buf, 0xff, 64);
	ret = eeprom_page_read(eeprom_index, buf);
    if(ESP_OK != ret){
        ESP_LOGW(TAG, "eprom_write_data: eprom read page data fail.");
        goto exit;
    }
    ESP_LOG_BUFFER_HEX(TAG, buf, 64);
    ESP_LOGI(TAG, "decode.");
	x_decode((uint32_t *) buf, 64/4, x_key);
    ESP_LOG_BUFFER_HEX(TAG, buf, 64);

	if(crc_check((eprom_data *) buf) && 
		((eprom_data *) buf)->voltage_normal_init==voltage_normal_init &&
		((eprom_data *) buf)->freq_normal==freq_normal){
		ret = ESP_OK;
	}

exit:
    return ret;
}

int testWriteEpromData(int chain){
	eprom_data data;
	uint8_t buf[64+1];

    memset(buf, 0xff, sizeof(buf));

	data.pcb_version=0xA5;
	//write fix data
	data.version=0xA5;
	data.algorithm=0xA5;//encode type
	data.chip_bin=0xA5;
	data.pcb_version=0xA5;;
	data.sn=0x5A5AA5A5;
	/*board mining param*/
	data.voltage_normal_init=0xFAFAFAFA;//version 0
	data.voltage_normal_target=0xFAFAFAFA; //version 0
	data.freq_normal=0xFAFAFAFA;//version 0
	data.voltage_over_init=0xFAFAFAFA; //version 0
	data.voltage_over_target=0xFAFAFAFA;//version 0
	data.freq_over=0xFAFAFAFA; //version 0
	data.rsv0[0]=eprom_sn_string[0];
	data.rsv0[1]=eprom_sn_string[1];
	data.rsv0[2]=eprom_sn_string[2];
	data.rsv0[3]=eprom_sn_string[3];
	
	data.rsv1[0]=eprom_sn_string[4];
	data.rsv1[1]=eprom_sn_string[5];
	data.rsv1[2]=eprom_sn_string[6];
	data.rsv1[3]=eprom_sn_string[7];
	
	data.rsv2[0]=eprom_sn_string[8];
	data.rsv2[1]=eprom_sn_string[9];
	data.rsv2[2]=eprom_sn_string[10];
	data.rsv2[3]=eprom_sn_string[11];

	data.rsv3[0]=eprom_sn_string[12];
	data.rsv3[1]=eprom_sn_string[13];
	data.rsv3[2]=eprom_sn_string[14];
	data.rsv3[3]=eprom_sn_string[15];

	data.rsv4[0]=eprom_sn_string[16];
	data.rsv4[1]=eprom_sn_string[17];
	data.rsv4[2]=eprom_sn_string[18];
	data.rsv4[3]=eprom_sn_string[19];
	
	data.rsv5[0]=eprom_sn_string[20];
	data.rsv5[1]=eprom_sn_string[21];
	data.rsv5[2]=eprom_sn_string[22];
	data.rsv5[3]=eprom_sn_string[23];
	data.rsv6[0]=data.rsv6[1]=data.rsv6[2]=data.rsv6[3]=0;
	data.rsv7_1=0;
	data.rsv7_2=0;
	data.rsv7_3=0;
	data.crc=crc5_bits((unsigned char *) &data, 63);

	x_encode((uint32_t *) &data, 64/4, x_key );

	esp_err_t ret  = eeprom_page_write(chain,(uint8_t *)&data);
    if(ESP_OK == ret)
        ESP_LOGI(TAG, "eeprom_write 64 bytes.");
    else
        ESP_LOGW(TAG, "eeprom write failed.");
    vTaskDelay(pdMS_TO_TICKS(100));

	eeprom_page_read(chain, buf);

	ESP_LOG_BUFFER_HEX(TAG, buf, 64);
    x_decode((uint32_t *) buf, 64/4, x_key);
    ESP_LOG_BUFFER_HEX(TAG, buf, 64);

	if(crc_check((eprom_data *) buf)){
		return 0;
	}else{
        ESP_LOGW(TAG, "read eprom crc error!!!!!");
    }

	return -1;
}

void factory_set_bin_type(uint8_t bin_class)
{
    factory_bin_class = bin_class;
}

uint8_t factory_get_bin_type()
{
    return factory_bin_class;
}