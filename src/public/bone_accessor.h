#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "studio.h"
import ks.math.mathlib;


class CBoneAccessor
{
public:
	
	CBoneAccessor();
	explicit CBoneAccessor( matrix3x4a_t *pBones ); // This can be used to allow access to all bones.
	
	void Init( matrix3x4a_t *pBones );
	
	int GetReadableBones() const;
	void SetReadableBones( int flags );

	int GetWritableBones() const;
	void SetWritableBones( int flags );

	// Get bones for read or write access.
	const matrix3x4a_t&	GetBone( int iBone ) const;
	const matrix3x4a_t&	operator[]( int iBone ) const;
	matrix3x4a_t&		GetBoneForWrite( int iBone );

	matrix3x4a_t			*GetBoneArrayForWrite( ) const;

private:

	matrix3x4a_t *m_pBones;

	int m_ReadableBones;		// Which bones can be read.
	int m_WritableBones;		// Which bones can be written.
};


inline CBoneAccessor::CBoneAccessor()
{
	m_pBones = nullptr;
	m_ReadableBones = m_WritableBones = 0;
}

inline CBoneAccessor::CBoneAccessor( matrix3x4a_t *pBones )
{
	m_pBones = pBones;
}

inline void CBoneAccessor::Init( matrix3x4a_t *pBones )
{
	m_pBones = pBones;
}

inline int CBoneAccessor::GetReadableBones() const
{
	return m_ReadableBones;
}

inline void CBoneAccessor::SetReadableBones( int flags )
{
	m_ReadableBones = flags;
}

inline int CBoneAccessor::GetWritableBones() const
{
	return m_WritableBones;
}

inline void CBoneAccessor::SetWritableBones( int flags )
{
	m_WritableBones = flags;
}

inline const matrix3x4a_t& CBoneAccessor::GetBone( int iBone ) const
{
	return m_pBones[iBone];
}

inline const matrix3x4a_t& CBoneAccessor::operator[]( int iBone ) const
{
	return m_pBones[iBone];
}

inline matrix3x4a_t& CBoneAccessor::GetBoneForWrite( int iBone )
{
	return m_pBones[iBone];
}

inline matrix3x4a_t *CBoneAccessor::GetBoneArrayForWrite( void ) const
{
	return m_pBones;
}
