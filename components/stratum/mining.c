#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <esp_log.h>
#include <esp_system.h>

#include "scrypt.h"
#include "mining.h"
#include "utils.h"
#include "mbedtls/sha256.h"

#define TAG "mining"

void free_bm_job(bm_job *job)
{
    free(job->jobid);
    free(job->extranonce2);
    free(job);
}

char *construct_coinbase_tx(const char *coinbase_1, const char *coinbase_2,
                            const char *extranonce, const char *extranonce_2)
{
    int coinbase_tx_len = strlen(coinbase_1) + strlen(coinbase_2) + strlen(extranonce) + strlen(extranonce_2) + 1;

    char *coinbase_tx = malloc(coinbase_tx_len);
    strcpy(coinbase_tx, coinbase_1);
    strcat(coinbase_tx, extranonce);
    strcat(coinbase_tx, extranonce_2);
    strcat(coinbase_tx, coinbase_2);
    coinbase_tx[coinbase_tx_len - 1] = '\0';

    return coinbase_tx;
}

char *calculate_merkle_root_hash(const char *coinbase_tx, const uint8_t merkle_branches[][32], const int num_merkle_branches)
{
    size_t coinbase_tx_bin_len = strlen(coinbase_tx) / 2;
    uint8_t *coinbase_tx_bin = malloc(coinbase_tx_bin_len);
    if(NULL == coinbase_tx_bin)
        ESP_LOGW(TAG, "coinbase_tx_bin fail to malloc.");
    hex2bin(coinbase_tx, coinbase_tx_bin, coinbase_tx_bin_len);

    uint8_t both_merkles[64];
    uint8_t *new_root = double_sha256_bin(coinbase_tx_bin, coinbase_tx_bin_len);
    if(NULL != coinbase_tx_bin)
        free(coinbase_tx_bin);
    memcpy(both_merkles, new_root, 32);
    if(NULL != new_root)
        free(new_root);

    for (int i = 0; i < num_merkle_branches; i++)
    {
        memcpy(both_merkles + 32, merkle_branches[i], 32);
        uint8_t *new_root = double_sha256_bin(both_merkles, 64);
        memcpy(both_merkles, new_root, 32);
        free(new_root);
    }

    char *merkle_root_hash = malloc(65);
    bin2hex(both_merkles, 32, merkle_root_hash, 65);
    return merkle_root_hash;
}

// take a mining_notify struct with ascii hex strings and convert it to a bm_job struct
bm_job construct_bm_job(mining_notify *params, const char *merkle_root, const uint32_t version_mask, const uint32_t difficulty)
{
    bm_job new_job;

    new_job.version = params->version;
    new_job.starting_nonce = 0;
    new_job.target = params->target;
    new_job.ntime = params->ntime;
    new_job.pool_diff = difficulty;
 
    hex2bin(merkle_root, new_job.merkle_root, 32);
     // hex2bin(merkle_root, new_job.merkle_root_be, 32);
    swap_endian_words(merkle_root, new_job.merkle_root_be);
    reverse_bytes(new_job.merkle_root_be, 32); 

    swap_endian_words(params->prev_block_hash, new_job.prev_block_hash);
    hex2bin(params->prev_block_hash, new_job.prev_block_hash_be, 32);
    reverse_bytes(new_job.prev_block_hash_be, 32);
 
    ////make the midstate hash
    uint8_t midstate_data[64];

    // copy 68 bytes header data into midstate (and deal with endianess)
    memcpy(midstate_data, &new_job.version, 4);             // copy version
    memcpy(midstate_data + 4, new_job.prev_block_hash, 32); // copy prev_block_hash
    memcpy(midstate_data + 36, new_job.merkle_root, 28);    // copy merkle_root

    midstate_sha256_bin(midstate_data, 64, new_job.midstate); // make the midstate hash
    reverse_bytes(new_job.midstate, 32);                      // reverse the midstate bytes for the BM job packet

    if (version_mask != 0)
    {
        uint32_t rolled_version = increment_bitmask(new_job.version, version_mask);
        memcpy(midstate_data, &rolled_version, 4);
        midstate_sha256_bin(midstate_data, 64, new_job.midstate1);
        reverse_bytes(new_job.midstate1, 32);

        rolled_version = increment_bitmask(rolled_version, version_mask);
        memcpy(midstate_data, &rolled_version, 4);
        midstate_sha256_bin(midstate_data, 64, new_job.midstate2);
        reverse_bytes(new_job.midstate2, 32);

        rolled_version = increment_bitmask(rolled_version, version_mask);
        memcpy(midstate_data, &rolled_version, 4);
        midstate_sha256_bin(midstate_data, 64, new_job.midstate3);
        reverse_bytes(new_job.midstate3, 32);
        new_job.num_midstates = 4;
    }
    else
    {
        new_job.num_midstates = 1;
    }

    return new_job;
}

// take a mining_notify struct with ascii hex strings and convert it to a bm_job struct
bm_job construct_ltc_job(mining_notify *params, const char *merkle_root)
{
    bm_job new_job;

    new_job.version = params->version;
    new_job.starting_nonce = 0;
    new_job.target = params->target;
    new_job.ntime = params->ntime;
    new_job.pool_diff = params->difficulty;

    hex2bin(merkle_root, new_job.merkle_root, 32);
    // hex2bin(merkle_root, new_job.merkle_root_be, 32);
    swap_endian_words(merkle_root, new_job.merkle_root_be);
    //reverse_bytes(new_job.merkle_root_be, 32); reverse in LT0051_send_work

    swap_endian_words(params->prev_block_hash, new_job.prev_block_hash);
    hex2bin(params->prev_block_hash, new_job.prev_block_hash_be, 32);
    //reverse_bytes(new_job.prev_block_hash_be, 32); reverse in LT0051_send_work

    return new_job;
}

char *extranonce_2_generate(uint32_t extranonce_2, uint32_t length)
{
    char *extranonce_2_str = malloc(length * 2 + 1);
    memset(extranonce_2_str, '0', length * 2);
    extranonce_2_str[length * 2] = '\0';
    bin2hex((uint8_t *)&extranonce_2, length, extranonce_2_str, length * 2 + 1);
    if (length > 4)
    {
        extranonce_2_str[8] = '0';
    }
    return extranonce_2_str;
}

///////cgminer nonce testing btc
/* truediffone == 0x00000000FFFF0000000000000000000000000000000000000000000000000000
static const double truediffone = 26959535291011309493156476344723991336010898738574164086137773096960.0;
 */
/*
cgminer nonce testing ltc
truediffone == 0x0000FFFF00000000000000000000000000000000000000000000000000000000
static const double truediffone = 1766820104831717178943502833727831496196810259731196417549125097682370560.0;
*/


static const double truediffone = 26959535291011309493156476344723991336010898738574164086137773096960.0;

/* testing a nonce and return the diff - 0 means invalid */
double test_nonce_value(const bm_job *job, const uint32_t nonce, const uint32_t rolled_version)
{
    double d64, s64, ds;
    unsigned char header[80];

    // // TODO: use the midstate hash instead of hashing the whole header
    // uint32_t rolled_version = job->version;
    // for (int i = 0; i < midstate_index; i++) {
    //     rolled_version = increment_bitmask(rolled_version, job->version_mask);
    // }

    // copy data from job to header
    memcpy(header, &rolled_version, 4);
    memcpy(header + 4, job->prev_block_hash, 32);
    memcpy(header + 36, job->merkle_root, 32);
    memcpy(header + 68, &job->ntime, 4);
    memcpy(header + 72, &job->target, 4);
    memcpy(header + 76, &nonce, 4);

    unsigned char hash_buffer[32];
    unsigned char hash_result[32];

    // double hash the header
    mbedtls_sha256(header, 80, hash_buffer, 0);
    mbedtls_sha256(hash_buffer, 32, hash_result, 0);

    /*
    char dump_buffer[200];
    bin2hex(header, 80, dump_buffer, 161);
    ESP_LOGD(TAG, "test_nonce_value input %s.", dump_buffer);
    bin2hex(hash_buffer, 32, dump_buffer, 65);
    ESP_LOGD(TAG, "hash_buffer 1 %s.", dump_buffer);
    bin2hex(hash_result, 32, dump_buffer, 65);
    ESP_LOGD(TAG, "hash_result 2 %s.", dump_buffer);
    */

    d64 = truediffone;
    s64 = le256todouble(hash_result);
    ds = d64 / s64;

    return ds;
}

#define N 1024
//static uint8_t scratchpad[128*N + 63] __attribute__((aligned(4)));
uint8_t *scratchpad[2] = {NULL, NULL};

/* testing a nonce and return the diff - 0 means invalid */
double test_ltc_nonce_value(
    const bm_job *job, const uint32_t nonce,
    uint8_t chip_id, uint8_t core_id, uint32_t chain_num, uint8_t job_id 
)
{
    double d64, s64, ds;
    unsigned char header[80];
    unsigned char hash_result[32];
    
    // copy data from job to header
    memcpy(header, &job->version, 4);
    memcpy(header + 4, job->prev_block_hash, 32);
    memcpy(header + 36, job->merkle_root, 32);
    memcpy(header + 68, &job->ntime, 4);
    memcpy(header + 72, &job->target, 4);
    memcpy(header + 76, &nonce, 4);

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 表示使用 SHA256，而不是 SHA224
    mbedtls_sha256_update(&ctx, (const unsigned char *)header, 64);

    if(NULL == scratchpad[chain_num]){
        scratchpad[chain_num] = (uint8_t *)malloc(128*N + 63);
        ESP_ERROR_CHECK(scratchpad[chain_num] == NULL);        
    }
    scrypt_1024_1_1_256((const uint32_t *)header, (uint32_t*)hash_result, ctx.state, scratchpad[chain_num], N);
    /*revert has result*/
    //reverse_bytes(hash_result, 32);

    mbedtls_sha256_free(&ctx);
    d64 = truediffone;
    s64 = le256todouble(hash_result);
    ds = d64 / s64;

   if(ds < 1 && false){
        char dump_buffer[65];
        bin2hex(hash_result, 32, dump_buffer, 65);
        ESP_LOGI(TAG, "hw err(%"PRIu32", %"PRIu8", %"PRIu8", %"PRIu8"): hash result is %s, diff is %f", 
                chain_num, job_id, chip_id, core_id, dump_buffer, ds);
        prettyHex(header, 80);
   }
   
    return ds;
}

uint32_t increment_bitmask(const uint32_t value, const uint32_t mask)
{
    // if mask is zero, just return the original value
    if (mask == 0)
        return value;

    uint32_t carry = (value & mask) + (mask & -mask);      // increment the least significant bit of the mask
    uint32_t overflow = carry & ~mask;                     // find overflowed bits that are not in the mask
    uint32_t new_value = (value & ~mask) | (carry & mask); // set bits according to the mask

    // Handle carry propagation
    if (overflow > 0)
    {
        uint32_t carry_mask = (overflow << 1);                // shift left to get the mask where carry should be propagated
        new_value = increment_bitmask(new_value, carry_mask); // recursively handle carry propagation
    }

    return new_value;
}
            
