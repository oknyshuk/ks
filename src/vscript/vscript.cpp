// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2008 Valve Corporation

import std;
import ks.vstdlib;
#include "vscript/ivscript.h"
#include "appframework/apptiers.h"
#include "languages/squirrel/vsquirrel/vsquirrel.h"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CScriptManager : public IScriptManager
{
public:
	CScriptManager()
	{
	}

	// App system protocol (a tier 1 system)
	bool Connect( CreateInterfaceFn factory ) { return AppSystemTier1Connect( factory ); }
	void Disconnect() { AppSystemTier1Disconnect(); }
	void *QueryInterface( const char *pInterfaceName ) { return nullptr; }
	InitReturnVal_t Init() { return AppSystemTier1Init(); }
	void Shutdown() { AppSystemTier1Shutdown(); }

	IScriptVM *CreateVM( ScriptLanguage_t language )
	{
		IScriptVM *pVM = nullptr;

		if ( language == SL_SQUIRREL )
		{
			pVM = ScriptCreateSquirrelVM();
		}

		contract_assert( pVM && "Unknown script language\n" );
		if ( pVM )
		{
			pVM->Init();
			ScriptRegisterFunction( pVM, RandomFloat, "Generate a random floating point number within a range, inclusive" );
			ScriptRegisterFunction( pVM, RandomInt, "Generate a random integer within a range, inclusive" );
		}
		return pVM;
	}

	void DestroyVM( IScriptVM *p )
	{
		if ( p )
		{
			p->Shutdown();
			if ( p->GetLanguage() == SL_SQUIRREL )
			{
				ScriptDestroySquirrelVM( p );
			}
			else
				contract_assert( false && "Unknown script language\n" );
		}
	}
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CScriptManager g_ScriptManager;

static InterfaceReg s_Reg_CScriptManager_IScriptManager( []() -> void * { return static_cast<IScriptManager *>( &g_ScriptManager ); }, VSCRIPT_INTERFACE_VERSION );;

//-----------------------------------------------------------------------------
// The IScriptManager members forward to CScriptManager
//-----------------------------------------------------------------------------
#include "facade/iscriptmanager.inl"
