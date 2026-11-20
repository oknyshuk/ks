// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <stddef.h>
#include "client_class.h"
#include "declare_class.h"
#include "reflect_annotations.h"
module ks.client:cstrike15.cs_team;

import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :sharedvar;

class CEconQuestDefinition;
class CTeam;

using namespace ::ks::reflect;


// Purpose: Client side CTFTeam class

class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class CCSTeam : public CTeam
{
	[[= ks::reflect::NetTable{ .name = "DT_CSTeam" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CCSTeam, CTeam );
public:
	DECLARE_CLIENTCLASS();

					CCSTeam();
	virtual			~CCSTeam();
};

const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest );
bool Helper_GetDecoratedAssassinationTargetName( const CEconQuestDefinition *pQuest, wchar_t* pszBuffer, size_t nBuffSizeInCharacters );
