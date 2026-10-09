#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "baseentity.h"

// Purpose: Hooks and classes for the support of humanoid NPCs with
//			groovy facial animation capabilities, aka, "Actors"

//-----------------------------------------------------------------------------
// CAI_BaseActor
//
// Purpose: The base class for all facially expressive NPCS.
//
//-----------------------------------------------------------------------------

class CAI_InterestTarget_t
{
public:
	enum CAI_InterestTarget_e
	{
		LOOKAT_ENTITY = 0,
		LOOKAT_POSITION, 
		LOOKAT_BOTH
	};

public:
	bool			IsThis( CBaseEntity *pThis );
	const Vector	&GetPosition( void );
	bool			IsActive( void );
	float			Interest( void );

public:
	CAI_InterestTarget_e	m_eType; // ????

	EHANDLE		m_hTarget;
	Vector		m_vecPosition;
	float		m_flStartTime;
	float		m_flEndTime;
	float		m_flRamp;
	float		m_flInterest;

	DECLARE_SIMPLE_DATADESC();
};


class CAI_InterestTarget : public std::vector< CAI_InterestTarget_t >
{
public:
	void Add( CBaseEntity *pTarget, float flImportance, float flDuration, float flRamp );
	void Add( const Vector &vecPosition, float flImportance, float flDuration, float flRamp );
	void Add( CBaseEntity *pTarget, const Vector &vecPosition, float flImportance, float flDuration, float flRamp );
	int Find( CBaseEntity *pTarget )
	{
		int i;
		for ( i = 0; i < std::ssize( *this ); i++)
		{
			if (pTarget == (*this)[i].m_hTarget)
				return i;
		}
		return -1;
	}
	
	void Cleanup( void )
	{
		int i;
		for (i = std::ssize( *this ) - 1; i >= 0; i--)
		{
			if (!(*this)[ i ].IsActive())
			{
				this->erase( this->begin() + i );
			}
		}
	};

private:
	void Add( CAI_InterestTarget_t::CAI_InterestTarget_e type, CBaseEntity *pTarget, const Vector &vecPosition, float flImportance, float flDuration, float flRamp );
};


//-----------------------------------------------------------------------------
