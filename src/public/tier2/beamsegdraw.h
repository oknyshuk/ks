#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#define NOISE_DIVISIONS		128

import std;
import ks.core.basetypes;
import ks.core.numeric;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector4d;
import ks.platform;
import ks.simd;
#include "materialsystem/imesh.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterial;


//-----------------------------------------------------------------------------
// CBeamSegDraw is a simple interface to beam rendering.
//-----------------------------------------------------------------------------
struct BeamSeg_t
{
	VectorAligned	m_vPos;
	color32			m_color;
	float			m_flTexCoord;	// Y texture coordinate
	float			m_flWidth;

	void SetColor( float r, float g, float b, float a )
	{
		// Specify the points.
		contract_assert( IsFinite(r) && IsFinite(g) && IsFinite(b) && IsFinite(a) );
		contract_assert( (r >= 0.0) && (g >= 0.0) && (b >= 0.0) && (a >= 0.0) );
		contract_assert( (r <= 1.0) && (g <= 1.0) && (b <= 1.0) && (a <= 1.0) );

		m_color.r = FastFToC( r );
		m_color.g = FastFToC( g );
		m_color.b = FastFToC( b );
		m_color.a = FastFToC( a );
	}

	void SetColor( float r, float g, float b )
	{
		// Specify the points.
		contract_assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
		contract_assert( (r >= 0.0) && (g >= 0.0) && (b >= 0.0) );
		contract_assert( (r <= 1.0) && (g <= 1.0) && (b <= 1.0) );

		m_color.r = FastFToC( r );
		m_color.g = FastFToC( g );
		m_color.b = FastFToC( b );
	}

	void SetAlpha( float a )
	{
		// Specify the points.
		contract_assert( IsFinite(a) );
		contract_assert( (a >= 0.0) );
		contract_assert( (a <= 1.0) );

		m_color.a = FastFToC( a );
	}

	void SetColor( const Vector &vecColor, float a )
	{
		SetColor( vecColor.x, vecColor.y, vecColor.z, a );
	}

	void SetColor( const Vector4D &vecColor )
	{
		SetColor( vecColor.x, vecColor.y, vecColor.z, vecColor.w );
	}

	void SetColor( const Vector &vecColor )
	{
		SetColor( vecColor.x, vecColor.y, vecColor.z );
	}

	void GetColor( Vector4D *pColor )
	{
		pColor->x = m_color.r / 255.0f;
		pColor->y = m_color.g / 255.0f;
		pColor->z = m_color.b / 255.0f;
		pColor->w = m_color.a / 255.0f;
	}

	void GetColor( Vector *pColor )
	{
		pColor->x = m_color.r / 255.0f;
		pColor->y = m_color.g / 255.0f;
		pColor->z = m_color.b / 255.0f;
	}
};

struct BeamSegRenderInfo_t
{
	Vector	m_vecPoint1;
	Vector	m_vecPoint2;
	Vector	m_vecCenter;
	Vector	m_vecTangentS;
	Vector	m_vecTangentT;
	float	m_flTexCoord;
	color32	m_color;
};

class CBeamSegDraw
{
public:
	CBeamSegDraw() : m_pRenderContext( nullptr ) {}
	// Pass null for pMaterial if you have already set the material you want.
	void			Start( IMatRenderContext *pRenderContext, int nSegs, IMaterial *pMaterial=0, CMeshBuilder *pMeshBuilder = nullptr, int nMeshVertCount = 0 );

	void			ComputeRenderInfo( BeamSegRenderInfo_t *pRenderInfo, const Vector &vecCameraPos, int nSegCount, const BeamSeg_t *pSegs ) __restrict__;
	void			NextSeg( BeamSeg_t *pSeg );
	void			End();

protected:
	void			SpecifySeg( const Vector &vecCameraPos, const Vector &vNextPos );
	void			ComputeNormal( const Vector &vecCameraPos, const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal );
	static void		LoadSIMDData( FourVectors *pV4StartPos, FourVectors *pV4EndPos, FourVectors *pV4HalfWidth, int nSegCount, const BeamSeg_t *pSegs );
	CMeshBuilder	*m_pMeshBuilder;
	int				m_nMeshVertCount;

	CMeshBuilder	m_Mesh;
	BeamSeg_t		m_Seg;	

	int				m_nTotalSegs;
	int				m_nSegsDrawn;

	Vector			m_vNormalLast;
	IMatRenderContext *m_pRenderContext;

	Vector			m_vecCameraPos;
};

//-----------------------------------------------------------------------------
// Assumes the material has already been bound
//-----------------------------------------------------------------------------
