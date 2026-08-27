#include "unity.h"
#include "mining.h"
#include "utils.h"

#include <limits.h>
#include <malloc.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "test-mining"

TEST_CASE("Check coinbase tx construction", "[mining]")
{
    const char *coinbase_1 = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff20020862062f503253482f04b8864e5008";
    const char *coinbase_2 = "072f736c7573682f000000000100f2052a010000001976a914d23fcdf86f7e756a64a7a9688ef9903327048ed988ac00000000";
    const char *extranonce = "e9695791";
    const char *extranonce_2 = "99999999";
    char *coinbase_tx = construct_coinbase_tx(coinbase_1, coinbase_2, extranonce, extranonce_2);
    TEST_ASSERT_EQUAL_STRING(coinbase_tx, "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff20020862062f503253482f04b8864e5008e969579199999999072f736c7573682f000000000100f2052a010000001976a914d23fcdf86f7e756a64a7a9688ef9903327048ed988ac00000000");
    free(coinbase_tx);
}

// Values calculated from esp-miner/components/stratum/test/verifiers/merklecalc.py
TEST_CASE("Validate merkle root calculation", "[mining]")
{
    const char *coinbase_tx = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff20020862062f503253482f04b8864e5008e969579199999999072f736c7573682f000000000100f2052a010000001976a914d23fcdf86f7e756a64a7a9688ef9903327048ed988ac00000000";
    uint8_t merkles[12][32];
    int num_merkles = 12;

    hex2bin("ae23055e00f0f697cc3640124812d96d4fe8bdfa03484c1c638ce5a1c0e9aa81", merkles[0], 32);
    hex2bin("980fb87cb61021dd7afd314fcb0dabd096f3d56a7377f6f320684652e7410a21", merkles[1], 32);
    hex2bin("a52e9868343c55ce405be8971ff340f562ae9ab6353f07140d01666180e19b52", merkles[2], 32);
    hex2bin("7435bdfa004e603953b2ed39f118803934d9cf17b06d979ceb682f2251bafac2", merkles[3], 32);
    hex2bin("2a91f061a22d27cb8f44eea79938fb241ebeb359891aa907f05ffde7ed44e52e", merkles[4], 32);
    hex2bin("302401f80eb5e958155135e25200bb8ea181ad2d05e804a531c7314d86403cdc", merkles[5], 32);
    hex2bin("318ecb6161eb9b4cfd802bd730e2d36c167ddf102e70aa7b4158e2870dd47392", merkles[6], 32);
    hex2bin("1114332a9858e0cf84b2425bb1e59eaabf91dd102d114aa443d57fc1b3beb0c9", merkles[7], 32);
    hex2bin("f43f38095c810613ed795a44d9fab02ff25269706f454885db9be05cdf9c06e1", merkles[8], 32);
    hex2bin("3e2fc26b27fddc39668b59099cd9635761bb72ed92404204e12bdff08b16fb75", merkles[9], 32);
    hex2bin("463c19427286342120039a83218fa87ce45448e246895abac11fff0036076758", merkles[10], 32);
    hex2bin("03d287f655813e540ddb9c4e7aeb922478662b0f5d8e9d0cbd564b20146bab76", merkles[11], 32);

    char *root_hash = calculate_merkle_root_hash(coinbase_tx, merkles, num_merkles);
    TEST_ASSERT_EQUAL_STRING("adbcbc21e20388422198a55957aedfa0e61be0b8f2b87d7c08510bb9f099a893", root_hash);
    free(root_hash);
}

TEST_CASE("Validate another merkle root calculation", "[mining]")
{
    const char *coinbase_tx = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff2503777d07062f503253482f0405b8c75208f800880e000000000b2f436f696e48756e74722f0000000001603f352a010000001976a914c633315d376c20a973a758f7422d67f7bfed9c5888ac00000000";
    uint8_t merkles[5][32];
    int num_merkles = 5;

    hex2bin("f0dbca1ee1a9f6388d07d97c1ab0de0e41acdf2edac4b95780ba0a1ec14103b3", merkles[0], 32);
    hex2bin("8e43fd2988ac40c5d97702b7e5ccdf5b06d58f0e0d323f74dd5082232c1aedf7", merkles[1], 32);
    hex2bin("1177601320ac928b8c145d771dae78a3901a089fa4aca8def01cbff747355818", merkles[2], 32);
    hex2bin("9f64f3b0d9edddb14be6f71c3ac2e80455916e207ffc003316c6a515452aa7b4", merkles[3], 32);
    hex2bin("2d0b54af60fad4ae59ec02031f661d026f2bb95e2eeb1e6657a35036c017c595", merkles[4], 32);

    char *root_hash = calculate_merkle_root_hash(coinbase_tx, merkles, num_merkles);
    TEST_ASSERT_EQUAL_STRING("5cc58f5e84aafc740d521b92a7bf72f4e56c4cc3ad1c2159f1d094f97ac34eee", root_hash);
    free(root_hash);
}


//for btc
// Values calculated from esp-miner/components/stratum/test/verifiers/bm1397.py
TEST_CASE("Validate bm job construction", "[mining]")
{
    mining_notify notify_message;
    notify_message.prev_block_hash = "bf44fd3513dc7b837d60e5c628b572b448d204a8000007490000000000000000";
    notify_message.version = 0x20000004;
    notify_message.target = 0x1705dd01;
    notify_message.ntime = 0x64658bd8;
    const char *merkle_root = "cd1be82132ef0d12053dcece1fa0247fcfdb61d4dbd3eb32ea9ef9b4c604a846";
    bm_job job = construct_bm_job(&notify_message, merkle_root, 0);

    uint8_t expected_midstate_bin[32];
    hex2bin("91DFEA528A9F73683D0D495DD6DD7415E1CA21CB411759E3E05D7D5FF285314D", expected_midstate_bin, 32);
    // bytes are reversed for the midstate on the bm job command packet
    reverse_bytes(expected_midstate_bin, 32);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_midstate_bin, job.midstate, 32);
}

TEST_CASE("Test extranonce 2 generation", "[mining extranonce2]")
{
    char *first = extranonce_2_generate(0, 4);
    TEST_ASSERT_EQUAL_STRING("00000000", first);
    free(first);

    char *second = extranonce_2_generate(1, 4);
    TEST_ASSERT_EQUAL_STRING("01000000", second);
    free(second);

    char *third = extranonce_2_generate(2, 4);
    TEST_ASSERT_EQUAL_STRING("02000000", third);
    free(third);

    char *fourth = extranonce_2_generate(UINT_MAX - 1, 4);
    TEST_ASSERT_EQUAL_STRING("feffffff", fourth);
    free(fourth);

    char *fifth = extranonce_2_generate(UINT_MAX / 2, 6);
    TEST_ASSERT_EQUAL_STRING("ffffff7f0000", fifth);
    free(fifth);
}

//for ltc
/*
TEST_CASE("Validate_ltc_job_construction", "[mining]")
{
    mining_notify notify_message;
    notify_message.prev_block_hash = "";
    notify_message.version = ;
    notify_message.target = ;
    notify_message.ntime = ;
    const char *merkle_root = "";
    bm_job ltc_job = construct_ltc_job(&notify_message, merkle_root);

}
*/

TEST_CASE("Test ltc_nonce diff checking", "[mining][not-on-qemu]")
{
    mining_notify notify_message;
    notify_message.prev_block_hash = "5b39d15d5fc13e42c9b31b289b5ca01033869963a90c57b43950aae59bb86502";
    notify_message.version = 0x20000000;
    notify_message.target = 0x1a0186a3;
    notify_message.ntime = 0x6079508f;
    const char *merkle_root = "9d0dcd99d89d97f1a005998f2188960b6c7d7fadc13ad7b695bbe6223a7a2675";
    bm_job job = construct_ltc_job(&notify_message, merkle_root);

    ESP_LOGD(TAG, "hello test ltc nonce debug");
    uint32_t nonce = 0x796681db;
    double diff = test_ltc_nonce_value(&job, nonce, 0, 0, 0, 0);
    //TEST_ASSERT_GREATER_OR_EQUAL_UINT32(256*256, (int)diff);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(256, (int)diff);
}

/*
correct pattern:
20 00 00 14 
58 32 BE 09 12 45 6D 74 12 A9 88 F0 08 6A CB 07 91 CD F6 C1 4F 07 CC 8F 7F 95 60 70 C9 6B 74 16 
AE 48 36 FB F6 26 DA C4 D9 EC FE 88 B8 E6 DB 87 70 6B E0 9D 70 F6 E1 EE 2A FB C3 DA B5 AB 75 9B 
68 8E C7 C1 
19 2F 80 7E

[AB CD 00 53 90 26 EB 15 90 53 0A]
["xinmai01.mini-pre-723", "L9DcNZBgI", "4d000000004e", "688ec7c1", "eb269053"]}

[7E 80 2F 19 
C1 C7 8E 68 
9B 75 AB B5 DA C3 FB 2A EE E1 F6 70 9D E0 6B 70 87 DB E6 B8 88 FE EC D9 C4 DA 26 F6 FB 36 48 AE
16 74 6B C9 70 60 95 7F 8F CC 07 4F C1 F6 CD 91 07 CB 6A 08 F0 88 A9 12 74 6D 45 12 09 BE 32 58 
14 00 00 20]
*/
extern void enc32be(void *dst, uint32_t val);

TEST_CASE("Test ltc_nonce diff checking_for_debug_hw", "[mining][not-on-qemu]")
{
    mining_notify notify_message;
    notify_message.version = 0x20000014;
    notify_message.prev_block_hash = "5832be0912456d7412a988f0086acb0791cdf6c14f07cc8f7f956070c96b7416";
    const char *merkle_root = "FB3648AEC4DA26F688FEECD987DBE6B89DE06B70EEE1F670DAC3FB2A9B75ABB5";
    notify_message.ntime = 0x688EC7C1 ;
    notify_message.target = 0x192F807E;

    bm_job job = construct_ltc_job(&notify_message, merkle_root);
    uint8_t job1_data[76] = {0};

    /*construct the hashjob1 data from bm_job.*/
    //memcpy(job1_data, &(next_bm_job->version), 4);
    enc32be(job1_data, job.version);
    memcpy(job1_data + 4, job.prev_block_hash_be, 32);
    memcpy(job1_data + 4 + 32, job.merkle_root_be, 32);
    enc32be(job1_data + 4 + 32 + 32, job.ntime);
    enc32be(job1_data + 4 + 32 + 32 + 4, job.target);
    reverse_bytes(job1_data, 76);
    prettyHex(job1_data, 76);

    ESP_LOGD(TAG, "hello test ltc nonce debug");
    uint32_t nonce = 0xeb269053;
    int64_t time_start = esp_timer_get_time() / 1000;
    double diff = test_ltc_nonce_value(&job, nonce, 0, 0, 0, 0);
    ESP_LOGI(TAG, "test_ltc_nonce_value cost %"PRIi64"", esp_timer_get_time()/1000 - time_start);
    //TEST_ASSERT_GREATER_OR_EQUAL_UINT32(256*256, (int)diff);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(256, (int)diff);    
}

/*

[AB CD 00 4E 18 AA D0 16 18 4E 06]


[AB CD 00 32 49 85 D4 16 48 32 16]
I (188792) mining: hw err(0, 22, 72, 50): hash result is 274d9ed67fbb3c22179055acb60baa9697603c64932b2a9af6cd6fb4c41f3c89, diff is 0.000028

[AB CD 00 7B 3B 0A 57 16 38 7B 08]
I (189242) mining: hw err(0, 22, 56, 123): hash result is e76747f60b9d9d072ad67872a8bef0f20b031aa0ac19955965f060243f9ba19e, diff is 0.000025

[7E 80 2F 19 
 C1 C7 8E 68 
 BF 37 D5 2E F2 1A 2C D1 B9 1F 73 86 EC 5A AA 02 A3 0E 64 4F B9 C4 43 73 FB EE 9F 67 9C 79 E0 BA 
 16 74 6B C9 70 60 95 7F 8F CC 07 4F C1 F6 CD 91 07 CB 6A 08 F0 88 A9 12 74 6D 45 12 09 BE 32 58 
 14 00 00 20]
*/

/*

[AB CD 00 69 29 96 43 10 28 69 0A]

[7E 80 2F 19
 58 FC 8E 68
 F8 D5 0E 31 BB 5B 47 30 0D A0 5E DD 6D E4 E7 04 C3 68 E4 23 BC 44 F1 85 16 A2 83 F1 F3 C4 06 BB 
 9C D0 2E 4D 8B 44 FC 3B 1D 92 53 BC 00 F2 A4 2E B1 66 E8 56 C9 02 1E 95 81 89 15 27 3C 5A 5F 25 
 14 00 00 20]

hw err(0, 16, 40, 105): hash result is bfd2c691006d696295fb334c2b355957ffb136fd4d03a3c62fea9164bd6f69be, diff is 0.000021
*/
TEST_CASE("Test ltc_nonce diff checking_hw", "[mining][not-on-qemu]")
{
    mining_notify notify_message;
#if 1
    notify_message.version = 0x20000014;
    notify_message.prev_block_hash = "5832be0912456d7412a988f0086acb0791cdf6c14f07cc8f7f956070c96b7416";
    const char *merkle_root = "9C79E0BAFBEE9F67B9C44373A30E644FEC5AAA02B91F7386F21A2CD1BF37D52E";
    notify_message.ntime = 0x688EC7C1 ;
    notify_message.target = 0x192F807E;
    uint32_t nonce = 0x570A3B7B;   // 0xD0AA184E pass
#else
    notify_message.version = 0x20000014;
    notify_message.prev_block_hash = "255F5A3C27158981951E02C956E866B12EA4F200BC53921D3BFC448B4D2ED09C";
    const char *merkle_root = "F3C406BB16A283F1BC44F185C368E4236DE4E7040DA05EDDBB5B4730F8D50E31";
    notify_message.ntime = 0x688EFC58 ;
    notify_message.target = 0x192F807E;
    uint32_t nonce = 0x43962969;   // 0xD0AA184E pass
#endif
    bm_job job = construct_ltc_job(&notify_message, merkle_root);
    uint8_t job1_data[76] = {0};

    /*construct the hashjob1 data from bm_job.*/
    //memcpy(job1_data, &(next_bm_job->version), 4);
    enc32be(job1_data, job.version);
    memcpy(job1_data + 4, job.prev_block_hash_be, 32);
    memcpy(job1_data + 4 + 32, job.merkle_root_be, 32);
    enc32be(job1_data + 4 + 32 + 32, job.ntime);
    enc32be(job1_data + 4 + 32 + 32 + 4, job.target);
    reverse_bytes(job1_data, 76);
    prettyHex(job1_data, 76);

    ESP_LOGD(TAG, "hello test ltc nonce debug");
    int64_t time_start = esp_timer_get_time() / 1000;
    double diff = test_ltc_nonce_value(&job, nonce, 0, 0, 0, 0);
    ESP_LOGI(TAG, "test_ltc_nonce_value cost %"PRIi64"", esp_timer_get_time()/1000 - time_start);
    //TEST_ASSERT_GREATER_OR_EQUAL_UINT32(256*256, (int)diff);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(1, (int)diff);    
}


TEST_CASE("Validate version mask incrementing", "[mining]")
{
    uint32_t version = 0x20000004;
    uint32_t version_mask = 0x00ffff00;

    uint32_t rolled_version = increment_bitmask(version, version_mask);
    TEST_ASSERT_EQUAL_UINT32(0x20000104, rolled_version);
    rolled_version = increment_bitmask(rolled_version, version_mask);
    TEST_ASSERT_EQUAL_UINT32(0x20000204, rolled_version);
    rolled_version = increment_bitmask(rolled_version, version_mask);
    TEST_ASSERT_EQUAL_UINT32(0x20000304, rolled_version);
    rolled_version = increment_bitmask(rolled_version, version_mask);
    TEST_ASSERT_EQUAL_UINT32(0x20000404, rolled_version);
}

// Values calculated from esp-miner/components/stratum/test/verifiers/bm1397.py
// TEST_CASE("Validate bm job construction 2", "[mining]")
// {
//     const char * notify_json_str = "{\"id\":null,\"method\":\"mining.notify\","
//     "\"params\":[\"21554471e8\",\"8bc8707eb169ad3bda101ae60c8d48bd00aff68a00006c8b0000000000000000\",\"01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4b03d8130cfabe6d6db0ba74b36edc62c9268c945b53ebf1a7865b88bcdd40235a7a63d0f5ed5b6c400100000000000000\",\"e8714455212f736c7573682f00000000033de04728000000001976a9147c154ed1dc59609e3d26abb2df2ea3d587cd8c4188ac00000000000000002c6a4c2952534b424c4f434b3ae8c3686251b5ced65b6a65ea3e0491ac2975cd87c02b0640d3ec3c20005167770000000000000000266a24aa21a9eddffbecb5ef0a46324a3dd902fa84509a38d2c91548768845db5d6c2de0e33f6100000000\","
//     "[\"8ef6b79382a1fc5152c7e69b2dd4e3795ed758d6fe7748ef4d96e3ad8ac180b8\",\"5b6e1cfecd94050b763c2c6a08d4caabd54daee665aa8e41b53b39ec76b62707\",\"f85b768f83fffbb3927f7f440cb57a5ed386f368aae88ad9b9d92e7bc1cdce15\",\"e51b9391c39019d8a2a27becc048cc770f5d33b49a29779fdc7bed04767ca962\",\"9f5d08316ead260455ec532a58935411a3eecf3c9948a52325de495d7dd7b776\",\"57e10cad23a646ad3a87fcd34eae454567dbd44946e746ee6310a86b98afa4ac\",\"f3b65cc08b25901b657efb22f0a9a23e1a61ce1e268f801d8cfe782b4a0c5e5d\",\"648e00fe2a57dca155c7d4260bc52273b28adb42e1bceb45d5ee03f4a4c5d174\",\"43ad393f7efe4b7a29775dbbc10b3b2737e9457764a7b39bc8ac6b470b968ac8\",\"4964b9b2bf601dfb2bd62067acafe556650412b1e6fe32df48c39310f7dc255d\",\"44d354ac57fcb68b408df7f5396122195384914dd2db13d5766c334fc48c2069\",\"568514a2db82a055772218f52db2f5fa157c37a9ba16c1a239819e57f0d16218\"],"
//     "\"20000004\",\"1705ae3a\",\"6470e2a1\",true]}";
//     mining_notify * params = parse_mining_notify_message(notify_json_str, 512);
//     char * coinbase_tx = construct_coinbase_tx(params->coinbase_1, params->coinbase_2, "336508070fca95", "0000000000000000");
//     char * merkle_root = calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32])params->merkle_branches, params->n_merkle_branches);
//     bm_job job = construct_bm_job(params, merkle_root);

//     uint8_t expected_midstate_bin[32];
//     hex2bin("5FD281AF6A1750EAEE502C04067738BD46C82FC22112FFE797CE7F035D276126", expected_midstate_bin, 32);
//     // bytes are reversed for the midstate on the bm job command packet
//     reverse_bytes(expected_midstate_bin, 32);
//     TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_midstate_bin, job.midstate, 32);
//     TEST_ASSERT_EQUAL_UINT32(0x1705ae3a, job.target);
//     TEST_ASSERT_EQUAL_UINT32(0x6470e2a1, job.ntime);
//     TEST_ASSERT_EQUAL_UINT8(0x8a, job.merkle_root[28]);
//     TEST_ASSERT_EQUAL_UINT8(0xdd, job.merkle_root[29]);
//     TEST_ASSERT_EQUAL_UINT8(0xa8, job.merkle_root[30]);
//     TEST_ASSERT_EQUAL_UINT8(0x6a, job.merkle_root[31]);
// }

/*
TEST_CASE("Test nonce diff checking", "[mining test_nonce][not-on-qemu]")
{
    mining_notify notify_message;
    notify_message.prev_block_hash = "d02b10fc0d4711eae1a805af50a8a83312a2215e00017f2b0000000000000000";
    notify_message.version = 0x20000004;
    notify_message.target = 0x1705ae3a;
    notify_message.ntime = 0x646ff1a9;
    const char *merkle_root = "6d0359c451434605c52a5a9ce074340be47c2c63840731f9edf1db3f26b1cdd9";
    bm_job job = construct_bm_job(&notify_message, merkle_root, 0);

    uint32_t nonce = 0x276E8947;
    ESP_LOGD(TAG, "ntime is 0x%" PRIx32, job.ntime);
    ESP_LOGD(TAG, "target is 0x%" PRIx32, job.target);
    double diff = test_ltc_nonce_value(&job, nonce, 0, 0, 0, 0);
    TEST_ASSERT_EQUAL_INT(18, (int)diff);
}

TEST_CASE("Test nonce diff checking_2", "[mining test_nonce][not-on-qemu]")
{
    mining_notify notify_message;
    notify_message.prev_block_hash = "d02b10fc0d4711eae1a805af50a8a83312a2215e00017f2b0000000000000000";
    notify_message.version = 0x20000004;
    notify_message.target = 0x1705ae3a;
    notify_message.ntime = 0x646ff1a9;
    const char *merkle_root = "6d0359c451434605c52a5a9ce074340be47c2c63840731f9edf1db3f26b1cdd9";
    bm_job job = construct_bm_job(&notify_message, merkle_root, 0);

    uint32_t nonce = 0x276E8947;
    ESP_LOGD(TAG, "ntime is 0x%" PRIx32, job.ntime);
    ESP_LOGD(TAG, "target is 0x%" PRIx32, job.target);
    double diff = test_nonce_value(&job, nonce, notify_message.version);
    TEST_ASSERT_EQUAL_INT(18, (int)diff);
}

TEST_CASE("Test nonce diff checking 2", "[mining test_nonce][not-on-qemu]")
{
    mining_notify notify_message;
    notify_message.prev_block_hash = "0c859545a3498373a57452fac22eb7113df2a465000543520000000000000000";
    notify_message.version = 0x20000004;
    notify_message.target = 0x1705ae3a;
    notify_message.ntime = 0x647025b5;

    const char *coinbase_tx = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4b0389130cfabe6d6d5cbab26a2599e92916edec5657a94a0708ddb970f5c45b5d12905085617eff8e010000000000000031650707758de07b010000000000001cfd7038212f736c7573682f000000000379ad0c2a000000001976a9147c154ed1dc59609e3d26abb2df2ea3d587cd8c4188ac00000000000000002c6a4c2952534b424c4f434b3ae725d3994b811572c1f345deb98b56b465ef8e153ecbbd27fa37bf1b005161380000000000000000266a24aa21a9ed63b06a7946b190a3fda1d76165b25c9b883bcc6621b040773050ee2a1bb18f1800000000";
    uint8_t merkles[13][32];
    int num_merkles = 13;

    hex2bin("2b77d9e413e8121cd7a17ff46029591051d0922bd90b2b2a38811af1cb57a2b2", merkles[0], 32);
    hex2bin("5c8874cef00f3a233939516950e160949ef327891c9090467cead995441d22c5", merkles[1], 32);
    hex2bin("2d91ff8e19ac5fa69a40081f26c5852d366d608b04d2efe0d5b65d111d0d8074", merkles[2], 32);
    hex2bin("0ae96f609ad2264112a0b2dfb65624bedbcea3b036a59c0173394bba3a74e887", merkles[3], 32);
    hex2bin("e62172e63973d69574a82828aeb5711fc5ff97946db10fc7ec32830b24df7bde", merkles[4], 32);
    hex2bin("adb49456453aab49549a9eb46bb26787fb538e0a5f656992275194c04651ec97", merkles[5], 32);
    hex2bin("a7bc56d04d2672a8683892d6c8d376c73d250a4871fdf6f57019bcc737d6d2c2", merkles[6], 32);
    hex2bin("d94eceb8182b4f418cd071e93ec2a8993a0898d4c93bc33d9302f60dbbd0ed10", merkles[7], 32);
    hex2bin("5ad7788b8c66f8f50d332b88a80077ce10e54281ca472b4ed9bbbbcb6cf99083", merkles[8], 32);
    hex2bin("9f9d784b33df1b3ed3edb4211afc0dc1909af9758c6f8267e469f5148ed04809", merkles[9], 32);
    hex2bin("48fd17affa76b23e6fb2257df30374da839d6cb264656a82e34b350722b05123", merkles[10], 32);
    hex2bin("c4f5ab01913fc186d550c1a28f3f3e9ffaca2016b961a6a751f8cca0089df924", merkles[11], 32);
    hex2bin("cff737e1d00176dd6bbfa73071adbb370f227cfb5fba186562e4060fcec877e1", merkles[12], 32);

    char *merkle_root = calculate_merkle_root_hash(coinbase_tx, merkles, num_merkles);
    TEST_ASSERT_EQUAL_STRING("5bdc1968499c3393873edf8e07a1c3a50a97fc3a9d1a376bbf77087dd63778eb", merkle_root);

    bm_job job = construct_bm_job(&notify_message, merkle_root, 0);

    uint32_t nonce = 0x0a029ed1;
    double diff = test_nonce_value(&job, nonce, 0);
    TEST_ASSERT_EQUAL_INT(683, (int)diff);
}
*/

extern double truediffone;

TEST_CASE("test_calc_diff", "[mining]")
{
    double d64, s64, ds;

    uint8_t hash_result[32];
    char *str_hash_result = "aa04ac5947e56c67576c67f19263dec7a7820ade073bb5faa884924e5f030000";
    hex2bin(str_hash_result, hash_result, 32);

    d64 = truediffone;
    s64 = le256todouble(hash_result);
    ds = d64/s64;

    ESP_LOGD(TAG, "s64 is %f", s64);
    ESP_LOGD(TAG, "ds is %f", ds);
    TEST_ASSERT_GREATER_OR_EQUAL_DOUBLE(32, ds);
}