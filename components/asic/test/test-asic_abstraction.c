#include "unity.h"
#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "esp_log.h"

#include "asic_abstration.h"
#include "common.h"
//#include "util.h"

static const char *TAG = "test_asic_absraction";

extern size_t bin2hex(const uint8_t *buf, size_t buflen, char *hex, size_t hexlen);

bool compare_buf(uint8_t *buf, size_t buf_len, char *p_str)
{
    bool b_ret = true;
    char *p_tmp_str = (char *)malloc(buf_len*2+1);
    int ret = 0;
#if 0
    uint8_t *tmp_buf = (uint8_t *)malloc(buf_len);

    if(12 == buf_len){
        flip12(tmp_buf, buf);
    }else if(16 == buf_len){
        flip16(tmp_buf, buf);
    }else if(buf_len > 90){
        flip96(tmp_buf, buf);
    }
    bin2hex(tmp_buf, buf_len, p_tmp_str, 2*buf_len);
#else
    bin2hex(buf, buf_len, p_tmp_str, 2*buf_len);
#endif

    ret = strncmp(p_tmp_str, p_str, 2*buf_len);
    if(0 == ret){
        b_ret = true;
    }else{
        ESP_LOGW(TAG, "bin:\t\t %s", p_tmp_str);
        ESP_LOGW(TAG, "p_str:\t\t %s", p_str);
        b_ret = false;
    }

    /*if(NULL != tmp_buf)
        free(tmp_buf);*/
    if(NULL != p_tmp_str)
        free(p_tmp_str);

    return b_ret;
}

TEST_CASE("check pack_ms_cmd_getreg", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_CMD_LEN] = {0};
    char *str_top_cmd_get_reg = "abcd0008830000000140081d";
    pack_ms_cmd_getreg(cmd_buf, 1, 0x4, 0x3);

    TEST_ASSERT(compare_buf(cmd_buf, MS_CMD_LEN, str_top_cmd_get_reg));
}

TEST_CASE("check pack_ms_cmd_setreg", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_CMD_LEN] = {0};
    char *str_top_cmd_set_reg = "abcd0008820010810047001e";
    pack_ms_cmd_setreg(cmd_buf, 1, 0x2, 0x2, 0x1234);

    TEST_ASSERT(compare_buf(cmd_buf, MS_CMD_LEN, str_top_cmd_set_reg));
}

TEST_CASE("check pack_ms_cmd_inactive", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_CMD_LEN] = {0};
    char *str_chain_inactive = "abcd00080100000000000008";
    pack_ms_cmd_inactive(cmd_buf);

    TEST_ASSERT(compare_buf(cmd_buf, MS_CMD_LEN, str_chain_inactive));
}

TEST_CASE("check pack_ms_cmd_setaddr", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_CMD_LEN] = {0};
    char *str_set_address = "abcd00088000000000020009";
    pack_ms_cmd_setaddr(cmd_buf, 1, 0x2);

    TEST_ASSERT(compare_buf(cmd_buf, MS_CMD_LEN, str_set_address));
}

TEST_CASE("check pack_ms_job_setCoreReg", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_CORE_SET_CMD_LEN] = {0};
    char *str_set_core_reg = "abcd01000b02021000c25dbb13009ed3";
    pack_ms_job_setCoreReg(cmd_buf, 0, 0x2, 0, 0x10, 0x2, 0xffff0000);

    TEST_ASSERT(compare_buf(cmd_buf, MS_CORE_SET_CMD_LEN, str_set_core_reg));
}

TEST_CASE("check pack_ms_job_getCoreReg", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_CORE_GET_CMD_LEN] = {0};
    char *str_get_core_reg = "abcd0100070302100002d317";
    pack_ms_job_getCoreReg(cmd_buf, 0, 0x2, 0, 0x10, 0x2);

    TEST_ASSERT(compare_buf(cmd_buf, MS_CORE_GET_CMD_LEN, str_get_core_reg));
}

TEST_CASE("check pack_ms_job_hashJob1", "[test_asic_abstraction]")
{
    uint8_t cmd_buf[MS_HASHJOB1_LEN+3] = {0};
    uint8_t data[76] = {0};
    char *str_hash_job_1 = "abcd01005801021000012000004000000000002000002000000000000000000000000000000108000c0000000600100000001000000000000400000000000000000400020000000400004000000000000120000000000000000000fd3a";
    //char *str_hash_job_1 = "abcd01005801021000007f12345678000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003bd1";
    pack_ms_job_hashJob1(cmd_buf, 0, 2, 0, 0x10, 0x7f, data, 0x78563412);

    TEST_ASSERT(compare_buf(cmd_buf, MS_HASHJOB1_LEN, str_hash_job_1));
}

TEST_CASE("test_crc5", "test_asic_abstraction")
{
    //uint8_t nonce_response[MS_RESP_LEN] = {0xAB, 0xCD, 0x00, 0x06, 0x00, 0x4D, 0xC7, 0x03, 0x00, 0x06,  0x0D};
    /*abcd002c205d04491e2c15*/
    uint8_t nonce_response[MS_RESP_LEN] = {0xab, 0xcd, 0x00, 0x4f, 0x96, 0xe5, 0x00, 0x66, 0x96, 0x4f, 0x1f};
    TEST_ASSERT_EQUAL_UINT8(CRC5(nonce_response + 2, 64), nonce_response[MS_RESP_LEN - 1]);
}


