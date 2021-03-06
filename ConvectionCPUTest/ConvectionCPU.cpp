#include <stdint.h>
#include <cfloat>
#include <math.h>

#include <ppl.h>

#include "../DirectXTex/DirectXTex.h"
#include "../stb_image/stb_image.h"

static uint16_t g_partitionMap[64] =
{
	0xCCCC, 0x8888, 0xEEEE, 0xECC8,
	0xC880, 0xFEEC, 0xFEC8, 0xEC80,
	0xC800, 0xFFEC, 0xFE80, 0xE800,
	0xFFE8, 0xFF00, 0xFFF0, 0xF000,
	0xF710, 0x008E, 0x7100, 0x08CE,
	0x008C, 0x7310, 0x3100, 0x8CCE,
	0x088C, 0x3110, 0x6666, 0x366C,
	0x17E8, 0x0FF0, 0x718E, 0x399C,
	0xaaaa, 0xf0f0, 0x5a5a, 0x33cc,
	0x3c3c, 0x55aa, 0x9696, 0xa55a,
	0x73ce, 0x13c8, 0x324c, 0x3bdc,
	0x6996, 0xc33c, 0x9966, 0x660,
	0x272, 0x4e4, 0x4e40, 0x2720,
	0xc936, 0x936c, 0x39c6, 0x639c,
	0x9336, 0x9cc6, 0x817e, 0xe718,
	0xccf0, 0xfcc, 0x7744, 0xee22,
};

static uint32_t g_partitionMap2[64] =
{
	0xaa685050, 0x6a5a5040, 0x5a5a4200, 0x5450a0a8,
	0xa5a50000, 0xa0a05050, 0x5555a0a0, 0x5a5a5050,
	0xaa550000, 0xaa555500, 0xaaaa5500, 0x90909090,
	0x94949494, 0xa4a4a4a4, 0xa9a59450, 0x2a0a4250,
	0xa5945040, 0x0a425054, 0xa5a5a500, 0x55a0a0a0,
	0xa8a85454, 0x6a6a4040, 0xa4a45000, 0x1a1a0500,
	0x0050a4a4, 0xaaa59090, 0x14696914, 0x69691400,
	0xa08585a0, 0xaa821414, 0x50a4a450, 0x6a5a0200,
	0xa9a58000, 0x5090a0a8, 0xa8a09050, 0x24242424,
	0x00aa5500, 0x24924924, 0x24499224, 0x50a50a50,
	0x500aa550, 0xaaaa4444, 0x66660000, 0xa5a0a5a0,
	0x50a050a0, 0x69286928, 0x44aaaa44, 0x66666600,
	0xaa444444, 0x54a854a8, 0x95809580, 0x96969600,
	0xa85454a8, 0x80959580, 0xaa141414, 0x96960000,
	0xaaaa1414, 0xa05050a0, 0xa0a5a5a0, 0x96000000,
	0x40804080, 0xa9a8a9a8, 0xaaaaaa44, 0x2a4a5254,
};

static int g_fixupIndexes2[64] =
{
	15,15,15,15,
	15,15,15,15,
	15,15,15,15,
	15,15,15,15,
	15, 2, 8, 2,
	2, 8, 8,15,
	2, 8, 2, 2,
	8, 8, 2, 2,

	15,15, 6, 8,
	2, 8,15,15,
	2, 8, 2, 2,
	2,15,15, 6,
	6, 2, 6, 8,
	15,15, 2, 2,
	15,15,15,15,
	15, 2, 2,15,
};

static int g_fixupIndexes3[64][2] =
{
	{ 3,15 },{ 3, 8 },{ 15, 8 },{ 15, 3 },
	{ 8,15 },{ 3,15 },{ 15, 3 },{ 15, 8 },
	{ 8,15 },{ 8,15 },{ 6,15 },{ 6,15 },
	{ 6,15 },{ 5,15 },{ 3,15 },{ 3, 8 },
	{ 3,15 },{ 3, 8 },{ 8,15 },{ 15, 3 },
	{ 3,15 },{ 3, 8 },{ 6,15 },{ 10, 8 },
	{ 5, 3 },{ 8,15 },{ 8, 6 },{ 6,10 },
	{ 8,15 },{ 5,15 },{ 15,10 },{ 15, 8 },

	{ 8,15 },{ 15, 3 },{ 3,15 },{ 5,10 },
	{ 6,10 },{ 10, 8 },{ 8, 9 },{ 15,10 },
	{ 15, 6 },{ 3,15 },{ 15, 8 },{ 5,15 },
	{ 15, 3 },{ 15, 6 },{ 15, 6 },{ 15, 8 },
	{ 3,15 },{ 15, 3 },{ 5,15 },{ 5,15 },
	{ 5,15 },{ 8,15 },{ 5,15 },{ 10,15 },
	{ 5,15 },{ 10,15 },{ 8,15 },{ 13,15 },
	{ 15, 3 },{ 12,15 },{ 3,15 },{ 3, 8 },
};

static const int g_weightRcp[5] = { 0, 0, 21824, 9344, 4352 };

struct InputBlock
{
	int32_t m_pixels[16];
};

enum
{
	MathTypes_Scalar,
	MathTypes_SSE2,
	MathTypes_AVX2,
};


template<int TMath>
struct ParallelMath
{
	static const int ParallelSize = 1;

	typedef float Float;
	typedef int16_t Int16;
	typedef int32_t Int32;
	typedef bool Int16CompFlag;
	typedef bool FloatCompFlag;

	struct PackingVector
	{
		uint32_t m_vector[4];
		int m_offset;

		void Init()
		{
			for (int i = 0; i < 4; i++)
				m_vector[i] = 0;

			m_offset = 0;
		}

		void ExtractUInt16(uint16_t value, int offset)
		{
			return value;
		}

		void Pack(uint16_t value, int bits)
		{
			int vOffset = m_offset >> 5;
			int bitOffset = m_offset & 0x1f;

			m_vector[vOffset] |= (static_cast<uint32_t>(value) << bitOffset) & static_cast<uint32_t>(0xffffffff);

			int overflowBits = bitOffset + bits - 32;
			if (overflowBits > 0)
				m_vector[vOffset + 1] |= (static_cast<uint32_t>(value) >> (bits - overflowBits));

			m_offset += bits;
		}

		void Flush(uint8_t* output)
		{
			assert(m_offset == 128);

			for (int v = 0; v < 4; v++)
			{
				uint32_t chunk = m_vector[v];
				for (int b = 0; b < 4; b++)
					output[v * 4 + b] = static_cast<uint8_t>((chunk >> (b * 8)) & 0xff);
			}
		}
	};

	template<class T>
	static void ConditionalSet(T& dest, bool flag, const T src)
	{
		if (flag)
			dest = src;
	}

	template<class T>
	static T Select(bool flag, T a, T b)
	{
		return flag ? a : b;
	}

	template<class T>
	static T Min(T a, T b)
	{
		if (a < b)
			return a;
		return b;
	}

	template<class T>
	static T Max(T a, T b)
	{
		if (a > b)
			return a;
		return b;
	}

	template<class T>
	static T Clamp(T v, T min, T max)
	{
		return Max(Min(v, max), min);
	}

	static void ReadPackedInputs(const InputBlock* inputBlocks, int pxOffset, Int32& outPackedPx)
	{
		outPackedPx = inputBlocks[0].m_pixels[pxOffset];
	}

	static void UnpackChannel(Int32 inputPx, int ch, Int16& chOut)
	{
		chOut = static_cast<uint16_t>((inputPx >> (ch * 8)) & 0xff);
	}

	static float MakeFloat(float v)
	{
		return v;
	}

	static float MakeFloatZero()
	{
		return 0.f;
	}

	static int16_t MakeUInt16(int16_t v)
	{
		return v;
	}

	static int16_t ExtractUInt16(int16_t v, int offset)
	{
		return v;
	}

	static float ExtractFloat(float v, int offset)
	{
		return v;
	}

	template<class T>
	static bool Less(T a, T b)
	{
		return a < b;
	}

	template<class T>
	static bool Equal(T a, T b)
	{
		return a == b;
	}

	static float UInt16ToFloat(uint16_t v)
	{
		return static_cast<float>(v);
	}

	static Int16CompFlag FloatFlagToInt16(FloatCompFlag v)
	{
		return v;
	}

	static uint16_t FloatToUInt16(float v)
	{
		return static_cast<uint16_t>(floorf(v + 0.5f));
	}

	static float Sqrt(float f)
	{
		return sqrtf(f);
	}

	static uint16_t SqDiff(uint16_t a, uint16_t b)
	{
		int diff = static_cast<int>(a) - static_cast<int>(b);
		return static_cast<uint16_t>(diff * diff);
	}

	static bool AnySet(bool b)
	{
		return b;
	}

	static int16_t UnsignedRightShift(int16_t v, int bits)
	{
		uint32_t i = static_cast<uint32_t>(v) & 0xffff;
		return static_cast<int16_t>(i >> bits);
	}
};


template<>
struct ParallelMath<MathTypes_SSE2>
{
	static const int ParallelSize = 8;

	struct Int16
	{
		__m128i m_value;

		Int16 operator+(int16_t other) const
		{
			Int16 result;
			result.m_value = _mm_add_epi16(m_value, _mm_set1_epi16(other));
			return result;
		}

		Int16 operator+(Int16 other) const
		{
			Int16 result;
			result.m_value = _mm_add_epi16(m_value, other.m_value);
			return result;
		}

		Int16 operator|(Int16 other) const
		{
			Int16 result;
			result.m_value = _mm_or_si128(m_value, other.m_value);
			return result;
		}

		Int16 operator-(Int16 other) const
		{
			Int16 result;
			result.m_value = _mm_sub_epi16(m_value, other.m_value);
			return result;
		}

		Int16 operator*(const Int16& other) const
		{
			Int16 result;
			result.m_value = _mm_mullo_epi16(m_value, other.m_value);
			return result;
		}

		Int16 operator<<(int bits) const
		{
			Int16 result;
			result.m_value = _mm_slli_epi16(m_value, bits);
			return result;
		}
	};

	struct Int32
	{
		__m128i m_values[2];
	};

	struct Float
	{
		__m128 m_values[2];

		Float operator+(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm_add_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm_add_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator-(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm_sub_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm_sub_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator*(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm_mul_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm_mul_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator*(float other) const
		{
			Float result;
			result.m_values[0] = _mm_mul_ps(m_values[0], _mm_set1_ps(other));
			result.m_values[1] = _mm_mul_ps(m_values[1], _mm_set1_ps(other));
			return result;
		}

		Float operator/(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm_div_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm_div_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator/(float other) const
		{
			Float result;
			result.m_values[0] = _mm_div_ps(m_values[0], _mm_set1_ps(other));
			result.m_values[1] = _mm_div_ps(m_values[1], _mm_set1_ps(other));
			return result;
		}
	};

	struct Int16CompFlag
	{
		__m128i m_value;
	};

	struct FloatCompFlag
	{
		__m128 m_values[2];
	};

	typedef ParallelMath<MathTypes_Scalar>::PackingVector PackingVector;

	static Float Select(FloatCompFlag flag, Float a, Float b)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_or_ps(_mm_and_ps(flag.m_values[i], a.m_values[i]), _mm_andnot_ps(flag.m_values[i], b.m_values[i]));
		return result;
	}

	static Int16 Select(Int16CompFlag flag, Int16 a, Int16 b)
	{
		Int16 result;
		result.m_value = _mm_or_si128(_mm_and_si128(flag.m_value, a.m_value), _mm_andnot_si128(flag.m_value, b.m_value));
		return result;
	}

	static void ConditionalSet(Int16& dest, Int16CompFlag flag, const Int16 src)
	{
		dest.m_value = _mm_or_si128(_mm_andnot_si128(flag.m_value, dest.m_value), _mm_and_si128(flag.m_value, src.m_value));
	}

	static void ConditionalSet(Float& dest, FloatCompFlag flag, const Float src)
	{
		for (int i = 0; i < 2; i++)
			dest.m_values[i] = _mm_or_ps(_mm_andnot_ps(flag.m_values[i], dest.m_values[i]), _mm_and_ps(flag.m_values[i], src.m_values[i]));
	}

	static Int16 Min(Int16 a, Int16 b)
	{
		Int16 result;
		result.m_value = _mm_min_epi16(a.m_value, b.m_value);
		return result;
	}

	static Float Min(Float a, Float b)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_min_ps(a.m_values[i], b.m_values[i]);
		return result;
	}

	static Int16 Max(Int16 a, Int16 b)
	{
		Int16 result;
		result.m_value = _mm_max_epi16(a.m_value, b.m_value);
		return result;
	}

	static Float Max(Float a, Float b)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_max_ps(a.m_values[i], b.m_values[i]);
		return result;
	}

	static Float Clamp(Float v, float min, float max)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_max_ps(_mm_min_ps(v.m_values[i], _mm_set1_ps(max)), _mm_set1_ps(min));
		return result;
	}

	static void ReadPackedInputs(const InputBlock* inputBlocks, int pxOffset, Int32& outPackedPx)
	{
		for (int i = 0; i < 4; i++)
			reinterpret_cast<int32_t*>(&outPackedPx.m_values[0])[i] = inputBlocks[i].m_pixels[pxOffset];
		for (int i = 0; i < 4; i++)
			reinterpret_cast<int32_t*>(&outPackedPx.m_values[1])[i] = inputBlocks[i + 4].m_pixels[pxOffset];
	}

	static void UnpackChannel(Int32 inputPx, int ch, Int16& chOut)
	{
		__m128i ch0 = _mm_srli_epi32(inputPx.m_values[0], ch * 8);
		__m128i ch1 = _mm_srli_epi32(inputPx.m_values[1], ch * 8);
		ch0 = _mm_and_si128(ch0, _mm_set1_epi32(0xff));
		ch1 = _mm_and_si128(ch1, _mm_set1_epi32(0xff));

		chOut.m_value = _mm_packs_epi32(ch0, ch1);
	}

	static Float MakeFloat(float v)
	{
		Float f;
		f.m_values[0] = f.m_values[1] = _mm_set1_ps(v);
		return f;
	}

	static Float MakeFloatZero()
	{
		Float f;
		f.m_values[0] = f.m_values[1] = _mm_setzero_ps();
		return f;
	}

	static Int16 MakeUInt16(uint16_t v)
	{
		Int16 result;
		result.m_value = _mm_set1_epi16(static_cast<short>(v));
		return result;
	}

	static uint16_t ExtractUInt16(const Int16& v, int offset)
	{
		return reinterpret_cast<const uint16_t*>(&v)[offset];
	}

	static float ExtractFloat(float v, int offset)
	{
		return reinterpret_cast<const float*>(&v)[offset];
	}

	static Int16CompFlag Less(Int16 a, Int16 b)
	{
		Int16CompFlag result;
		result.m_value = _mm_cmplt_epi16(a.m_value, b.m_value);
		return result;
	}

	static FloatCompFlag Less(Float a, Float b)
	{
		FloatCompFlag result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_cmplt_ps(a.m_values[i], b.m_values[i]);
		return result;
	}

	static Int16CompFlag Equal(Int16 a, Int16 b)
	{
		Int16CompFlag result;
		result.m_value = _mm_cmpeq_epi16(a.m_value, b.m_value);
		return result;
	}

	static FloatCompFlag Equal(Float a, Float b)
	{
		FloatCompFlag result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_cmpeq_ps(a.m_values[i], b.m_values[i]);
		return result;
	}

	static Float UInt16ToFloat(Int16 v)
	{
		Float result;
		result.m_values[0] = _mm_cvtepi32_ps(_mm_unpacklo_epi16(v.m_value, _mm_setzero_si128()));
		result.m_values[1] = _mm_cvtepi32_ps(_mm_unpackhi_epi16(v.m_value, _mm_setzero_si128()));
		return result;
	}

	static Int16CompFlag FloatFlagToInt16(FloatCompFlag v)
	{
		__m128i lo = _mm_castps_si128(v.m_values[0]);
		__m128i hi = _mm_castps_si128(v.m_values[1]);

		Int16CompFlag result;
		result.m_value = _mm_packs_epi32(lo, hi);
		return result;
	}

	static Int16 FloatToUInt16(Float v)
	{
		__m128 half = _mm_set1_ps(0.5f);
		__m128i lo = _mm_cvttps_epi32(_mm_add_ps(v.m_values[0], half));
		__m128i hi = _mm_cvttps_epi32(_mm_add_ps(v.m_values[1], half));

		Int16 result;
		result.m_value = _mm_packs_epi32(lo, hi);
		return result;
	}

	static Float Sqrt(Float f)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm_sqrt_ps(f.m_values[i]);
		return result;
	}

	static Int16 SqDiff(Int16 a, Int16 b)
	{
		__m128i diff = _mm_sub_epi16(a.m_value, b.m_value);

		Int16 result;
		result.m_value = _mm_mullo_epi16(diff, diff);
		return result;
	}

	static Int16 UnsignedRightShift(Int16 v, int bits)
	{
		Int16 result;
		result.m_value = _mm_srli_epi16(v.m_value, bits);
		return result;
	}

	static bool AnySet(Int16CompFlag v)
	{
		return _mm_movemask_epi8(v.m_value) != 0;
	}
};

template<>
struct ParallelMath<MathTypes_AVX2>
{
	static const int ParallelSize = 16;

	struct Int16
	{
		__m256i m_value;

		Int16 operator+(int16_t other) const
		{
			Int16 result;
			result.m_value = _mm256_add_epi16(m_value, _mm256_set1_epi16(other));
			return result;
		}

		Int16 operator+(Int16 other) const
		{
			Int16 result;
			result.m_value = _mm256_add_epi16(m_value, other.m_value);
			return result;
		}

		Int16 operator|(Int16 other) const
		{
			Int16 result;
			result.m_value = _mm256_or_si256(m_value, other.m_value);
			return result;
		}

		Int16 operator-(Int16 other) const
		{
			Int16 result;
			result.m_value = _mm256_sub_epi16(m_value, other.m_value);
			return result;
		}

		Int16 operator*(const Int16& other) const
		{
			Int16 result;
			result.m_value = _mm256_mullo_epi16(m_value, other.m_value);
			return result;
		}

		Int16 operator<<(int bits) const
		{
			Int16 result;
			result.m_value = _mm256_slli_epi16(m_value, bits);
			return result;
		}
	};

	struct Int32
	{
		__m256i m_values[2];
	};

	struct Float
	{
		__m256 m_values[2];

		Float operator+(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm256_add_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm256_add_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator-(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm256_sub_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm256_sub_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator*(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm256_mul_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm256_mul_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator*(float other) const
		{
			Float result;
			result.m_values[0] = _mm256_mul_ps(m_values[0], _mm256_set1_ps(other));
			result.m_values[1] = _mm256_mul_ps(m_values[1], _mm256_set1_ps(other));
			return result;
		}

		Float operator/(const Float& other) const
		{
			Float result;
			result.m_values[0] = _mm256_div_ps(m_values[0], other.m_values[0]);
			result.m_values[1] = _mm256_div_ps(m_values[1], other.m_values[1]);
			return result;
		}

		Float operator/(float other) const
		{
			Float result;
			result.m_values[0] = _mm256_div_ps(m_values[0], _mm256_set1_ps(other));
			result.m_values[1] = _mm256_div_ps(m_values[1], _mm256_set1_ps(other));
			return result;
		}
	};

	struct Int16CompFlag
	{
		__m256i m_value;
	};

	struct FloatCompFlag
	{
		__m256 m_values[2];
	};

	typedef ParallelMath<MathTypes_Scalar>::PackingVector PackingVector;

	static Float Select(FloatCompFlag flag, Float a, Float b)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_or_ps(_mm256_and_ps(flag.m_values[i], a.m_values[i]), _mm256_andnot_ps(flag.m_values[i], b.m_values[i]));
		return result;
	}

	static Int16 Select(Int16CompFlag flag, Int16 a, Int16 b)
	{
		Int16 result;
		result.m_value = _mm256_or_si256(_mm256_and_si256(flag.m_value, a.m_value), _mm256_andnot_si256(flag.m_value, b.m_value));
		return result;
	}

	static void ConditionalSet(Int16& dest, Int16CompFlag flag, const Int16 src)
	{
		dest.m_value = _mm256_or_si256(_mm256_andnot_si256(flag.m_value, dest.m_value), _mm256_and_si256(flag.m_value, src.m_value));
	}

	static void ConditionalSet(Float& dest, FloatCompFlag flag, const Float src)
	{
		for (int i = 0; i < 2; i++)
			dest.m_values[i] = _mm256_or_ps(_mm256_andnot_ps(flag.m_values[i], dest.m_values[i]), _mm256_and_ps(flag.m_values[i], src.m_values[i]));
	}

	static Int16 Min(Int16 a, Int16 b)
	{
		Int16 result;
		result.m_value = _mm256_min_epi16(a.m_value, b.m_value);
		return result;
	}

	static Float Min(Float a, Float b)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_min_ps(a.m_values[i], b.m_values[i]);
		return result;
	}

	static Int16 Max(Int16 a, Int16 b)
	{
		Int16 result;
		result.m_value = _mm256_max_epi16(a.m_value, b.m_value);
		return result;
	}

	static Float Max(Float a, Float b)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_max_ps(a.m_values[i], b.m_values[i]);
		return result;
	}

	static Float Clamp(Float v, float min, float max)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_max_ps(_mm256_min_ps(v.m_values[i], _mm256_set1_ps(max)), _mm256_set1_ps(min));
		return result;
	}

	static void ReadPackedInputs(const InputBlock* inputBlocks, int pxOffset, Int32& outPackedPx)
	{
		// This is weird, but _mm256_pack* interleave for some reason 
		for (int i = 0; i < 4; i++)
			reinterpret_cast<int32_t*>(&outPackedPx.m_values[0])[i] = inputBlocks[i].m_pixels[pxOffset];
		for (int i = 0; i < 4; i++)
			reinterpret_cast<int32_t*>(&outPackedPx.m_values[1])[i] = inputBlocks[i + 4].m_pixels[pxOffset];
		for (int i = 0; i < 4; i++)
			reinterpret_cast<int32_t*>(&outPackedPx.m_values[0])[i + 4] = inputBlocks[i + 8].m_pixels[pxOffset];
		for (int i = 0; i < 4; i++)
			reinterpret_cast<int32_t*>(&outPackedPx.m_values[1])[i + 4] = inputBlocks[i + 12].m_pixels[pxOffset];
	}

	static void UnpackChannel(Int32 inputPx, int ch, Int16& chOut)
	{
		__m256i ch0 = _mm256_srli_epi32(inputPx.m_values[0], ch * 8);
		__m256i ch1 = _mm256_srli_epi32(inputPx.m_values[1], ch * 8);
		ch0 = _mm256_and_si256(ch0, _mm256_set1_epi32(0xff));
		ch1 = _mm256_and_si256(ch1, _mm256_set1_epi32(0xff));

		chOut.m_value = _mm256_packs_epi32(ch0, ch1);
	}

	static Float MakeFloat(float v)
	{
		Float f;
		f.m_values[0] = f.m_values[1] = _mm256_set1_ps(v);
		return f;
	}

	static Float MakeFloatZero()
	{
		Float f;
		f.m_values[0] = f.m_values[1] = _mm256_setzero_ps();
		return f;
	}

	static Int16 MakeUInt16(uint16_t v)
	{
		Int16 result;
		result.m_value = _mm256_set1_epi16(static_cast<short>(v));
		return result;
	}

	static uint16_t ExtractUInt16(const Int16& v, int offset)
	{
		return reinterpret_cast<const uint16_t*>(&v)[offset];
	}

	static float ExtractFloat(float v, int offset)
	{
		return reinterpret_cast<const float*>(&v)[offset];
	}

	static Int16CompFlag Less(Int16 a, Int16 b)
	{
		Int16CompFlag result;
		result.m_value = _mm256_cmpgt_epi16(b.m_value, a.m_value);
		return result;
	}

	static FloatCompFlag Less(Float a, Float b)
	{
		FloatCompFlag result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_cmp_ps(a.m_values[i], b.m_values[i], _CMP_LT_OQ);
		return result;
	}

	static Int16CompFlag Equal(Int16 a, Int16 b)
	{
		Int16CompFlag result;
		result.m_value = _mm256_cmpeq_epi16(a.m_value, b.m_value);
		return result;
	}

	static FloatCompFlag Equal(Float a, Float b)
	{
		FloatCompFlag result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_cmp_ps(a.m_values[i], b.m_values[i], _CMP_EQ_OQ);
		return result;
	}

	static Float UInt16ToFloat(Int16 v)
	{
		Float result;
		result.m_values[0] = _mm256_cvtepi32_ps(_mm256_unpacklo_epi16(v.m_value, _mm256_setzero_si256()));
		result.m_values[1] = _mm256_cvtepi32_ps(_mm256_unpackhi_epi16(v.m_value, _mm256_setzero_si256()));
		return result;
	}

	static Int16CompFlag FloatFlagToInt16(FloatCompFlag v)
	{
		__m256i lo = _mm256_castps_si256(v.m_values[0]);
		__m256i hi = _mm256_castps_si256(v.m_values[1]);

		Int16CompFlag result;
		result.m_value = _mm256_packs_epi32(lo, hi);
		return result;
	}

	static Int16 FloatToUInt16(Float v)
	{
		__m256 half = _mm256_set1_ps(0.5f);
		__m256i lo = _mm256_cvttps_epi32(_mm256_add_ps(v.m_values[0], half));
		__m256i hi = _mm256_cvttps_epi32(_mm256_add_ps(v.m_values[1], half));

		Int16 result;
		result.m_value = _mm256_packs_epi32(lo, hi);
		return result;
	}

	static Float Sqrt(Float f)
	{
		Float result;
		for (int i = 0; i < 2; i++)
			result.m_values[i] = _mm256_sqrt_ps(f.m_values[i]);
		return result;
	}

	static Int16 SqDiff(Int16 a, Int16 b)
	{
		__m256i diff = _mm256_sub_epi16(a.m_value, b.m_value);

		Int16 result;
		result.m_value = _mm256_mullo_epi16(diff, diff);
		return result;
	}

	static Int16 UnsignedRightShift(Int16 v, int bits)
	{
		Int16 result;
		result.m_value = _mm256_srli_epi16(v.m_value, bits);
		return result;
	}

	static bool AnySet(Int16CompFlag v)
	{
		return _mm256_movemask_epi8(v.m_value) != 0;
	}
};


void ComputeTweakFactors(int tweak, int bits, float* outFactors)
{
	int totalUnits = (1 << bits) - 1;
	int minOutsideUnits = ((tweak >> 1) & 1);
	int maxOutsideUnits = (tweak & 1);
	int insideUnits = totalUnits - minOutsideUnits - maxOutsideUnits;

	outFactors[0] = -static_cast<float>(minOutsideUnits) / static_cast<float>(insideUnits);
	outFactors[1] = static_cast<float>(maxOutsideUnits) / static_cast<float>(insideUnits) + 1.0f;
}

template<int TMath, int TVectorSize>
class UnfinishedEndpoints
{
public:
	typedef ParallelMath<TMath> Math;
	typedef typename Math::Float MFloat;
	typedef typename Math::Int16 MInt16;

	typename MFloat m_base[TVectorSize];
	typename MFloat m_offset[TVectorSize];

	void Finish(int tweak, int bits, MInt16* outEP0, MInt16* outEP1)
	{
		float tweakFactors[2];
		ComputeTweakFactors(tweak, bits, tweakFactors);

		for (int ch = 0; ch < TVectorSize; ch++)
		{
			MFloat ep0f = Math::Clamp(m_base[ch] + m_offset[ch] * tweakFactors[0], 0.0f, 255.0f);
			MFloat ep1f = Math::Clamp(m_base[ch] + m_offset[ch] * tweakFactors[1], 0.0f, 255.0f);
			outEP0[ch] = Math::FloatToUInt16(ep0f);
			outEP1[ch] = Math::FloatToUInt16(ep1f);
		}
	}
};

template<int TMath>
class BC7EndpointSelectorRGBA
{
public:
	static const int NumPasses = 3;
	static const int NumPowerIterations = 8;

	typedef ParallelMath<TMath> Math;
	typedef typename Math::Float MFloat;
	typedef typename Math::Int16 MInt16;

	MFloat m_total[4];
	MFloat m_ctr[4];
	MFloat m_axis[4];
	MFloat m_xx;
	MFloat m_xy;
	MFloat m_xz;
	MFloat m_xw;
	MFloat m_yy;
	MFloat m_yz;
	MFloat m_yw;
	MFloat m_zz;
	MFloat m_zw;
	MFloat m_ww;
	MFloat m_minDist;
	MFloat m_maxDist;

	BC7EndpointSelectorRGBA()
	{
		for (int i = 0; i < 4; i++)
		{
			m_total[i] = Math::MakeFloatZero();
			m_ctr[i] = Math::MakeFloatZero();
			m_axis[i] = Math::MakeFloatZero();
		}
		m_xx = Math::MakeFloatZero();
		m_xy = Math::MakeFloatZero();
		m_xz = Math::MakeFloatZero();
		m_xw = Math::MakeFloatZero();
		m_yy = Math::MakeFloatZero();
		m_yz = Math::MakeFloatZero();
		m_yw = Math::MakeFloatZero();
		m_zz = Math::MakeFloatZero();
		m_zw = Math::MakeFloatZero();
		m_ww = Math::MakeFloatZero();
		m_minDist = Math::MakeFloat(1000.0f);
		m_maxDist = Math::MakeFloat(-1000.0f);
	}

	void InitPass(int step)
	{
		if (step == 1)
		{
			for (int i = 0; i < 4; i++)
				m_ctr[i] = m_ctr[i] / Math::Max(m_total[i], Math::MakeFloat(0.0001f));
		}
		else if (step == 2)
		{
			MFloat matrix[4][4] =
			{
				{ m_xx, m_xy, m_xz, m_xw },
				{ m_xy, m_yy, m_yz, m_yw },
				{ m_xz, m_yz, m_zz, m_zw },
				{ m_xw, m_yw, m_zw, m_ww }
			};

			MFloat v[4] = { Math::MakeFloat(1.0f), Math::MakeFloat(1.0f), Math::MakeFloat(1.0f), Math::MakeFloat(1.0f) };
			for (int i = 0; i < NumPowerIterations; ++i)
			{
				// matrix multiply
				MFloat w[4];
				for (int i = 0; i < 4; i++)
				{
					w[i] = matrix[0][i] * v[0];
					for (int row = 1; row < 4; row++)
						w[i] = w[i] + matrix[row][i] * v[row];
				}

				MFloat a = Math::Max(w[0], Math::Max(w[1], Math::Max(w[2], w[3])));

				typename Math::FloatCompFlag aZero = Math::Equal(a, Math::MakeFloatZero());

				Math::ConditionalSet(a, aZero, Math::MakeFloat(1.0f));

				for (int c = 0; c < 4; c++)
					v[c] = w[c] / a;
			}

			MFloat vlen = Math::Sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);

			typename Math::FloatCompFlag vZero = Math::Equal(vlen, Math::MakeFloatZero());
			Math::ConditionalSet(vlen, vZero, Math::MakeFloat(1.0f));

			for (int i = 0; i < 4; i++)
				m_axis[i] = v[i] / vlen;
		}
	}

	void Contribute(int step, const MInt16* pixel, MFloat weight)
	{
		MFloat pt[4];
		for (int i = 0; i < 4; i++)
			pt[i] = Math::UInt16ToFloat(pixel[i]);

		if (step == 0)
		{
			for (int i = 0; i < 4; i++)
			{
				m_total[i] = m_total[i] + weight;
				m_ctr[i] = m_ctr[i] + weight * pt[i];
			}
		}
		else if (step == 1)
		{
			MFloat a[4];
			MFloat b[4];

			for (int i = 0; i < 4; i++)
			{
				a[i] = pt[i] - m_ctr[i];
				b[i] = weight * a[i];
			}

			m_xx = m_xx + a[0] * b[0];
			m_xy = m_xy + a[0] * b[1];
			m_xz = m_xz + a[0] * b[2];
			m_xw = m_xw + a[0] * b[3];
			m_yy = m_yy + a[1] * b[1];
			m_yz = m_yz + a[1] * b[2];
			m_yw = m_yw + a[1] * b[3];
			m_zz = m_zz + a[2] * b[2];
			m_zw = m_zw + a[2] * b[3];
			m_ww = m_ww + a[3] * b[3];
		}
		else if (step == 2)
		{
			MFloat diff[4];
			for (int i = 0; i < 4; i++)
				diff[i] = pt[i] - m_ctr[i];

			MFloat dist = diff[0] * m_axis[0] + diff[1] * m_axis[1] + diff[2] * m_axis[2] + diff[3] * m_axis[3];
			m_minDist = Math::Min(dist, m_minDist);
			m_maxDist = Math::Max(dist, m_maxDist);
		}
	}

	UnfinishedEndpoints<TMath, 4> GetEndpoints() const
	{
		MFloat len = m_maxDist - m_minDist;

		UnfinishedEndpoints<TMath, 4> result;
		for (int i = 0; i < 4; i++)
		{
			result.m_base[i] = m_ctr[i] + m_axis[i] * m_minDist;
			result.m_offset[i] = m_axis[i] * len;
		}
		return result;
	}
};


template<int TMath>
class BC7EndpointSelectorRGB
{
public:
	static const int NumPasses = 3;
	static const int NumPowerIterations = 8;

	typedef ParallelMath<TMath> Math;
	typedef typename Math::Float MFloat;
	typedef typename Math::Int16 MInt16;

	MFloat m_total[3];
	MFloat m_ctr[3];
	MFloat m_axis[3];
	MFloat m_xx;
	MFloat m_xy;
	MFloat m_xz;
	MFloat m_xw;
	MFloat m_yy;
	MFloat m_yz;
	MFloat m_yw;
	MFloat m_zz;
	MFloat m_zw;
	MFloat m_ww;
	MFloat m_minDist;
	MFloat m_maxDist;

	BC7EndpointSelectorRGB()
	{
		for (int i = 0; i < 3; i++)
		{
			m_total[i] = Math::MakeFloatZero();
			m_ctr[i] = Math::MakeFloatZero();
			m_axis[i] = Math::MakeFloatZero();
		}
		m_xx = Math::MakeFloatZero();
		m_xy = Math::MakeFloatZero();
		m_xz = Math::MakeFloatZero();
		m_xw = Math::MakeFloatZero();
		m_yy = Math::MakeFloatZero();
		m_yz = Math::MakeFloatZero();
		m_yw = Math::MakeFloatZero();
		m_zz = Math::MakeFloatZero();
		m_zw = Math::MakeFloatZero();
		m_ww = Math::MakeFloatZero();
		m_minDist = Math::MakeFloat(1000.0f);
		m_maxDist = Math::MakeFloat(-1000.0f);
	}

	void InitPass(int step)
	{
		if (step == 1)
		{
			for (int i = 0; i < 3; i++)
				m_ctr[i] = m_ctr[i] / Math::Max(m_total[i], Math::MakeFloat(0.0001f));
		}
		else if (step == 2)
		{
			MFloat matrix[3][3] =
			{
				{ m_xx, m_xy, m_xz },
				{ m_xy, m_yy, m_yz },
				{ m_xz, m_yz, m_zz },
			};

			MFloat v[3] = { Math::MakeFloat(1.0f), Math::MakeFloat(1.0f), Math::MakeFloat(1.0f) };
			for (int i = 0; i < NumPowerIterations; ++i)
			{
				// matrix multiply
				MFloat w[3];
				for (int i = 0; i < 3; i++)
				{
					w[i] = matrix[0][i] * v[0];
					for (int row = 1; row < 3; row++)
						w[i] = w[i] + matrix[row][i] * v[row];
				}

				MFloat a = Math::Max(w[0], Math::Max(w[1], w[2]));

				typename Math::FloatCompFlag aZero = Math::Equal(a, Math::MakeFloatZero());

				Math::ConditionalSet(a, aZero, Math::MakeFloat(1.0f));

				for (int c = 0; c < 3; c++)
					v[c] = w[c] / a;
			}

			MFloat vlen = Math::Sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

			typename Math::FloatCompFlag vZero = Math::Equal(vlen, Math::MakeFloatZero());
			Math::ConditionalSet(vlen, vZero, Math::MakeFloat(1.0f));

			for (int i = 0; i < 3; i++)
				m_axis[i] = v[i] / vlen;
		}
	}

	void Contribute(int step, const MInt16* pixel, MFloat weight)
	{
		MFloat pt[3];
		for (int i = 0; i < 3; i++)
			pt[i] = Math::UInt16ToFloat(pixel[i]);

		if (step == 0)
		{
			for (int i = 0; i < 3; i++)
			{
				m_total[i] = m_total[i] + weight;
				m_ctr[i] = m_ctr[i] + weight * pt[i];
			}
		}
		else if (step == 1)
		{
			MFloat a[3];
			MFloat b[3];

			for (int i = 0; i < 3; i++)
			{
				a[i] = pt[i] - m_ctr[i];
				b[i] = weight * a[i];
			}

			m_xx = m_xx + a[0] * b[0];
			m_xy = m_xy + a[0] * b[1];
			m_xz = m_xz + a[0] * b[2];
			m_yy = m_yy + a[1] * b[1];
			m_yz = m_yz + a[1] * b[2];
			m_zz = m_zz + a[2] * b[2];
		}
		else if (step == 2)
		{
			MFloat diff[3];
			for (int i = 0; i < 3; i++)
				diff[i] = pt[i] - m_ctr[i];

			MFloat dist = diff[0] * m_axis[0] + diff[1] * m_axis[1] + diff[2] * m_axis[2];
			m_minDist = Math::Min(dist, m_minDist);
			m_maxDist = Math::Max(dist, m_maxDist);
		}
	}

	UnfinishedEndpoints<TMath, 3> GetEndpoints() const
	{
		MFloat len = m_maxDist - m_minDist;

		UnfinishedEndpoints<TMath, 3> result;
		for (int i = 0; i < 3; i++)
		{
			result.m_base[i] = m_ctr[i] + m_axis[i] * m_minDist;
			result.m_offset[i] = m_axis[i] * len;
		}
		return result;
	}
};

template<int TMath, int TVectorSize>
class IndexSelector
{
public:
	typedef ParallelMath<TMath> Math;

	typedef typename Math::Float MFloat;
	typedef typename Math::Int16 MInt16;

	MInt16 m_endPoint[2][TVectorSize];
	int m_prec;
	float m_maxValue;
	MFloat m_origin[TVectorSize];
	MFloat m_axis[TVectorSize];

	void Init(MInt16 endPoint[2][TVectorSize], int prec)
	{
		for (int ep = 0; ep < 2; ep++)
			for (int ch = 0; ch < TVectorSize; ch++)
				m_endPoint[ep][ch] = endPoint[ep][ch];

		m_prec = prec;
		m_maxValue = static_cast<float>((1 << m_prec) - 1);

		MFloat axis[TVectorSize];
		for (int ch = 0; ch < TVectorSize; ch++)
		{
			m_origin[ch] = Math::UInt16ToFloat(endPoint[0][ch]);

			axis[ch] = Math::UInt16ToFloat(endPoint[1][ch]) - m_origin[ch];
		}

		MFloat lenSquared = axis[0] * axis[0];
		for (int ch = 1; ch < TVectorSize; ch++)
			lenSquared = lenSquared + axis[ch] * axis[ch];

		typename Math::FloatCompFlag lenSquaredZero = Math::Equal(lenSquared, Math::MakeFloatZero());

		Math::ConditionalSet(lenSquared, lenSquaredZero, Math::MakeFloat(1.0f));

		for (int ch = 0; ch < TVectorSize; ch++)
			m_axis[ch] = (axis[ch] / lenSquared) * m_maxValue;
	}

	void Reconstruct(MInt16 index, MInt16* pixel)
	{
		MInt16 weightRcp = Math::MakeUInt16(0);
		if (m_prec == 2)
			weightRcp = Math::MakeUInt16(10923);
		else if (m_prec == 3)
			weightRcp = Math::MakeUInt16(4681);
		else if (m_prec == 4)
			weightRcp = Math::MakeUInt16(2184);

		MInt16 weight = Math::UnsignedRightShift(index * weightRcp + 256, 9);

		for (int ch = 0; ch < TVectorSize; ch++)
			pixel[ch] = Math::UnsignedRightShift(((Math::MakeUInt16(64) - weight) * m_endPoint[0][ch] + weight * m_endPoint[1][ch] + Math::MakeUInt16(32)), 6);
	}

	MInt16 SelectIndex(const MInt16* pixel)
	{
		MFloat diff[TVectorSize];
		for (int ch = 0; ch < TVectorSize; ch++)
			diff[ch] = Math::UInt16ToFloat(pixel[ch]) - m_origin[ch];

		MFloat dist = diff[0] * m_axis[0];
		for (int ch = 1; ch < TVectorSize; ch++)
			dist = dist + diff[ch] * m_axis[ch];

		return Math::FloatToUInt16(Math::Clamp(dist, 0.0f, m_maxValue));
	}
};

// Solve for a, b where v = a*t + b
// This allows endpoints to be mapped to where T=0 and T=1
// Least squares from totals:
// a = (tv - t*v/w)/(tt - t*t/w)
// b = (v - a*t)/w
template<int TMath, int TVectorSize>
class EndpointRefiner
{
public:
	typedef ParallelMath<TMath> Math;

	typedef typename Math::Float MFloat;
	typedef typename Math::Int16 MInt16;

	MFloat m_tv[TVectorSize];
	MFloat m_v[TVectorSize];
	MFloat m_tt;
	MFloat m_t;
	MFloat m_w;

	float m_maxIndex;

	void Init(int indexBits)
	{
		for (int ch = 0; ch < TVectorSize; ch++)
		{
			m_tv[ch] = Math::MakeFloatZero();
			m_v[ch] = Math::MakeFloatZero();
		}
		m_tt = Math::MakeFloatZero();
		m_t = Math::MakeFloatZero();
		m_w = Math::MakeFloatZero();

		m_maxIndex = static_cast<float>((1 << indexBits) - 1);
	}

	void Contribute(const MInt16* pixel, MInt16 index, MFloat weight)
	{
		MFloat v[TVectorSize];

		for (int ch = 0; ch < TVectorSize; ch++)
			v[ch] = Math::UInt16ToFloat(pixel[ch]);

		MFloat t = Math::UInt16ToFloat(index) / m_maxIndex;

		for (int ch = 0; ch < TVectorSize; ch++)
		{
			m_tv[ch] = m_tv[ch] + weight * t * v[ch];
			m_v[ch] = m_v[ch] + weight * v[ch];
		}
		m_tt = m_tt + weight * t * t;
		m_t = m_t + weight * t;
		m_w = m_w + weight;
	}

	void GetRefinedEndpoints(MInt16 endPoint[2][TVectorSize])
	{
		// a = (tv - t*v/w)/(tt - t*t/w)
		// b = (v - a*t)/w
		typename Math::FloatCompFlag wZero = Math::Equal(m_w, Math::MakeFloatZero());

		MFloat w = Math::Select(wZero, Math::MakeFloat(1.0f), m_w);

		MFloat adenom = (m_tt - m_t * m_t / w);

		typename Math::FloatCompFlag adenomZero = Math::Equal(adenom, Math::MakeFloatZero());
		Math::ConditionalSet(adenom, adenomZero, Math::MakeFloat(1.0f));

		for (int ch = 0; ch < TVectorSize; ch++)
		{
			/*
			if (adenom == 0.0)
				p1 = p2 = er.v / er.w;
			else
			{
				float4 a = (er.tv - er.t*er.v / er.w) / adenom;
				float4 b = (er.v - a * er.t) / er.w;
				p1 = b;
				p2 = a + b;
			}
			*/

			MFloat a = (m_tv[ch] - m_t * m_v[ch] / w) / adenom;
			MFloat b = (m_v[ch] - a * m_t) / w;

			MFloat p1 = Math::Clamp(b, 0.0f, 255.0f);
			MFloat p2 = Math::Clamp(a + b, 0.0f, 255.0f);

			Math::ConditionalSet(p1, adenomZero, (m_v[ch] / w));
			Math::ConditionalSet(p2, adenomZero, p1);

			endPoint[0][ch] = Math::FloatToUInt16(p1);
			endPoint[1][ch] = Math::FloatToUInt16(p2);
		}
	}
};

template<int TMath>
class BC7Computer
{
public:
	static const int NumTweakRounds = 4;
	static const int NumRefineRounds = 2;

	typedef ParallelMath<TMath> Math;
	typedef BC7EndpointSelectorRGB<TMath> EndpointSelectorRGB;
	typedef BC7EndpointSelectorRGBA<TMath> EndpointSelectorRGBA;

	typedef typename Math::Int16 MInt16;
	typedef typename Math::Int32 MInt32;
	typedef typename Math::Float MFloat;

	struct WorkInfo
	{
		MInt16 m_mode;
		MFloat m_error;
		MInt16 m_ep[3][2][4];
		MInt16 m_indexes[16];
		MInt16 m_indexes2[16];

		union
		{
			struct
			{
				MInt16 m_partition;
			};
			struct
			{
				MInt16 m_indexSelector;
				MInt16 m_rotation;
			};
		};
	};

	static void TweakAlpha(const MInt16 original[2], int tweak, int bits, MInt16 result[2])
	{
		float tf[2];
		ComputeTweakFactors(tweak, bits, tf);

		MFloat base = Math::UInt16ToFloat(original[0]);
		MFloat offs = Math::UInt16ToFloat(original[1]) - base;

		result[0] = Math::FloatToUInt16(Math::Clamp(base + offs * tf[0], 0.0f, 255.0f));
		result[1] = Math::FloatToUInt16(Math::Clamp(base + offs * tf[1], 0.0f, 255.0f));
	}

	static void UnpackInputBlock(const InputBlock& inputBlock, MInt16 pixels[16][4])
	{
		for (int px = 0; px < 16; px++)
			for (int ch = 0; ch < 4; ch++)
				Math::UnpackChannel(inputBlock.m_pixels, ch, pixels[px][ch]);
	}

	static void Quantize(MInt16* color, int bits, int channels)
	{
		float maxColor = static_cast<float>((1 << bits) - 1);

		for (int i = 0; i < channels; i++)
			color[i] = Math::FloatToUInt16(Math::Clamp(Math::UInt16ToFloat(color[i]) * Math::MakeFloat(1.0f / 255.0f) * maxColor, 0.f, 255.f));
	}

	static void QuantizeP(MInt16* color, int bits, uint16_t p, int channels)
	{
		uint16_t pShift = static_cast<uint16_t>(1 << (7 - bits));
		MInt16 pShiftV = Math::MakeUInt16(pShift);

		float maxColorF = static_cast<float>(255 - (1 << (7 - bits)));

		float maxQuantized = static_cast<float>((1 << bits) - 1);

		for (int ch = 0; ch < channels; ch++)
		{
			MInt16 clr = color[ch];
			if (p)
				clr = Math::Max(clr, pShiftV) - pShiftV;

			MFloat rerangedColor = Math::UInt16ToFloat(clr) * maxQuantized / maxColorF;

			clr = Math::FloatToUInt16(Math::Clamp(rerangedColor, 0.0f, maxQuantized)) << 1;
			if (p)
				clr = clr | Math::MakeUInt16(1);

			color[ch] = clr;
		}
	}

	static void Unquantize(MInt16* color, int bits, int channels)
	{
		for (int ch = 0; ch < channels; ch++)
		{
			MInt16 clr = color[ch];
			clr = clr << (8 - bits);
			color[ch] = clr | Math::UnsignedRightShift(clr, bits);
		}
	}

	static void CompressEndpoints0(MInt16 ep[2][4], uint16_t p[2])
	{
		for (int j = 0; j < 2; j++)
		{
			QuantizeP(ep[j], 4, p[j], 3);
			Unquantize(ep[j], 5, 3);
			ep[j][3] = Math::MakeUInt16(255);
		}
	}

	static void CompressEndpoints1(MInt16 ep[2][4], uint16_t p)
	{
		for (int j = 0; j < 2; j++)
		{
			QuantizeP(ep[j], 6, p, 3);
			Unquantize(ep[j], 7, 3);
			ep[j][3] = Math::MakeUInt16(255);
		}
	}

	static void CompressEndpoints2(MInt16 ep[2][4])
	{
		for (int j = 0; j < 2; j++)
		{
			Quantize(ep[j], 5, 3);
			Unquantize(ep[j], 5, 3);
			ep[j][3] = Math::MakeUInt16(255);
		}
	}

	static void CompressEndpoints3(MInt16 ep[2][4], uint16_t p[2])
	{
		for (int j = 0; j < 2; j++)
			QuantizeP(ep[j], 7, p[j], 3);
	}

	static void CompressEndpoints4(MInt16 epRGB[2][3], MInt16 epA[2])
	{
		for (int j = 0; j < 2; j++)
		{
			Quantize(epRGB[j], 5, 3);
			Unquantize(epRGB[j], 5, 3);

			Quantize(epA + j, 6, 1);
			Unquantize(epA + j, 6, 1);
		}
	}

	static void CompressEndpoints5(MInt16 epRGB[2][3], MInt16 epA[2])
	{
		for (int j = 0; j < 2; j++)
		{
			Quantize(epRGB[j], 7, 3);
			Unquantize(epRGB[j], 7, 3);
		}

		// Alpha is full precision
	}

	static void CompressEndpoints6(MInt16 ep[2][4], uint16_t p[2])
	{
		for (int j = 0; j < 2; j++)
			QuantizeP(ep[j], 7, p[j], 4);
	}

	static void CompressEndpoints7(MInt16 ep[2][4], uint16_t p[2])
	{
		for (int j = 0; j < 2; j++)
		{
			QuantizeP(ep[j], 5, p[j], 4);
			Unquantize(ep[j], 6, 4);
		}
	}

	static MFloat ComputeError(const MInt16 reconstructed[4], const MInt16 original[4])
	{
		MFloat error = Math::MakeFloatZero();
		for (int ch = 0; ch < 4; ch++)
			error = error + Math::UInt16ToFloat(Math::SqDiff(reconstructed[ch], original[ch]));

		return error;
	}

	static void TrySinglePlane(const MInt16 pixels[16][4], WorkInfo& work)
	{
		for (uint16_t mode = 0; mode <= 7; mode++)
		{
			if (mode == 4 || mode == 5)
				continue;

			MInt16 rgbAdjustedPixels[16][4];
			for (int px = 0; px < 16; px++)
			{
				for (int ch = 0; ch < 3; ch++)
					rgbAdjustedPixels[px][ch] = pixels[px][ch];

				if (s_modes[mode].m_alphaMode == AlphaMode_None)
					rgbAdjustedPixels[px][3] = Math::MakeUInt16(255);
				else
					rgbAdjustedPixels[px][3] = pixels[px][3];
			}

			unsigned int numPartitions = 1 << s_modes[mode].m_partitionBits;
			int numSubsets = s_modes[mode].m_numSubsets;
			int indexPrec = s_modes[mode].m_indexBits;

			int parityBitMax = 1;
			if (s_modes[mode].m_pBitMode == PBitMode_PerEndpoint)
				parityBitMax = 4;
			else if (s_modes[mode].m_pBitMode == PBitMode_PerSubset)
				parityBitMax = 2;

			for (uint16_t partition = 0; partition < numPartitions; partition++)
			{
				EndpointSelectorRGBA epSelectors[3];

				for (int epPass = 0; epPass < EndpointSelectorRGBA::NumPasses; epPass++)
				{
					for (int subset = 0; subset < numSubsets; subset++)
						epSelectors[subset].InitPass(epPass);

					for (int px = 0; px < 16; px++)
					{
						int subset = 0;
						if (numSubsets == 2)
							subset = (g_partitionMap[partition] >> px) & 1;
						else if (numSubsets == 3)
							subset = g_partitionMap2[partition] >> (px * 2) & 3;

						assert(subset < 3);

						epSelectors[subset].Contribute(epPass, rgbAdjustedPixels[px], Math::MakeFloat(1.0f));
					}
				}

				UnfinishedEndpoints<TMath, 4> unfinishedEPs[3];
				for (int subset = 0; subset < numSubsets; subset++)
					unfinishedEPs[subset] = epSelectors[subset].GetEndpoints();

				MInt16 bestIndexes[16];
				MInt16 bestEP[3][2][4];
				MFloat bestSubsetError[3] = { Math::MakeFloat(FLT_MAX), Math::MakeFloat(FLT_MAX), Math::MakeFloat(FLT_MAX) };

				for (int px = 0; px < 16; px++)
					bestIndexes[px] = Math::MakeUInt16(0);

				for (int tweak = 0; tweak < NumTweakRounds; tweak++)
				{
					MInt16 baseEP[3][2][4];

					for (int subset = 0; subset < numSubsets; subset++)
						unfinishedEPs[subset].Finish(tweak, indexPrec, baseEP[subset][0], baseEP[subset][1]);

					for (int pIter = 0; pIter < parityBitMax; pIter++)
					{
						uint16_t p[2];
						p[0] = (pIter & 1);
						p[1] = ((pIter >> 1) & 1);

						MInt16 ep[3][2][4];

						for (int subset = 0; subset < numSubsets; subset++)
							for (int epi = 0; epi < 2; epi++)
								for (int ch = 0; ch < 4; ch++)
									ep[subset][epi][ch] = baseEP[subset][epi][ch];

						for (int refine = 0; refine < NumRefineRounds; refine++)
						{
							switch (mode)
							{
							case 0:
								for (int subset = 0; subset < 3; subset++)
									CompressEndpoints0(ep[subset], p);
								break;
							case 1:
								for (int subset = 0; subset < 2; subset++)
									CompressEndpoints1(ep[subset], p[0]);
								break;
							case 2:
								for (int subset = 0; subset < 3; subset++)
									CompressEndpoints2(ep[subset]);
								break;
							case 3:
								for (int subset = 0; subset < 2; subset++)
									CompressEndpoints3(ep[subset], p);
								break;
							case 6:
								CompressEndpoints6(ep[0], p);
								break;
							case 7:
								for (int subset = 0; subset < 2; subset++)
									CompressEndpoints7(ep[subset], p);
								break;
							default:
								assert(false);
								break;
							};

							IndexSelector<TMath, 4> indexSelectors[3];

							for (int subset = 0; subset < numSubsets; subset++)
								indexSelectors[subset].Init(ep[subset], indexPrec);

							EndpointRefiner<TMath, 4> epRefiners[3];

							for (int subset = 0; subset < numSubsets; subset++)
								epRefiners[subset].Init(indexPrec);

							MFloat subsetError[3] = { Math::MakeFloatZero(), Math::MakeFloatZero(), Math::MakeFloatZero() };

							MInt16 indexes[16];

							for (int px = 0; px < 16; px++)
							{
								int subset = 0;
								if (numSubsets == 2)
									subset = (g_partitionMap[partition] >> px) & 1;
								else if (numSubsets == 3)
									subset = g_partitionMap2[partition] >> (px * 2) & 3;

								assert(subset < 3);

								MInt16 index = indexSelectors[subset].SelectIndex(rgbAdjustedPixels[px]);

								epRefiners[subset].Contribute(rgbAdjustedPixels[px], index, Math::MakeFloat(1.0f));

								MInt16 reconstructed[4];

								indexSelectors[subset].Reconstruct(index, reconstructed);

								subsetError[subset] = subsetError[subset] + ComputeError(reconstructed, pixels[px]);

								indexes[px] = index;
							}

							typename Math::FloatCompFlag subsetErrorBetter[3];
							typename Math::Int16CompFlag subsetErrorBetter16[3];

							bool anyImprovements = false;
							for (int subset = 0; subset < numSubsets; subset++)
							{
								subsetErrorBetter[subset] = Math::Less(subsetError[subset], bestSubsetError[subset]);
								subsetErrorBetter16[subset] = Math::FloatFlagToInt16(subsetErrorBetter[subset]);

								if (Math::AnySet(subsetErrorBetter16[subset]))
								{
									Math::ConditionalSet(bestSubsetError[subset], subsetErrorBetter[subset], subsetError[subset]);
									for (int epi = 0; epi < 2; epi++)
										for (int ch = 0; ch < 4; ch++)
											Math::ConditionalSet(bestEP[subset][epi][ch], subsetErrorBetter16[subset], ep[subset][epi][ch]);

									anyImprovements = true;
								}
							}

							if (anyImprovements)
							{
								for (int px = 0; px < 16; px++)
								{
									int subset = 0;
									if (numSubsets == 2)
										subset = (g_partitionMap[partition] >> px) & 1;
									else if (numSubsets == 3)
										subset = g_partitionMap2[partition] >> (px * 2) & 3;

									Math::ConditionalSet(bestIndexes[px], subsetErrorBetter16[subset], indexes[px]);
								}
							}

							if (refine != NumRefineRounds - 1)
							{
								for (int subset = 0; subset < numSubsets; subset++)
									epRefiners[subset].GetRefinedEndpoints(ep[subset]);
							}
						} // refine
					} // p
				} // tweak

				MFloat totalError = bestSubsetError[0];
				for (int subset = 1; subset < numSubsets; subset++)
					totalError = totalError + bestSubsetError[subset];

				typename Math::FloatCompFlag errorBetter = Math::Less(totalError, work.m_error);
				typename Math::Int16CompFlag errorBetter16 = Math::FloatFlagToInt16(errorBetter);

				if (Math::AnySet(errorBetter16))
				{
					work.m_error = Math::Min(totalError, work.m_error);
					Math::ConditionalSet(work.m_mode, errorBetter16, Math::MakeUInt16(mode));
					Math::ConditionalSet(work.m_partition, errorBetter16, Math::MakeUInt16(partition));

					for (int px = 0; px < 16; px++)
						Math::ConditionalSet(work.m_indexes[px], errorBetter16, bestIndexes[px]);

					for (int subset = 0; subset < numSubsets; subset++)
						for (int epi = 0; epi < 2; epi++)
							for (int ch = 0; ch < 4; ch++)
								Math::ConditionalSet(work.m_ep[subset][epi][ch], errorBetter16, bestEP[subset][epi][ch]);
				}
			}
		}
	}

	static void TryDualPlane(const MInt16 pixels[16][4], WorkInfo& work)
	{
		for (uint16_t mode = 4; mode <= 5; mode++)
		{
			for (uint16_t rotation = 0; rotation < 4; rotation++)
			{
				int alphaChannel = (rotation + 3) & 3;
				int redChannel = (rotation == 1) ? 3 : 0;
				int greenChannel = (rotation == 2) ? 3 : 1;
				int blueChannel = (rotation == 3) ? 3 : 2;

				MInt16 rotatedRGB[16][3];

				for (int px = 0; px < 16; px++)
				{
					rotatedRGB[px][0] = pixels[px][redChannel];
					rotatedRGB[px][1] = pixels[px][greenChannel];
					rotatedRGB[px][2] = pixels[px][blueChannel];
				}

				uint16_t maxIndexSelector = (mode == 4) ? 2 : 1;

				for (uint16_t indexSelector = 0; indexSelector < maxIndexSelector; indexSelector++)
				{
					EndpointSelectorRGB rgbSelector;

					for (int epPass = 0; epPass < EndpointSelectorRGB::NumPasses; epPass++)
					{
						rgbSelector.InitPass(epPass);
						for (int px = 0; px < 16; px++)
							rgbSelector.Contribute(epPass, rotatedRGB[px], Math::MakeFloat(1.0f));
					}

					MInt16 alphaRange[2];

					alphaRange[0] = alphaRange[1] = pixels[0][alphaChannel];
					for (int px = 1; px < 16; px++)
					{
						alphaRange[0] = Math::Min(pixels[px][alphaChannel], alphaRange[0]);
						alphaRange[1] = Math::Max(pixels[px][alphaChannel], alphaRange[1]);
					}

					int rgbPrec = 0;
					int alphaPrec = 0;

					if (mode == 4)
					{
						rgbPrec = indexSelector ? 3 : 2;
						alphaPrec = indexSelector ? 2 : 3;
					}
					else
						rgbPrec = alphaPrec = 2;

					UnfinishedEndpoints<TMath, 3> unfinishedRGB = rgbSelector.GetEndpoints();

					MFloat bestRGBError = Math::MakeFloat(FLT_MAX);
					MFloat bestAlphaError = Math::MakeFloat(FLT_MAX);

					MInt16 bestRGBIndexes[16];
					MInt16 bestAlphaIndexes[16];
					MInt16 bestEP[2][4];

					for (int px = 0; px < 16; px++)
						bestRGBIndexes[px] = bestAlphaIndexes[px] = Math::MakeUInt16(0);

					for (int tweak = 0; tweak < NumTweakRounds; tweak++)
					{
						MInt16 rgbEP[2][3];
						MInt16 alphaEP[2];

						unfinishedRGB.Finish(tweak, rgbPrec, rgbEP[0], rgbEP[1]);

						TweakAlpha(alphaRange, tweak, alphaPrec, alphaEP);

						for (int refine = 0; refine < NumRefineRounds; refine++)
						{
							if (mode == 4)
								CompressEndpoints4(rgbEP, alphaEP);
							else
								CompressEndpoints5(rgbEP, alphaEP);

							IndexSelector<TMath, 1> alphaSelector;
							IndexSelector<TMath, 3> rgbSelector;

							{
								MInt16 alphaEPTemp[2][1] = { { alphaEP[0] }, { alphaEP[1] } };
								alphaSelector.Init(alphaEPTemp, alphaPrec);
							}
							rgbSelector.Init(rgbEP, rgbPrec);

							EndpointRefiner<TMath, 3> rgbRefiner;
							EndpointRefiner<TMath, 1> alphaRefiner;

							rgbRefiner.Init(rgbPrec);
							alphaRefiner.Init(alphaPrec);

							MFloat errorRGB = Math::MakeFloatZero();
							MFloat errorA = Math::MakeFloatZero();

							MInt16 rgbIndexes[16];
							MInt16 alphaIndexes[16];

							for (int px = 0; px < 16; px++)
							{
								MInt16 rgbIndex = rgbSelector.SelectIndex(rotatedRGB[px]);
								MInt16 alphaIndex = alphaSelector.SelectIndex(pixels[px] + alphaChannel);

								rgbRefiner.Contribute(rotatedRGB[px], rgbIndex, Math::MakeFloat(1.0f));
								alphaRefiner.Contribute(pixels[px] + alphaChannel, alphaIndex, Math::MakeFloat(1.0f));

								MInt16 reconstructedRGB[3];
								MInt16 reconstructedAlpha[1];

								rgbSelector.Reconstruct(rgbIndex, reconstructedRGB);
								alphaSelector.Reconstruct(alphaIndex, reconstructedAlpha);

								MInt16 reconstructedRGBA[4];
								reconstructedRGBA[redChannel] = reconstructedRGB[0];
								reconstructedRGBA[greenChannel] = reconstructedRGB[1];
								reconstructedRGBA[blueChannel] = reconstructedRGB[2];
								reconstructedRGBA[alphaChannel] = pixels[px][alphaChannel];

								errorRGB = errorRGB + ComputeError(reconstructedRGBA, pixels[px]);

								reconstructedRGBA[redChannel] = pixels[px][redChannel];
								reconstructedRGBA[greenChannel] = pixels[px][greenChannel];
								reconstructedRGBA[blueChannel] = pixels[px][blueChannel];
								reconstructedRGBA[alphaChannel] = reconstructedAlpha[0];

								errorA = errorA + ComputeError(reconstructedRGBA, pixels[px]);

								rgbIndexes[px] = rgbIndex;
								alphaIndexes[px] = alphaIndex;
							}

							typename Math::FloatCompFlag rgbBetter = Math::Less(errorRGB, bestRGBError);
							typename Math::FloatCompFlag alphaBetter = Math::Less(errorA, bestAlphaError);

							typename Math::Int16CompFlag rgbBetterInt16 = Math::FloatFlagToInt16(rgbBetter);
							typename Math::Int16CompFlag alphaBetterInt16 = Math::FloatFlagToInt16(alphaBetter);

							bestRGBError = Math::Min(errorRGB, bestRGBError);
							bestAlphaError = Math::Min(errorA, bestAlphaError);

							for (int px = 0; px < 16; px++)
							{
								Math::ConditionalSet(bestRGBIndexes[px], rgbBetterInt16, rgbIndexes[px]);
								Math::ConditionalSet(bestAlphaIndexes[px], alphaBetterInt16, alphaIndexes[px]);
							}

							for (int ep = 0; ep < 2; ep++)
							{
								for (int ch = 0; ch < 3; ch++)
									Math::ConditionalSet(bestEP[ep][ch], rgbBetterInt16, rgbEP[ep][ch]);
								Math::ConditionalSet(bestEP[ep][3], alphaBetterInt16, alphaEP[ep]);
							}

							if (refine != NumRefineRounds - 1)
							{
								rgbRefiner.GetRefinedEndpoints(rgbEP);

								MInt16 alphaEPTemp[2][1];
								alphaRefiner.GetRefinedEndpoints(alphaEPTemp);

								for (int i = 0; i < 2; i++)
									alphaEP[i] = alphaEPTemp[i][0];
							}
						}	// refine
					} // tweak

					MFloat combinedError = bestRGBError + bestAlphaError;

					typename Math::FloatCompFlag errorBetter = Math::Less(combinedError, work.m_error);
					typename Math::Int16CompFlag errorBetter16 = Math::FloatFlagToInt16(errorBetter);

					work.m_error = Math::Min(combinedError, work.m_error);

					Math::ConditionalSet(work.m_mode, errorBetter16, Math::MakeUInt16(mode));
					Math::ConditionalSet(work.m_rotation, errorBetter16, Math::MakeUInt16(rotation));
					Math::ConditionalSet(work.m_indexSelector, errorBetter16, Math::MakeUInt16(indexSelector));

					for (int px = 0; px < 16; px++)
					{
						Math::ConditionalSet(work.m_indexes[px], errorBetter16, indexSelector ? bestAlphaIndexes[px] : bestRGBIndexes[px]);
						Math::ConditionalSet(work.m_indexes2[px], errorBetter16, indexSelector ? bestRGBIndexes[px] : bestAlphaIndexes[px]);
					}

					for (int ep = 0; ep < 2; ep++)
						for (int ch = 0; ch < 4; ch++)
							Math::ConditionalSet(work.m_ep[0][ep][ch], errorBetter16, bestEP[ep][ch]);
				}
			}
		}
	}

	template<class T>
	static void Swap(T& a, T& b)
	{
		T temp = a;
		a = b;
		b = temp;
	}

	static void Pack(const InputBlock* inputs, uint8_t* packedBlocks)
	{
		MInt16 pixels[16][4];

		for (int px = 0; px < 16; px++)
		{
			MInt32 packedPx;
			Math::ReadPackedInputs(inputs, px, packedPx);

			for (int ch = 0; ch < 4; ch++)
				Math::UnpackChannel(packedPx, ch, pixels[px][ch]);
		}

		WorkInfo work;
		memset(&work, 0, sizeof(work));

		work.m_error = Math::MakeFloat(FLT_MAX);

		TryDualPlane(pixels, work);
		TrySinglePlane(pixels, work);

		for (int block = 0; block < Math::ParallelSize; block++)
		{
			typename Math::PackingVector pv;
			pv.Init();

			uint16_t mode = Math::ExtractUInt16(work.m_mode, block);
			uint16_t partition = Math::ExtractUInt16(work.m_partition, block);
			uint16_t indexSelector = Math::ExtractUInt16(work.m_indexSelector, block);

			const BC7ModeInfo& modeInfo = s_modes[mode];

			uint16_t indexes[16];
			uint16_t indexes2[16];
			uint16_t endPoints[3][2][4];

			for (int i = 0; i < 16; i++)
			{
				indexes[i] = Math::ExtractUInt16(work.m_indexes[i], block);
				if (modeInfo.m_alphaMode == AlphaMode_Separate)
					indexes2[i] = Math::ExtractUInt16(work.m_indexes2[i], block);
			}

			for (int subset = 0; subset < 3; subset++)
			{
				for (int ep = 0; ep < 2; ep++)
				{
					for (int ch = 0; ch < 4; ch++)
						endPoints[subset][ep][ch] = Math::ExtractUInt16(work.m_ep[subset][ep][ch], block);
				}
			}

			int fixups[3] = { 0, 0, 0 };

			if (modeInfo.m_alphaMode == AlphaMode_Separate)
			{
				bool flipRGB = ((indexes[0] & (1 << (modeInfo.m_indexBits - 1))) != 0);
				bool flipAlpha = ((indexes2[0] & (1 << (modeInfo.m_alphaIndexBits - 1))) != 0);

				if (flipRGB)
				{
					uint16_t highIndex = (1 << modeInfo.m_indexBits) - 1;
					for (int px = 0; px < 16; px++)
						indexes[px] = highIndex - indexes[px];
				}

				if (flipAlpha)
				{
					uint16_t highIndex = (1 << modeInfo.m_alphaIndexBits) - 1;
					for (int px = 0; px < 16; px++)
						indexes2[px] = highIndex - indexes2[px];
				}

				if (indexSelector)
					Swap(flipRGB, flipAlpha);

				if (flipRGB)
				{
					for (int ch = 0; ch < 3; ch++)
						Swap(endPoints[0][0][ch], endPoints[0][1][ch]);
				}
				if (flipAlpha)
					Swap(endPoints[0][0][3], endPoints[0][1][3]);

			}
			else
			{
				if (modeInfo.m_numSubsets == 2)
					fixups[1] = g_fixupIndexes2[partition];
				else if (modeInfo.m_numSubsets == 3)
				{
					fixups[1] = g_fixupIndexes3[partition][0];
					fixups[2] = g_fixupIndexes3[partition][1];
				}

				bool flip[3] = { false, false, false };
				for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
					flip[subset] = ((indexes[fixups[subset]] & (1 << (modeInfo.m_indexBits - 1))) != 0);

				if (flip[0] || flip[1] || flip[2])
				{
					uint16_t highIndex = (1 << modeInfo.m_indexBits) - 1;
					for (int px = 0; px < 16; px++)
					{
						int subset = 0;
						if (modeInfo.m_numSubsets == 2)
							subset = (g_partitionMap[partition] >> px) & 1;
						else if (modeInfo.m_numSubsets == 3)
							subset = (g_partitionMap2[partition] >> (px * 2)) & 3;

						if (flip[subset])
							indexes[px] = highIndex - indexes[px];
					}

					int maxCH = (modeInfo.m_alphaMode == AlphaMode_Combined) ? 4 : 3;
					for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
					{
						if (flip[subset])
							for (int ch = 0; ch < maxCH; ch++)
								Swap(endPoints[subset][0][ch], endPoints[subset][1][ch]);
					}
				}
			}

			pv.Pack(static_cast<uint8_t>(1 << mode), mode + 1);

			if (modeInfo.m_partitionBits)
				pv.Pack(partition, modeInfo.m_partitionBits);

			if (modeInfo.m_alphaMode == AlphaMode_Separate)
			{
				uint16_t rotation = Math::ExtractUInt16(work.m_rotation, block);
				pv.Pack(rotation, 2);
			}

			if (modeInfo.m_hasIndexSelector)
				pv.Pack(indexSelector, 1);

			// Encode RGB
			for (int ch = 0; ch < 3; ch++)
			{
				for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
				{
					for (int ep = 0; ep < 2; ep++)
					{
						uint16_t epPart = endPoints[subset][ep][ch];
						epPart >>= (8 - modeInfo.m_rgbBits);

						pv.Pack(epPart, modeInfo.m_rgbBits);
					}
				}
			}

			// Encode alpha
			if (modeInfo.m_alphaMode != AlphaMode_None)
			{
				for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
				{
					for (int ep = 0; ep < 2; ep++)
					{
						uint16_t epPart = endPoints[subset][ep][3];
						epPart >>= (8 - modeInfo.m_alphaBits);

						pv.Pack(epPart, modeInfo.m_alphaBits);
					}
				}
			}

			// Encode parity bits
			if (modeInfo.m_pBitMode == PBitMode_PerSubset)
			{
				for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
				{
					uint16_t epPart = endPoints[subset][0][0];
					epPart >>= (7 - modeInfo.m_rgbBits);
					epPart &= 1;

					pv.Pack(epPart, 1);
				}
			}
			else if (modeInfo.m_pBitMode == PBitMode_PerEndpoint)
			{
				for (int subset = 0; subset < modeInfo.m_numSubsets; subset++)
				{
					for (int ep = 0; ep < 2; ep++)
					{
						uint16_t epPart = endPoints[subset][ep][0];
						epPart >>= (7 - modeInfo.m_rgbBits);
						epPart &= 1;

						pv.Pack(epPart, 1);
					}
				}
			}

			// Encode indexes
			for (int px = 0; px < 16; px++)
			{
				int bits = modeInfo.m_indexBits;
				if ((px == 0) || (px == fixups[1]) || (px == fixups[2]))
					bits--;

				pv.Pack(indexes[px], bits);
			}

			// Encode secondary indexes
			if (modeInfo.m_alphaMode == AlphaMode_Separate)
			{
				for (int px = 0; px < 16; px++)
				{
					int bits = modeInfo.m_alphaIndexBits;
					if (px == 0)
						bits--;

					pv.Pack(indexes2[px], bits);
				}
			}

			pv.Flush(packedBlocks);

			packedBlocks += 16;
		}
	}
};

enum AlphaMode
{
	AlphaMode_Combined,
	AlphaMode_Separate,
	AlphaMode_None,
};

enum PBitMode
{
	PBitMode_PerEndpoint,
	PBitMode_PerSubset,
	PBitMode_None
};

struct BC7ModeInfo
{
	PBitMode m_pBitMode;
	AlphaMode m_alphaMode;
	int m_rgbBits;
	int m_alphaBits;
	int m_partitionBits;
	int m_numSubsets;
	int m_indexBits;
	int m_alphaIndexBits;
	bool m_hasIndexSelector;
};

BC7ModeInfo s_modes[] =
{
	{ PBitMode_PerEndpoint, AlphaMode_None, 4, 0, 4, 3, 3, 0, false },     // 0
	{ PBitMode_PerSubset, AlphaMode_None, 6, 0, 6, 2, 3, 0, false },       // 1
	{ PBitMode_None, AlphaMode_None, 5, 0, 6, 3, 2, 0, false },            // 2
	{ PBitMode_PerEndpoint, AlphaMode_None, 7, 0, 6, 2, 2, 0, false },     // 3 (Mode reference has an error, P-bit is really per-endpoint)

	{ PBitMode_None, AlphaMode_Separate, 5, 6, 0, 1, 2, 3, true },         // 4
	{ PBitMode_None, AlphaMode_Separate, 7, 8, 0, 1, 2, 2, false },        // 5
	{ PBitMode_PerEndpoint, AlphaMode_Combined, 7, 7, 0, 1, 4, 0, false }, // 6
	{ PBitMode_PerEndpoint, AlphaMode_Combined, 5, 5, 6, 2, 2, 0, false }  // 7
};

struct OutputBlock
{
	uint8_t m_result;
};

int main(int argc, const char **argv)
{
	int w, h, channels;

	stbi_uc* img = stbi_load(argv[1], &w, &h, &channels, 4);

	static const int mathType = MathTypes_SSE2;

	int cols = w / 4;
	int rows = h / 4;

	InputBlock *inputBlocks = new InputBlock[cols * rows + ParallelMath<mathType>::ParallelSize];
	uint8_t *packedBlocks = new uint8_t[w * h + ParallelMath<mathType>::ParallelSize * 16];
	uint8_t *outLocation = packedBlocks;

	memset(packedBlocks, 0, w * h);

	typedef BC7Computer<mathType> BC7Kernel_t;

	int numBlocks = w * h / 16;

	int blockIndex = 0;
	for (int y = 0; y < h; y += 4)
	{
		for (int x = 0; x < w; x += 4)
		{
			InputBlock& currentBlock = inputBlocks[blockIndex++];

			int offset = 0;
			for (int suby = 0; suby < 4; suby++)
			{
				const stbi_uc* rowStart = img + ((y + suby) * w + x) * 4;
				for (int subx = 0; subx < 4; subx++)
				{
					const stbi_uc* colStart = rowStart + subx * 4;
					int32_t packedPixel = 0;
					for (int i = 0; i < 4; i++)
						packedPixel |= static_cast<int>(colStart[i] << (i * 8));

					currentBlock.m_pixels[offset++] = packedPixel;
				}
			}
		}
	}

#if 1
	concurrency::parallel_for<int>(0, numBlocks, ParallelMath<mathType>::ParallelSize, [inputBlocks, packedBlocks](int i)
	{
		BC7Computer<mathType>::Pack(inputBlocks + i, packedBlocks + i * 16);
	});
#else
	for (int i = 0; i < numBlocks; i += ParallelMath<mathType>::ParallelSize)
	{
		BC7Computer<mathType>::Pack(inputBlocks + i, packedBlocks + i * 16);
	}
#endif

	DirectX::Image image;
	image.format = DXGI_FORMAT_BC7_UNORM;
	image.width = w;
	image.height = h;
	image.rowPitch = (w / 4) * 16;
	image.slicePitch = w * h;
	image.pixels = packedBlocks;

	size_t outLen = strlen(argv[2]);
	wchar_t* outPathW = new wchar_t[outLen + 1];
	outPathW[outLen] = 0;
	for (size_t i = 0; i < outLen; i++)
		outPathW[i] = static_cast<wchar_t>(argv[2][i]);

	DirectX::SaveToDDSFile(image, 0, outPathW);

	delete[] outPathW;
	delete[] packedBlocks;

	stbi_image_free(img);
}
