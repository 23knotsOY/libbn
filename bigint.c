#include "bigint.h"
#include <string.h>
#include "error.h"


const uint64_t	USTORE_MAX =		0xFFFFFFFF;
const int64_t	STORE_MAX =			0xFFFFFFFF;

#define BYTE_BIT_SIZE 8
#define MACHINE_WORD_SIZE 8

#define LITTLE_ENDIAN

#define BIGINT_SIZE(x) x->u.small[0]
#define BIGINT_DATA(x) ((uint8_t*)x) + sizeof(*x)
#define BIGINT_SIGNED(x) 0

//

void* bytewise_memset(void* s, int c, size_t sz) {
	uint8_t* p = (uint8_t*)s;

	uint8_t x = c & 0xff;

	while (sz--)
		* p++ = x;
	return s;
}

void bigint_init(struct bigint* in)
{
	eassert(BIGINT_SIZE(in) % 8 == 0);

	bytewise_memset(BIGINT_DATA(in), 0, BIGINT_SIZE(in) / BYTE_BIT_SIZE);
}

void bigint_initRaw(void* in, unsigned int bitSize)
{
	eassert(bitSize % 8 == 0);

	bytewise_memset(in, 0, bitSize / BYTE_BIT_SIZE);
}

static void _bigint_flipvert(const void* data, void* output, size_t size)
{
	int i;
	uint8_t copy;

	eassert(size % 2 == 0);

	for (i = 0; i < size; i++)
	{
		copy = ((uint8_t*)data)[i];
		((uint8_t*)output)[i] = ((uint8_t*)data)[size - i - 1];
		((uint8_t*)output)[size - i - 1] = copy;
	}
}

static void _bigint_flipvertSelf(void* data, size_t size)
{
	_bigint_flipvert(data, data, size);
}

// Helpers

static inline void _impl_bigint_toHexString(unsigned int bitSize, const void *data, char* out, size_t outlen)
{
	#define APPEND_WORD "0x"

	const char* _encodeTable = "0123456789ABCDEF";
	int i, x;
	const uint8_t* input;
	uint32_t wordInput;
	char* _out = out;
	int factor;
	uint8_t a, b, c, d;

	eassert(!((uint64_t)outlen < bitSize / (uint64_t)BYTE_BIT_SIZE + (uint64_t)3));

	input = (uint8_t*)data;
	
	// Append "0x" to the output buffer.
	*out++ = '0';
	*out++ = 'x';

	while (*input == 0)
		input++;

	if (*input / 16 == 0)
	{
		*out++ = _encodeTable[*input % 16];
		input++;
	}

	// Calculate diff after skipping nulls.
	factor = bitSize / BYTE_BIT_SIZE - (input - (uint8_t*)data);

	if (factor % SMALL_WORD_SIZE / BYTE_BIT_SIZE != 0)
	{
		input -= factor % SMALL_WORD_SIZE / BYTE_BIT_SIZE;
		factor += factor % SMALL_WORD_SIZE / BYTE_BIT_SIZE;
	}

	for (i = 0; i < factor / (SMALL_WORD_SIZE / BYTE_BIT_SIZE); i++)
	{
		wordInput = *(uint32_t*)input;

		a = ((uint8_t*)&wordInput)[0];
		b = ((uint8_t*)&wordInput)[1];
		c = ((uint8_t*)&wordInput)[2];
		d = ((uint8_t*)&wordInput)[3];

		((uint8_t*)&wordInput)[0] = d;
		((uint8_t*)&wordInput)[1] = c;
		((uint8_t*)&wordInput)[2] = b;
		((uint8_t*)&wordInput)[3] = a;

		for (x = 0; x < SMALL_WORD_SIZE / BYTE_BIT_SIZE; x++)
		{
			*out++ = _encodeTable[((uint8_t*)& wordInput)[x] / 16];
			*out++ = _encodeTable[((uint8_t*)& wordInput)[x] % 16];
		}

		input += 4;
	}

	// Null terminate output buffer.
	*out++ = 0;

	// Trim leading zeros after align.
	i = 0;
	out = _out + 2;
	while (*out++ == '0') i++;
	out = _out + 2;

	if (i > 0)
	{
		do
		{
			*out = *(out + i);
		} while (*out++ != 0);
	}

	#undef APPEND_WORD
}

void bigint_toHexString(const struct bigint* source, char* out, size_t outlen)
{
	_impl_bigint_toHexString(BIGINT_SIZE(source), BIGINT_DATA(source), out, outlen);
}

void bigint_toHexStringRaw(const void* source, char* out, size_t outlen, unsigned int bitSize)
{
	_impl_bigint_toHexString(bitSize, source, out, outlen);
}

static inline uint8_t _impl_bigint_asciiToHex(char ascii)
{
	if (ascii - '0' >= 0 && ascii - '0' < 10)
		return ascii - '0';
	else
	{
		if (ascii - 'a' < 6)
			return ascii - 'a' + 10;
		else
		{
			eassert(ascii - 'A' < 6);
			return ascii - 'A' + 10;
		}
	}
}

static inline void _impl_bigint_fromHexString(void* rawOut, size_t bitSize, const char *dataIn, size_t dataLen)
{
	uint8_t* output;
	uint8_t padLow;
	int i;
	int hi = 0;
	int lo = 0;

	// Skip prefix.

	if (dataIn[0] == '0' && (dataIn[1] == 'x' || dataIn[1] == 'X'))
	{
		dataIn += 2;
		dataLen -= 2;
	}

	output = (uint8_t*)rawOut;

	// Pad odd byte.

	if (dataLen % 2 == 1)
	{
		padLow = _impl_bigint_asciiToHex(*dataIn++);
		dataLen++;
	}
	else
		padLow = 0;

	// Pad zero bytes.
	
	while (dataLen < bitSize / BYTE_BIT_SIZE * 2)
	{
		*output++ = 0;
		dataLen += 2;
	}

	// Redner odd byte.

	if (padLow != 0)
		*output++ = padLow;

	// Decode hex.
	while (*dataIn != 0)
	{
		*output++ = _impl_bigint_asciiToHex(*dataIn++) * 16 + _impl_bigint_asciiToHex(*dataIn++);
	}

	output = (uint8_t*)rawOut;

	// Convert endianess.
	for (i = 0; i < bitSize / SMALL_WORD_SIZE; i++)
	{
		uint8_t a, b, c, d;
		a = output[i * 4];
		b = output[i * 4 + 1];
		c = output[i * 4 + 2];
		d = output[i * 4 + 3];

		output[i * 4] = d;
		output[i * 4 + 1] = c;
		output[i * 4 + 2] = b;
		output[i * 4 + 3] = a;
	}
	
}

void bigint_fromHexString(struct bigint* out, const char* in, size_t len)
{
	_impl_bigint_fromHexString(BIGINT_DATA(out), BIGINT_SIZE(out), in, len);
}

void bigint_fromHexStringRaw(void* dataOut, size_t bitSize, const char* in, size_t len)
{
	_impl_bigint_fromHexString(dataOut, bitSize, in, len);
}

static inline void _impl_bigint_add(void* raw, const void* raw2, size_t bitSize1, size_t bitSize2)
{
	int i;
	uint32_t carry = 0;

	for (i = bitSize2 / SMALL_WORD_SIZE - 1; i >= 0; i--)
	{
		union { uint64_t v64; uint32_t v32[2]; } result;
		result.v64 = ((uint64_t)(((uint32_t*)raw)[i])) + ((uint64_t)(((uint32_t*)raw2)[i]));
		result.v64 += carry;

		if (result.v64 > USTORE_MAX)
			carry = result.v32[1];
		else
			carry = 0;

		(((uint32_t*)raw)[i]) = result.v32[0];
	}

	// Append small to big carry.
	if (carry != 0 && bitSize1 > bitSize2)
		((uint32_t*)raw)[bitSize2 / SMALL_WORD_SIZE] += carry;
}

void bigint_add(struct bigint* target, const struct bigint* source)
{
	_impl_bigint_add(BIGINT_DATA(target), BIGINT_DATA(source), BIGINT_SIZE(target), BIGINT_SIZE(source));
}

static inline void _impl_bigint_sub(void* raw, const void* raw2, size_t bitSize1, size_t bitSize2, int isSigned)
{
	int i;
	int64_t borrow = 0;

	eassert(isSigned == 0);

	for (i = bitSize1 / SMALL_WORD_SIZE - 1; i > -1; i--)
	{
		int64_t result;
		result = (int64_t)(((uint32_t*)raw)[i]) - (int64_t)(((uint32_t*)raw2)[i]) - borrow;

		if (result < 0)
		{
			borrow = 1;
			result = STORE_MAX + 1 + result;
			(((uint32_t*)raw)[i]) = (uint32_t)result;
		}
		else
		{
			(((uint32_t*)raw)[i]) = (uint32_t)result;
			borrow = 0;
		}
	}

	// Borrow from higher to smaller.
	if (borrow != 0 && bitSize1 > bitSize2)
		((uint32_t*)raw)[bitSize2 / SMALL_WORD_SIZE] -= borrow;
}

void bigint_sub(struct bigint* target, const struct bigint* source)
{
	_impl_bigint_sub(BIGINT_DATA(target), BIGINT_DATA(source), BIGINT_SIZE(target), BIGINT_SIZE(source), BIGINT_SIGNED(source));
}

static void _bigint_shiftWordsLeft(void *raw, size_t bitSize, int length)
{
	int i;

	for (i = 0; i < length; i++)
	{
		if (bitSize / SMALL_WORD_SIZE - i - 1 > 0)
		{
			((uint32_t*)raw)[bitSize / SMALL_WORD_SIZE - i - 2] = ((uint32_t*)raw)[bitSize / SMALL_WORD_SIZE - i - 1];
			((uint32_t*)raw)[bitSize / SMALL_WORD_SIZE - i - 1] = 0;
		}
	}
}

void bigint_fromWord(struct bigint* target, IWORD word, int shift)
{
	bigint_init(target);
	((uint32_t*)BIGINT_DATA(target))[BIGINT_SIZE(target) / SMALL_WORD_SIZE - shift - 1] = word;
}

void bigint_copy(struct bigint* target, const struct bigint* source)
{
	memcpy(BIGINT_DATA(target), BIGINT_DATA(source), BIGINT_SIZE(source) / SMALL_WORD_SIZE * SIZEOF_WORD);
}

/*static inline void _impl_bigint_mul(void* raw1, void* raw2, size_t bitSize1, size_t bitSize2)
{
	int i, j, shift = 0;
	uint32_t carry = 0;
	uint8_t temp[bitSize1];
}

void bigint_mul(struct uint256* target, const struct uint256* source)
{
	
	struct uint256 tr;

	bigint_init(&tr);

	for (i = BIT_SIZE / SMALL_WORD_SIZE - 1; i > -1; i--)
	{
		for (j = BIT_SIZE / SMALL_WORD_SIZE - 1; j > -1; j--)
		{
			union { uint64_t v64; uint32_t v32[2]; } result;
			result.v64 = (uint64_t)target->store[j] * (uint64_t)source->store[i] + (uint64_t)carry;


			if (result.v64 > USTORE_MAX)
				carry = result.v32[1];
			else
				carry = 0;

			struct uint256 temp;
			bigint_fromWord(&temp, result.v32[0], 0);
			_bigint_shiftWordsLeft(&temp, 7 - j + shift);
			bigint_add(&tr, &temp);
		}
		shift++;
	}

	bigint_copy(target, &tr);
}*/

