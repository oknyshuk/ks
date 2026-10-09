#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.math;

//=========================================================
// Plane
//=========================================================
class CPlane 
{
public:
	CPlane ( void );

	//=========================================================
	// InitializePlane - Takes a normal for the plane and a
	// point on the plane and 
	//=========================================================
	void InitializePlane ( const Vector &vecNormal, const Vector &vecPoint );

	//=========================================================
	// PointInFront - determines whether the given vector is 
	// in front of the plane. 
	//=========================================================
	bool PointInFront ( const Vector &vecPoint );

	//=========================================================
	// How far off the plane is this point?
	//=========================================================
	float PointDist( const Vector &vecPoint );

private:
	Vector	m_vecNormal;
	float	m_flDist;
	bool	m_fInitialized;
};
