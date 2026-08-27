
#include "crc.h"


// Poly x⁵ + x² + 1 MSB-first
uint8_t crc5(uint8_t *data, uint8_t len) {

    uint8_t crc = 0x1F;
    uint8_t bit_counter, byte_counter;

    for (byte_counter = 0; byte_counter < len; byte_counter++) {
        uint8_t byte = data[byte_counter];
        for (bit_counter = 0; bit_counter < 8; bit_counter++) {
            uint8_t bit = (byte >> 7) & 1;
            byte <<= 1;

            uint8_t new_bit = ((crc >> 4) ^ bit) & 1;
            crc = ((crc << 1) | new_bit) ^ (new_bit << 2);
            crc &= 0x1F;
        }
    }

    return crc;
}

// with loop unrolling
uint16_t crc16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;

    while(len >= 4) {
        crc = crc16_table[(crc >> 8) ^ *data++] ^ (crc << 8);
        crc = crc16_table[(crc >> 8) ^ *data++] ^ (crc << 8);
        crc = crc16_table[(crc >> 8) ^ *data++] ^ (crc << 8);
        crc = crc16_table[(crc >> 8) ^ *data++] ^ (crc << 8);
        len -= 4;
    }

    while(len--) {
        crc = crc16_table[(crc >> 8) ^ *data++] ^ (crc << 8);
    }

    return crc;
}

uint16_t crc16_false(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    while(len--) {
        crc = crc16_table[(crc >> 8) ^ *data++] ^ (crc << 8);
    }

    return crc;
}

/*
 * Bit-length CRC-5, replacing the CRC5() that was only available from the
 * components/a binary blob.
 *
 * Same polynomial and preset as crc5() above, but the length is counted in
 * BITS rather than bytes, so it can cover a message that does not end on a
 * byte boundary. The blob's implementation was bit-serial for that reason;
 * its DWARF debug information still shows the crcin[]/crcout[] shift
 * registers this mirrors.
 *
 * Verified against both vectors the vendor shipped in
 * components/asic/test/test-asic_abstraction.c.
 */
uint8_t crc5_bits(const uint8_t *data, uint8_t bit_len)
{
    /* Shift register preset to all ones, i.e. 0x1F. */
    uint8_t reg[5] = {1, 1, 1, 1, 1};
    uint8_t mask = 0x80;
    uint8_t byte = 0;

    for (uint8_t i = 0; i < bit_len; i++) {
        uint8_t din = (data[byte] & mask) ? 1 : 0;
        uint8_t next[5];

        next[0] = reg[4] ^ din;
        next[1] = reg[0];
        next[2] = reg[1] ^ reg[4] ^ din;
        next[3] = reg[2];
        next[4] = reg[3];

        reg[0] = next[0];
        reg[1] = next[1];
        reg[2] = next[2];
        reg[3] = next[3];
        reg[4] = next[4];

        mask >>= 1;
        if (mask == 0) {
            mask = 0x80;
            byte++;
        }
    }

    return (uint8_t)((reg[4] << 4) | (reg[3] << 3) | (reg[2] << 2) |
                     (reg[1] << 1) | reg[0]);
}
