#pragma once
//----------------------------------------------------------------------------------------------------------
#define LOAD_NODES_POS( POS, V, IDX ) {																  \
	fltx4 _pos0 = POS[ IDX[ 0 ] ], _pos1 = POS[ IDX[ 1 ] ], _pos2 = POS[ IDX[ 2 ] ], _pos3 = POS[ IDX[ 3 ] ]; \
	__m128 tmp3, tmp2, tmp1, tmp0;                          								  \
	tmp0 = ks::simd::shuffle<0,1,0,1>( ( _pos0 ), ( _pos1 ) );  										  \
	tmp2 = ks::simd::shuffle<2,3,2,3>( ( _pos0 ), ( _pos1 ) );  										  \
	tmp1 = ks::simd::shuffle<0,1,0,1>( ( _pos2 ), ( _pos3 ) );  										  \
	tmp3 = ks::simd::shuffle<2,3,2,3>( ( _pos2 ), ( _pos3 ) );  										  \
	V.x = ks::simd::shuffle<0,2,0,2>( tmp0, tmp1 );      											  \
	V.y = ks::simd::shuffle<1,3,1,3>( tmp0, tmp1 );      											  \
	V.z = ks::simd::shuffle<0,2,0,2>( tmp2, tmp3 );      											  \
}

#define LOAD_NODES( V, IDX ) {																  \
	fltx4 _pos0 = pPos[ IDX[ 0 ] ], _pos1 = pPos[ IDX[ 1 ] ], _pos2 = pPos[ IDX[ 2 ] ], _pos3 = pPos[ IDX[ 3 ] ]; \
	__m128 tmp3, tmp2, tmp1, tmp0;                          								  \
	tmp0 = ks::simd::shuffle<0,1,0,1>( ( _pos0 ), ( _pos1 ) );  										  \
	tmp2 = ks::simd::shuffle<2,3,2,3>( ( _pos0 ), ( _pos1 ) );  										  \
	tmp1 = ks::simd::shuffle<0,1,0,1>( ( _pos2 ), ( _pos3 ) );  										  \
	tmp3 = ks::simd::shuffle<2,3,2,3>( ( _pos2 ), ( _pos3 ) );  										  \
	V.x = ks::simd::shuffle<0,2,0,2>( tmp0, tmp1 );      											  \
	V.y = ks::simd::shuffle<1,3,1,3>( tmp0, tmp1 );      											  \
	V.z = ks::simd::shuffle<0,2,0,2>( tmp2, tmp3 );      											  \
}


#define SAVE_NODES_POS( POS, V, IDX ) {														  \
	__m128 tmp3, tmp2, tmp1, tmp0;                          								  \
	tmp0 = ks::simd::shuffle<0,1,0,1>( V.x, V.y );  												  \
	tmp2 = ks::simd::shuffle<2,3,2,3>( V.x, V.y );  												  \
	tmp1 = ks::simd::shuffle<0,1,0,1>( V.z, Four_Zeros );  										  \
	tmp3 = ks::simd::shuffle<2,3,2,3>( V.z, Four_Zeros );  										  \
	POS[ IDX[ 0 ] ] = ks::simd::shuffle<0,2,0,2>( tmp0, tmp1 );								      \
	POS[ IDX[ 1 ] ] = ks::simd::shuffle<1,3,1,3>( tmp0, tmp1 );								      \
	POS[ IDX[ 2 ] ] = ks::simd::shuffle<0,2,0,2>( tmp2, tmp3 );								      \
	POS[ IDX[ 3 ] ] = ks::simd::shuffle<1,3,1,3>( tmp2, tmp3 );								      \
}



#define SAVE_NODES( V, IDX ) {																  \
	__m128 tmp3, tmp2, tmp1, tmp0;                          								  \
	tmp0 = ks::simd::shuffle<0,1,0,1>( V.x, V.y );  												  \
	tmp2 = ks::simd::shuffle<2,3,2,3>( V.x, V.y );  												  \
	tmp1 = ks::simd::shuffle<0,1,0,1>( V.z, Four_Zeros );  										  \
	tmp3 = ks::simd::shuffle<2,3,2,3>( V.z, Four_Zeros );  										  \
	pPos[ IDX[ 0 ] ] = ks::simd::shuffle<0,2,0,2>( tmp0, tmp1 );								      \
	pPos[ IDX[ 1 ] ] = ks::simd::shuffle<1,3,1,3>( tmp0, tmp1 );								      \
	pPos[ IDX[ 2 ] ] = ks::simd::shuffle<0,2,0,2>( tmp2, tmp3 );								      \
	pPos[ IDX[ 3 ] ] = ks::simd::shuffle<1,3,1,3>( tmp2, tmp3 );								      \
}




inline void CovMatrix3::InitForWahba( float m, const Vector &x )
{
	m_vDiag.x = m * ( Sqr( x.y ) + Sqr( x.z ) );
	m_vDiag.y = m * ( Sqr( x.x ) + Sqr( x.z ) );
	m_vDiag.z = m * ( Sqr( x.x ) + Sqr( x.y ) );
	m_flXY = -m * x.x * x.y;
	m_flXZ = -m * x.x * x.z;
	m_flYZ = -m * x.y * x.z;
}

inline void CovMatrix3::Reset()
{
	m_vDiag = vec3_origin;
	m_flXY = m_flXZ = m_flYZ = 0;
}

inline void CovMatrix3::AddCov( const Vector &d ) // d is supposedly a vector relatively to the mean of the set; i.e. we assume here that we're actually summing up voth d and -d 
{
	m_vDiag.x += Sqr( d.x );
	m_vDiag.y += Sqr( d.y );
	m_vDiag.z += Sqr( d.z );
	m_flXY += d.x * d.y;
	m_flXZ += d.x * d.z;
	m_flYZ += d.y * d.z;
}


inline void CovMatrix3::AddCov( const Vector &d, float m ) // d is supposedly a vector relatively to the mean of the set; i.e. we assume here that we're actually summing up voth d and -d 
{
	m_vDiag.x += m * Sqr( d.x );
	m_vDiag.y += m * Sqr( d.y );
	m_vDiag.z += m * Sqr( d.z );
	m_flXY += m * d.x * d.y;
	m_flXZ += m * d.x * d.z;
	m_flYZ += m * d.y * d.z;
}

// the element of the sum on the left side of the approximate solution of Wahba's problem (see wahba.nb for details)
// thi sis essentially Sum[Mi Xi * w * Xi], Mi = weights, "*" means cross product, Xi is a deformed polygon vertex relative to center of mass, 
// 21 flops, with madd
inline void CovMatrix3::AddForWahba( float m, const Vector &x )
{
	m_vDiag.x += m * ( Sqr( x.y ) + Sqr( x.z ) );
	m_vDiag.y += m * ( Sqr( x.x ) + Sqr( x.z ) );
	m_vDiag.z += m * ( Sqr( x.x ) + Sqr( x.y ) );
	m_flXY -= m * x.x * x.y;
	m_flXZ -= m * x.x * x.z;
	m_flYZ -= m * x.y * x.z;
}

inline void CovMatrix3::NormalizeEigenvalues( )
{
	// trace is the sum of eigenvalues; it's not a perfect way , but it's one way
	float flNorm = 1.0f / ( m_vDiag.x + m_vDiag.y + m_vDiag.z );
	m_vDiag *= flNorm;
	m_flXY *= flNorm;
	m_flXZ *= flNorm;
	m_flYZ *= flNorm;
}

inline void CovMatrix3::RegularizeEigenvalues( )
{
	m_vDiag += Vector( .001f, .001f, .001f );
	NormalizeEigenvalues( );
	m_vDiag += Vector( .1f, .1f, .1f );
}

inline Vector CovMatrix3::operator * ( const Vector &d )
{
	return Vector(
		m_vDiag.x * d.x + m_flXY * d.y + m_flXZ * d.z,
		m_flXY * d.x + m_vDiag.y * d.y + m_flYZ * d.z,
		m_flXZ * d.x + m_flYZ * d.y + m_vDiag.z * d.z
		);
}



inline void FourCovMatrices3::InitForWahba( const fltx4 &m, const FourVectors &x )
{
	m_vDiag.x = m * (  x.y  *  x.y  +  x.z  *  x.z  );
	m_vDiag.y = m * (  x.x  *  x.x  +  x.z  *  x.z  );
	m_vDiag.z = m * (  x.x  *  x.x  +  x.y  *  x.y  );
	m_flXY = -m * x.x * x.y;
	m_flXZ = -m * x.x * x.z;
	m_flYZ = -m * x.y * x.z;
}

// the element of the sum on the left side of the approximate solution of Wahba's problem (see wahba.nb for details)
// thi sis essentially Sum[Mi Xi * w * Xi], Mi = weights, "*" means cross product, Xi is a deformed polygon vertex relative to center of mass, 
// 21 flops, with madd
inline void FourCovMatrices3::AddForWahba( const fltx4 &m, const FourVectors &x )
{
	m_vDiag.x += m * (  x.y  *  x.y  +  x.z  *  x.z  );
	m_vDiag.y += m * (  x.x  *  x.x  +  x.z  *  x.z  );
	m_vDiag.z += m * (  x.x  *  x.x  +  x.y  *  x.y  );
	m_flXY -= m * x.x * x.y;
	m_flXZ -= m * x.x * x.z;
	m_flYZ -= m * x.y * x.z;
}

inline FourVectors FourCovMatrices3::operator * ( const FourVectors &d )
{
	return FourVectors(
		m_vDiag.x * d.x + m_flXY * d.y + m_flXZ * d.z,
		m_flXY * d.x + m_vDiag.y * d.y + m_flYZ * d.z,
		m_flXZ * d.x + m_flYZ * d.y + m_vDiag.z * d.z
		);
}


inline float Perimeter( const FeQuad_t &quad )
{
	return
		( quad.vShape[ 0 ].AsVector3D( ) - quad.vShape[ 1 ].AsVector3D( ) ).Length( ) +
		( quad.vShape[ 1 ].AsVector3D( ) - quad.vShape[ 2 ].AsVector3D( ) ).Length( ) +
		( quad.vShape[ 2 ].AsVector3D( ) - quad.vShape[ 3 ].AsVector3D( ) ).Length( ) +
		( quad.vShape[ 3 ].AsVector3D( ) - quad.vShape[ 0 ].AsVector3D( ) ).Length( );
}

inline fltx4 Perimeter( const FeSimdQuad_t &quad )
{
	return
		( quad.vShape[ 0 ] - quad.vShape[ 1 ] ).Length( ) +
		( quad.vShape[ 1 ] - quad.vShape[ 2 ] ).Length( ) +
		( quad.vShape[ 2 ] - quad.vShape[ 3 ] ).Length( ) +
		( quad.vShape[ 3 ] - quad.vShape[ 0 ] ).Length( );
}


inline float Perimeter( const FeTri_t &tri )
{
	return std::fabsf( tri.v1x ) + tri.v2.Length() + std::sqrtf( tri.v2.y * tri.v2.y + ( tri.v2.x - tri.v1x ) * ( tri.v2.x - tri.v1x ) );
}

inline fltx4 Perimeter( const FeSimdTri_t &tri )
{
	return AbsSIMD( tri.v1x ) + tri.v2.Length( ) + SqrtSIMD( tri.v2.y * tri.v2.y + ( tri.v2.x - tri.v1x ) * ( tri.v2.x - tri.v1x ) );
}


[[gnu::always_inline]] inline float CrossProductZ( const Vector2D &v1, const Vector2D &v2 )
{
	return v1.x * v2.y - v1.y * v2.x;
}

[[gnu::always_inline]] inline float CrossProductZ( const Vector2D &v1, const Vector4D &v2 )
{
	return v1.x * v2.y - v1.y * v2.x;
}

[[gnu::always_inline]] inline float CrossProductZ( const Vector4D &v1, const Vector2D &v2 )
{
	return v1.x * v2.y - v1.y * v2.x;
}

[[gnu::always_inline]] inline fltx4 CrossProductZ( const FourVectors &v1, const FourVectors2D &v2 )
{
	return v1.x * v2.y - v1.y * v2.x;
}

[[gnu::always_inline]] inline float DotProduct( const Vector4D &v1, const Vector2D &v2 )
{
	return v1.x * v2.x + v1.y * v2.y;
}


[[gnu::always_inline]] inline FourVectors AndSIMD( const FourVectors &left, const fltx4 &right )
{
	FourVectors out;
	out.x = AndSIMD( left.x, right );
	out.y = AndSIMD( left.y, right );
	out.z = AndSIMD( left.z, right );
	return out;
}
