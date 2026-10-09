// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.math:aabb;

import std;
import ks.core;
import ks.simd;
export import :vector4d;
export import :mathlib;

export extern "C++" {

/// Axis-aligned 3d bounding box.
struct AABB_t
{
public:
	Vector				m_vMinBounds; 
	Vector				m_vMaxBounds;

	[[gnu::always_inline]] inline AABB_t() {}

	[[gnu::always_inline]] inline AABB_t( const Vector &vMins, const Vector &vMaxs )
	{
		m_vMinBounds = vMins;
		m_vMaxBounds = vMaxs;
	}

	[[gnu::always_inline]] inline Vector GetCenter() const { return ( m_vMaxBounds + m_vMinBounds ) / 2.0f; }

	/// radius of bounding sphere centered at GetCenter()
	[[gnu::always_inline]] inline float GetBoundingRadius( void ) const
	{
		return ( m_vMinBounds - GetCenter() ).Length();
	}

	[[gnu::always_inline]] inline float GetSurfaceArea( void ) const { return BoxSurfaceArea( m_vMinBounds, m_vMaxBounds ); }

	/// Calculate the volume. Does not contain special handling for inside-out volumes - if an odd
	/// number of axes are inside-out, this will return a negative volume, but if an even number,
	/// it will return a positive one.
	[[gnu::always_inline]] inline float GetVolume( void ) const { return ComputeVolume( m_vMinBounds, m_vMaxBounds ); }

	[[gnu::always_inline]] inline float GetMinDistToPoint( const Vector &vPoint ) const
	{
		return CalcDistanceToAABB( m_vMinBounds, m_vMaxBounds, vPoint );
	}

	[[gnu::always_inline]] inline float GetMinAxialDistanceToPoint( const Vector &vPoint ) const
	{
		float flXInterval = std::max( 0.0f, std::max( vPoint.x - m_vMaxBounds.x, m_vMinBounds.x - vPoint.x ) );
		float flYInterval = std::max( 0.0f, std::max( vPoint.y - m_vMaxBounds.y, m_vMinBounds.y - vPoint.y ) );
		float flZInterval = std::max( 0.0f, std::max( vPoint.z - m_vMaxBounds.z, m_vMinBounds.z - vPoint.z ) );
		return std::max( flXInterval, std::max( flYInterval, flZInterval ) );
	}

	/// expand the aabt_t to contain a point
	[[gnu::always_inline]] inline void operator |=( const Vector &vPoint )
	{
		AddPointToBounds( vPoint, m_vMinBounds, m_vMaxBounds );
	}

	/// expand the bounds to enclose another aabb_t
	[[gnu::always_inline]] inline void operator |=( const AABB_t &other )
	{
		VectorMin( other.m_vMinBounds, m_vMinBounds, m_vMinBounds );
		VectorMax( other.m_vMaxBounds, m_vMaxBounds, m_vMaxBounds );
	}

	/// set the bounds to the bounds of the union of this and another aabb_t
	[[gnu::always_inline]] inline void operator &=( const AABB_t &other )
	{
		VectorMax( other.m_vMinBounds, m_vMinBounds, m_vMinBounds );
		VectorMin( other.m_vMaxBounds, m_vMaxBounds, m_vMaxBounds );
	}

	void CreatePlanesFrom( Vector4D *pPlanes ) const
	{
		// X
		pPlanes[0] = Vector4D( 1, 0, 0, -m_vMaxBounds.x  );
		pPlanes[1] = Vector4D( -1, 0, 0, m_vMinBounds.x );

		// Y
		pPlanes[2] = Vector4D( 0, 1, 0, -m_vMaxBounds.y );
		pPlanes[3] = Vector4D( 0, -1, 0, m_vMinBounds.y );

		// Z
		pPlanes[4] = Vector4D( 0, 0, 1, -m_vMaxBounds.z );
		pPlanes[5] = Vector4D( 0, 0, -1, m_vMinBounds.z );
	}

	/// Set the aabb to be invalid (max < min )
	void MakeInvalid( void )
	{
		m_vMinBounds.Init( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
		m_vMaxBounds.Init( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );
	}

	// Returns if the bounds are invalid (negative volume), this is different
	// than empty, the bounds are still considered valid if min == max.
	[[gnu::always_inline]] inline bool IsInvalid() const
	{
		return ( ( m_vMinBounds.x > m_vMaxBounds.x ) ||
				 ( m_vMinBounds.y > m_vMaxBounds.y ) ||
				 ( m_vMinBounds.z > m_vMaxBounds.z ) );
	}

	/// Return if the bounding box has either 0 or negative volume (i.e. if min >= max for any
	/// coord ). Note that this treats bounds set to a single point as empty.
	[[gnu::always_inline]] inline bool IsEmpty( void ) const
	{
		return (
			( m_vMinBounds.x >= m_vMaxBounds.x ) ||
			( m_vMinBounds.y >= m_vMaxBounds.y ) ||
			( m_vMinBounds.z >= m_vMaxBounds.z )
			);
	}

	[[gnu::always_inline]] inline bool Overlaps( AABB_t bBox ) const
	{
		bBox &= *this;
		return (! bBox.IsEmpty() );
	}

	[[gnu::always_inline]] inline bool ContainsPoint( Vector const &vPnt ) const
	{
		return (
			( vPnt.x >= m_vMinBounds.x ) &&
			( vPnt.y >= m_vMinBounds.y ) &&
			( vPnt.z >= m_vMinBounds.z ) &&
			( vPnt.x <= m_vMaxBounds.x ) &&
			( vPnt.y <= m_vMaxBounds.y ) &&
			( vPnt.z <= m_vMaxBounds.z )
			);
	}

	[[gnu::always_inline]] inline bool Contains( const AABB_t &box ) const
	{
		return (
			( box.m_vMinBounds.x >= m_vMinBounds.x ) &&
			( box.m_vMinBounds.y >= m_vMinBounds.y ) &&
			( box.m_vMinBounds.z >= m_vMinBounds.z ) &&
			( box.m_vMaxBounds.x <= m_vMaxBounds.x ) &&
			( box.m_vMaxBounds.y <= m_vMaxBounds.y ) &&
			( box.m_vMaxBounds.z <= m_vMaxBounds.z )
		);
	}

	/// set the aabb_t to a zero volume point in space.
	[[gnu::always_inline]] inline void SetToPoint( Vector const &vPnt )
	{
		m_vMinBounds = vPnt;
		m_vMaxBounds = vPnt;
	}

	[[gnu::always_inline]] inline float LengthOfSmallestDimension() const
	{
		Vector vDelta = m_vMaxBounds - m_vMinBounds;
		return vDelta.SmallestComponentValue();
	}

	[[gnu::always_inline]] inline const Vector GetSize() const
	{
		return m_vMaxBounds - m_vMinBounds;
	}

	[[gnu::always_inline]] inline void EnsureMinSize( const Vector &vMinSize )
	{
		Vector vHalfExpand = VectorMax( Vector( 0,0,0 ), vMinSize - GetSize() ) * 0.5f;
		m_vMaxBounds += vHalfExpand;
		m_vMinBounds -= vHalfExpand;
	}

	[[gnu::always_inline]] inline void Move( const Vector &vDelta )
	{
		m_vMinBounds += vDelta;
		m_vMaxBounds += vDelta;
	}

	[[gnu::always_inline]] inline void Expand( float flRadius )
	{
		m_vMinBounds -= Vector( flRadius, flRadius, flRadius );
		m_vMaxBounds += Vector( flRadius, flRadius, flRadius );
	}
};

inline const AABB_t Snap( const AABB_t& aabb, float flSnap )
{
	return AABB_t( Snap( aabb.m_vMinBounds, flSnap ), Snap( aabb.m_vMaxBounds, flSnap ) );
}

inline AABB_t operator+( const AABB_t& aabb1, const AABB_t& aabb2 )
{
	Vector vMin = VectorMin( aabb1.m_vMinBounds, aabb2.m_vMinBounds );
	Vector vMax = VectorMax( aabb1.m_vMaxBounds, aabb2.m_vMaxBounds );
	return AABB_t( vMin, vMax );
}

[[gnu::always_inline]] inline void TransformAABB( const matrix3x4_t &matTransform, AABB_t const &boundsIn, AABB_t *pBoundsOut )
{
	TransformAABB( matTransform, boundsIn.m_vMinBounds, boundsIn.m_vMaxBounds, pBoundsOut->m_vMinBounds, pBoundsOut->m_vMaxBounds );
	
}

inline AABB_t GetAabb( const VectorAligned *pPos, int nCount )
{
	AABB_t aabb;
	aabb.MakeInvalid();
	for ( int i = 0; i < nCount; ++i )
	{
		aabb |= pPos[ i ];
	}
	return aabb;
};

}
