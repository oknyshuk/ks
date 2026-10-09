// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.tier1;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "cl_mat_stub.h"
#include "materialsystem/imaterialsystemstub.h"

// Hook the engine's mat_stub cvar.
ConVar mat_stub( "mat_stub", "0", FCVAR_CHEAT );
extern ConVar gl_clear;


IMaterialSystemStub* GetStubMaterialSystem()
{
	return materials_stub;
}

// ---------------------------------------------------------------------------------------- //
// CMatStubHandler implementation.
// ---------------------------------------------------------------------------------------- //

CMatStubHandler::CMatStubHandler()
{
	if ( mat_stub.GetInt() )
	{
		m_pOldMaterialSystem = materials;

		// Replace all material system pointers with the stub.
		GetStubMaterialSystem()->SetRealMaterialSystem( materials );
		materials->SetInStubMode( true );
		materials = GetStubMaterialSystem();
		engine->Mat_Stub( materials );
	}
	else
	{
		m_pOldMaterialSystem = 0;
	}
}


CMatStubHandler::~CMatStubHandler()
{
	End();
}


void CMatStubHandler::End()
{
	// Put back the original material system pointer.
	if ( m_pOldMaterialSystem )
	{
		materials = m_pOldMaterialSystem;
		materials->SetInStubMode( false );
		engine->Mat_Stub( materials );
		m_pOldMaterialSystem = 0;
//		if( gl_clear.GetBool() )
		{
			materials->ClearBuffers( true, true );
		}
	}
}


bool IsMatStubEnabled()
{
	return mat_stub.GetBool();
}
