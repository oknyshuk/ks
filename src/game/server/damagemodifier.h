#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class CBaseEntity;


#include "ehandle.h"

//-----------------------------------------------------------------------------
// Purpose: Class handling generic damage modification to & from a player
//-----------------------------------------------------------------------------
class CDamageModifier
{
public:
				CDamageModifier();

	void		AddModifierToEntity( CBaseEntity *pChar );
	void		RemoveModifier();

	void		SetModifier( float flDamageScale );
	float		GetModifier() const;

	void		SetDoneToMe( bool bDoneToMe );
	bool		IsDamageDoneToMe() const;

	CBaseEntity	*GetCharacter() const;

private:
	float							m_flModifier;
	CHandle<CBaseEntity>			m_hEnt;
	bool							m_bDoneToMe;	// True = modifies damage done to the entity, false = damage done by the entity
};
