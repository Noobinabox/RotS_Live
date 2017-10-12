/* MTRand.h 
 * This code is based on the Mersenne twister algorithm by Makoto Matsumoto.
 */

#ifndef MTRAND_H
#define MTRAND_H

namespace Math
{
	class MTRand
	{
	private:
		static const unsigned int magic[2];
		enum MTRandPrivateConstants
		{
			StateLength = 624,        // length of the state vector, aka 'N'
			Period = 397,             // period parameter, aka 'M'
			DefaultSeed = 2147438663, // The Default seed, this is a prime number
		};
		
	public:
		static const unsigned int SEED = 2388106741;  // The public seed.  This is a prime number.
		
	public:
		MTRand(unsigned int seed_value = DefaultSeed);
		virtual ~MTRand() { }
		void seed(unsigned int seed_value);
		unsigned int rand();
		
		/* generates a random number on [0,1] */
		float rand_float();
		unsigned int rand_range(unsigned int range);
		unsigned int rand_range(unsigned int min, unsigned int max);
		float frand_range(float range);
		float frand_range(float min, float max);
		
	private:
		void initialize(unsigned int seed);
		void reload();
		unsigned int generate_next_value(int start_index, int end_index) const;
		void assign_value(int index, int basis_index, unsigned int value);
		
	private:
		unsigned int state[StateLength];
		unsigned int next_index;
	};
}

#endif
