// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ai_responsesystem.h"
#include "igamesystem.h"
#include "ai_criteria.h"
#include "filesystem.h"
#include "ai_speech.h"
#include "sceneentity.h"
#include "isaverestore.h"
#include "multiplay_gamerules.h"
#include "responserules/response_host_interface.h"
#include "responserules/response_types_internal.h"

using namespace ResponseRules;



static characterset_t	g_BreakSetIncludingColons;

// Simple class to initialize breakset
class CBreakInit
{
public:
	CBreakInit()
	{
		CharacterSetBuild( &g_BreakSetIncludingColons, "{}()':" );
	}
} g_BreakInit;

inline char rr_tolower( char c )
{
	if ( c >= 'A' && c <= 'Z' )
		return c - 'A' + 'a';
	return c;
}
// BUG BUG:  Note that this function doesn't check for data overruns!!!
// Also, this function lowercases the token as it parses!!!
inline const char *RR_Parse(const char *data, char *token )
{
	unsigned char    c;
	int             len;
	characterset_t	*breaks = &g_BreakSetIncludingColons;
	len = 0;
	token[0] = 0;

	if (!data)
		return nullptr;

	// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return nullptr;                    // end of file;
		data++;
	}

	// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = rr_tolower( *data++ );
			if (c=='\"' || !c)
			{
				token[len] = 0;
				return data;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if ( IN_CHARACTERSET( *breaks, c ) )
	{
		token[len] = c;
		len++;
		token[len] = 0;
		return data+1;
	}

	// parse a regular word
	do
	{
		token[len] = rr_tolower( c );
		data++;
		len++;
		c = rr_tolower( *data );
		if ( IN_CHARACTERSET( *breaks, c ) )
			break;
	} while (c>32);

	token[len] = 0;
	return data;
}

// Host functions required by the ResponseRules::IEngineEmulator interface
class CResponseRulesToEngineInterface : public ResponseRules::IEngineEmulator
{
	/// Given an input text buffer data pointer, parses a single token into the variable token and returns the new
	///  reading position
	virtual const char			*ParseFile( const char *data, char *token, int maxlen ) 
	{
		(void)maxlen;
		return RR_Parse( data, token );
	}

	/// Return a pointer to an IFileSystem we can use to read and process scripts.
	virtual IFileSystem *GetFilesystem() 
	{
		return filesystem;
	}

	/// Return a pointer to an instance of CUniformRandomStream
	virtual CUniformRandomStream *GetRandomStream() 
	{
		return random;
	}

	/// Return a pointer to a tier0 ICommandLine
	virtual ICommandLine *GetCommandLine() 
	{
		return CommandLine();
	}

	/// Emulates the server's UTIL_LoadFileForMe
	virtual byte *LoadFileForMe( const char *filename, int *pLength )
	{
		return UTIL_LoadFileForMe( filename, pLength );
	}

	/// Emulates the server's UTIL_FreeFile
	virtual void  FreeFile( byte *buffer ) 
	{
		return UTIL_FreeFile( buffer );
	}

};

CResponseRulesToEngineInterface g_ResponseRulesEngineWrapper;
static const bool s_bEngineEmulatorInstalled = ( IEngineEmulator::s_pSingleton = &g_ResponseRulesEngineWrapper, true );


IMPLEMENT_REFLECT_DATAMAP_FILL( ParserResponse )


IMPLEMENT_REFLECT_DATAMAP_FILL( ResponseGroup )


/// Add some game-specific code to the basic response system
/// (eg, the scene precacher, which requires the client and server
///  to work)

class CGameResponseSystem : public CResponseSystem
{
public:
	CGameResponseSystem();

	virtual void Precache();
	virtual void PrecacheResponses( bool bEnable )
	{
		m_bPrecache = bEnable;
	}
	bool		ShouldPrecache()	{ return m_bPrecache; }

protected:
	bool		m_bPrecache;	
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGameResponseSystem::CGameResponseSystem() : m_bPrecache(true)
{};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------


static void TouchFile( char const *pchFileName )
{
	IEngineEmulator::Get()->GetFilesystem()->Size( pchFileName );
}

void CGameResponseSystem::Precache()
{
	bool bTouchFiles = CommandLine()->FindParm( "-makereslists" ) != 0;

	// enumerate and mark all the scripts so we know they're referenced
	for ( auto &[name, group] : m_Responses )
	{
		for ( int j = 0; j < std::ssize( group.group ); j++)
		{
			ParserResponse &response = group.group[j];

			switch ( response.type )
			{
			default:
				break;
			case RESPONSE_SCENE:
				{
					// fixup $gender references
					char file[k_cMaxPath];
					V_strncpy( file, response.value, sizeof(file) );
					char *gender = std::strstr( file, "$gender" );
					if ( gender )
					{
						// replace with male & female
						const char *postGender = gender + std::strlen("$gender");
						*gender = 0;
						char genderFile[k_cMaxPath];
						// male
						V_snprintf( genderFile, sizeof(genderFile), "%smale%s", file, postGender);

						PrecacheInstancedScene( genderFile );
						if ( bTouchFiles )
						{
							TouchFile( genderFile );
						}

						V_snprintf( genderFile, sizeof(genderFile), "%sfemale%s", file, postGender);

						PrecacheInstancedScene( genderFile );
						if ( bTouchFiles )
						{
							TouchFile( genderFile );
						}
					}
					else
					{
						PrecacheInstancedScene( file );
						if ( bTouchFiles )
						{
							TouchFile( file );
						}
					}
				}
				break;
			case RESPONSE_SPEAK:
				{
					CBaseEntity::PrecacheScriptSound( response.value );
				}
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: The default response system for expressive AIs
//-----------------------------------------------------------------------------
class CDefaultResponseSystem : public CGameResponseSystem, public CAutoGameSystem
{
	typedef CAutoGameSystem BaseClass;

public:
	CDefaultResponseSystem() : CAutoGameSystem( "CDefaultResponseSystem" )
	{
	}

	virtual const char *GetScriptFile( void ) 
	{
		return "scripts/talker/response_rules.txt";
	}

	// CAutoServerSystem
	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPostEntity()
	{
	}

	virtual void Release()
	{
		contract_assert( 0 );
	}

	virtual void LevelInitPreEntity()
	{
		// This will precache the default system

		// FIXME:  This is SLOW the first time you run the engine (can take 3 - 10 seconds!!!)
		if ( ShouldPrecache() )
		{
			Precache();
		}

		ResetResponseGroups();
	}

	void ReloadAllResponseSystems()
	{
		Clear();
		Init();

		// precache sounds in case we added new ones
		Precache();

	}

	friend void CC_RR_DumpHashInfo( const CCommand &args );
};

static CDefaultResponseSystem defaultresponsesytem;
IResponseSystem *g_pResponseSystem = &defaultresponsesytem;

static void rr_reloadresponsesystems( const CCommand &args );
static ConCommand rr_reloadresponsesystems_command( "rr_reloadresponsesystems", rr_reloadresponsesystems, "Reload all response system scripts.", FCVAR_CHEAT );
static void rr_reloadresponsesystems( const CCommand &args )
{
	defaultresponsesytem.ReloadAllResponseSystems();
}

static short RESPONSESYSTEM_SAVE_RESTORE_VERSION = 1;

// note:  this won't save/restore settings from instanced response systems.  Could add that with a CDefSaveRestoreOps implementation if needed
// 


//-----------------------------------------------------------------------------
// CResponseSystemSaveRestoreOps
//
// Purpose: Handles save and load for instanced response systems...
//
// BUGBUG:  This will save the same response system to file multiple times for "shared" response systems and 
//  therefore it'll restore the same data onto the same pointer N times on reload (probably benign for now, but we could
//  write code to save/restore the instanced ones by filename in the block handler above maybe?
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDefaultResponseSystem::Init()
{
	/*
	Warning( "sizeof( Response ) == %d\n", sizeof( Response ) );
	Warning( "sizeof( ResponseGroup ) == %d\n", sizeof( ResponseGroup ) );
	Warning( "sizeof( Criteria ) == %d\n", sizeof( Criteria ) );
	Warning( "sizeof( AI_ResponseParams ) == %d\n", sizeof( AI_ResponseParams ) );
	*/
	const char *basescript = GetScriptFile();

	LoadRuleSet( basescript );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDefaultResponseSystem::Shutdown()
{
	// Clear outselves
	Clear();
	// IServerSystem chain
	BaseClass::Shutdown();
}
