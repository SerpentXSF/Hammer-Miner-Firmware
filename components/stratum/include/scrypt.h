#ifndef SCRYPT_H_
#define SCRYPT_H_

#include <stdint.h>

void HMAC_SHA256_80_init(
    const uint32_t *key, uint32_t *tstate, uint32_t *ostate
);

void PBKDF2_SHA256_80_128(const uint32_t *tstate,
	const uint32_t *ostate, const uint32_t *salt, uint32_t *output);

void PBKDF2_SHA256_128_32(uint32_t *tstate, uint32_t *ostate,
	const uint32_t *salt, uint32_t *output);    

unsigned char *scrypt_buffer_alloc(int N);
void scrypt_core(uint32_t *X, uint32_t *V, int N);

void scrypt_1024_1_1_256(const uint32_t *input, uint32_t *output,
	uint32_t *midstate, unsigned char *scratchpad, int N);


#endif