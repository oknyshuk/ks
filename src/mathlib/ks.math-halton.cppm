// $Id$
// halton.h - classes, etc for generating numbers using the Halton pseudo-random sequence.  See
// http://halton-sequences.wikiverse.org/.
//
// what this function is useful for is any sort of sampling/integration problem where
// you want to solve it by random sampling. Each call the NextValue() generates
// a random number between 0 and 1, in an unclumped manner, so that the space can be more
// or less evenly sampled with a minimum number of samples.
//
// It is NOT useful for generating random numbers dynamically, since the outputs aren't
// particularly random.
//
// To generate multidimensional sample values (points in a plane, etc), use two
// HaltonSequenceGenerator_t's, with different (primes) bases.
// Copyright (c) 2026 oknyshuk

export module ks.math:halton;

import std;
import ks.core;
import ks.simd;
export import :vector;

export extern "C++" {

class HaltonSequenceGenerator_t
{
	int seed;
	int base;
	float fbase;											//< base as a float

public:
	HaltonSequenceGenerator_t(int base);					//< base MUST be prime, >=2

	float GetElement(int element);

	inline float NextValue(void)
	{
		return GetElement(seed++);
	}

};

class DirectionalSampler_t									//< pseudo-random sphere sampling
{
	HaltonSequenceGenerator_t zdot;
	HaltonSequenceGenerator_t vrot;
public:
	DirectionalSampler_t(void)
		: zdot(2),vrot(3)
	{
	}

	Vector NextValue(void)
	{
		float zvalue=zdot.NextValue();
		zvalue=2*zvalue-1.0;								// map from 0..1 to -1..1
		float phi=std::acos(zvalue);
		// now, generate a random rotation angle for x/y
		float theta=2.0*std::numbers::pi*vrot.NextValue();
		float sin_p=std::sin(phi);
		return Vector(std::cos(theta)*sin_p,
					  std::sin(theta)*sin_p,
					  zvalue);

	}
};

}
