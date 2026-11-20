// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sphere.h"
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
//#include "body.h"
//#include "gjk.h"
//#include "toi.h"

//--------------------------------------------------------------------------------------------------
// Local utilities 
//--------------------------------------------------------------------------------------------------
static void CastStationaryHit( CShapeCastResult& out, float c, const Vector &p, const Vector &m, float mm )
{
	// return a sphere hit for zero-length ray at point p, with
	// m = p - m_vCenter
	// mm = DotProduct( m, m )
	// c = mm - Sqr( m_flRadius )

	if( c <= 0 )
	{
		out.m_flHitTime = 0;
		out.m_vHitPoint = p;
		if( mm > std::numeric_limits<float>::epsilon() )
		{
			out.m_vHitNormal = m / std::sqrtf( mm );
		}
		else
		{
			out.m_vHitNormal = Vector( 0,0,1 );
		}
	}
	else
	{
		// we didn't hit - we're outside and we don't move
		out.m_flHitTime = std::numeric_limits<float>::max();
	}
}


//--------------------------------------------------------------------------------------------------
void CastSphereRay( CShapeCastResult& out, const Vector &m, const Vector& p, const Vector& d, float flRadius )
{
	float a = DotProduct( d, d ), mm = DotProduct( m, m ), c = mm - Sqr( flRadius );
	if( a < std::numeric_limits<float>::epsilon() * std::numeric_limits<float>::epsilon() )
	{
		// we barely move; just detect if we're in the sphere or not
		CastStationaryHit( out, c, p, m, mm );
		return;
	}

	float b = DotProduct( m, d ); // solve: at^2+2bt+c=0; t = (-b±sqrt(b^2-ac))/a = -b/a ± sqrt((b/a)^2-c/a))
	float D = Sqr( b ) - a * c;
	if( D < 0 )
	{
		// no intersection at all
		out.m_flHitTime = std::numeric_limits<float>::max();
		return;
	}
	float sqrtD = std::sqrtf( D );
	float t = ( -b - sqrtD ) / a;
	if( t < 0 )
	{
		// this was the first hit in the past - determine if we're still inside the sphere at time t=0
		// we could do that by checking if float t1 = ( b + sqrtD ) / a; is > 0 or not, but it's easier to:
		// we barely move; just detect if we're in the sphere or not
		CastStationaryHit( out, c, p, m, mm );
	}
	else
	{
		out.m_flHitTime = t;
		Vector dt = d * t;
		out.m_vHitPoint = p + dt;
		out.m_vHitNormal = ( m + dt ) / flRadius; // Should I normalize this here or is this sufficient precision?
	}
}
