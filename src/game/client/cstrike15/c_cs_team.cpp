// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Client side C_CSTeam class

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "engine/IEngineSound.h"
#include "hud.h"
#include "c_cs_team.h"
#include "localize/ilocalize.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_CSTeam, DT_CSTeam, CCSTeam )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CSTeam::C_CSTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CSTeam::~C_CSTeam()
{
}

const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest )
{
	if ( !pQuest )
		return nullptr;

	KeyValues *pTargetKV = pQuest->GetStringTokens()->FindKey( "target" );
	return pTargetKV ? pTargetKV->GetString() : nullptr;
}

bool Helper_GetDecoratedAssassinationTargetName( const CEconQuestDefinition *pQuest, wchar_t* pszBuffer, size_t nBuffSizeInCharacters )
{
	const char* szToken = Helper_GetLocalPlayerAssassinationQuestLocToken( pQuest );
	if ( wchar_t *wszUndecoratedName = g_pLocalize->Find( szToken ) )
	{
		V_snwprintf( pszBuffer, nBuffSizeInCharacters, L"<font color = '#FF0000'><i>" L"%ls" L"</i></font>", wszUndecoratedName );
		return true;
	}
	return false;
}
