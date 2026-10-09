// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: An entity that can be used to control speech behaviour for a group
//			of NPCs.

import std;
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "reflect_datamap.h"
#include "ai_speechfilter.h"
#ifndef CSTRIKE_DLL
#include "ai_playerally.h"
#endif

LINK_ENTITY_TO_CLASS( ai_speechfilter, CAI_SpeechFilter );

IMPLEMENT_REFLECT_DATAMAP( CAI_SpeechFilter )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::Spawn( void )
{
	BaseClass::Spawn();

	gEntList.AddListenerEntity( this );
}

//-----------------------------------------------------------------------------
// Purpose: Connect to all our NPCs
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::Activate( void )
{
	BaseClass::Activate();

	PopulateSubjectList( m_bDisabled );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::UpdateOnRemove(void)
{
	gEntList.RemoveListenerEntity( this );
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::Enable( bool bEnable )
{
	m_bDisabled = !bEnable;

	PopulateSubjectList( m_bDisabled );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::InputEnable( inputdata_t &inputdata )
{
	Enable( true );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::InputDisable( inputdata_t &inputdata )
{
	Enable( false );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::InputSetIdleModifier( inputdata_t &inputdata )
{
	m_flIdleModifier = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::PopulateSubjectList( bool purge )
{
	// Populate the subject list. Try targetname first.
	CBaseEntity *pSearch = nullptr;
	int iNumSubjects = 0;
	do
	{
		pSearch = gEntList.FindEntityByName( pSearch, m_iszSubject );
		if ( pSearch )
		{
#ifndef CSTRIKE_DLL
			CAI_PlayerAlly *pAlly = dynamic_cast<CAI_PlayerAlly *>(pSearch);
			if ( pAlly )
			{
				if( purge )
				{
					pAlly->SetSpeechFilter( nullptr );
				}
				else
				{
					if( pAlly->GetSpeechFilter() != nullptr )
					{
						DevWarning("ai_speechfilter %s is slamming NPC %s's current speech filter.\n", STRING(GetEntityName()), STRING(pSearch->GetEntityName()) );
					}

					pAlly->SetSpeechFilter( this );
				}
			}
			else if ( pSearch->IsNPC() )
			{
				DevWarning("ai_speechfilter %s tries to use %s as a subject, but it's not a talking NPC.\n", STRING(GetEntityName()), STRING(pSearch->GetEntityName()) );
			}
#endif
			iNumSubjects++;
		}
	} while( pSearch );

	if ( !iNumSubjects )
	{
		// No subjects found by targetname! Assume classname.
		do
		{
			pSearch = gEntList.FindEntityByClassname( pSearch, STRING(m_iszSubject) );
			if( pSearch )
			{
#ifndef CSTRIKE_DLL
				CAI_PlayerAlly *pAlly = dynamic_cast<CAI_PlayerAlly *>(pSearch);
				if ( pAlly )
				{
					if( purge )
					{
						pAlly->SetSpeechFilter( nullptr );
					}
					else
					{
						if( pAlly->GetSpeechFilter() != nullptr )
						{
							DevWarning("ai_speechfilter %s is slamming NPC %s's current speech filter.\n", STRING(GetEntityName()), STRING(pSearch->GetEntityName()) );
						}

						pAlly->SetSpeechFilter( this );
					}
				}
				else if ( pSearch->IsNPC() )
				{
					DevWarning("ai_speechfilter %s tries to use %s as a subject, but it's not a talking NPC.\n", STRING(GetEntityName()), STRING(pSearch->GetEntityName()) );
				}
#endif
				iNumSubjects++;
			}
		} while( pSearch );
	}

	// If the subject list is still empty, we have an error!
	if ( !iNumSubjects )
	{
		DevMsg( 2, "ai_speechfilter finds no subject(s) called: %s\n", STRING( m_iszSubject ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::OnEntityCreated( CBaseEntity *pEntity )
{
	if ( !m_bDisabled && pEntity->NameMatches( m_iszSubject ) || pEntity->ClassMatches( m_iszSubject ) )
	{
#ifndef CSTRIKE_DLL
		CAI_PlayerAlly *pAlly = dynamic_cast<CAI_PlayerAlly *>(pEntity);
		if ( pAlly )
		{
			pAlly->SetSpeechFilter( this );
		}
		else if ( pEntity->IsNPC() )
		{
			DevWarning("ai_speechfilter %s tries to use %s as a subject, but it's not a talking NPC.\n", STRING(GetEntityName()), STRING(pEntity->GetEntityName()) );
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void CAI_SpeechFilter::OnEntityDeleted( CBaseEntity *pEntity )
{
}
