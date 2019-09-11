#pragma once

#include <stdint.h>

#define SMALL_WORD_SIZE 32
#define SIZEOF_WORD 4
#define IWORD uint32_t

#define MACHINE_WORD uint64_t
#define HALF_WORD uint32_t
#define QH_WORD uint16_t

struct bigint
{
	union { MACHINE_WORD big;  HALF_WORD small[2]; } u;
};

struct bigint_256
{
	struct bigint header;
	HALF_WORD container[256 / SMALL_WORD_SIZE];
};

struct bigint_512
{
	struct bigint header;
	HALF_WORD container[512 / SMALL_WORD_SIZE];
};

#define BIGINT_HEADER_INIT(x, y, z) x.u.small[0] = y;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize biginteger to the zero.
void bigint_init(struct bigint* in);
void bigint_initRaw(void* in, unsigned int bitSize);


void bigint_toHexString(const struct bigint* source, char* out, size_t outlen);
void bigint_toHexStringRaw(const void* source, char* out, size_t outlen, unsigned int bitSize);

void bigint_fromHexString(struct bigint* out, const char* in, size_t len);
void bigint_fromHexStringRaw(void *dataOut, size_t bitSize, const char* in, size_t len);

void bigint_add(struct bigint* target, const struct bigint* source);
void bigint_sub(struct bigint* target, const struct bigint* source);
void bigint_mul(struct bigint* target, const struct bigint* source);
void bigint_div(const struct bigint* target, const struct bigint* by, struct bigint* result);


#ifdef __cplusplus
}
#endif