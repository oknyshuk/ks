// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2008 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "recipientfilter.h"
#endif
#include "vscript_shared.h"
#include "filesystem.h"
#include "gamerules.h"

IScriptVM * g_pScriptVM;
extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

LoggingChannelID_t LOG_VScript = LoggingSystem_RegisterLoggingChannel( "VScript", nullptr, LCF_CONSOLE_ONLY, LS_WARNING );;
HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return nullptr;
	}

	static const char *pszExtensions[] =
	{
		"",		// SL_NONE
		".gm",	// SL_GAMEMONKEY
		".nut",	// SL_SQUIRREL
		".lua", // SL_LUA
		".py",  // SL_PYTHON
	};

	const char *pszVMExtension = pszExtensions[g_pScriptVM->GetLanguage()];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_strcmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
		Log_Warning( LOG_VScript, "Script file type does not match VM type\n" );
		return nullptr;
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath.sprintf( "scripts/vscripts/%s", pszScriptName );
	}
	else
	{	
		scriptPath.sprintf( "scripts/vscripts/%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	std::string scriptText;

	if ( g_pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = nullptr;
	}
	else
	{
		auto scriptData = filesystem->ReadFile( scriptPath, "GAME" );

		if( !scriptData )
		{
			Log_Warning( LOG_VScript, "Script not found (%s) \n", scriptPath.operator const char *() );
			contract_assert( "Error running script" );
		}

		if ( scriptData )
		{
			scriptText.assign( ks::text_view( *scriptData ) );
		}
		pBase = scriptText.empty() ? nullptr : scriptText.c_str();

		if ( !pBase || !*pBase )
		{
			return nullptr;
		}
	}


	const char *pszFilename = V_strrchr( scriptPath, '/' );
	pszFilename++;
	HSCRIPT hScript = g_pScriptVM->CompileScript( pBase, pszFilename );
	if ( !hScript )
	{
		Log_Warning( LOG_VScript, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
		contract_assert( "Error running script" );
	}
	return hScript;
}

static int g_ScriptServerRunScriptDepth;

bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	if ( !pszScriptName || !*pszScriptName )
	{
		Log_Warning( LOG_VScript, "Cannot run script: NULL script name\n" );
		return false;
	}

	// Prevent infinite recursion in VM
	if ( g_ScriptServerRunScriptDepth > 16 )
	{
		Log_Warning( LOG_VScript, "IncludeScript stack overflow\n" );
		return false;
	}

	g_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptCompileScript( pszScriptName, bWarnMissing );
	bool bSuccess = false;
	if ( hScript )
	{
#ifdef GAME_DLL
		if ( gpGlobals->maxClients == 1 )
		{
			CBaseEntity *pPlayer = UTIL_GetLocalPlayer();
			if ( pPlayer )
			{
				g_pScriptVM->SetValue( "player", pPlayer->GetScriptInstance() );
			}
		}
#endif
		bSuccess = ( g_pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
			Log_Warning( LOG_VScript, "Error running script named %s\n", pszScriptName );
			contract_assert( "Error running script" );
		}
	}
	g_ScriptServerRunScriptDepth--;
	return bSuccess;
}

#ifdef CLIENT_DLL
static void script_client( const CCommand &args );
static ConCommand script_client_command( "script_client", script_client, "Run the text as a script" );
static void script_client( const CCommand &args )
#else
static void script( const CCommand &args );
static ConCommand script_command( "script", script, "Run the text as a script" );
static void script( const CCommand &args )
#endif
{
#ifdef CLIENT_DLL
	if ( !engine->IsClientLocalToActiveServer() )
		return;
#else
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No function name specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	const char *pszScript = args.GetCommandString();

#ifdef CLIENT_DLL
	pszScript += 13;
#else
	pszScript += 6;
#endif
	
	while ( *pszScript == ' ' )
	{
		pszScript++;
	}

	if ( !*pszScript )
	{
		return;
	}

	if ( *pszScript != '\"' )
	{
		g_pScriptVM->Run( pszScript );
	}
	else
	{
		pszScript++;
		const char *pszEndQuote = pszScript;
		while ( *pszEndQuote !=  '\"' )
		{
			pszEndQuote++;
		}
		if ( !*pszEndQuote )
		{
			return;
		}
		*((char *)pszEndQuote) = 0;
		g_pScriptVM->Run( pszScript );
		*((char *)pszEndQuote) = '\"';
	}
}



#ifdef CLIENT_DLL
static void script_execute_client( const CCommand &args );
static ConCommand script_execute_client_command( "script_execute_client", script_execute_client, "Run a vscript file" );
static void script_execute_client( const CCommand &args )
#else
static void script_execute( const CCommand &args );
static ConCommand script_execute_command( "script_execute", script_execute, "Run a vscript file" );
static void script_execute( const CCommand &args )
#endif
{
#ifdef CLIENT_DLL
	if ( !engine->IsClientLocalToActiveServer() )
		return;
#else
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No script specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	VScriptRunScript( args[1], true );
}

#ifdef CLIENT_DLL
static void script_debug_client( const CCommand &args );
static ConCommand script_debug_client_command( "script_debug_client", script_debug_client, "Connect the vscript VM to the script debugger" );
static void script_debug_client( const CCommand &args )
#else
static void script_debug( const CCommand &args );
static ConCommand script_debug_command( "script_debug", script_debug, "Connect the vscript VM to the script debugger" );
static void script_debug( const CCommand &args )
#endif
{
#ifdef CLIENT_DLL
	if ( !engine->IsClientLocalToActiveServer() )
		return;
#else
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->ConnectDebugger();
}

#ifdef CLIENT_DLL
static void script_help_client( const CCommand &args );
static ConCommand script_help_client_command( "script_help_client", script_help_client, "Output help for script functions, optionally with a search string" );
static void script_help_client( const CCommand &args )
#else
static void script_help( const CCommand &args );
static ConCommand script_help_command( "script_help", script_help, "Output help for script functions, optionally with a search string" );
static void script_help( const CCommand &args )
#endif
{
#ifdef CLIENT_DLL
	if ( !engine->IsClientLocalToActiveServer() )
		return;
#else
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	const char *pszArg1 = "*";
	if ( *args[1] )
	{
		pszArg1 = args[1];
	}

	g_pScriptVM->Run( CFmtStr( "PrintHelp( \"%s\" );", pszArg1 ) );
}

#ifdef CLIENT_DLL
static void script_dump_all_client( const CCommand &args );
static ConCommand script_dump_all_client_command( "script_dump_all_client", script_dump_all_client, "Dump the state of the VM to the console" );
static void script_dump_all_client( const CCommand &args )
#else
static void script_dump_all( const CCommand &args );
static ConCommand script_dump_all_command( "script_dump_all", script_dump_all, "Dump the state of the VM to the console" );
static void script_dump_all( const CCommand &args )
#endif
{
#ifdef CLIENT_DLL
	if ( !engine->IsClientLocalToActiveServer() )
		return;
#else
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
#endif

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->DumpState();
}
