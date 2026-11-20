#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;

// nonlinear transformations which may be applied to model vertices when rendering. must be powers of two
enum DeformationType_t	
{
	DEFORMATION_CLAMP_TO_BOX_IN_WORLDSPACE = 1,							// minxyz.minsoftness / maxxyz.maxsoftness
};


struct DeformationBase_t										// base class. don't use this
{
	DeformationType_t m_eType;
};


struct BoxDeformation_t : DeformationBase_t
{
	// don't change the layout without changing code in shaderapidx8!!!!
	Vector m_SourceMins;									// cube to clamp within
	float m_flPad0;
	Vector m_SourceMaxes;
	float m_flPad1;

	Vector m_ClampMins;
	float m_flPad2;
	Vector m_ClampMaxes;
	float m_flPad3;

	[[gnu::always_inline]] inline BoxDeformation_t( void )
	{
		m_eType = DEFORMATION_CLAMP_TO_BOX_IN_WORLDSPACE;
		// invalid cube
		m_SourceMins.Init( 0,0,0 );
		m_SourceMaxes.Init( -1, -1, -1 );

		// no clamp
		m_ClampMins.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
		m_ClampMaxes.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
	}

};
