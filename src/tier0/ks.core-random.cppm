// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.random;

import std;

export extern "C++" {

inline constexpr auto NTAB = 32;

//-----------------------------------------------------------------------------
// The standard generator of uniformly distributed random numbers
//-----------------------------------------------------------------------------
class CUniformRandomStream
{
public:
	CUniformRandomStream();

	// Sets the seed of the random number generator
	void	SetSeed( int iSeed );

	// Generates random numbers
	float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f );
	int		RandomInt( int iMinVal, int iMaxVal );
	float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f );

private:
	int		GenerateRandomNumber();

	int m_idum;
	int m_iy;
	int m_iv[NTAB];

	std::recursive_mutex m_mutex;
};

//-----------------------------------------------------------------------------
// A generator of gaussian distributed random numbers
//-----------------------------------------------------------------------------
class CGaussianRandomStream
{
public:
	// Passing in NULL will cause the gaussian stream to use the
	// installed global random number generator
	CGaussianRandomStream( CUniformRandomStream *pUniformStream = nullptr );

	// Attaches to a random uniform stream
	void	AttachToStream( CUniformRandomStream *pUniformStream = nullptr );

	// Generates random numbers
	float	RandomFloat( float flMean = 0.0f, float flStdDev = 1.0f );

private:
	CUniformRandomStream *m_pUniformStream;
	bool	m_bHaveValue;
	float	m_flRandomValue;

	std::recursive_mutex m_mutex;
};

//-----------------------------------------------------------------------------
// A couple of convenience functions to access the library's global uniform stream
//-----------------------------------------------------------------------------
void	RandomSeed( int iSeed );
float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f );
float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f );
int	RandomInt( int iMinVal, int iMaxVal );
float	RandomGaussianFloat( float flMean = 0.0f, float flStdDev = 1.0f );

//-----------------------------------------------------------------------------
// The library's global uniform stream, which the Random functions above draw from
//-----------------------------------------------------------------------------
CUniformRandomStream &GlobalUniformRandomStream();

}
