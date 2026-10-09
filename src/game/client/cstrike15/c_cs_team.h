#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Client side CTFTeam class

#include "reflect_annotations.h"

#include "c_team.h"
#include "shareddefs.h"
#include "weapon_parse.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_CSTeam" } ]]
      C_CSTeam : public C_Team
{
	DECLARE_CLASS( C_CSTeam, C_Team );
public:
	DECLARE_CLIENTCLASS();

					C_CSTeam();
	virtual			~C_CSTeam();
};

const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest );
bool Helper_GetDecoratedAssassinationTargetName( const CEconQuestDefinition *pQuest, wchar_t* pszBuffer, size_t nBuffSizeInCharacters );
