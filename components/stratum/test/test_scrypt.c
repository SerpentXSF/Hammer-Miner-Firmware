#include "unity.h"
#include "scrypt.h"
#include "utils.h"
#include "mbedtls/sha256.h"

#include <limits.h>
#include "esp_log.h"

#define TAG "test-scrypt"

TEST_CASE("test sha256", "[scrypt][not-on-qemu]")
{
    uint8_t data[64] = {0};
    char *str_input = "000000205dd1395b423ec15f281bb3c910a05c9b63998633b4570ca9e5aa50390265b89b9d0dcd99d89d97f1a005998f2188960b6c7d7fadc13ad7b695bbe6223a7a26758f507960a386011adb816679";
    hex2bin(str_input, data, 64);

    mbedtls_sha256_context ctx_tstate;
    mbedtls_sha256_init(&ctx_tstate);
    mbedtls_sha256_starts(&ctx_tstate, 0);
    ctx_tstate.sha_state = 1; //ESP_SHA256_STATE_IN_PROCESS
    ctx_tstate.state[0] = 0x00000001;
    ctx_tstate.state[5] = 0x78563412;

    mbedtls_sha256_update(&ctx_tstate, data, 64);
    ESP_LOGD(TAG, "state is 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32"", 
        ctx_tstate.state[0], ctx_tstate.state[1], ctx_tstate.state[6], ctx_tstate.state[7]
    );
}

/*pattern for the scrypt vs code.*/
TEST_CASE("Test HMAC_SHA256_80_init", "[scrypt][not-on-qemu]")
{
    uint32_t tstate[8] = {0xbbdf79bf, 0x52dd1919, 0xb11134b6, 0xcbe72223, 0xff2e7c1, 0x22bebe42, 0x62a19e6d, 0x92e7eaba};
    uint32_t ostate[8];
    uint8_t header[80];
    char *str_input = "000000205dd1395b423ec15f281bb3c910a05c9b63998633b4570ca9e5aa50390265b89b9d0dcd99d89d97f1a005998f2188960b6c7d7fadc13ad7b695bbe6223a7a26758f507960a386011adb816679";

    hex2bin(str_input, header, 80);
    HMAC_SHA256_80_init((const uint32_t *)header, tstate, ostate);

    ESP_LOGD(TAG, "tstate is 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32"", tstate[0], tstate[1], tstate[2], tstate[3]);
    ESP_LOGD(TAG, "tstate is 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32"", tstate[4], tstate[5], tstate[6], tstate[7]);
    ESP_LOGD(TAG, "ostate is 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32"", ostate[0], ostate[1], ostate[2], ostate[3]);
    ESP_LOGD(TAG, "ostate is 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32"", ostate[4], ostate[5], ostate[6], ostate[7]);
    TEST_ASSERT_EQUAL_UINT32(tstate[0], 0xc57fb3f5);
    TEST_ASSERT_EQUAL_UINT32(tstate[7], 0x24fc4e87);
    TEST_ASSERT_EQUAL_UINT32(ostate[0], 0xd10bf1a0);
    TEST_ASSERT_EQUAL_UINT32(ostate[7], 0x42300698);
}

TEST_CASE("Test PBKDF2_SHA256_80_128", "[scrypt][not-on-qemu]")
{
    uint32_t tstate[8] = {0xc57fb3f5, 0x22cbe85c, 0x73bac5cc, 0xa62125c, 0xc4052b8b, 0x47d20f68, 0x4fde9886, 0x24fc4e87};
    uint32_t ostate[8] = {0xd10bf1a0, 0xb048ab57, 0x36a54f25, 0x4ebd9efe, 0x888113fb, 0x8c77abd4, 0x7fe4b7c6, 0x42300698};
    uint32_t X[32];
    uint8_t header[80];
    char *str_input = "000000205dd1395b423ec15f281bb3c910a05c9b63998633b4570ca9e5aa50390265b89b9d0dcd99d89d97f1a005998f2188960b6c7d7fadc13ad7b695bbe6223a7a26758f507960a386011adb816679";

    hex2bin(str_input, header, 80);
    PBKDF2_SHA256_80_128(tstate, ostate, (const uint32_t*)header, X);

    ESP_LOGD(TAG, "*********X 1********");
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[0], X[1], X[2], X[3], X[4], X[5], X[6], X[7]);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[8], X[9], X[10], X[11], X[12], X[13], X[14], X[15]);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[16], X[17], X[18], X[19], X[20], X[21], X[22], X[23]);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[24], X[25], X[26], X[27], X[28], X[29], X[30], X[31]);

    TEST_ASSERT_EQUAL_UINT32(X[0], 0x548ccf3d);
    TEST_ASSERT_EQUAL_UINT32(X[8], 0xb46dd246);
    TEST_ASSERT_EQUAL_UINT32(X[16], 0x0c18b1c5);
    TEST_ASSERT_EQUAL_UINT32(X[24], 0xfb4cd90d);
}

#define N 1024
static uint8_t scratchpad[128*N + 63] __attribute__((aligned(4)));

TEST_CASE("Test_scrypt_core", "[scrypt][not-on-qemu]")
{
	uint32_t X[32] = {
        0x548ccf3d, 0x96a11c44, 0xcdfa3b2, 0xc46566e7, 0xe443606f, 0xa6e01267, 0xd5bb0a96, 0xd5b4e59,
        0xb46dd246, 0xbf0e7a26, 0xa4acae62, 0x507d7c65, 0x1dd37772, 0x27b024c5, 0x314ba5cf, 0xaa4aee6f,
        0xc18b1c5, 0x2fe6555d, 0x72ebe5c5, 0xa1b19156, 0x10b419d2, 0x30f30e4e, 0xf006b3b7, 0x55b39eee,
        0xfb4cd90d, 0xd667212b, 0x4914ffa1, 0x7ef5b671, 0xd772b3f8, 0x7cfca523, 0x98bc99aa, 0xec983793
    };
	uint32_t *V;
    /*uint32_t N = 1024;
    uint8_t *scratchpad = scrypt_buffer_alloc(N);*/
	
	V = (uint32_t *)(((uintptr_t)(scratchpad) + 63) & ~ (uintptr_t)(63));

    /*swap32 X*/
    flip128bytes(X, X);
    scrypt_core(X, V, N);
    flip128bytes(X, X);

    /*free(scratchpad);*/

    ESP_LOGD(TAG, "*********X 2********");
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[0], X[1], X[2], X[3], X[4], X[5], X[6], X[7]);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[8], X[9], X[10], X[11], X[12], X[13], X[14], X[15]);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[16], X[17], X[18], X[19], X[20], X[21], X[22], X[23]);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        X[24], X[25], X[26], X[27], X[28], X[29], X[30], X[31]);

    TEST_ASSERT_EQUAL_UINT32(X[0], 0xfd608bdc);
    TEST_ASSERT_EQUAL_UINT32(X[8], 0xd691eda5);
    TEST_ASSERT_EQUAL_UINT32(X[16], 0xf3747ada);
    TEST_ASSERT_EQUAL_UINT32(X[24], 0xf50951a8);
}

TEST_CASE("Test PBKDF2_SHA256_128_32", "[scrypt][not-on-qemu]")
{
    uint32_t tstate[8] = {0xc57fb3f5, 0x22cbe85c, 0x73bac5cc, 0xa62125c, 0xc4052b8b, 0x47d20f68, 0x4fde9886, 0x24fc4e87};
    uint32_t ostate[8] = {0xd10bf1a0, 0xb048ab57, 0x36a54f25, 0x4ebd9efe, 0x888113fb, 0x8c77abd4, 0x7fe4b7c6, 0x42300698};
    uint32_t output[8] = {0};

    uint32_t X[32] = {
        0xfd608bdc, 0xbc24339e, 0xf8a463f4, 0x7e1c133a, 0xd504ab03, 0xca019a70, 0x2314ce61, 0x8375cce2,
        0xd691eda5, 0x59dd4953, 0xcd353b58, 0x92588a7a, 0xaf800af7, 0xc02b2262, 0x50d94f34, 0x35918f68,
        0xf3747ada, 0x9d493db6, 0x7a6f9094, 0xa15d2050, 0xff701929, 0x53f1b6a1, 0x4ad11802, 0x8a835193,
        0xf50951a8, 0xe652810d, 0xb99812b, 0x50bb2ccd, 0xb4c1970e, 0xf264f3e7, 0xcc8ee0fc, 0x386c77cf
    };
    
    PBKDF2_SHA256_128_32(tstate, ostate, X, output);
    TEST_ASSERT_EQUAL_UINT32(output[0], 0xaab57dc1);
    TEST_ASSERT_EQUAL_UINT32(output[7], 0x00000000);    
}

TEST_CASE("Test scrypt_1024_1_1_256", "[scrypt][not-on-qemu]")
{
    uint8_t data[80] = {0};
    char *str_input = "000000205dd1395b423ec15f281bb3c910a05c9b63998633b4570ca9e5aa50390265b89b9d0dcd99d89d97f1a005998f2188960b6c7d7fadc13ad7b695bbe6223a7a26758f507960a386011adb816679";
    hex2bin(str_input, data, 80);
    uint32_t tstate[8] = {0xbbdf79bf, 0x52dd1919, 0xb11134b6, 0xcbe72223, 0xff2e7c1, 0x22bebe42, 0x62a19e6d, 0x92e7eaba};
    uint32_t ostate[8];
    uint32_t output[8];
    
    /*uint32_t N = 1024;
    uint8_t *scratchpad = scrypt_buffer_alloc(N);*/

    scrypt_1024_1_1_256((const uint32_t *)data, output, tstate, scratchpad, N);
    ESP_LOGD(TAG, "0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32",", 
        output[0], output[1], output[2], output[3], output[4], output[5], output[6], output[7]);
    /*free(scratchpad);*/
    TEST_ASSERT_EQUAL_UINT32(output[0], 0xc17db5aa);
    TEST_ASSERT_EQUAL_UINT32(output[4], 0xa043411f);
    TEST_ASSERT_EQUAL_UINT32(output[7], 0x00000000);   
}