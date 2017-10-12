#include "MTRand.h"

namespace Math
{
	const unsigned int MTRand::magic[2] = { 0, 0x9908b0df };

	//============================================================================
	MTRand::MTRand(unsigned int seed_value) : next_index(0)
	{
		seed(seed_value);
	}

	//============================================================================
	void MTRand::seed(unsigned int seed_value)
	{
		initialize(seed_value);
		reload();
	}

	//============================================================================
	void MTRand::initialize(unsigned int seed_value)
	{
		unsigned int* s = &state[0];
		unsigned int* r = s;

		*s++ = seed_value;

		for (unsigned int index = 1; index < StateLength; ++index)
		{
			*s++ = 1812433253 * (*r ^ (*r >> 30)) + index;
			++r;
		}
	}

	//============================================================================
	void MTRand::reload()
	{
		const unsigned int UPPER_MASK = 0x80000000;
		const unsigned int LOWER_MASK = 0x7fffffff;

		for (unsigned int index = 0; index < StateLength - Period; ++index)
		{
			unsigned int value = generate_next_value(index, index + 1);
			assign_value(index, index + Period, value);
		}

		for (unsigned int index = StateLength - Period; index < StateLength - 1; ++index)
		{
			unsigned int value = generate_next_value(index, index + 1);
			assign_value(index, index + Period - StateLength, value);
		}

		unsigned int value = generate_next_value(StateLength - 1, 0);
		assign_value(StateLength - 1, Period - 1, value);

		next_index = 0;
	}

	//============================================================================
	unsigned int MTRand::generate_next_value(int start_index, int end_index) const
	{
		const unsigned int UPPER_MASK = 0x80000000;
		const unsigned int LOWER_MASK = 0x7fffffff;

		unsigned int next_value = (state[start_index] & UPPER_MASK) | (state[end_index] & LOWER_MASK);
		return next_value;
	}

	//============================================================================
	void MTRand::assign_value(int index, int basis_index, unsigned int value)
	{
		unsigned int next_value = state[basis_index] ^ (value >> 1) ^ magic[value & 0x1];
		state[index] = next_value;
	}

	//============================================================================
	unsigned int MTRand::rand()
	{
		if (++next_index > StateLength)
			reload();

		unsigned int value = state[next_index];
		value ^= (value >> 11);
		value ^= (value << 7) & 0x9d2c5680;
		value ^= (value << 15) & 0xefc60000;
		value ^= (value >> 18);

		return value;
	}

	//============================================================================
	float MTRand::rand_float()
	{
		return (float)(rand() * 1.0f / 4294967295.0f);
	}

	//============================================================================
	unsigned int MTRand::rand_range(unsigned int range)
	{
		return rand_range(0, range);
	}

	//============================================================================
	unsigned int MTRand::rand_range(unsigned int min, unsigned int max)
	{
		unsigned int delta = max - min;
		unsigned int value = min + rand() % delta;
		return value;
	}

	//============================================================================
	float MTRand::frand_range(float range)
	{
		return frand_range(0.0f, range);
	}

	//============================================================================
	float MTRand::frand_range(float min, float max)
	{
		return min + (rand_float() * (max - min));
	}
}
