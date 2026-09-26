//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "damagemodifier.h"
#include "tier1/vector_ops.h"

#if defined(ENABLE_DAMAGE_MODIFIERS)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDamageModifier::CDamageModifier()
{
	m_flModifier = 1;
	m_bDoneToMe = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDamageModifier::AddModifierToEntity( CBaseEntity *pEntity )
{
	RemoveModifier();

	pEntity->m_DamageModifiers.push_back( this );
	m_hEnt = pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDamageModifier::RemoveModifier()
{
	if ( m_hEnt.Get() )
	{
		ks::erase_first( m_hEnt->m_DamageModifiers, this );
		m_hEnt = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDamageModifier::SetModifier( float flScale )
{
	m_flModifier = flScale;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CDamageModifier::GetModifier() const
{
	return m_flModifier;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity* CDamageModifier::GetCharacter() const
{
	return m_hEnt.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDamageModifier::SetDoneToMe( bool bDoneToMe )
{
	m_bDoneToMe = bDoneToMe;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDamageModifier::IsDamageDoneToMe() const
{
	return m_bDoneToMe;
}

#endif
