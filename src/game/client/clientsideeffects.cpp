// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation
// Copyright (c) 2026 oknyshuk
// Copyright (c) Valve Corporation

module;
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "collisionutils.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "tier2/resourceprecacher.h"
#include "cdll_client_int.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "cmodel.h"
#include "dt_recv.h"
#include "iclientrenderable.h"
#include "vphysics_interface.h"
#include "worldsize.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "dt_send.h"
#include "engine/ICollideable.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "predictable_entity.h"
#include "client_class.h"
#include "declare_class.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"
#include "materialsystem/icustommaterial.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "mathlib/camera.h"
module ks.client;

import ks.core.dbg;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import :iinput;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import std;
import ks.core.types;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.core.numeric;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.tslist;
import ks.math.aabb;
import ks.math.ssemath;
import ks.math.vmatrix;
import ks.math.transform;
import ks.tier1.vector_ops;
import :triggers;
import ks.math.vector4d;
import ks.math.volumeculler;
import ks.math.vplane;
import ks.tier1.exprevaluator;

bool g_FXCreationAllowed = false;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void SetFXCreationAllowed( bool state )
{
	g_FXCreationAllowed = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FXCreationAllowed( void )
{
	return g_FXCreationAllowed;
}

// TODO:  Sort effects and their children back to front from view positon?  At least with buckets or something.

//
//-----------------------------------------------------------------------------
// Purpose: Construct and activate effect
// Input  : *name - 
//-----------------------------------------------------------------------------
CClientSideEffect::CClientSideEffect( const char *name )
{
	m_pszName = name;
	contract_assert( name );

	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: Destroy effect
//-----------------------------------------------------------------------------
CClientSideEffect::~CClientSideEffect( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get name of effect
// Output : const char
//-----------------------------------------------------------------------------
const char *CClientSideEffect::GetName( void )
{
	return m_pszName;
}

//-----------------------------------------------------------------------------
// Purpose: Is effect still active?
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CClientSideEffect::IsActive( void )
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: Mark effect for destruction
//-----------------------------------------------------------------------------
void CClientSideEffect::Destroy( void )
{
	m_bActive = false;
}

static constexpr auto MAX_EFFECTS = 256;

//-----------------------------------------------------------------------------
// Purpose: Implements effects list interface
//-----------------------------------------------------------------------------
class CEffectsList : public IEffectsList
{
public:
					CEffectsList( void );
	virtual			~CEffectsList( void );

	//	Add an effect to the effects list
	void			AddEffect( CClientSideEffect *effect );
	// Remove the specified effect
	// Draw/update all effects in the current list
	void			DrawEffects( double frametime );
	// Flush out all effects from the list
	void			Flush( void );
private:
	void			RemoveEffect( int effectIndex );
	// Current number of effects
	int				m_nEffects;
	// Pointers to current effects
	CClientSideEffect *m_rgEffects[ MAX_EFFECTS ];
};

// Implements effects list and exposes interface
static CEffectsList g_EffectsList;
// Public interface
IEffectsList *clienteffects = ( IEffectsList * )&g_EffectsList;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEffectsList::CEffectsList( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEffectsList::~CEffectsList( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Add effect to effects list
// Input  : *effect - 
//-----------------------------------------------------------------------------
void CEffectsList::AddEffect( CClientSideEffect *effect )
{

	if ( effect == nullptr )
		return;

	if ( m_nEffects >= MAX_EFFECTS )
	{
		DevWarning( 1, "No room for effect %s\n", effect->GetName() );
		return;
	}

	m_rgEffects[ m_nEffects++ ] = effect;
}

//-----------------------------------------------------------------------------
// Purpose: Remove specified effect by index
// Input  : effectIndex - 
//-----------------------------------------------------------------------------
void CEffectsList::RemoveEffect( int effectIndex )
{
	if ( effectIndex >= m_nEffects || effectIndex < 0 )
		return;

	CClientSideEffect *pEffect = m_rgEffects[effectIndex];
	m_nEffects--;

	if ( m_nEffects > 0 && effectIndex != m_nEffects )
	{
		// move the last one down to fill the empty slot
		m_rgEffects[effectIndex] = m_rgEffects[m_nEffects];
	}

	pEffect->Destroy();

	delete pEffect;	//FIXME: Yes, no?
}

//-----------------------------------------------------------------------------
// Purpose: Iterate through list and simulate/draw stuff
// Input  : frametime - 
//-----------------------------------------------------------------------------
void CEffectsList::DrawEffects( double frametime )
{
	CVProfScope _( "CEffectsList::DrawEffects", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	int i;
	CClientSideEffect *effect;

	// Go backwards so deleting effects doesn't screw up
	for ( i = m_nEffects - 1 ; i >= 0; i-- )
	{
		effect = m_rgEffects[ i ];
		if ( !effect )
			continue;

		// Simulate
		effect->Draw( frametime );

		// Remove it if needed
		if ( !effect->IsActive() )
		{
			RemoveEffect( i );
		}
	}
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CEffectsList::Flush( void )
{
	int i;
	CClientSideEffect *effect;

	// Go backwards so deleting effects doesn't screw up
	for ( i = m_nEffects - 1 ; i >= 0; i-- )
	{
		effect = m_rgEffects[ i ];
		
		if ( effect == nullptr )
			continue;

		RemoveEffect( i );
	}
}

class CBaseEntity;
struct FXLineData_t;
struct FXQuadData_t;

using namespace ::ks::reflect;

//FIXME: All these functions will be moved out to FX_Main.CPP or a individual folder

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectsTest )
PRECACHE( MATERIAL, "effects/spark" )
PRECACHE( MATERIAL, "effects/gunshiptracer" )
PRECACHE( MATERIAL, "effects/bluespark" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void FX_AddLine( const FXLineData_t &data )
{
	CFXLine *t = new CFXLine( "Line", data );
	contract_assert( t );

	//Throw it into the list
	clienteffects->AddEffect( t );
}

/*
==================================================
FX_AddStaticLine
==================================================
*/

void FX_AddStaticLine( const Vector& start, const Vector& end, float scale, float life, const char *materialName, unsigned char flags )
{
	CFXStaticLine	*t = new CFXStaticLine( "StaticLine", start, end, scale, life, materialName, flags );
	contract_assert( t );

	//Throw it into the list
	clienteffects->AddEffect( t );
}

/*
==================================================
FX_AddDiscreetLine
==================================================
*/

void FX_AddDiscreetLine( const Vector& start, const Vector& direction, float velocity, float length, float clipLength, float scale, float life, const char *shader )
{
	CFXDiscreetLine	*t = new CFXDiscreetLine( "Line", start, direction, velocity, length, clipLength, scale, life, shader );
	contract_assert( t );

	//Throw it into the list
	clienteffects->AddEffect( t );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FX_AddQuad( const FXQuadData_t &data )
{
	CFXQuad *quad = new CFXQuad( data );

	contract_assert( quad != nullptr );

	clienteffects->AddEffect( quad );
}
//-----------------------------------------------------------------------------
// Purpose: Parameter heavy version
//-----------------------------------------------------------------------------
void FX_AddQuad( const Vector &origin, 
				 const Vector &normal, 
				 float startSize, 
				 float endSize, 
				 float sizeBias,
				 float startAlpha, 
				 float endAlpha,
				 float alphaBias,
				 float yaw,
				 float deltaYaw,
				 const Vector &color, 
				 float lifeTime, 
				 const char *shader, 
				 unsigned int flags )
{
	FXQuadData_t data;

	//Setup the data
	data.SetAlpha( startAlpha, endAlpha );
	data.SetScale( startSize, endSize );
	data.SetFlags( flags );
	data.SetMaterial( shader );
	data.SetNormal( normal );
	data.SetOrigin( origin );
	data.SetLifeTime( lifeTime );
	data.SetColor( color[0], color[1], color[2] );
	data.SetScaleBias( sizeBias );
	data.SetAlphaBias( alphaBias );
	data.SetYaw( yaw, deltaYaw );

	//Output it
	FX_AddQuad( data );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a test effect
//-----------------------------------------------------------------------------
void CreateTestEffect( void )
{
	//NOTENOTE: Add any test effects here
	//FX_BulletPass( NULL, NULL );
}

/*
==================================================
FX_PlayerTracer
==================================================
*/

static constexpr auto TRACER_BASE_OFFSET = 8;

void FX_PlayerTracer( Vector& start, Vector& end )
{
	CVProfScope _( "FX_PlayerTracer", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	Vector	shotDir, dStart, dEnd;
	float	length;

	//Find the direction of the tracer
	VectorSubtract( end, start, shotDir );
	length = VectorNormalize( shotDir );

	//We don't want to draw them if they're too close to us
	if ( length < 256 )
		return;

	//Randomly place the tracer along this line, with a random length
	VectorMA( start, TRACER_BASE_OFFSET + random_valve->RandomFloat( -24.0f, 64.0f ), shotDir, dStart );
	VectorMA( dStart, ( length * random_valve->RandomFloat( 0.1f, 0.6f ) ), shotDir, dEnd );

	//Create the line
	CFXStaticLine	*t;
	const char		*materialName;

	//materialName = ( random->RandomInt( 0, 1 ) ) ? "effects/tracer_middle" : "effects/tracer_middle2";
	materialName = "effects/spark";

	t = new CFXStaticLine( "Tracer", dStart, dEnd, random_valve->RandomFloat( 0.5f, 0.75f ), 0.01f, materialName, 0 );
	contract_assert( t );

	//Throw it into the list
	clienteffects->AddEffect( t );
}

/*
==================================================
FX_Tracer
==================================================
*/
// Tracer must be this close to be considered for hearing
static constexpr auto TRACER_MAX_HEAR_DIST = (6*12);
static constexpr auto TRACER_SOUND_TIME_MIN = 0.1f;
static constexpr auto TRACER_SOUND_TIME_MAX = 0.1f;

class CBulletWhizTimer : public CAutoGameSystem
{
public:
	CBulletWhizTimer( char const *name ) : CAutoGameSystem( name )
	{
	}

	void LevelInitPreEntity()
	{
		m_nextWhizTime = 0;
	}

	float	m_nextWhizTime;
};

// Client-side tracking for whiz noises
CBulletWhizTimer g_BulletWhiz( "CBulletWhizTimer" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecStart - 
//			&vecDir - 
//			iTracerType - 
//-----------------------------------------------------------------------------
static constexpr auto LISTENER_HEIGHT = 24;

ConVar cl_tracer_whiz_distance( side_registry(), "cl_tracer_whiz_distance", "72" );	// putting TRACER_MAX_HEAR_DIST on a cvar, so KellyT can find a good value

void FX_TracerSound( const Vector &start, const Vector &end, int iTracerType )
{
	const char *pszSoundName = nullptr;
	float flWhizDist = TRACER_MAX_HEAR_DIST;
	float flMinWhizTime = TRACER_SOUND_TIME_MIN;
	float flMaxWhizTime = TRACER_SOUND_TIME_MAX;
	HACK_GETLOCALPLAYER_GUARD( "FX_TracerSound" );
	Vector vecListenOrigin = MainViewOrigin(GET_ACTIVE_SPLITSCREEN_SLOT());
	switch( iTracerType )
	{
	case TRACER_TYPE_DEFAULT:
		{
			pszSoundName = "Bullets.DefaultNearmiss";
			flWhizDist = cl_tracer_whiz_distance.GetFloat();	// was 24

			Ray_t bullet, listener;
			bullet.Init( start, end );

			Vector vecLower = vecListenOrigin;
			vecLower.z -= LISTENER_HEIGHT;
			listener.Init( vecListenOrigin,	vecLower );

			float s, t;
			IntersectRayWithRay( bullet, listener, s, t );
			t = clamp( t, 0, 1 );
			vecListenOrigin.z -= t * LISTENER_HEIGHT;
		}
		break;

	case TRACER_TYPE_GUNSHIP:
		pszSoundName = "Bullets.GunshipNearmiss";
		break;

	case TRACER_TYPE_STRIDER:
		pszSoundName = "Bullets.StriderNearmiss";
		break;

	case TRACER_TYPE_WATERBULLET:
		pszSoundName = "Underwater.BulletImpact";
		flWhizDist = 48;
		flMinWhizTime = 0.3f;
		flMaxWhizTime = 0.6f;
		break;

	default:
		return;
	}

	if( !pszSoundName )
		return;

	// Is it time yet?
	float dt = g_BulletWhiz.m_nextWhizTime - gpGlobals->curtime;
	if ( dt > 0 )
		return;

	// Did the thing pass close enough to our head?
	float vDist = CalcDistanceSqrToLineSegment( vecListenOrigin, start, end );
	if ( vDist >= (flWhizDist * flWhizDist) )
		return;

	CSoundParameters params;
	if( CBaseEntity::GetParametersForSound( pszSoundName, params, nullptr ) )
	{
		// Get shot direction
		Vector shotDir;
		VectorSubtract( end, start, shotDir );
		VectorNormalize( shotDir );

		CLocalPlayerFilter filter;
		enginesound->EmitSound(	filter, SOUND_FROM_WORLD, CHAN_STATIC, pszSoundName, SOUNDEMITTER_INVALID_HASH, params.soundname, 
			params.volume, SNDLVL_TO_ATTN(params.soundlevel), params.m_nRandomSeed, 0, params.pitch, &start, &shotDir, nullptr);
	}

	// FIXME: This has a bad behavior when both bullet + strider shots are whizzing by at the same time
	// Could use different timers for the different types.

	// Don't play another bullet whiz for this client until this time has run out
	g_BulletWhiz.m_nextWhizTime = gpGlobals->curtime + random_valve->RandomFloat( flMinWhizTime, flMaxWhizTime );
}

void FX_Tracer( Vector& start, Vector& end, int velocity, bool makeWhiz )
{
	CVProfScope _( "FX_Tracer", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	//Don't make small tracers
	float dist;
	Vector dir;

	VectorSubtract( end, start, dir );
	dist = VectorNormalize( dir );
	int minDist;
	float flMinWidth;
	float flMaxWidth;

	// Don't make short tracers.
	minDist = 256;
	flMinWidth = 0.75;
	flMaxWidth = 0.9;

	if ( dist >= minDist )
	{
		float length = random_valve->RandomFloat( 64.0f, 128.0f );
		float life = ( dist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well
		
		//Add it
		FX_AddDiscreetLine( start, dir, velocity, length, dist, random_valve->RandomFloat( flMinWidth, flMaxWidth ), life, "effects/spark" );
	}

	if( makeWhiz )
	{
		FX_TracerSound( start, end, TRACER_TYPE_DEFAULT );	
	}
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Console GPU alignment stub (not used on PC)
#ifndef GPU_RESOLVE_ALIGNMENT
#define GPU_RESOLVE_ALIGNMENT 1
#endif

bool IsWidescreen( void );

void ss_pipsplit_changed( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	UI_OnSplitScreenStateChanged();
}
static ConVar ss_pipsplit( side_registry(), "ss_pipsplit", "1", 0, "If enabled, use PIP instead of splitscreen. (Only works for 2 players)", ss_pipsplit_changed );
static ConVar ss_pipscale( side_registry(), "ss_pipscale", "0.3f", 0, "Scale of the PIP aspect ratio to our resolution.", ss_pipsplit_changed );
static ConVar ss_pip_right_offset( side_registry(), "ss_pip_right_offset", "25", 0, "PIP offset vector from the right of the screen", ss_pipsplit_changed );
static ConVar ss_pip_bottom_offset( side_registry(), "ss_pip_bottom_offset", "25", 0, "PIP offset vector from the bottom of the screen", ss_pipsplit_changed );
static ConVar ss_force_primary_fullscreen( side_registry(), "ss_force_primary_fullscreen", "0", 0, "If enabled, all splitscreen users will only see the first user's screen full screen", ss_pipsplit_changed );
bool UI_UsePipSplit();

void ss_verticalsplit_changed( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.GetBool() != !!(int)flOldValue )
	{
		UI_OnSplitScreenStateChanged();

		if ( GetFullscreenClientMode() )
		{
			// we have to force re-layout, because the screen dimensions haven't changed,
			// but our layout is going to be different.
			GetFullscreenClientMode()->Layout( true );
		}

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( i );
			GetClientMode()->Layout();
			GetHud().OnSplitScreenStateChanged();
		}
	}
}
static ConVar ss_verticalsplit( side_registry(), "ss_verticalsplit", "0", 0, "Two player split screen uses vertical split (do not set this directly, use ss_splitmode instead).", ss_verticalsplit_changed );

void ss_splitmode_changed( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( !IsWidescreen() )
	{
		// Non-widescreen is alway horizontal
		ss_verticalsplit.SetValue( 0 );
	}
	else
	{
		if ( var.GetInt() == 1 )
		{
			// Horizontal
			ss_verticalsplit.SetValue( 0 );
		}
		else if ( var.GetInt() == 2 )
		{
			// Vertical
			ss_verticalsplit.SetValue( 1 );
		}
		else
		{
			// Vertical is default for widescreen
			ss_verticalsplit.SetValue( 1 );
		}
	}
}
static ConVar ss_splitmode( side_registry(), "ss_splitmode", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Two player split screen mode (0 - recommended settings base on the width, 1 - horizontal, 2 - vertical (only allowed in widescreen)", ss_splitmode_changed );
static ConVar ss_enable( side_registry(), "ss_enable", "0", FCVAR_RELEASE, "Enables Split Screen support. Play Single Player now launches into split screen mode. NO ONLINE SUPPORT" );

class CSplitScreenLetterBox
{
public:

	enum
	{
		SPLITSCREEN_NONWIDESCREEN_HORIZONTAL_SPLIT = 0,
		SPLITSCREEN_WIDESCREEN_HORIZONTAL_SPLIT,
		SPLITSCREEN_WIDESCREEN_VERTICAL_SPLIT,

		NUM_SPLITSCREEN_TYPES,
	};

	void Init();

	void SetNumSplitScreenPlayers( int nPlayers );

	bool GetSettings( bool *pbInsetHud, float *pflAspect, float *pFOV, float *pViewmodelFOV );

private:

	struct LetterBox_t
	{
		LetterBox_t() : m_flAspectRatio( 4.0f / 3.0f ), m_bInsetHud( false ) {}
		float	m_flAspectRatio;
		bool	m_bInsetHud;
		float	m_flFOV;
		float 	m_flViewModelFOV;
	};

	bool		m_bValid;
	LetterBox_t	m_Settings[ NUM_SPLITSCREEN_TYPES ];
	int			m_nSplitScreenPlayers;
};

void CSplitScreenLetterBox::Init()
{
	m_nSplitScreenPlayers = 1;
	char const *pchSlotNames[] = { "nonwidescreen", "widescreen_horizontal_split", "widescreen_vertical_split" };
	char const *pchConfigFile = "splitscreen_config.txt";

	m_bValid = true;
	KeyValues *kv = new KeyValues( "splitscreen" );
	if ( kv->LoadFromFile( g_pFullFileSystem, pchConfigFile, "MOD" ) )
	{
		for ( int i = 0; i < NUM_SPLITSCREEN_TYPES && m_bValid; ++i )
		{
			KeyValues *settings = kv->FindKey( pchSlotNames[ i ], false );
			if ( settings )
			{
				// Get settings
				char const *pchAspect = settings->GetString( "aspect", "4 by 3" );
				if ( pchAspect )
				{
					// Allowable syntax is "16 by 9" or "16 x 9" or "1.77"
					if ( V_stristr( pchAspect, " by " ) )
					{
						float f1, f2;
						if ( 2 == sscanf( pchAspect, "%f by %f", &f1, &f2 ) && f2 > 0.001f )
						{
							m_Settings[ i ].m_flAspectRatio = f1 / f2;
						}
						else
						{
							Error( "%s:  Invalid aspect ratio string '%s'\n", pchConfigFile, pchAspect );
							m_bValid = false;
						}
					}
					else if ( V_stristr( pchAspect, " x " ) )
					{
						float f1, f2;
						if ( 2 == sscanf( pchAspect, "%f x %f", &f1, &f2 ) && f2 > 0.001f )
						{
							m_Settings[ i ].m_flAspectRatio = f1 / f2;
						}
						else
						{
							Error( "%s:  Invalid aspect ratio string '%s'\n", pchConfigFile, pchAspect );
							m_bValid = false;
						}
					}
					else if ( V_atof( pchAspect ) > 0.1f )
					{
						m_Settings[ i ].m_flAspectRatio = V_atof( pchAspect );
					}
					else
					{
						Error( "%s:  Invalid aspect ratio string '%s'\n", pchConfigFile, pchAspect );
						m_bValid = false;
					}
				}

				// Get inset for hud
				m_Settings[ i ].m_bInsetHud = settings->GetBool( "insethud", false );

				// Get FOV
				m_Settings[ i ].m_flFOV = settings->GetFloat( "fov", 90.0f );

				// Get viewmodel FOVs
				m_Settings[ i ].m_flViewModelFOV = settings->GetFloat( "viewmodelfov", 50.0f );
			}
			else
			{
				Error( "%s:  Missing settings block for split screen mode '%s'\n", pchConfigFile, pchSlotNames[ i ] );
				m_bValid = false;
				break;
			}
		}
	}
	else
	{
		Msg( "No split screen config file '%s', using defaults\n", pchConfigFile );
		m_bValid = false;
	}
	kv->deleteThis();
}

void CSplitScreenLetterBox::SetNumSplitScreenPlayers( int nPlayers )
{
	m_nSplitScreenPlayers = nPlayers;
}

bool IsWidescreen( void )
{
	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
	return aspectRatioInfo.m_bIsWidescreen;
}

bool CSplitScreenLetterBox::GetSettings( bool *pbInsetHud, float *pflAspect, float *pFOV, float *pViewModelFOV )
{
	contract_assert( pbInsetHud );
	contract_assert( pflAspect );
	contract_assert( pFOV );
	contract_assert( pViewModelFOV );
	static bool bUsedDefaultsLastTime = false;
	if ( !m_bValid || m_nSplitScreenPlayers == 1 || UI_UsePipSplit() || ss_force_primary_fullscreen.GetBool() )
	{
		if ( !bUsedDefaultsLastTime )
		{
			bUsedDefaultsLastTime = true;
		}
		*pbInsetHud = false;
		*pflAspect = 4.0f / 3.0f;
		// FIXME: These are the non-splitscreen defaults for L4D.  This code needs to be sanitized for other games.
		*pFOV = 90.0f;
		*pViewModelFOV = 50.0f;
		return false;
	}

	// Figure out which splitscreen mode to use based on current configuration.
	int slot;
	if ( IsWidescreen() )
	{
		if ( ss_verticalsplit.GetBool() )
		{
			slot = SPLITSCREEN_WIDESCREEN_VERTICAL_SPLIT;
		}
		else
		{
			slot = SPLITSCREEN_WIDESCREEN_HORIZONTAL_SPLIT;
		}
	}
	else
	{
		slot = SPLITSCREEN_NONWIDESCREEN_HORIZONTAL_SPLIT;
	}

	bUsedDefaultsLastTime = false;

	const LetterBox_t &lb = m_Settings[ slot ];

	*pbInsetHud = lb.m_bInsetHud;
	*pflAspect = lb.m_flAspectRatio;
	*pFOV = lb.m_flFOV;
	*pViewModelFOV = lb.m_flViewModelFOV;
	return true;
}

static CSplitScreenLetterBox g_LetterBox;

static void ss_reloadletterbox( const CCommand &args );
static ConCommand ss_reloadletterbox_command( side_registry(), "ss_reloadletterbox", ss_reloadletterbox, "ss_reloadletterbox" );
static void ss_reloadletterbox( const CCommand &args )
{
	g_LetterBox.Init();
	UI_OnSplitScreenStateChanged();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( i );
		GetClientMode()->Layout();
		GetHud().OnSplitScreenStateChanged();
	}
}

static void UI_OneTimeInit()
{
	static bool initialized = false;
	if ( initialized )
		return;
	initialized = true;

	g_LetterBox.Init();
}

bool UI_Startup( CreateInterfaceFn appSystemFactory )
{
	UI_OneTimeInit();
	UI_OnSplitScreenStateChanged();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UI_CreateGlobalPanels( void )
{
}

void UI_Shutdown()
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
		if ( GetClientMode() )
		{
			GetClientMode()->UI_Shutdown();

			if ( hh == 0 )
			{
				GetFullscreenClientMode()->UI_Shutdown();
			}
		}
	}
}

static ConVar cl_showpausedimage( side_registry(), "cl_showpausedimage", "1", 0, "Show the 'Paused' image when game is paused." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void GetHudSize( int& w, int &h )
{
	engine->GetScreenSize( w, h );
}

static vrect_t g_TrueScreenSize;
static vrect_t g_ScreenSpaceBounds[ MAX_SPLITSCREEN_CLIENTS ];

void UI_GetTrueScreenSize( int &w, int &h )
{
	w = g_TrueScreenSize.width;
	h = g_TrueScreenSize.height;
}

void UI_SetScreenSpaceBounds( int slot, int x, int y, int w, int h )
{
	vrect_t &r = g_ScreenSpaceBounds[ slot ];
	r.x = x;
	r.y = y;
	r.width = w;
	r.height = h;
}

void UI_UpdateScreenSpaceBounds( int nNumSplits, int sx, int sy, int sw, int sh )
{
	g_TrueScreenSize.x = sx;
	g_TrueScreenSize.y = sy;
	g_TrueScreenSize.width = sw;
	g_TrueScreenSize.height = sh;

	std::vector< int > validSlots;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		validSlots.push_back( i );
	}

	contract_assert( std::ssize( validSlots ) == nNumSplits );

	switch ( nNumSplits )
	{
	default:
	case 1:
		// Make it screen sized
		{
			UI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
		}
		break;
	case 2:
		{
			if ( ss_force_primary_fullscreen.GetBool() )
			{
				// fullscreen
				UI_SetScreenSpaceBounds( validSlots[ 0 ], 0, 0, sw, sh );
				UI_SetScreenSpaceBounds( validSlots[ 1 ], sw, sh, 1, 1 );
			}
			else if ( UI_UsePipSplit() )
			{
				UI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
				// scale with PIP resolution
				float flPIPScale = ss_pipscale.GetFloat();
				int pipWidth = sw * flPIPScale;
				int pipHeight = sh * flPIPScale;
				int x = sw - pipWidth - ss_pip_right_offset.GetInt();
				int y = sh - pipHeight - ss_pip_bottom_offset.GetInt();
				// round upper left corner down to the nearest multiple of 8 for X360 (resolve alignment requirements)
				UI_SetScreenSpaceBounds( validSlots[ 1 ], x, y, pipWidth, pipHeight );
			}
			else if ( ss_verticalsplit.GetBool() )
			{
				sw /= 2;
				// Stack two horiz, side by side
				UI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
				UI_SetScreenSpaceBounds( validSlots[ 1 ], sx + sw, sy, sw, sh );
			}
			else
			{
				sh /= 2;
				// Stack two wide on top of one another
				UI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
				UI_SetScreenSpaceBounds( validSlots[ 1 ], sx, sy + sh, sw, sh );
			}
		}
		break;
	case 3:
		{
			int fullw = sw;

			sw /= 2;
			sh /= 2;

			UI_SetScreenSpaceBounds( validSlots[ 0 ], sx + ( fullw - sw ) / 2, sy, sw, sh );
			UI_SetScreenSpaceBounds( validSlots[ 1 ], sx, sy + sh, sw, sh );
			UI_SetScreenSpaceBounds( validSlots[ 2 ], sx + sw, sy + sh, sw, sh );
		}
		break;
	case 4:
		{
			sw /= 2;
			sh /= 2;

			// Stack two wide on top of one another
			UI_SetScreenSpaceBounds( validSlots[ 0 ], sx, sy, sw, sh );
			UI_SetScreenSpaceBounds( validSlots[ 1 ], sx + sw, sy, sw, sh );
			UI_SetScreenSpaceBounds( validSlots[ 2 ], sx, sy + sh, sw, sh );
			UI_SetScreenSpaceBounds( validSlots[ 3 ], sx + sw, sy + sh, sw, sh );
		}
		break;
	}
}

std::bitset< MAX_SPLITSCREEN_PLAYERS > g_SplitScreenPlayers;

bool g_bIterateRemoteSplitScreenPlayers = false;
CBasePlayer *g_RemoteSplitScreenPlayers[MAX_SPLITSCREEN_PLAYERS];

void AddRemoteSplitScreenViewPlayer( CBasePlayer *pPlayer )
{
	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( g_RemoteSplitScreenPlayers[i] == pPlayer )
			return; //don't add it twice
	}

	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( !g_SplitScreenPlayers[i] && (g_RemoteSplitScreenPlayers[i] == nullptr) )
		{
			g_RemoteSplitScreenPlayers[i] = pPlayer;
			UI_OnSplitScreenStateChanged();
			return;
		}
	}
}

void RemoveRemoteSplitScreenViewPlayer( CBasePlayer *pPlayer )
{
	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( g_RemoteSplitScreenPlayers[i] == pPlayer )
		{
			g_RemoteSplitScreenPlayers[i] = nullptr;
			UI_OnSplitScreenStateChanged();
			return;
		}
	}
}

CBasePlayer *GetSplitScreenViewPlayer( int nSlot )
{
	return g_SplitScreenPlayers[nSlot] ? CBasePlayer::GetLocalPlayer( nSlot ) : g_RemoteSplitScreenPlayers[nSlot];
}

void cl_enable_remote_splitscreen_callback_f( ConVar *var, const char *pOldValue, float flOldValue )
{
	UI_OnSplitScreenStateChanged();
}

ConVar cl_enable_remote_splitscreen( side_registry(), "cl_enable_remote_splitscreen", "0", 0, "Allows viewing of nonlocal players in a split screen fashion", cl_enable_remote_splitscreen_callback_f );
static std::vector< bool > s_IterateNetworkedSplitScreenSlotsPushedValues;
void IterateRemoteSplitScreenViewSlots_Push( bool bSet )
{
	if( !cl_enable_remote_splitscreen.GetBool() )
	{
		bSet = false;
	}

	s_IterateNetworkedSplitScreenSlotsPushedValues.push_back( g_bIterateRemoteSplitScreenPlayers );
	g_bIterateRemoteSplitScreenPlayers = bSet;
}

void IterateRemoteSplitScreenViewSlots_Pop( void )
{
	contract_assert( std::ssize( s_IterateNetworkedSplitScreenSlotsPushedValues ) > 0 );
	g_bIterateRemoteSplitScreenPlayers = s_IterateNetworkedSplitScreenSlotsPushedValues.back();
	s_IterateNetworkedSplitScreenSlotsPushedValues.resize( s_IterateNetworkedSplitScreenSlotsPushedValues.size() - 1 );
}

bool IsLocalSplitScreenPlayer( int nSlot )
{
	return g_SplitScreenPlayers[nSlot];
}

int FirstValidSplitScreenSlot()
{
	return 0;
}

int NextValidSplitScreenSlot( int i )
{
	++i;
	while ( i<  MAX_SPLITSCREEN_PLAYERS )
	{
		if ( g_SplitScreenPlayers[i] )
			return i;

		if( g_bIterateRemoteSplitScreenPlayers && cl_enable_remote_splitscreen.GetBool() && (g_RemoteSplitScreenPlayers[i] != nullptr) )
			return i;

		++i;
	}
	return -1;
}

bool IsValidSplitScreenSlot( int i )
{
	return g_SplitScreenPlayers[i] || (g_bIterateRemoteSplitScreenPlayers && (g_RemoteSplitScreenPlayers[i] != nullptr));
}

static int g_nCachedScreenSize[ 2 ] = { -1, -1 };

void UI_OnScreenSizeChanged()
{
	engine->GetScreenSize( g_nCachedScreenSize[ 0 ], g_nCachedScreenSize[ 1 ] );

	UI_OnSplitScreenStateChanged();
}

static int g_nNumSplits = 1; //number of logical splits (local players + remote splits)
static int g_nNumLocalSplits = 1; //number of local players sitting at this computer

bool UI_IsSplitScreen()
{
	return g_nNumSplits >= 2;
}

bool UI_IsSplitScreenPIP()
{
	return UI_IsSplitScreen() && g_nNumLocalSplits == ss_pipsplit.GetInt();
}

bool UI_UsePipSplit()
{
	return g_nNumLocalSplits <= ss_pipsplit.GetInt(); //ss_pipsplit 1 for remote splitscreen pip, ss_pipsplit 2 to use pip even with 2 local players
}

bool g_bSuppressConfigSystemLevelDueToPIPTransitions;
void UI_OnSplitScreenStateChanged()
{
	g_SplitScreenPlayers.reset();
	g_nNumSplits = 0;
	g_nNumLocalSplits = 0;
	for ( int i = engine->FirstValidSplitScreenSlot();				
		i != -1;												
		i = engine->NextValidSplitScreenSlot( i ) )	
	{
		g_SplitScreenPlayers.set( i );
		g_RemoteSplitScreenPlayers[i] = nullptr; //actual splitscreen players nuke networked splitscreen players
		++g_nNumSplits;
		++g_nNumLocalSplits;
	}

	if( cl_enable_remote_splitscreen.GetBool() )
	{
		for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			if( g_RemoteSplitScreenPlayers[i] != nullptr )
			{
				++g_nNumSplits;
			}
		}
	}

	IterateRemoteSplitScreenViewSlots_Push( true );
	g_LetterBox.SetNumSplitScreenPlayers( g_nNumSplits );

	// Now tile, etc. the rest of them
	int sw, sh;
	if ( g_nCachedScreenSize[ 0 ] == -1 )
	{
		engine->GetScreenSize( g_nCachedScreenSize[ 0 ], g_nCachedScreenSize[ 1 ] );
	}

	sw = g_nCachedScreenSize[ 0 ];
	sh = g_nCachedScreenSize[ 1 ];

	UI_UpdateScreenSpaceBounds( g_nNumSplits, 0, 0, sw, sh );

	// get the current splitscreen/letterbox settings.  We only care about fov and viewmodelfov.
	bool bDummy;
	float flDummy, flFOV, flViewModelFOV;
	g_LetterBox.GetSettings( &bDummy, &flDummy, &flFOV, &flViewModelFOV );

	static SplitScreenConVarRef fov_desired( "fov_desired", true );

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( fov_desired.IsValid() )
		{
			fov_desired.SetValue( i, flFOV );
		}
	}

	if ( !g_bSuppressConfigSystemLevelDueToPIPTransitions )
	{
		ConfigureCurrentSystemLevel( );
	}

	IterateRemoteSplitScreenViewSlots_Pop();
	CBaseEntity::UpdateVisibilityAllEntities();
}

void UI_GetPanelBounds( int slot, int &x, int &y, int &w, int &h )
{
	if ( !IsValidSplitScreenSlot( slot ) || g_nNumSplits == 1 )
	{
		x = y = 0;
		engine->GetScreenSize( w, h );
		return;
	}

	vrect_t &r = g_ScreenSpaceBounds[ slot ];
	x = r.x;
	y = r.y;
	w = r.width;
	h = r.height;
}

void UI_GetEngineRenderBounds( int slot, int &x, int &y, int &w, int &h, int &insetX, int &insetY )
{
	insetX = insetY = 0;

	if ( !IsValidSplitScreenSlot( slot ) || g_nNumSplits == 1 )
	{
		x = y = 0;
		engine->GetScreenSize( w, h );
		return;
	}

	UI_GetPanelBounds( slot, x, y, w, h );

	bool bDummy = false;
	float flDummy = 0;
	float flAspect = 1.0f;
	if ( !g_LetterBox.GetSettings( &bDummy, &flAspect, &flDummy, &flDummy )  )
	{
		return;
	}

	// Need to convert from physical to pixel aspect ratio.  These aren't the same when using non-square pixels.
	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
	flAspect *= aspectRatioInfo.m_flPhysicalToFrameBufferScalar;

	// Figure out current aspect ratio
	float flCurrentAspect = (float)w / (float)h;
	float ratio = flAspect / flCurrentAspect;

	if ( ratio > 1.0f )
	{
		// Screen is wider, need bars at top and bottom
		int usetall = (float)w / flAspect;
		insetY = ( h - usetall ) / 2;
		y += insetY;
		h = usetall;
	}
	else
	{
		// Screen is narrower, need bars at left/right
		int usewide = (float)h * flAspect;
		insetX = ( w - usewide  ) / 2;
		x += insetX;
		w = usewide;
	}
}

void UI_GetHudBounds( int slot, int &x, int &y, int &w, int &h )
{
	if ( !IsValidSplitScreenSlot( slot ) || g_nNumSplits == 1 )
	{
		x = y = 0;
		engine->GetScreenSize( w, h );
		return;
	}

	bool bInset = false;
	float dummy = 1.0f;

	if ( !g_LetterBox.GetSettings( &bInset, &dummy, &dummy, &dummy ) || 
		 !bInset )
	{
		// Use entire bounds for HUD
		UI_GetPanelBounds( slot, x, y, w, h );
		return;
	}

	int insetX = 0, insetY = 0;
	UI_GetEngineRenderBounds( slot, x, y, w, h, insetX, insetY );
}

#undef GPU_RESOLVE_ALIGNMENT

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

//#define DEBUG_SNC_TYPE_PUN_BUG

//-----------------------------------------------------------------------------
// KD tree query callbacks
//-----------------------------------------------------------------------------
class CDirtySpatialPartitionEntityList : public CAutoGameSystem, public IPartitionQueryCallback
{
public:
	CDirtySpatialPartitionEntityList( char const *name );

	// Members of IGameSystem
	virtual bool Init();
	virtual void Shutdown();
	virtual void LevelShutdownPostEntity();

	// Members of IPartitionQueryCallback
	virtual void OnPreQuery( SpatialPartitionListMask_t listMask );
	virtual void OnPostQuery( SpatialPartitionListMask_t listMask );

	void AddEntity( CBaseEntity *pEntity );
	
	~CDirtySpatialPartitionEntityList();
	void LockPartitionForRead()
	{
		int nThreadId = g_nThreadID;
		if ( m_nReadLockCount[nThreadId] == 0 )
		{
			m_partitionMutex.lock_shared();
		}
		m_nReadLockCount[nThreadId]++;
	}
	void UnlockPartitionForRead()
	{
		int nThreadId = g_nThreadID;
		m_nReadLockCount[nThreadId]--;
		if ( m_nReadLockCount[nThreadId] == 0 )
		{
			m_partitionMutex.unlock_shared();
		}
	}

private:
	CTSListWithFreeList<CBaseHandle> m_DirtyEntities;
	std::shared_mutex	 m_partitionMutex;
	int			 m_partitionWriteId;
	int m_nReadLockCount[MAX_THREADS_SUPPORTED];
};

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDirtySpatialPartitionEntityList s_DirtyKDTree( "CDirtySpatialPartitionEntityList" );

//-----------------------------------------------------------------------------
// Force spatial partition updates (to avoid threading problems caused by lazy update)
//-----------------------------------------------------------------------------
void UpdateDirtySpatialPartitionEntities()
{
	SpatialPartitionListMask_t listMask;
	listMask = PARTITION_CLIENT_GAME_EDICTS;
	s_DirtyKDTree.OnPreQuery( listMask );
	s_DirtyKDTree.OnPostQuery( listMask );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CDirtySpatialPartitionEntityList::CDirtySpatialPartitionEntityList( char const *name ) : CAutoGameSystem( name )
{
	m_DirtyEntities.Purge();
	std::memset( m_nReadLockCount, 0, sizeof( m_nReadLockCount ) );
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor.
//-----------------------------------------------------------------------------
CDirtySpatialPartitionEntityList::~CDirtySpatialPartitionEntityList()
{
	m_DirtyEntities.Purge();
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CDirtySpatialPartitionEntityList::Init()
{
	::partition->InstallQueryCallback( this );
	return true;
}

void CDirtySpatialPartitionEntityList::Shutdown()
{
	::partition->RemoveQueryCallback( this );
}

//-----------------------------------------------------------------------------
// Makes sure all entries in the KD tree are in the correct position
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::AddEntity( CBaseEntity *pEntity )
{
	m_DirtyEntities.PushItem( pEntity->GetRefEHandle() );
}

//-----------------------------------------------------------------------------
// Members of IGameSystem
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::LevelShutdownPostEntity()
{
	m_DirtyEntities.RemoveAll();
}

//-----------------------------------------------------------------------------
// Makes sure all entries in the KD tree are in the correct position
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::OnPreQuery( SpatialPartitionListMask_t listMask )
{
	const int validMask = PARTITION_CLIENT_GAME_EDICTS;

	if ( !( listMask & validMask ) )
		return;

	int nThreadID = g_nThreadID;

	if ( m_partitionWriteId != 0 && m_partitionWriteId == nThreadID + 1 )
		return;

	// FIXME: This should really be an assertion... feh!
	if ( !CBaseEntity::IsAbsRecomputationsEnabled() )
	{
		LockPartitionForRead();
		return;
	}

	// if you're holding a read lock, then these are entities that were still dirty after your trace started
	// or became dirty due to some other thread or callback. Updating them may cause corruption further up the
	// stack (e.g. partition iterator).  Ignoring the state change should be safe since it happened after the 
	// trace was requested or was unable to be resolved in a previous attempt (still dirty).
	if ( m_DirtyEntities.Count() && !m_nReadLockCount[nThreadID] )
	{
		std::vector< CBaseHandle > vecStillDirty;
		std::scoped_lock writeLock( m_partitionMutex );
		m_partitionWriteId = nThreadID + 1;
		CTSListWithFreeList<CBaseHandle>::Node_t *pCurrent, *pNext;
		while ( ( pCurrent = m_DirtyEntities.Detach() ) != nullptr )
		{
			while ( pCurrent )
			{
				CBaseHandle handle = pCurrent->elem;
				pNext = (CTSListWithFreeList<CBaseHandle>::Node_t *)pCurrent->Next;
				m_DirtyEntities.FreeNode( pCurrent );
				pCurrent = pNext;

				CBaseEntity *pEntity = cl_entitylist->GetBaseEntityFromHandle( handle );

				if ( pEntity )
				{
					// If an entity is in the middle of bone setup, don't call UpdatePartition
					//  which can cause it to redo bone setup on the same frame causing a recursive
					//  call to bone setup.
					if ( !pEntity->IsEFlagSet( EFL_SETTING_UP_BONES ) )
					{
						pEntity->CollisionProp()->UpdatePartition();
					}
					else
					{
						vecStillDirty.push_back( handle );
					}
				}
			}
		}
		if ( !vecStillDirty.empty() )
		{
			for ( int i = 0; i < std::ssize( vecStillDirty ); i++ )
			{
				m_DirtyEntities.PushItem( vecStillDirty[i] );
			}
		}
		m_partitionWriteId = 0;
	}
	LockPartitionForRead();
}

//-----------------------------------------------------------------------------
// Makes sure all entries in the KD tree are in the correct position
//-----------------------------------------------------------------------------
void CDirtySpatialPartitionEntityList::OnPostQuery( SpatialPartitionListMask_t listMask )
{
	if ( !( listMask & PARTITION_CLIENT_GAME_EDICTS ) )
		return;

	if ( m_partitionWriteId != 0 )
		return;

	UnlockPartitionForRead();
}

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_PREDMAP_NO_BASE( CCollisionProperty );

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------

 void RecvProxy_Solid( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CCollisionProperty*)pStruct)->SetSolid( (SolidType_t)pData->m_Value.m_Int );
}

 void RecvProxy_SolidFlags( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((CCollisionProperty*)pStruct)->SetSolidFlags( pData->m_Value.m_Int );
}

 void RecvProxy_OBBMins( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CCollisionProperty *pProp = ((CCollisionProperty*)pStruct);
	Vector &vecMins = *((Vector*)pData->m_Value.m_Vector);
	pProp->SetCollisionBounds( vecMins, pProp->OBBMaxs() );
}

 void RecvProxy_OBBMaxs( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CCollisionProperty *pProp = ((CCollisionProperty*)pStruct);
	Vector &vecMaxs = *((Vector*)pData->m_Value.m_Vector);
	pProp->SetCollisionBounds( pProp->OBBMins(), vecMaxs );
}

 void RecvProxy_VectorDirtySurround( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	Vector &vecold = *((Vector*)pOut);
	Vector vecnew( pData->m_Value.m_Vector[0], pData->m_Value.m_Vector[1], pData->m_Value.m_Vector[2] );

	if ( vecold != vecnew )
	{
		vecold = vecnew;
		((CCollisionProperty*)pStruct)->MarkSurroundingBoundsDirty();
	}
}

 void RecvProxy_IntDirtySurround( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	if ( *((unsigned char*)pOut) != pData->m_Value.m_Int )
	{
		*((unsigned char*)pOut) = pData->m_Value.m_Int;
		((CCollisionProperty*)pStruct)->MarkSurroundingBoundsDirty();
	}
}

IMPLEMENT_REFLECT_TABLE( CCollisionProperty, DT_CollisionProperty );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CCollisionProperty::CCollisionProperty()
{
	m_Partition = PARTITION_INVALID_HANDLE;
	Init( nullptr );
}

CCollisionProperty::~CCollisionProperty()
{
	DestroyPartitionHandle();
}

const Vector&	CCollisionProperty::GetCollisionOrigin_Inline() const 
{ 
	return GetOuter()->GetAbsOrigin(); 
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CCollisionProperty::Init( CBaseEntity *pEntity )
{
	m_pOuter = pEntity;
	m_vecMins.GetForModify().Init();
	m_vecMaxs.GetForModify().Init();
	m_flRadius = 0.0f;
	m_triggerBloat = 0;
	m_usSolidFlags = 0;
	m_nSolidType = SOLID_NONE;

	// NOTE: This replicates previous behavior; we may always want to use BEST_COLLISION_BOUNDS
	m_nSurroundType = USE_OBB_COLLISION_BOUNDS;
	m_vecSurroundingMins = vec3_origin;
	m_vecSurroundingMaxs = vec3_origin;
	m_vecSpecifiedSurroundingMins.GetForModify().Init();
	m_vecSpecifiedSurroundingMaxs.GetForModify().Init();
}

//-----------------------------------------------------------------------------
// EntityHandle
//-----------------------------------------------------------------------------
IHandleEntity *CCollisionProperty::GetEntityHandle()
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Collision group
//-----------------------------------------------------------------------------
int CCollisionProperty::GetCollisionGroup() const
{
	return m_pOuter->GetCollisionGroup();
}

uint CCollisionProperty::GetRequiredTriggerFlags() const
{
	// debris only touches certain triggers
	if ( GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
		return FSOLID_TRIGGER_TOUCH_DEBRIS;

	// triggers don't touch other triggers (might be solid to other ents as well as trigger)
	if ( IsSolidFlagSet( FSOLID_TRIGGER ) )
		return 0;

	return FSOLID_TRIGGER;
}

const matrix3x4_t *CCollisionProperty::GetRootParentToWorldTransform() const
{
	if ( IsSolidFlagSet( FSOLID_ROOT_PARENT_ALIGNED ) )
	{
		CBaseEntity *pEntity = m_pOuter->GetRootMoveParent();
		contract_assert( pEntity );
		if ( pEntity )
		{
			return &pEntity->CollisionProp()->CollisionToWorldTransform();
		}
	}
	return nullptr;
}

IPhysicsObject *CCollisionProperty::GetVPhysicsObject() const
{
	return m_pOuter->VPhysicsGetObject();
}

//-----------------------------------------------------------------------------
// IClientUnknown
//-----------------------------------------------------------------------------
IClientUnknown* CCollisionProperty::GetIClientUnknown()
{
	return ( m_pOuter != nullptr ) ? m_pOuter->GetIClientUnknown() : nullptr;
}

//-----------------------------------------------------------------------------
// Check for untouch
//-----------------------------------------------------------------------------
void CCollisionProperty::CheckForUntouch()
{
}

//-----------------------------------------------------------------------------
// Sets the solid type
//-----------------------------------------------------------------------------
void CCollisionProperty::SetSolid( SolidType_t val )
{
	if ( m_nSolidType == val )
		return;

	MarkSurroundingBoundsDirty();

	// OBB is not yet implemented
	if ( val == SOLID_BSP )
	{
		if ( GetOuter()->GetMoveParent() )
		{
			if ( GetOuter()->GetRootMoveParent()->GetSolid() != SOLID_BSP )
			{
				// must be SOLID_VPHYSICS because parent might rotate
				val = SOLID_VPHYSICS;
			}
		}
	}

	m_nSolidType = val;

}

SolidType_t CCollisionProperty::GetSolid() const
{
	return (SolidType_t)m_nSolidType.Get();
}

//-----------------------------------------------------------------------------
// Sets the solid flags
//-----------------------------------------------------------------------------
void CCollisionProperty::SetSolidFlags( int flags )
{
	int oldFlags = m_usSolidFlags;
	m_usSolidFlags = (unsigned short)(flags & 0xFFFF);
	if ( oldFlags == m_usSolidFlags )
		return;

	// These two flags, if changed, can produce different surrounding bounds
	if ( (oldFlags & (FSOLID_FORCE_WORLD_ALIGNED | FSOLID_USE_TRIGGER_BOUNDS)) != 
		 (m_usSolidFlags & (FSOLID_FORCE_WORLD_ALIGNED | FSOLID_USE_TRIGGER_BOUNDS)) )
	{
		MarkSurroundingBoundsDirty();
	}

	if ( (oldFlags & (FSOLID_NOT_SOLID|FSOLID_TRIGGER)) != (m_usSolidFlags & (FSOLID_NOT_SOLID|FSOLID_TRIGGER)) )
	{
		m_pOuter->CollisionRulesChanged();
	}

}

//-----------------------------------------------------------------------------
// Coordinate system of the collision model
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::GetCollisionOrigin() const
{
	return GetCollisionOrigin_Inline();
}

const QAngle& CCollisionProperty::GetCollisionAngles() const
{
	if ( IsBoundsDefinedInEntitySpace() )
	{
		return m_pOuter->GetAbsAngles();
	}

	return vec3_angle;
}

const matrix3x4_t& CCollisionProperty::CollisionToWorldTransform() const
{
	static matrix3x4_t s_matTemp[4];
	static int s_nIndex = 0;

	matrix3x4_t &matResult = s_matTemp[s_nIndex];
	s_nIndex = (s_nIndex+1) & 0x3;

	if ( IsBoundsDefinedInEntitySpace() )
	{
		return m_pOuter->EntityToWorldTransform();
	}

	SetIdentityMatrix( matResult );
	MatrixSetColumn( GetCollisionOrigin_Inline(), 3, matResult );
	return matResult;
}

//-----------------------------------------------------------------------------
// Sets the collision bounds + the size
//-----------------------------------------------------------------------------
void CCollisionProperty::SetCollisionBounds( const Vector& mins, const Vector &maxs )
{
	if ( (m_vecMins == mins) && (m_vecMaxs == maxs) )
		return;
		
	m_vecMins = mins;
	m_vecMaxs = maxs;
	
	ASSERT_COORD( mins );
	ASSERT_COORD( maxs );

	Vector vecSize;
	VectorSubtract( maxs, mins, vecSize );
	m_flRadius = vecSize.Length() * 0.5f;

	MarkSurroundingBoundsDirty();
}

//-----------------------------------------------------------------------------
// Lazily calculates the 2D bounding radius. If we do this enough, we should
// calculate this in SetCollisionBounds above and cache the results in a data member!
//-----------------------------------------------------------------------------
float CCollisionProperty::BoundingRadius2D() const
{
	Vector vecSize;
	VectorSubtract( m_vecMaxs, m_vecMins, vecSize );

	vecSize.z = 0;	
	return vecSize.Length() * 0.5f;
}

//-----------------------------------------------------------------------------
// Bounding representation (OBB)
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::OBBMins( ) const
{
	return m_vecMins.Get();
}

const Vector& CCollisionProperty::OBBMaxs( ) const
{
	return m_vecMaxs.Get();
}

//-----------------------------------------------------------------------------
// Special trigger representation (OBB)
//-----------------------------------------------------------------------------
void CCollisionProperty::WorldSpaceTriggerBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs ) const
{
	WorldSpaceAABB( pVecWorldMins, pVecWorldMaxs );
	if ( ( GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS ) == 0 )
		return;

	// Don't bloat below, we don't want to trigger it with our heads
	pVecWorldMins->x -= m_triggerBloat;
	pVecWorldMins->y -= m_triggerBloat;

	pVecWorldMaxs->x += m_triggerBloat;
	pVecWorldMaxs->y += m_triggerBloat;
	pVecWorldMaxs->z += (float)m_triggerBloat * 0.5f;
}

void CCollisionProperty::UseTriggerBounds( bool bEnable, float flBloat )
{
	contract_assert( flBloat <= 127.0f );
	m_triggerBloat = (char )flBloat;
	if ( bEnable )
	{
		AddSolidFlags( FSOLID_USE_TRIGGER_BOUNDS );
		contract_assert( flBloat > 0.0f );
	}
	else
	{
		RemoveSolidFlags( FSOLID_USE_TRIGGER_BOUNDS );
	}
}

//-----------------------------------------------------------------------------
// Collision model (BSP)
//-----------------------------------------------------------------------------
int CCollisionProperty::GetCollisionModelIndex()
{
	return m_pOuter->GetModelIndex();
}

const model_t* CCollisionProperty::GetCollisionModel()
{
	return m_pOuter->GetModel();
}

//-----------------------------------------------------------------------------
// Collision methods implemented in the entity
// FIXME: This shouldn't happen there!!
//-----------------------------------------------------------------------------
bool CCollisionProperty::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return m_pOuter->TestCollision( ray, fContentsMask, tr );
}

bool CCollisionProperty::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return m_pOuter->TestHitboxes( ray, fContentsMask, tr );
}

//-----------------------------------------------------------------------------
// Computes a "normalized" point (range 0,0,0 - 1,1,1) in collision space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::NormalizedToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	pResult->x = Lerp( in.x, m_vecMins.Get().x, m_vecMaxs.Get().x );
	pResult->y = Lerp( in.y, m_vecMins.Get().y, m_vecMaxs.Get().y );
	pResult->z = Lerp( in.z, m_vecMins.Get().z, m_vecMaxs.Get().z );
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in collision space to normalized space
//-----------------------------------------------------------------------------
const Vector &	CCollisionProperty::CollisionToNormalizedSpace( const Vector &in, Vector *pResult ) const
{
	Vector vecSize = OBBSize( );
	pResult->x = ( vecSize.x != 0.0f ) ? ( in.x - m_vecMins.Get().x ) / vecSize.x : 0.5f;
	pResult->y = ( vecSize.y != 0.0f ) ? ( in.y - m_vecMins.Get().y ) / vecSize.y : 0.5f;
	pResult->z = ( vecSize.z != 0.0f ) ? ( in.z - m_vecMins.Get().z ) / vecSize.z : 0.5f;
	return *pResult;
}

//-----------------------------------------------------------------------------
// Computes a "normalized" point (range 0,0,0 - 1,1,1) in world space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::NormalizedToWorldSpace( const Vector &in, Vector *pResult ) const
{
	Vector vecCollisionSpace;
	NormalizedToCollisionSpace( in, &vecCollisionSpace );
	CollisionToWorldSpace( vecCollisionSpace, pResult );
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in world space to normalized space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::WorldToNormalizedSpace( const Vector &in, Vector *pResult ) const
{
	Vector vecCollisionSpace;
	WorldToCollisionSpace( in, &vecCollisionSpace );
	CollisionToNormalizedSpace( vecCollisionSpace, pResult );
	return *pResult;
}

//-----------------------------------------------------------------------------
// Selects a random point in the bounds given the normalized 0-1 bounds 
//-----------------------------------------------------------------------------
void CCollisionProperty::RandomPointInBounds( const Vector &vecNormalizedMins, const Vector &vecNormalizedMaxs, Vector *pPoint) const
{
	Vector vecNormalizedSpace;
	vecNormalizedSpace.x = random_valve->RandomFloat( vecNormalizedMins.x, vecNormalizedMaxs.x );
	vecNormalizedSpace.y = random_valve->RandomFloat( vecNormalizedMins.y, vecNormalizedMaxs.y );
	vecNormalizedSpace.z = random_valve->RandomFloat( vecNormalizedMins.z, vecNormalizedMaxs.z );
	NormalizedToWorldSpace( vecNormalizedSpace, pPoint );
}

const
bool g_bRepeatTransformAABB = false;

//-----------------------------------------------------------------------------
// Transforms an AABB measured in entity space to a box that surrounds it in world space
//-----------------------------------------------------------------------------
void CCollisionProperty::CollisionAABBToWorldAABB( const Vector &entityMins, 
	const Vector &entityMaxs, Vector *pWorldMins, Vector *pWorldMaxs ) const
{
	Vector copyEntityMins = entityMins, copyEntityMaxs = entityMaxs;
	ASSERT_COORD( entityMins );
	ASSERT_COORD( entityMaxs );
	if ( !IsBoundsDefinedInEntitySpace() || (GetCollisionAngles() == vec3_angle) )
	{
		VectorAdd( entityMins, GetCollisionOrigin_Inline(), *pWorldMins );
		VectorAdd( entityMaxs, GetCollisionOrigin_Inline(), *pWorldMaxs );
		ASSERT_COORD( *pWorldMins );
		ASSERT_COORD( *pWorldMaxs );
	}
	else
	{
		do
		{
			matrix3x4_t tm = CollisionToWorldTransform();
			ASSERT_COORD( entityMins );
			ASSERT_COORD( entityMaxs );
			TransformAABB( tm, entityMins, entityMaxs, *pWorldMins, *pWorldMaxs );
			ASSERT_COORD( *pWorldMins );
			ASSERT_COORD( *pWorldMaxs );
		}
		while( g_bRepeatTransformAABB );
	}
}

/*
void CCollisionProperty::WorldAABBToCollisionAABB( const Vector &worldMins, const Vector &worldMaxs, Vector *pEntityMins, Vector *pEntityMaxs ) const
{
	if ( !IsBoundsDefinedInEntitySpace() || (GetCollisionAngles() == vec3_angle) )
	{
		VectorSubtract( worldMins, GetAbsOrigin(), *pEntityMins );
		VectorSubtract( worldMaxs, GetAbsOrigin(), *pEntityMaxs );
	}
	else
	{
		ITransformAABB( CollisionToWorldTransform(), worldMins, worldMaxs, *pEntityMins, *pEntityMaxs );
	}
}
*/

//-----------------------------------------------------------------------------
// Is a worldspace point within the bounds of the OBB?
//-----------------------------------------------------------------------------
bool CCollisionProperty::IsPointInBounds( const Vector &vecWorldPt ) const
{
	Vector vecLocalSpace;
	WorldToCollisionSpace( vecWorldPt, &vecLocalSpace );
	return ( ( vecLocalSpace.x >= m_vecMins.Get().x && vecLocalSpace.x <= m_vecMaxs.Get().x ) &&
			( vecLocalSpace.y >= m_vecMins.Get().y && vecLocalSpace.y <= m_vecMaxs.Get().y ) &&
			( vecLocalSpace.z >= m_vecMins.Get().z && vecLocalSpace.z <= m_vecMaxs.Get().z ) );
}

//-----------------------------------------------------------------------------
// Computes the nearest point in the OBB to a point specified in world space
//-----------------------------------------------------------------------------
void CCollisionProperty::CalcNearestPoint( const Vector &vecWorldPt, Vector *pVecNearestWorldPt ) const
{
	// Calculate physics force
	Vector localPt, localClosestPt;
	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( m_vecMins.Get(), m_vecMaxs.Get(), localPt, localClosestPt );
	CollisionToWorldSpace( localClosestPt, pVecNearestWorldPt );
}

//-----------------------------------------------------------------------------
// Computes the nearest point in the OBB to a point specified in world space
//-----------------------------------------------------------------------------
float CCollisionProperty::CalcDistanceFromPoint( const Vector &vecWorldPt ) const
{
	// Calculate physics force
	Vector localPt, localClosestPt;
	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( m_vecMins.Get(), m_vecMaxs.Get(), localPt, localClosestPt );
	return localPt.DistTo( localClosestPt );
}

//-----------------------------------------------------------------------------
// Computes the square distance of the closest point in the OBB to a point specified in world space
//-----------------------------------------------------------------------------
float CCollisionProperty::CalcSqrDistanceFromPoint( const Vector &vecWorldPt ) const
{
	// Calculate physics force
	Vector localPt, localClosestPt;
	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( m_vecMins.Get(), m_vecMaxs.Get(), localPt, localClosestPt );
	return localPt.DistToSqr( localClosestPt );
}

//-----------------------------------------------------------------------------
// Compute the largest dot product of the OBB and the specified direction vector
//-----------------------------------------------------------------------------
float CCollisionProperty::ComputeSupportMap( const Vector &vecDirection ) const
{
	Vector vecCollisionDir;
	WorldDirectionToCollisionSpace( vecDirection, &vecCollisionDir );

	float flResult = DotProduct( GetCollisionOrigin_Inline(), vecDirection );
	flResult += (( vecCollisionDir.x >= 0.0f ) ? m_vecMaxs.Get().x : m_vecMins.Get().x) * vecCollisionDir.x;
	flResult += (( vecCollisionDir.y >= 0.0f ) ? m_vecMaxs.Get().y : m_vecMins.Get().y) * vecCollisionDir.y;
	flResult += (( vecCollisionDir.z >= 0.0f ) ? m_vecMaxs.Get().z : m_vecMins.Get().z) * vecCollisionDir.z;

	return flResult;
}

//-----------------------------------------------------------------------------
// Expand trigger bounds..
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeVPhysicsSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	bool bSetBounds = false;
	IPhysicsObject *pPhysicsObject = GetOuter()->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		if ( pPhysicsObject->GetCollide() )
		{
			physcollision->CollideGetAABB( pVecWorldMins, pVecWorldMaxs, 
				pPhysicsObject->GetCollide(), GetCollisionOrigin_Inline(), GetCollisionAngles() );
			bSetBounds = true;
		}
		else if ( pPhysicsObject->GetSphereRadius( ) )
		{
			float flRadius = pPhysicsObject->GetSphereRadius( );
			Vector vecExtents( flRadius, flRadius, flRadius );
			VectorSubtract( GetCollisionOrigin_Inline(), vecExtents, *pVecWorldMins );
			VectorAdd( GetCollisionOrigin_Inline(), vecExtents, *pVecWorldMaxs );
			bSetBounds = true;
		}
	}

	if ( !bSetBounds )
	{
		*pVecWorldMins = GetCollisionOrigin_Inline();
		*pVecWorldMaxs = *pVecWorldMins;
	}

	// Also, lets expand for the trigger bounds also
	if ( IsSolidFlagSet( FSOLID_USE_TRIGGER_BOUNDS ) )
	{
		Vector vecWorldTriggerMins, vecWorldTriggerMaxs;
		WorldSpaceTriggerBounds( &vecWorldTriggerMins, &vecWorldTriggerMaxs );
		VectorMin( vecWorldTriggerMins, *pVecWorldMins, *pVecWorldMins );
		VectorMax( vecWorldTriggerMaxs, *pVecWorldMaxs, *pVecWorldMaxs );
	}
}

//-----------------------------------------------------------------------------
// Expand trigger bounds..
//-----------------------------------------------------------------------------
bool CCollisionProperty::ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
	if (pAnim)
	{
		return pAnim->ComputeHitboxSurroundingBox( pVecWorldMins, pVecWorldMaxs );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on the current sequence box
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeOBBBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	bool bUseVPhysics = false;
	if ( ( GetSolid() == SOLID_VPHYSICS ) && ( GetOuter()->GetMoveType() == MOVETYPE_VPHYSICS ) )
	{
		// UNDONE: This may not be necessary any more.
		IPhysicsObject *pPhysics = GetOuter()->VPhysicsGetObject();
		bUseVPhysics = pPhysics && pPhysics->IsAsleep();
	}
	ComputeCollisionSurroundingBox( bUseVPhysics, pVecWorldMins, pVecWorldMaxs );
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds from the current sequence box
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeRotationExpandedSequenceBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
	if ( !pAnim )
	{
		ComputeOBBBounds( pVecWorldMins, pVecWorldMaxs );
		return;
	}

	Vector mins, maxs;
	pAnim->ExtractBbox( pAnim->GetSequence(), mins, maxs );

	float flRadius = std::max( std::max( FloatMakePositive( mins.x ), FloatMakePositive( maxs.x ) ),
					      std::max( FloatMakePositive( mins.y ), FloatMakePositive( maxs.y ) ) );
	mins.x = mins.y = -flRadius;
	maxs.x = maxs.y = flRadius;

	// Add bloat to account for gesture sequences
	Vector vecBloat( 6, 6, 0 );
	mins -= vecBloat;
	maxs += vecBloat;
	ASSERT_COORD( m_vecSurroundingMins );
	ASSERT_COORD( m_vecSurroundingMaxs );
	// NOTE: This is necessary because the server doesn't know how to blend
	// animations together. Therefore, we have to just pick a box that can
	// surround all of our potential sequences. This should be something we
	// should be able to compute @ tool time instead, however.
	VectorMin( mins, m_vecSurroundingMins, mins );
	VectorMax( maxs, m_vecSurroundingMaxs, maxs );

	VectorAdd( mins, GetCollisionOrigin_Inline(), *pVecWorldMins );
	VectorAdd( maxs, GetCollisionOrigin_Inline(), *pVecWorldMaxs );
}

//-----------------------------------------------------------------------------
// Expand trigger bounds..
//-----------------------------------------------------------------------------
bool CCollisionProperty::ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
	if (pAnim)
	{
		return pAnim->ComputeEntitySpaceHitboxSurroundingBox( pVecWorldMins, pVecWorldMaxs );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds from the the OBB (not vphysics)
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeRotationExpandedBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	if ( !IsBoundsDefinedInEntitySpace() )
	{
		*pVecWorldMins = m_vecMins;
		*pVecWorldMaxs = m_vecMaxs;
	}
	else
	{
		float flMaxVal;
		flMaxVal = std::max( FloatMakePositive(m_vecMins.Get().x), FloatMakePositive(m_vecMaxs.Get().x) );
		pVecWorldMins->x = -flMaxVal;
		pVecWorldMaxs->x = flMaxVal;

		flMaxVal = std::max( FloatMakePositive(m_vecMins.Get().y), FloatMakePositive(m_vecMaxs.Get().y) );
		pVecWorldMins->y = -flMaxVal;
		pVecWorldMaxs->y = flMaxVal;

		flMaxVal = std::max( FloatMakePositive(m_vecMins.Get().z), FloatMakePositive(m_vecMaxs.Get().z) );
		pVecWorldMins->z = -flMaxVal;
		pVecWorldMaxs->z = flMaxVal;
	}
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on whatever algorithm we want...
//-----------------------------------------------------------------------------
void CCollisionProperty::ComputeCollisionSurroundingBox( bool bUseVPhysics, Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	contract_assert( GetSolid() != SOLID_CUSTOM );

	// NOTE: For solid none, we are still going to use the bounds; necessary because
	// the surrounding box is used for the PVS...
	// FIXME: Should we make some other call for the PVS stuff?? If so, we should return
	// a point bounds for SOLID_NONE...
//	if ( GetSolid() == SOLID_NONE )
//	{
//		*pVecWorldMins = GetCollisionOrigin_Inline();
//		*pVecWorldMaxs = *pVecWorldMins;
//		return;
//	}

	if ( bUseVPhysics )
	{
		ComputeVPhysicsSurroundingBox( pVecWorldMins, pVecWorldMaxs );
	}
	else
	{
		// Will expand the bounds for the trigger, if it is a trigger
		WorldSpaceTriggerBounds( pVecWorldMins, pVecWorldMaxs );
	}
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on whatever algorithm we want...
//-----------------------------------------------------------------------------
static ConVar cl_show_bounds_errors( side_registry(), "cl_show_bounds_errors", "0" );

void CCollisionProperty::ComputeSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	if (( GetSolid() == SOLID_CUSTOM ) && (m_nSurroundType != USE_GAME_CODE ))
	{
		// NOTE: This can only happen in transition periods, say during network
		// reception on the client. We expect USE_GAME_CODE to be used with SOLID_CUSTOM
		*pVecWorldMins = GetCollisionOrigin_Inline();
		*pVecWorldMaxs = *pVecWorldMins;
		return;
	}

	switch( m_nSurroundType )
	{
	case USE_OBB_COLLISION_BOUNDS:
		contract_assert( GetSolid() != SOLID_CUSTOM );
		ComputeOBBBounds( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_BEST_COLLISION_BOUNDS:
		contract_assert( GetSolid() != SOLID_CUSTOM );
		ComputeCollisionSurroundingBox( (GetSolid() == SOLID_VPHYSICS), pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS:
		ComputeRotationExpandedSequenceBounds( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
		contract_assert( GetSolid() != SOLID_CUSTOM );
		ComputeCollisionSurroundingBox( false, pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_HITBOXES:
		ComputeHitboxSurroundingBox( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_ROTATION_EXPANDED_BOUNDS:
		ComputeRotationExpandedBounds( pVecWorldMins, pVecWorldMaxs );
		break;

	case USE_SPECIFIED_BOUNDS:
		VectorAdd( GetCollisionOrigin_Inline(), m_vecSpecifiedSurroundingMins, *pVecWorldMins );
		VectorAdd( GetCollisionOrigin_Inline(), m_vecSpecifiedSurroundingMaxs, *pVecWorldMaxs );
		break;

	case USE_GAME_CODE:
		GetOuter()->ComputeWorldSpaceSurroundingBox( pVecWorldMins, pVecWorldMaxs );
		contract_assert( pVecWorldMins->x <= pVecWorldMaxs->x );
		contract_assert( pVecWorldMins->y <= pVecWorldMaxs->y );
		contract_assert( pVecWorldMins->z <= pVecWorldMaxs->z );
		return;
	}

//#ifdef DEBUG
	if ( cl_show_bounds_errors.GetBool() && ( m_nSurroundType == USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS ) )
	{ 
		// For debugging purposes, make sure the bounds actually does surround the thing.
		// Otherwise the optimization we were using isn't really all that great, is it?
		Vector vecTestMins, vecTestMaxs;
		if ( GetOuter()->GetBaseAnimating() )
		{
			GetOuter()->GetBaseAnimating()->InvalidateBoneCache();
		}
		ComputeHitboxSurroundingBox( &vecTestMins, &vecTestMaxs );
		
		contract_assert( vecTestMins.x >= pVecWorldMins->x && vecTestMins.y >= pVecWorldMins->y && vecTestMins.z >= pVecWorldMins->z );
		contract_assert( vecTestMaxs.x <= pVecWorldMaxs->x && vecTestMaxs.y <= pVecWorldMaxs->y && vecTestMaxs.z <= pVecWorldMaxs->z );

		if ( vecTestMins.x < pVecWorldMins->x || vecTestMins.y < pVecWorldMins->y || vecTestMins.z < pVecWorldMins->z ||
			 vecTestMaxs.x > pVecWorldMaxs->x || vecTestMaxs.y > pVecWorldMaxs->y || vecTestMaxs.z > pVecWorldMaxs->z )
		{
			const char *pSeqName = "<unknown seq>";
			CBaseAnimating *pAnim = GetOuter()->GetBaseAnimating();
			if ( pAnim )
			{
				int nSequence = pAnim->GetSequence();
				pSeqName = pAnim->GetSequenceName( nSequence );
			}

			Warning( "*** Bounds problem, index %d Eng %s, Seqeuence %s ", GetOuter()->entindex(), GetOuter()->GetClassname(), pSeqName );
			Vector vecDelta = *pVecWorldMins - vecTestMins;
			Vector vecDelta2 = vecTestMaxs - *pVecWorldMaxs;
			if ( vecDelta.x > 0.0f || vecDelta2.x > 0.0f || vecDelta.y > 0.0f || vecDelta2.y > 0.0f )
			{
				Msg( "Outside X/Y by %.2f ", std::max( std::max( vecDelta.x, vecDelta2.x ), std::max( vecDelta.y, vecDelta2.y ) ) );
			}
			if ( vecDelta.z > 0.0f || vecDelta2.z > 0.0f )
			{
				Msg( "Outside Z by (below) %.2f, (above) %.2f ", std::max( vecDelta.z, 0.0f ), std::max( vecDelta2.z, 0.0f ) );
			}
			Msg( "\n" );

			char pTemp[MAX_PATH];
			V_snprintf( pTemp, sizeof(pTemp), "%s [seq: %s]", GetOuter()->GetClassname(), pSeqName ); 

			debugoverlay->AddBoxOverlay( vec3_origin, vecTestMins, vecTestMaxs, vec3_angle, 255, 0, 0, 0, 2 );
			debugoverlay->AddBoxOverlay( vec3_origin, *pVecWorldMins, *pVecWorldMaxs, vec3_angle, 0, 0, 255, 0, 2 );
			debugoverlay->AddTextOverlay( ( vecTestMins + vecTestMaxs ) * 0.5f, 2, "%s", pTemp );
		}
	}
//#endif
}

//-----------------------------------------------------------------------------
// Sets the method by which the surrounding collision bounds is set
//-----------------------------------------------------------------------------
void CCollisionProperty::SetSurroundingBoundsType( SurroundingBoundsType_t type, const Vector *pMins, const Vector *pMaxs )
{	
	m_nSurroundType = type;
	if (type != USE_SPECIFIED_BOUNDS)
	{
		contract_assert( !pMins && !pMaxs );
		MarkSurroundingBoundsDirty();
	}
	else
	{
		contract_assert( pMins && pMaxs );
		ASSERT_COORD( *pMins );
		ASSERT_COORD( *pMaxs );
		m_vecSpecifiedSurroundingMins = *pMins;
		m_vecSpecifiedSurroundingMaxs = *pMaxs;
		m_vecSurroundingMins = *pMins;
		m_vecSurroundingMaxs = *pMaxs;

		ASSERT_COORD( m_vecSurroundingMins );
		ASSERT_COORD( m_vecSurroundingMaxs );
	}
}

//-----------------------------------------------------------------------------
// Marks the entity has having a dirty surrounding box
//-----------------------------------------------------------------------------
void CCollisionProperty::MarkSurroundingBoundsDirty()
{
	// don't bother with the world
	if ( m_pOuter->entindex() == 0 )
		return;

	GetOuter()->AddEFlags( EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS );
	MarkPartitionHandleDirty();

	GetOuter()->MarkRenderHandleDirty();
	g_pClientShadowMgr->AddToDirtyShadowList( GetOuter() );
	g_pClientShadowMgr->MarkRenderToTextureShadowDirty( GetOuter()->GetShadowHandle() );
}

//-----------------------------------------------------------------------------
// Does VPhysicsUpdate make us need to recompute the surrounding box?
//-----------------------------------------------------------------------------
bool CCollisionProperty::DoesVPhysicsInvalidateSurroundingBox( ) const
{
	switch ( m_nSurroundType )
	{
	case USE_BEST_COLLISION_BOUNDS:
		return true;

	case USE_OBB_COLLISION_BOUNDS:
		return (GetSolid() == SOLID_VPHYSICS) && (GetOuter()->GetMoveType() == MOVETYPE_VPHYSICS) && GetOuter()->VPhysicsGetObject();

	// In the case of game code, we don't really know, so we have to assume it does
	case USE_GAME_CODE:
		return true;

	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
	case USE_HITBOXES:
	case USE_ROTATION_EXPANDED_BOUNDS:
	case USE_SPECIFIED_BOUNDS:
	case USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS:
		return false;

	default:
		contract_assert( 0 );
		return true;
	}
}

//-----------------------------------------------------------------------------
// Computes the surrounding collision bounds based on whatever algorithm we want...
//-----------------------------------------------------------------------------
void CCollisionProperty::WorldSpaceSurroundingBounds( Vector *pVecMins, Vector *pVecMaxs )
{
	const Vector &vecAbsOrigin = GetCollisionOrigin_Inline();
	if ( GetOuter()->IsEFlagSet( EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS ))
	{
		GetOuter()->RemoveEFlags( EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS );
		ComputeSurroundingBox( pVecMins, pVecMaxs );
		ASSERT_COORD( *pVecMins );
		ASSERT_COORD( *pVecMaxs );
		VectorSubtract( *pVecMins, vecAbsOrigin, m_vecSurroundingMins );
		VectorSubtract( *pVecMaxs, vecAbsOrigin, m_vecSurroundingMaxs );
		
		ASSERT_COORD( m_vecSurroundingMins );
		ASSERT_COORD( m_vecSurroundingMaxs );
	}
	else
	{
		VectorAdd( m_vecSurroundingMins, vecAbsOrigin, *pVecMins );
		VectorAdd( m_vecSurroundingMaxs, vecAbsOrigin, *pVecMaxs );
	}
}

//-----------------------------------------------------------------------------
// Spatial partition
//-----------------------------------------------------------------------------
void CCollisionProperty::CreatePartitionHandle()
{
	// Put the entity into the spatial partition.
	contract_assert( m_Partition == PARTITION_INVALID_HANDLE );
	m_Partition = ::partition->CreateHandle( GetEntityHandle() );
}

void CCollisionProperty::DestroyPartitionHandle()
{
	if ( m_Partition != PARTITION_INVALID_HANDLE )
	{
		::partition->DestroyHandle( m_Partition );
		m_Partition = PARTITION_INVALID_HANDLE;
	}
}

uint CCollisionProperty::ComputeServerPartitionMask( )
{
	uint nMask = 0;
	return nMask;
}

//-----------------------------------------------------------------------------
// Updates the spatial partition
//-----------------------------------------------------------------------------
void CCollisionProperty::UpdateServerPartitionMask( )
{
}

//-----------------------------------------------------------------------------
// Marks the spatial partition dirty
//-----------------------------------------------------------------------------
void CCollisionProperty::MarkPartitionHandleDirty()
{
	if ( !m_pOuter->IsEFlagSet( EFL_DIRTY_SPATIAL_PARTITION ) )
	{
		s_DirtyKDTree.AddEntity( m_pOuter );
		m_pOuter->AddEFlags( EFL_DIRTY_SPATIAL_PARTITION );
	}
}

//-----------------------------------------------------------------------------
// Updates the spatial partition
//-----------------------------------------------------------------------------
void CCollisionProperty::UpdatePartition( )
{
	if ( m_pOuter->IsEFlagSet( EFL_DIRTY_SPATIAL_PARTITION ) )
	{
		m_pOuter->RemoveEFlags( EFL_DIRTY_SPATIAL_PARTITION );

		if ( GetPartitionHandle() == PARTITION_INVALID_HANDLE )
			return;

		// We don't need to bother if it's not a trigger or solid
		if ( IsSolid() || IsSolidFlagSet( FSOLID_TRIGGER ) || m_pOuter->IsEFlagSet( EFL_USE_PARTITION_WHEN_NOT_SOLID ) )
		{
			// Bloat a little bit...
			if ( BoundingRadius() != 0.0f )
			{
				Vector vecSurroundMins, vecSurroundMaxs;
				ASSERT_COORD( m_vecSurroundingMins );
				ASSERT_COORD( m_vecSurroundingMaxs );
				WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
				ASSERT_COORD( m_vecSurroundingMins );
				ASSERT_COORD( m_vecSurroundingMaxs );
				vecSurroundMins -= Vector( 1, 1, 1 );
				vecSurroundMaxs += Vector( 1, 1, 1 );
				::partition->ElementMoved( GetPartitionHandle(), vecSurroundMins,  vecSurroundMaxs );
			}
			else
			{
				::partition->ElementMoved( GetPartitionHandle(), GetCollisionOrigin_Inline(),  GetCollisionOrigin_Inline() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
CBaseEntity *CCollisionProperty::GetOuter()
{
	return m_pOuter;
}

const CBaseEntity *CCollisionProperty::GetOuter() const
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Spatial partition
//-----------------------------------------------------------------------------
SpatialPartitionHandle_t CCollisionProperty::GetPartitionHandle() const
{
	return m_Partition;
}

SurroundingBoundsType_t CCollisionProperty::GetSurroundingBoundsType() const
{
	return (SurroundingBoundsType_t)m_nSurroundType.Get();
}

//-----------------------------------------------------------------------------
// Methods related to size
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::OBBSize( ) const
{
	// NOTE: Could precache this, but it's not used that often..
	Vector &temp = AllocTempVector();
	VectorSubtract( m_vecMaxs, m_vecMins, temp );
	return temp;
}

//-----------------------------------------------------------------------------
// Bounding radius size
//-----------------------------------------------------------------------------
float CCollisionProperty::BoundingRadius() const
{
	return m_flRadius;
}

//-----------------------------------------------------------------------------
// Methods relating to solid flags
//-----------------------------------------------------------------------------
bool CCollisionProperty::IsBoundsDefinedInEntitySpace() const
{
	return (( m_usSolidFlags & FSOLID_FORCE_WORLD_ALIGNED ) == 0 ) &&
			( m_nSolidType != SOLID_BBOX ) && ( m_nSolidType != SOLID_NONE );
}

void CCollisionProperty::ClearSolidFlags( void )
{
	SetSolidFlags( 0 );
}

void CCollisionProperty::RemoveSolidFlags( int flags )
{
	SetSolidFlags( m_usSolidFlags & ~flags );
}

void CCollisionProperty::AddSolidFlags( int flags )
{
	SetSolidFlags( m_usSolidFlags | flags );
}

int CCollisionProperty::GetSolidFlags( void ) const
{
	return m_usSolidFlags;
}

bool CCollisionProperty::IsSolidFlagSet( int flagMask ) const
{
	return (m_usSolidFlags & flagMask) != 0;
}

bool CCollisionProperty::IsSolid() const
{
	return ::IsSolid( (SolidType_t)(unsigned char)m_nSolidType, m_usSolidFlags );
}

//-----------------------------------------------------------------------------
// Returns the center in OBB space
//-----------------------------------------------------------------------------
const Vector& CCollisionProperty::OBBCenter( ) const
{
	Vector &vecResult = AllocTempVector();
	VectorLerp( m_vecMins, m_vecMaxs, 0.5f, vecResult );
	return vecResult;
}

//-----------------------------------------------------------------------------
// center point of entity
//-----------------------------------------------------------------------------
const Vector &CCollisionProperty::WorldSpaceCenter( ) const 
{
	Vector &vecResult = AllocTempVector();
	CollisionToWorldSpace( OBBCenter(), &vecResult );
	return vecResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in OBB space to world space
//-----------------------------------------------------------------------------
const Vector &CCollisionProperty::CollisionToWorldSpace( const Vector &in, Vector *pResult ) const 
{
	// Makes sure we don't re-use the same temp twice
	if ( !IsBoundsDefinedInEntitySpace() || ( GetCollisionAngles() == vec3_angle ) )
	{
		VectorAdd( in, GetCollisionOrigin(), *pResult );
	}
	else
	{
		VectorTransform( in, CollisionToWorldTransform(), *pResult );
	}
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a point in world space to OBB space
//-----------------------------------------------------------------------------
const Vector &CCollisionProperty::WorldToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	if ( !IsBoundsDefinedInEntitySpace() || ( GetCollisionAngles() == vec3_angle ) )
	{
		VectorSubtract( in, GetCollisionOrigin(), *pResult );
	}
	else
	{
		VectorITransform( in, CollisionToWorldTransform(), *pResult );
	}
	return *pResult;
}

//-----------------------------------------------------------------------------
// Transforms a direction in world space to OBB space
//-----------------------------------------------------------------------------
const Vector & CCollisionProperty::WorldDirectionToCollisionSpace( const Vector &in, Vector *pResult ) const
{
	if ( !IsBoundsDefinedInEntitySpace() || ( GetCollisionAngles() == vec3_angle ) )
	{
		*pResult = in;
	}
	else
	{
		VectorIRotate( in, CollisionToWorldTransform(), *pResult );
	}
	return *pResult;
}

//-----------------------------------------------------------------------------
// Computes a bounding box in world space surrounding the collision bounds
//-----------------------------------------------------------------------------
void CCollisionProperty::WorldSpaceAABB( Vector *pWorldMins, Vector *pWorldMaxs ) const
{
	CollisionAABBToWorldAABB( m_vecMins, m_vecMaxs, pWorldMins, pWorldMaxs );
}

//-----------------------------------------------------------------------------
// Does a rotation make us need to recompute the surrounding box?
//-----------------------------------------------------------------------------
bool CCollisionProperty::DoesSequenceChangeInvalidateSurroundingBox( ) const
{
	return ( m_nSurroundType == USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS );
}

//-----------------------------------------------------------------------------
// Does a rotation make us need to recompute the surrounding box?
//-----------------------------------------------------------------------------
bool CCollisionProperty::DoesRotationInvalidateSurroundingBox( ) const
{
	if ( IsSolidFlagSet(FSOLID_ROOT_PARENT_ALIGNED) )
		return true;

	switch ( m_nSurroundType )
	{
	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
	case USE_OBB_COLLISION_BOUNDS:
	case USE_BEST_COLLISION_BOUNDS:
		return IsBoundsDefinedInEntitySpace();

	// In the case of game code, we don't really know, so we have to assume it does
	case USE_HITBOXES:
	case USE_GAME_CODE:
		return true;

	case USE_ROTATION_EXPANDED_BOUNDS:
	case USE_SPECIFIED_BOUNDS:
	case USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS:
		return false;

	default:
		contract_assert( 0 );
		return true;
	}
}

class CBasePlayer;
class CColorCorrection;

using namespace ::ks::reflect;

// Purpose: Color correction entity with simple radial falloff

static ConVar mat_colcorrection_disableentities( side_registry(), "mat_colcorrection_disableentities", "0", FCVAR_NONE, "Disable map color-correction entities" );

static ConVar mat_colcorrection_forceentitiesclientside( side_registry(), "mat_colcorrection_forceentitiesclientside", "0", FCVAR_CHEAT, "Forces color correction entities to be updated on the client" );

IMPLEMENT_REFLECT_CLIENTCLASS( CColorCorrection, DT_ColorCorrection, CColorCorrection )

//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
CColorCorrection::CColorCorrection()
{
	m_minFalloff = -1.0f;
	m_maxFalloff = -1.0f;
	m_flFadeInDuration = 0.0f;
	m_flFadeOutDuration = 0.0f;
	m_flCurWeight = 0.0f;
	m_flMaxWeight = 1.0f;
	m_netLookupFilename[0] = '\0';
	m_bEnabled = false;
	m_bMaster = false;
	m_bExclusive = false;
	m_CCHandle = INVALID_CLIENT_CCHANDLE;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; i++ )
	{
		m_bEnabledOnClient[i] = false;
		m_flCurWeightOnClient[i] = 0.0f;
		m_bFadingIn[i] = false;
		m_flFadeStartWeight[i] = 0.0f;
		m_flFadeStartTime[i] = 0.0f;
		m_flFadeDuration[i] = 0.0f;
	}
}

CColorCorrection::~CColorCorrection()
{
	g_pColorCorrectionMgr->RemoveColorCorrectionEntity( this, m_CCHandle );
}

bool CColorCorrection::IsClientSide() const
{
	return m_bClientSide || mat_colcorrection_forceentitiesclientside.GetBool();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CColorCorrection::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( m_CCHandle == INVALID_CLIENT_CCHANDLE )
		{
			// forming a unique name without extension
			char cleanName[MAX_PATH];
			V_StripExtension( m_netLookupFilename, cleanName, sizeof( cleanName ) );
			char name[MAX_PATH];
			V_snprintf( name, MAX_PATH, "%s_%d", cleanName, entindex() );

			m_CCHandle = g_pColorCorrectionMgr->AddColorCorrectionEntity( this, name, m_netLookupFilename );
		}
	}
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool CColorCorrection::ShouldDraw()
{
	return false;
}

void CColorCorrection::Update( CBasePlayer *pPlayer, float ccScale )
{
	contract_assert( m_CCHandle != INVALID_CLIENT_CCHANDLE );

	if ( mat_colcorrection_disableentities.GetInt() )
	{
		// Allow the colorcorrectionui panel (or user) to turn off color-correction entities
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, 0.0f, m_bExclusive );
		return;
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	bool bEnabled = IsClientSide() ? m_bEnabledOnClient[nSlot] : m_bEnabled;

	// fade weight on client
	if ( IsClientSide() )
	{
		m_flCurWeightOnClient[nSlot] = Lerp( GetFadeRatio( nSlot ), m_flFadeStartWeight[nSlot], m_bFadingIn[nSlot] ? m_flMaxWeight : 0.0f );
	}

	float flCurWeight = IsClientSide() ? m_flCurWeightOnClient[nSlot] : m_flCurWeight;

	if( !bEnabled && flCurWeight == 0.0f )
	{
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, 0.0f, m_bExclusive );
		return;
	}

	Vector playerOrigin = pPlayer->GetAbsOrigin();

	float weight = 0;
	if ( ( m_minFalloff != -1 ) && ( m_maxFalloff != -1 ) && m_minFalloff != m_maxFalloff )
	{
		float dist = (playerOrigin - m_vecOrigin).Length();
		weight = (dist-m_minFalloff) / (m_maxFalloff-m_minFalloff);
		if ( weight<0.0f ) weight = 0.0f;	
		if ( weight>1.0f ) weight = 1.0f;	
	}

	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, flCurWeight * ( 1.0 - weight ) * ccScale, m_bExclusive );
}

void CColorCorrection::EnableOnClient( bool bEnable, bool bSkipFade )
{
	if ( !IsClientSide() )
	{
		return;
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if( m_bEnabledOnClient[nSlot] == bEnable )
	{
		return;
	}

	m_bFadingIn[nSlot] = bEnable;
	m_bEnabledOnClient[nSlot] = bEnable;

	// initialize countdown timer
	m_flFadeStartWeight[nSlot] = m_flCurWeightOnClient[nSlot];
	float flFadeTimeScale = 1.0f;
	if ( m_flMaxWeight != 0.0f )
	{
		flFadeTimeScale = m_flCurWeightOnClient[nSlot] / m_flMaxWeight;
	}

	if ( m_bFadingIn[nSlot] )
	{
		flFadeTimeScale = 1.0f - flFadeTimeScale;
	}

	if ( bSkipFade )
	{
		flFadeTimeScale = 0.0f;
	}

	StartFade( nSlot, flFadeTimeScale * ( m_bFadingIn[nSlot] ? m_flFadeInDuration : m_flFadeOutDuration ) );

	// update the clientside weight once here, in case the fade duration is 0
	m_flCurWeightOnClient[nSlot] = Lerp( GetFadeRatio( nSlot ), m_flFadeStartWeight[nSlot], m_bFadingIn[nSlot] ? m_flMaxWeight : 0.0f );
}

Vector CColorCorrection::GetOrigin()
{
	return m_vecOrigin;
}

float CColorCorrection::GetMinFalloff()
{
	return m_minFalloff;
}

float CColorCorrection::GetMaxFalloff()
{
	return m_maxFalloff;
}

void CColorCorrection::SetWeight( float fWeight )
{
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, fWeight, false );
}

void CColorCorrection::StartFade( int nSplitScreenSlot, float flDuration )
{
	m_flFadeStartTime[nSplitScreenSlot] = gpGlobals->curtime;
	m_flFadeDuration[nSplitScreenSlot] = std::max( flDuration, 0.0f );
}

float CColorCorrection::GetFadeRatio( int nSplitScreenSlot ) const
{
	float flRatio = 1.0f;
	
	if ( m_flFadeDuration[nSplitScreenSlot] != 0.0f )
	{
		flRatio = ( gpGlobals->curtime - m_flFadeStartTime[nSplitScreenSlot] ) / m_flFadeDuration[nSplitScreenSlot];
		flRatio = clamp( flRatio, 0.0f, 1.0f );
	}
	return flRatio;
}

bool CColorCorrection::IsFadeTimeElapsed( int nSplitScreenSlot ) const
{
	return	( ( gpGlobals->curtime - m_flFadeStartTime[nSplitScreenSlot] ) > m_flFadeDuration[nSplitScreenSlot] ) ||
			( ( gpGlobals->curtime - m_flFadeStartTime[nSplitScreenSlot] ) < 0.0f );
}

void UpdateColorCorrectionEntities( CBasePlayer *pPlayer, float ccScale, CColorCorrection **pList, int listCount )
{
	for ( int i = 0; i < listCount; i++ )
	{
		pList[i]->Update(pPlayer, ccScale);
	}
}

class CBasePlayer;
class CColorCorrection;
class IClientMode;

using namespace ::ks::reflect;

// Purpose: Singleton manager for color correction on the client

//------------------------------------------------------------------------------
// Singleton access
//------------------------------------------------------------------------------
static CColorCorrectionMgr s_ColorCorrectionMgr;
CColorCorrectionMgr *g_pColorCorrectionMgr = &s_ColorCorrectionMgr;

static ConVar mat_colcorrection_editor( side_registry(), "mat_colcorrection_editor", "0" );

static std::vector< CColorCorrection * > g_ColorCorrectionList;
static std::vector< CColorCorrectionVolume * > g_ColorCorrectionVolumeList;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
CColorCorrectionMgr::CColorCorrectionMgr()
{
	m_nActiveWeightCount = 0;
}

//------------------------------------------------------------------------------
// Creates, destroys color corrections
//------------------------------------------------------------------------------
ClientCCHandle_t CColorCorrectionMgr::AddColorCorrection( const char *pName, const char *pFileName )
{
	if ( !pFileName )
	{
		pFileName = pName;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ColorCorrectionHandle_t ccHandle = pRenderContext->AddLookup( pName );
	if ( ccHandle )
	{
		pRenderContext->LockLookup( ccHandle );
		pRenderContext->LoadLookup( ccHandle, pFileName );
		pRenderContext->UnlockLookup( ccHandle );
	}
	else
	{
		Warning("Cannot find color correction lookup file: '%s'\n", pFileName );
	}

	return (ClientCCHandle_t)ccHandle;
}

void CColorCorrectionMgr::RemoveColorCorrection( ClientCCHandle_t h )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->RemoveLookup( ccHandle );
	}
}

ClientCCHandle_t CColorCorrectionMgr::FindColorCorrection( const char *pName )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	ColorCorrectionHandle_t ccHandle = pRenderContext->FindLookup( pName );
	
	return (ClientCCHandle_t)ccHandle;
}

ClientCCHandle_t CColorCorrectionMgr::AddColorCorrectionEntity( CColorCorrection *pEntity, const char *pName, const char *pFileName )
{
	ClientCCHandle_t h = AddColorCorrection(pName, pFileName);
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		contract_assert( ks::find_index( g_ColorCorrectionList, pEntity ) == -1 );
		g_ColorCorrectionList.push_back( pEntity );
	}
	return h;
}

void CColorCorrectionMgr::RemoveColorCorrectionEntity( CColorCorrection *pEntity, ClientCCHandle_t h)
{
	RemoveColorCorrection(h);
	ks::swap_erase_first( g_ColorCorrectionList, pEntity );
}

ClientCCHandle_t CColorCorrectionMgr::AddColorCorrectionVolume( CColorCorrectionVolume *pVolume, const char *pName, const char *pFileName )
{
	ClientCCHandle_t h = AddColorCorrection(pName, pFileName);
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		contract_assert( ks::find_index( g_ColorCorrectionVolumeList, pVolume ) == -1 );
		g_ColorCorrectionVolumeList.push_back( pVolume );
	}
	return h;
}

void CColorCorrectionMgr::RemoveColorCorrectionVolume( CColorCorrectionVolume *pVolume, ClientCCHandle_t h)
{
	RemoveColorCorrection(h);
	ks::swap_erase_first( g_ColorCorrectionVolumeList, pVolume );
}

//------------------------------------------------------------------------------
// Modify color correction weights
//------------------------------------------------------------------------------
void CColorCorrectionMgr::SetColorCorrectionWeight( ClientCCHandle_t h, float flWeight, bool bExclusive )
{
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		SetWeightParams_t params = { h, flWeight, bExclusive };
		m_colorCorrectionWeights.push_back( params );
		if( bExclusive && m_bHaveExclusiveWeight && ( flWeight != 0.0f ) )
		{
			DevWarning( "Found multiple active color_correction entities with exclusive setting enabled. This is invalid.\n" );
		}
		if ( bExclusive )
		{
			m_bHaveExclusiveWeight = true;
			m_flExclusiveWeight = flWeight;
		}
	}
}

void CColorCorrectionMgr::CommitColorCorrectionWeights()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	for ( int i = 0; i < std::ssize( m_colorCorrectionWeights ); i++ )
	{
		ColorCorrectionHandle_t ccHandle = reinterpret_cast<ColorCorrectionHandle_t>( m_colorCorrectionWeights[i].handle );
		float flWeight = m_colorCorrectionWeights[i].flWeight;
		if ( !m_colorCorrectionWeights[i].bExclusive )
		{
			flWeight = (1.0f - m_flExclusiveWeight ) * m_colorCorrectionWeights[i].flWeight;
		}
		pRenderContext->SetLookupWeight( ccHandle, flWeight );

		// FIXME: NOTE! This doesn't work if the same handle has
		// its weight set twice with no intervening calls to ResetColorCorrectionWeights
		// which, at the moment, is true
		if ( flWeight != 0.0f )
		{
			++m_nActiveWeightCount;
		}
	}
	m_colorCorrectionWeights.clear();
}

void CColorCorrectionMgr::ResetColorCorrectionWeights()
{
	CVProfScope _( "ResetColorCorrectionWeights", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// FIXME: Where should I put this? It needs to happen prior to SimulateEntities()
	// which is where the client thinks for c_colorcorrection + c_colorcorrectionvolumes
	// update the color correction weights.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->ResetLookupWeights();
	m_nActiveWeightCount = 0;
	m_bHaveExclusiveWeight = false;
	m_flExclusiveWeight = 0.0f;
	m_colorCorrectionWeights.clear();
}

void CColorCorrectionMgr::SetResetable( ClientCCHandle_t h, bool bResetable )
{
	// NOTE: Setting stuff to be not resettable doesn't work when in queued mode
	// because the logic that sets m_nActiveWeightCount to 0 in ResetColorCorrectionWeights
	// is no longer valid when stuff is not resettable.
	contract_assert( bResetable || !g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED );
	if ( h != INVALID_CLIENT_CCHANDLE )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ColorCorrectionHandle_t ccHandle = (ColorCorrectionHandle_t)h;
		pRenderContext->SetResetable( ccHandle, bResetable );
	}
}

//------------------------------------------------------------------------------
// Is color correction active?
//------------------------------------------------------------------------------
bool CColorCorrectionMgr::HasNonZeroColorCorrectionWeights() const
{
	return ( m_nActiveWeightCount != 0 ) || mat_colcorrection_editor.GetBool();
}

void CColorCorrectionMgr::UpdateColorCorrection()
{
	ResetColorCorrectionWeights();
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	IClientMode *pClientMode = GetClientMode();

	contract_assert( pClientMode );
	if ( !pPlayer || !pClientMode )
	{
		return;
	}

	pClientMode->OnColorCorrectionWeightsReset();
	float ccScale = pClientMode->GetColorCorrectionScale();

	UpdateColorCorrectionEntities( pPlayer, ccScale, g_ColorCorrectionList.data(), std::ssize( g_ColorCorrectionList ) );
	UpdateColorCorrectionVolumes( pPlayer, ccScale, g_ColorCorrectionVolumeList.data(), std::ssize( g_ColorCorrectionVolumeList ) );
	CommitColorCorrectionWeights();
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Color correction entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class CColorCorrectionVolume : public CBaseTrigger
{
	[[= ks::reflect::NetTable{ .name = "DT_ColorCorrectionVolume" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CColorCorrectionVolume, CBaseTrigger );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	CColorCorrectionVolume();
	virtual ~CColorCorrectionVolume();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

	void Update( CBasePlayer *pPlayer, float ccScale );

	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

private:
	float	m_LastEnterWeight;
	float	m_LastEnterTime;

	float	m_LastExitWeight;
	float	m_LastExitTime;
	[[= ks::reflect::Net{} ]] bool	m_bEnabled;
	[[= ks::reflect::Net{} ]] float	m_MaxWeight;
	[[= ks::reflect::Net{} ]] float	m_FadeDuration;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] float	m_Weight;
	[[= ks::reflect::Net{} ]] char	m_lookupFilename[MAX_PATH];

	ClientCCHandle_t m_CCHandle;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CColorCorrectionVolume, DT_ColorCorrectionVolume, CColorCorrectionVolume )

IMPLEMENT_REFLECT_PREDMAP( CColorCorrectionVolume );

//------------------------------------------------------------------------------
// Constructor, destructor
//------------------------------------------------------------------------------
CColorCorrectionVolume::CColorCorrectionVolume()
{
	m_CCHandle = INVALID_CLIENT_CCHANDLE;
}

CColorCorrectionVolume::~CColorCorrectionVolume()
{
	g_pColorCorrectionMgr->RemoveColorCorrectionVolume( this, m_CCHandle );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CColorCorrectionVolume::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( m_CCHandle == INVALID_CLIENT_CCHANDLE )
		{
			// forming a unique name without extension
			char cleanName[MAX_PATH];
			V_StripExtension( m_lookupFilename, cleanName, sizeof( cleanName ) );
			char name[MAX_PATH];
			V_snprintf( name, MAX_PATH, "%s_%d", cleanName, entindex() );

			m_CCHandle = g_pColorCorrectionMgr->AddColorCorrectionVolume( this, name, m_lookupFilename );

			SetSolid( SOLID_BSP );
			SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
		}
	}
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool CColorCorrectionVolume::ShouldDraw()
{
	return false;
}

//--------------------------------------------------------------------------------------------------------
void CColorCorrectionVolume::StartTouch( CBaseEntity *pEntity )
{
	m_LastEnterTime = gpGlobals->curtime;
	m_LastEnterWeight = m_Weight;
}

//--------------------------------------------------------------------------------------------------------
void CColorCorrectionVolume::EndTouch( CBaseEntity *pEntity )
{
	m_LastExitTime = gpGlobals->curtime;
	m_LastExitWeight = m_Weight;
}

void CColorCorrectionVolume::Update( CBasePlayer *pPlayer, float ccScale )
{
	if ( pPlayer )
	{
		bool isTouching = CollisionProp()->IsPointInBounds( pPlayer->EyePosition() );
		bool wasTouching = m_LastEnterTime > m_LastExitTime;

		if ( isTouching && !wasTouching )
		{
			StartTouch( pPlayer );
		}
		else if ( !isTouching && wasTouching )
		{
			EndTouch( pPlayer );
		}
	}

	if( !m_bEnabled )
	{
		m_Weight = 0.0f;
	}
	else
	{
		if( m_LastEnterTime > m_LastExitTime )
		{
			// we most recently entered the volume

			if( m_Weight < 1.0f )
			{
				float dt = gpGlobals->curtime - m_LastEnterTime;
				float weight = m_LastEnterWeight + dt / ((1.0f-m_LastEnterWeight)*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = weight;
			}
		}
		else
		{
			// we most recently exitted the volume

			if( m_Weight > 0.0f )
			{
				float dt = gpGlobals->curtime - m_LastExitTime;
				float weight = (1.0f-m_LastExitWeight) + dt / (m_LastExitWeight*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = 1.0f - weight;
			}
		}
	}

	//	Vector entityPosition = GetAbsOrigin();
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCHandle, m_Weight * ccScale );
}

void UpdateColorCorrectionVolumes( CBasePlayer *pPlayer, float ccScale, CColorCorrectionVolume **pList, int listCount )
{
	for ( int i = 0; i < listCount; i++ )
	{
		pList[i]->Update(pPlayer, ccScale);
	}
}

using namespace ::ks::reflect;

// Purpose: Material proxy to stuff a convar into a material var.

// identifier was truncated to '255' characters in the debug information

class CConVarMaterialProxy: public IMaterialProxy
{
public:
	CConVarMaterialProxy()
	: m_pResult( nullptr ),
	  m_conVarRef( "", true )
	{
	}

	virtual ~CConVarMaterialProxy()
	{
	}

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		const char *pResult = pKeyValues->GetString( "resultVar" );
		if ( !pResult )
			return false;

		bool found;
		m_pResult = pMaterial->FindVar( pResult, &found );
		if ( !found )
		{
			m_pResult = nullptr;
			return false;
		}

		if ( !V_stricmp( pResult, "$alpha" ) )
		{
			pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
		}

		pResult = pKeyValues->GetString( "convar" );
		if( !pResult )
		{
			return false;
		}

		m_conVarRef.Init( pResult, false );
		if ( !m_conVarRef.IsValid() )
		{
			return false;
		}

		return true;
	}

	virtual void OnBind( void* )
	{
		switch( m_pResult->GetType() )
		{
		case MATERIAL_VAR_TYPE_VECTOR:
			{
				float f = m_conVarRef.GetFloat();
				Vector4D vec( f, f, f, f );
				m_pResult->SetVecValue( vec.Base(), m_pResult->VectorSize() );
			}
			break;

		case MATERIAL_VAR_TYPE_INT:
			m_pResult->SetIntValue( m_conVarRef.GetInt() );
			break;

		case MATERIAL_VAR_TYPE_FLOAT:
		default:
			m_pResult->SetFloatValue( m_conVarRef.GetFloat() );
			break;
		}
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pResult->GetOwningMaterial();
	}

	virtual void Release()
	{
		delete this;
	}

protected:
	IMaterialVar *m_pResult;
	ConVarRef m_conVarRef;
};

EXPOSE_MATERIAL_PROXY( CConVarMaterialProxy, ConVar );

class CBaseAnimating;

using namespace ::ks::reflect;

// Purpose: Provide custom material swapping for weapons (when switch from world to view or vice versa)

CCSCustomMaterialSwapManager g_CSCustomMaterialSwapManager;

//
// global custom material swap manager
//  the game uses this to swap custom materials (after the new one is done)
//

CCSCustomMaterialSwapManager::CCSCustomMaterialSwapManager()
{
	m_pPendingSwaps.reserve( 4 );
}

CCSCustomMaterialSwapManager::~CCSCustomMaterialSwapManager()
{	
	ClearAllPendingSwaps();
}

// this is called at the end of each frame
bool ProcessCustomMaterialSwapManager()
{
	return g_CSCustomMaterialSwapManager.Process();
}

bool CCSCustomMaterialSwapManager::Init()
{
	g_pMaterialSystem->AddEndFramePriorToNextContextFunc( ::ProcessCustomMaterialSwapManager );
	return true;
}

void CCSCustomMaterialSwapManager::Shutdown()
{
	g_pMaterialSystem->RemoveEndFramePriorToNextContextFunc( ::ProcessCustomMaterialSwapManager );
	ClearAllPendingSwaps();
}

// handles swapping materials that are pending swap and ready
bool CCSCustomMaterialSwapManager::Process()
{
	for ( int i = std::ssize( m_pPendingSwaps ) - 1; i >= 0 ; i-- )
	{
		if ( m_pPendingSwaps[ i ].m_pNewCustomMaterial->IsValid() )
		{
			CBaseAnimating* pOwner = ( m_pPendingSwaps[ i ].m_hOwner ) ?  m_pPendingSwaps[ i ].m_hOwner->GetBaseAnimating() : nullptr;
			if ( pOwner )
			{
				pOwner->SetCustomMaterial( m_pPendingSwaps[ i ].m_pNewCustomMaterial,  m_pPendingSwaps[ i ].m_nCustomMaterialIndex );
			}

			m_pPendingSwaps[ i ].m_hOwner = nullptr;
			m_pPendingSwaps[ i ].m_nCustomMaterialIndex = -1;
			m_pPendingSwaps[ i ].m_pNewCustomMaterial->Release();
			m_pPendingSwaps[ i ].m_pNewCustomMaterial = nullptr;
			if ( m_pPendingSwaps[ i ].m_pOldCustomMaterial )
			{
				m_pPendingSwaps[ i ].m_pOldCustomMaterial->Release();
				m_pPendingSwaps[ i ].m_pOldCustomMaterial = nullptr;
			}
			m_pPendingSwaps.erase( m_pPendingSwaps.begin() + i );
		}
	}

	return false;
}

void CCSCustomMaterialSwapManager::RequestMaterialSwap( EHANDLE hOwner, int nCustomMaterialIndex, ICustomMaterial *pNewCustomMaterialInterface )
{
	int nSwapIndex = std::ssize( m_pPendingSwaps ); m_pPendingSwaps.emplace_back();
	CCSPendingCustomMaterialSwap_t &materialSwap = m_pPendingSwaps[ nSwapIndex ];

	materialSwap.m_hOwner = hOwner;
	materialSwap.m_nCustomMaterialIndex = nCustomMaterialIndex;

	materialSwap.m_pNewCustomMaterial = pNewCustomMaterialInterface;
	materialSwap.m_pNewCustomMaterial->AddRef();

	ICustomMaterial *pOldCustomMaterialInterface = hOwner->GetBaseAnimating()->GetCustomMaterial( nCustomMaterialIndex );
	materialSwap.m_pOldCustomMaterial = pOldCustomMaterialInterface;
	if ( materialSwap.m_pOldCustomMaterial )
	{
		materialSwap.m_pOldCustomMaterial->AddRef();
	}
}

void CCSCustomMaterialSwapManager::ClearPendingSwaps( EHANDLE hOwner, int nCustomMaterialIndex )
{
	for ( int i = std::ssize( m_pPendingSwaps ) - 1; i >= 0 ; i-- )
	{
		if ( m_pPendingSwaps[ i ].m_hOwner == hOwner && m_pPendingSwaps[ i ].m_nCustomMaterialIndex == nCustomMaterialIndex )
		{
			m_pPendingSwaps[ i ].m_hOwner = nullptr;
			m_pPendingSwaps[ i ].m_nCustomMaterialIndex = -1;
			m_pPendingSwaps[ i ].m_pNewCustomMaterial->Release();
			m_pPendingSwaps[ i ].m_pNewCustomMaterial = nullptr;
			if ( m_pPendingSwaps[ i ].m_pOldCustomMaterial )
			{
				m_pPendingSwaps[ i ].m_pOldCustomMaterial->Release();
				m_pPendingSwaps[ i ].m_pOldCustomMaterial = nullptr;
			}
			m_pPendingSwaps.erase( m_pPendingSwaps.begin() + i );
		}
	}
}

void CCSCustomMaterialSwapManager::ClearAllPendingSwaps( void )
{
	for ( int i = 0; i < std::ssize( m_pPendingSwaps ); ++i )
	{
		CCSPendingCustomMaterialSwap_t &materialSwap = m_pPendingSwaps[ i ];
		materialSwap.m_hOwner = nullptr;
		materialSwap.m_nCustomMaterialIndex = -1;
		materialSwap.m_pNewCustomMaterial->Release();
		materialSwap.m_pNewCustomMaterial = nullptr;
		if ( m_pPendingSwaps[ i ].m_pOldCustomMaterial )
		{
			materialSwap.m_pOldCustomMaterial->Release();
			materialSwap.m_pOldCustomMaterial = nullptr;
		}
	}
	m_pPendingSwaps.clear();
}

using namespace ::ks::reflect;

// We don't currently support smooth cascade transitioning on Gameconsoles. This convar must be adjusted if we ever do.
ConVar cl_csm_shadow_split_lerp_factor_range( side_registry(), "cl_csm_shadow_split_lerp_factor_range", ".2", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_shadow_split_radial_dist_lerp_factor_multiplier( side_registry(), "cl_csm_shadow_split_radial_dist_lerp_factor_multiplier", ".85", FCVAR_DEVELOPMENTONLY );

namespace CCSMFrustumDefinition
{
	Vector4D g_vProjFrustumVerts[NUM_FRUSTUM_VERTS] = 
	{
		Vector4D( -1.0f, -1.0f, 0.0f, 1.0f ),
		Vector4D(  1.0f, -1.0f, 0.0f, 1.0f ),
		Vector4D(  1.0f,  1.0f, 0.0f, 1.0f ),
		Vector4D( -1.0f,  1.0f, 0.0f, 1.0f ),

		Vector4D( -1.0f, -1.0f, 1.0f, 1.0f ),
		Vector4D(  1.0f, -1.0f, 1.0f, 1.0f ),
		Vector4D(  1.0f,  1.0f, 1.0f, 1.0f ),
		Vector4D( -1.0f,  1.0f, 1.0f, 1.0f )
	};

	const uint8 g_nFrustumLineVertIndices[24] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	const uint8 g_nQuadVertIndices[NUM_FRUSTUM_QUADS][4] =
	{
		// 0 left
		{ 4, 0, 3, 7 }, 
		// 1 right
		{ 1, 5, 6, 2 }, 
		// 2 bottom
		{ 0, 4, 5, 1 }, 
		// 3 top
		{ 2, 6, 7, 3 }, 
		// 4 near
		{ 0, 1, 2, 3 }, 
		// 5 far
		{ 7, 6, 5, 4 }  
	};

	// The vertices of each frustum edge
	const uint8 g_nEdgeVertIndices[NUM_FRUSTUM_EDGES][2] =
	{
		{ 4, 0 }, { 0, 3 }, { 3, 7 }, { 7, 4 },
		{ 1, 5 }, { 5, 6 }, { 6, 2 }, { 2, 1 },
		{ 4, 5 }, { 1, 0 }, { 6, 7 }, { 3, 2 }
	};

	// The quads sharing each frustum edge
	const uint8 g_nEdgeQuadIndices[NUM_FRUSTUM_EDGES][2] =
	{
		{ 0, 2 }, { 0, 4 },
		{ 0, 3 }, { 0, 5 },
		{ 1, 2 }, { 1, 5 },
		{ 1, 3 }, { 1, 4 },
		{ 2, 5 }, { 2, 4 },
		{ 3, 5 }, { 3, 4 }
	};
} // namespace CCSMFrustumDefinition

static void InvertVMatrix( const VMatrix &src, VMatrix &dst )
{
	src.InverseGeneral( dst );
}

CCSMParallelSplit::CCSMParallelSplit() :
	m_bValid( false ),
	m_nShadowBufferSize( 0 ),
	m_nShadowCascadeMaxSize( 0 ),
	m_nShadowAtlasWidth( 0 ),
	m_nShadowAtlasHeight( 0 )
{
	V_memset( &m_viewState, 0, sizeof( m_viewState ) );
	V_memset( &m_lightState, 0, sizeof( m_lightState ) );
}

CCSMParallelSplit::~CCSMParallelSplit()
{
}

void CCSMParallelSplit::Clear()
{
	m_bValid = false;
	m_nShadowBufferSize = 0;
	m_nShadowCascadeMaxSize = 0;
	m_nShadowAtlasWidth = 0;
	m_nShadowAtlasHeight = 0;
	V_memset( &m_viewState, 0, sizeof( m_viewState ) );
	V_memset( &m_lightState, 0, sizeof( m_lightState ) );
}

CCSMParallelSplit::CCSMParallelSplit( const CCSMParallelSplit &other )
{
	m_bValid = other.m_bValid;
	m_nShadowBufferSize = other.m_nShadowBufferSize;	
	m_nShadowCascadeMaxSize = other.m_nShadowCascadeMaxSize;
	m_nShadowAtlasWidth = other.m_nShadowAtlasWidth;
	m_nShadowAtlasHeight = other.m_nShadowAtlasHeight;
	V_memcpy( &m_viewState, &other.m_viewState, sizeof( m_viewState ) );
	V_memcpy( &m_lightState, &other.m_lightState, sizeof( m_lightState ) );
}

const CCSMParallelSplit &CCSMParallelSplit::operator==( const CCSMParallelSplit &rhs )
{
	if ( this == &rhs )
		return *this;
	
	m_bValid = rhs.m_bValid;
	m_nShadowBufferSize = rhs.m_nShadowBufferSize;	
	m_nShadowCascadeMaxSize = rhs.m_nShadowCascadeMaxSize;
	m_nShadowAtlasWidth = rhs.m_nShadowAtlasWidth;
	m_nShadowAtlasHeight = rhs.m_nShadowAtlasHeight;
	V_memcpy( &m_viewState, &rhs.m_viewState, sizeof( m_viewState ) );
	V_memcpy( &m_lightState, &rhs.m_lightState, sizeof( m_lightState ) );	
	
	return *this;
}

void CCSMParallelSplit::Init( uint nMaxShadowBufferSize, uint nMaxCascadeSize )
{
	m_nShadowBufferSize = nMaxShadowBufferSize;
	m_nShadowCascadeMaxSize = nMaxCascadeSize;
	m_nShadowAtlasWidth = m_nShadowBufferSize * 2;
	m_nShadowAtlasHeight = m_nShadowBufferSize * 2;
}

ConVar cl_csm_parallel_split_log_lin_lerp( side_registry(), "cl_csm_parallel_split_log_lin_lerp", ".94", FCVAR_DEVELOPMENTONLY, "" );

ConVar cl_csm_parallel_split_dist1( side_registry(), "cl_csm_parallel_split_dist1", "-1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_parallel_split_dist2( side_registry(), "cl_csm_parallel_split_dist2", "-1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_parallel_split_dist3( side_registry(), "cl_csm_parallel_split_dist3", "-1", FCVAR_DEVELOPMENTONLY, "" );

float CCSMParallelSplit::CalculateSplitPlaneDistance( int nSplit, int nShadowSplits, float flMaxShadowDistance, float flZNear, float flZFar )
{
	if ( ( nSplit == 1 ) && ( cl_csm_parallel_split_dist1.GetFloat() >= 0.0f ) )
		return Lerp( cl_csm_parallel_split_dist1.GetFloat(), flZNear, flZFar );
	else if ( ( nSplit == 2 ) && ( cl_csm_parallel_split_dist2.GetFloat() >= 0.0f ) )
		return Lerp( cl_csm_parallel_split_dist2.GetFloat(), flZNear, flZFar );
	else if ( ( nSplit == 3 ) && ( cl_csm_parallel_split_dist3.GetFloat() >= 0.0f ) )
		return Lerp( cl_csm_parallel_split_dist3.GetFloat(), flZNear, flZFar );

	contract_assert( flZNear > 0.f );
	contract_assert( flZFar > flZNear );
	// http://appsrv.cse.cuhk.edu.hk/~fzhang/pssm_vrcia/shadow_vrcia.pdf
	const float fIM = (float)nSplit / (float)nShadowSplits;
	const float f = std::min( flMaxShadowDistance, flZFar );
	const float n = flZNear;
	const float fBias = 0.1f;

	const float flLog = n * std::powf( f / n, fIM );	
	const float flLinear = n + ( f - n ) * fIM;
	
	const float flLogScaler = clamp( cl_csm_parallel_split_log_lin_lerp.GetFloat(), 0.0f, 1.0f );
	const float flLinScaler = 1.0f - flLogScaler;

	return ( flLog * flLogScaler + flLinear * flLinScaler ) + fBias;
}

static constexpr auto MAX_INCLUSION_VOLUME_PLANES = 11;

template<typename T> inline void swapPlane( T &lhs, T &rhs ) { T temp( lhs ); lhs = rhs; rhs = temp; }

extern ConVar cl_csm_print_culling_planes;

// Note: flMaxDistBetweenAnyCasterAndReceiver is only used to ensure the "top" cap plane is placed far enough away from the view frustum to catch all potential casters.
uint CCSMParallelSplit::ComputeCullingVolumePlanes( VPlane *pOutPlanes, Vector vDirToLight, const CFrustum &sceneFrustum, float flMaxDistBetweenAnyCasterAndReceiver )
{
	Vector vLightDir( -vDirToLight );

	const VMatrix sceneWorldToProj( sceneFrustum.GetViewProj() );
	VMatrix sceneProjToWorld;
	InvertVMatrix( sceneWorldToProj, sceneProjToWorld );
		
	// Extract world relative frustum planes (UpdateCameraAndFrustumFromOrthoViewProjMatrices computes camera worldspace relative planes assuming no world->view translation).
	VPlane vWorldFrustumPlanes[CCSMFrustumDefinition::NUM_FRUSTUM_PLANES];
	ExtractClipPlanesFromNonTransposedMatrix( sceneWorldToProj, vWorldFrustumPlanes );

	// Canonicalize the plane order to be consistent with the predefined frustum edge/quad definitions, new order: posX, negX, posY, negY, posZ, negZ.
	swapPlane( vWorldFrustumPlanes[0], vWorldFrustumPlanes[1] );
	swapPlane( vWorldFrustumPlanes[2], vWorldFrustumPlanes[3] );
	
	// Compute frustum cornerpoints. First set are the actual cornerpoints, the 2nd set are pushed "up" towards the light.
	Vector vWorldFrustumVerts[CCSMFrustumDefinition::NUM_FRUSTUM_VERTS * 2];
	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
	{
		sceneProjToWorld.V3Mul( Vector( CCSMFrustumDefinition::g_vProjFrustumVerts[i].x, CCSMFrustumDefinition::g_vProjFrustumVerts[i].y, CCSMFrustumDefinition::g_vProjFrustumVerts[i].z ), vWorldFrustumVerts[i] );
		if ( !vWorldFrustumVerts[i].IsValid() )
		{
			// If this assert fires, something is probably wrong with the frustum's matrices.
			// this log triggers all the time with the particle editor up
			//Log_Warning(  LOG_SCENESYSTEM, "CCSMParallelSplit::ComputeCullingVolumePlanes: Failed computing world frustum vertices from inverted scene matrices!\n" );
			return 0;
		}
		vWorldFrustumVerts[i + CCSMFrustumDefinition::NUM_FRUSTUM_VERTS] = vWorldFrustumVerts[i] + vDirToLight * flMaxDistBetweenAnyCasterAndReceiver;
	}
	
	const Vector vFrustumCenter( ( vWorldFrustumVerts[0] + vWorldFrustumVerts[6] ) * .5f );

	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_PLANES; ++i )
	{
		if ( vWorldFrustumPlanes[i].DistTo( vFrustumCenter ) <= 0.0f )
		{
			// Invalid view frustum, don't even try to compute culling volume (should never happen).
			return 0;
		}
	}
	uint nNumCullingPlanes = 0;
	
	// First compute "lower" planes, which are any planes from the scene's view frustum which face towards the light.
	bool bFacingFlags[CCSMFrustumDefinition::NUM_FRUSTUM_QUADS];
	uint nNumFacingPlanes = 0, nNumEdgePlanes = 0;
	for ( uint nQuadIndex = 0; nQuadIndex < CCSMFrustumDefinition::NUM_FRUSTUM_QUADS; ++nQuadIndex )
	{
		const VPlane& lowerPlane = vWorldFrustumPlanes[nQuadIndex];

		contract_assert( lowerPlane.DistTo( vFrustumCenter ) >= 0.0f );

		const bool bFacingFlag = ( lowerPlane.m_Normal.Dot( vLightDir ) ) < 0.0f;

		bFacingFlags[nQuadIndex] = bFacingFlag;

		// Reject quads that face away from the light (i.e. the "uppermost" quads relative to the light dir).
		
		if ( !bFacingFlag )
			continue;

		pOutPlanes[nNumCullingPlanes++] = lowerPlane;
		
		nNumFacingPlanes++;
	}

	// Compute the upper plane, which faces towards the center of the view frustum but is very far away in the direction of the light to capture all potential shadow casters. 
	VPlane upperPlane;
	upperPlane.m_Normal = vLightDir;
	upperPlane.m_Dist = 0.0f;

	double flPlaneDist = 0.0f;

	for ( int i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS * 2; ++i )
	{
		double flDist = vWorldFrustumVerts[i].x * upperPlane.m_Normal.x + vWorldFrustumVerts[i].y * upperPlane.m_Normal.y + vWorldFrustumVerts[i].z * upperPlane.m_Normal.z - flPlaneDist;
		if ( flDist < 0.0f )
		{
			flPlaneDist += flDist;
		}
	}
		
	// FIXME: Find a better way to ensure the frustum verts (plus the extra "top" vert) are always on the pos halfspace of the top cap plane. This is a crappy way to account for double->float precision loss. 
	flPlaneDist *= 1.125f;

	upperPlane.m_Dist = static_cast< float >( flPlaneDist );
		
//#ifdef _DEBUG

	contract_assert( upperPlane.DistTo( vFrustumCenter ) >= 0.0f );
	pOutPlanes[nNumCullingPlanes++] = upperPlane;

	// Now compute "side" planes by finding the silhouette edges relative to the light dir.
	for ( uint nEdgeIndex = 0; nEdgeIndex < CCSMFrustumDefinition::NUM_FRUSTUM_EDGES; ++nEdgeIndex )
	{
		// Determine if the quads sharing this edge are a silhouette.
		if ( bFacingFlags[CCSMFrustumDefinition::g_nEdgeQuadIndices[nEdgeIndex][0]] == bFacingFlags[CCSMFrustumDefinition::g_nEdgeQuadIndices[nEdgeIndex][1]])
			continue;

		Vector vEdgeVec( vWorldFrustumVerts[CCSMFrustumDefinition::g_nEdgeVertIndices[nEdgeIndex][0]] - vWorldFrustumVerts[CCSMFrustumDefinition::g_nEdgeVertIndices[nEdgeIndex][1]]);
		vEdgeVec.NormalizeInPlace();
		
		Vector vNorm( vEdgeVec.Cross( vLightDir) );
		vNorm.NormalizeInPlace();
		VPlane sidePlane;
		sidePlane.m_Normal = vNorm;
		sidePlane.m_Dist = vNorm.Dot( vWorldFrustumVerts[CCSMFrustumDefinition::g_nEdgeVertIndices[nEdgeIndex][0]] );
			
		// Ensure side plane is always facing towards the center of the view frustum.
		if ( sidePlane.DistTo( vFrustumCenter ) < 0.0f)
		{
			sidePlane = sidePlane.Flip();
		}

		pOutPlanes[nNumCullingPlanes++] = sidePlane;
		
		nNumEdgePlanes++;
	}
		
	if ( nNumCullingPlanes < 6 )
	{
		// this log triggers continuously when editing particles
		//Log_Warning( LOG_SCENESYSTEM, "CCSMParallelSplit::ComputeCullingVolumePlanes: Failed computing closed culling volume!\n" );
		return 0;
	}

	contract_assert( nNumCullingPlanes <= CVolumeCuller::cMaxInclusionVolumePlanes );
	
	// nNumFacingPlanes can range from [1,5]
	// nNumEdgePlanes can either be 4 or 6.
	// There's always 1 "upper" cap plane.
	// Worst case number of planes (I think):
	// Facing=5, Edge=4, Total=10
	// Facing=4, Edge=6, Total=11
	if ( cl_csm_print_culling_planes.GetBool() ) 
	{
		Warning( "Total inclusion planes: %u (Facing: %u Edge: %u)\n", nNumCullingPlanes, nNumFacingPlanes, nNumEdgePlanes );
	}
	
	return nNumCullingPlanes;
}

float CCSMParallelSplit::ComputeFarPlaneCameraRelativePoints( Vector *pPoints, float flZNear, float flZFar, const CFrustum &frustum )
{
	float flClipSpaceBottomLeftX = -1.0f;
	float flClipSpaceBottomLeftY = -1.0f;
	float flClipSpaceTopRightX = 1.0f;
	float flClipSpaceTopRightY = 1.0f;
		
	if ( 0 ) //( GetFlag( SUNLIGHT_MANAGER_FLAG_FIXED_NEAR_FAR_PLANES_FOR_CASCADE_CREATION ) )
	{ 
		// Ensure that we're an ortho off-center frustum, push our bounds out
		if ( flClipSpaceBottomLeftX < -1 || flClipSpaceBottomLeftY < -1 || flClipSpaceTopRightX > 1 || flClipSpaceTopRightY > 1 )
		{
			flClipSpaceBottomLeftX -= 400.0f;
			flClipSpaceBottomLeftY -= 400.0f;
			flClipSpaceTopRightX += 400.0f;
			flClipSpaceTopRightY += 400.0f;
		}
	}

	frustum.CalcFarPlaneCameraRelativePoints( pPoints, flZNear, flClipSpaceBottomLeftX, flClipSpaceBottomLeftY, flClipSpaceTopRightX, flClipSpaceTopRightY );
	frustum.CalcFarPlaneCameraRelativePoints( &pPoints[ 4 ], flZFar, flClipSpaceBottomLeftX, flClipSpaceBottomLeftY, flClipSpaceTopRightX, flClipSpaceTopRightY );
	
	const Vector &vEye = frustum.GetCameraPosition();

	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
	{
		pPoints[i] += vEye;
	}

	// Brute force code to determine the largest possible projection of this cascade's subfrustum from the light's point of view.
	float flSubfrustumDiagonalLength = 0.0f;
	for ( uint i = 0; i < 8; i++ )
	{
		for ( uint j = i + 1; j < 8; j++ )
		{
			Vector vSubfrustumDiagonal( pPoints[i] - pPoints[j] );
			float flTrialDiagonalLength = vSubfrustumDiagonal.Length();
			if ( flTrialDiagonalLength > flSubfrustumDiagonalLength  )
			{
				flSubfrustumDiagonalLength  = flTrialDiagonalLength;
			}
		}
	}
	return flSubfrustumDiagonalLength;
}

extern ConVar cl_csm_xlat_continuity;
void CCSMParallelSplit::CalculateShadowFrustaParallelSplits( 
	const CFrustum &sceneFrustum,
	uint &nOutCascadeSize,
	CFrustum *pOutFrustums, 
	CVolumeCuller *pOutVolumeCullers,
	float &flOutActualMaxShadowDist, 
	int nMaxCascadeSize, 
	Vector vDirToLight, 
	float flMaxShadowDistance, 
	float flMaxVisibleDistance,
	int nCSMQualityLevel,
	ShadowFrustaDebugInfo_t* pDebugInfo )
{
	const uint NUM_WORLD_FOCUS_POINTS = 8;
	
	// vDirToLight - dir to light
	vDirToLight.NormalizeInPlace();
		
	//const Vector &vEye = sceneFrustum.GetCameraPosition();
	const Vector vEyeDir( sceneFrustum.CameraForward().Normalized() );

	//VMatrix invViewProj( sceneFrustum.GetInvViewProj().Transpose() );
		
	float flSplitPlaneDistances[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE];

	float flSceneZNear = std::max( 0.125f, sceneFrustum.GetCameraNearPlane() );
	float flSceneZFar = sceneFrustum.GetCameraFarPlane();
					
	for ( int nCascadeIndex = 0; nCascadeIndex < nMaxCascadeSize; ++nCascadeIndex )
	{
		flSplitPlaneDistances[nCascadeIndex] = CalculateSplitPlaneDistance( 1 + nCascadeIndex, nMaxCascadeSize, flMaxShadowDistance, flSceneZNear, flSceneZFar );
	}
	
	// Compute scene frustum cornerpoints at the max. shadow distance, which is used to compute the max. actual shadow distance used for smoothly transitioning to unshadowed based off distance.
	Vector vWorldFrustumVertsAtMaxShadowDist[CCSMFrustumDefinition::NUM_FRUSTUM_VERTS];
	float flMaxSubfrustumDiagonalLength = ComputeFarPlaneCameraRelativePoints( vWorldFrustumVertsAtMaxShadowDist, flSceneZNear, flSplitPlaneDistances[nMaxCascadeSize - 1], sceneFrustum );
	if ( ( nCSMQualityLevel <= CSMQUALITY_LOW ) )
	{
		// 7LS - check with RichG - this seems to give the appropriate length calc for using radial distance fade in the PS as opposed to the one below
		// RG - Yup, this is correct.
		flOutActualMaxShadowDist = flMaxSubfrustumDiagonalLength / 1.42f; // sqrt(2) - account for dist from center to corner of square texture
	}
	else
	{
		flOutActualMaxShadowDist = flMaxSubfrustumDiagonalLength * 1.42f; // sqrt(2) - account for dist from center to corner of square texture
	}
	ComputeFarPlaneCameraRelativePoints( vWorldFrustumVertsAtMaxShadowDist, flSceneZNear, flOutActualMaxShadowDist, sceneFrustum );
			
	// flMaxDistanceBetweenAnyCasterAndReceiver is only needed to capture all potential invisible shadow casters.
	// We don't have the scene's bounds so there's a big fudge factor here to account for large terrains and low sun angles.
	// We wouldn't need to push back the near plane so much if we where using pancaking.
	//const float flMaxDistanceBetweenAnyCasterAndReceiver = std::max( flOutActualMaxShadowDist, std::max( std::max( flMaxVisibleDistance, flMaxShadowDistance ), 15000.0f ) );
	const float flMaxDistanceBetweenAnyCasterAndReceiver = std::max( flOutActualMaxShadowDist, std::max( std::max( flMaxVisibleDistance, flMaxShadowDistance ), 2000.0f ) );

	//flOutActualMaxShadowDist = flMaxSubfrustumDiagonalLength; // sqrt(2) - account for dist from center to corner of square texture
		
	// Compute look-at matrix 
	Vector shadowWorldToViewAt( -vDirToLight );
	Vector shadowWorldToViewRight( CrossProduct( Vector( 0, 0, 1 ), shadowWorldToViewAt ) );
	if ( shadowWorldToViewRight.LengthSqr() < .001f )
	{
		shadowWorldToViewRight = CrossProduct( Vector( 0, 1, 0 ), shadowWorldToViewAt );
	}
	shadowWorldToViewRight.NormalizeInPlace();

	Vector shadowWorldToViewUp( CrossProduct( shadowWorldToViewAt, shadowWorldToViewRight ) );
	shadowWorldToViewUp.NormalizeInPlace();

	VMatrix shadowWorldToViewRot;
	
	shadowWorldToViewRot.Init(
		shadowWorldToViewRight.x, shadowWorldToViewRight.y, shadowWorldToViewRight.z, 0.0f, 
		-shadowWorldToViewUp.x, -shadowWorldToViewUp.y, -shadowWorldToViewUp.z, 0.0f,
		-shadowWorldToViewAt.x, -shadowWorldToViewAt.y, -shadowWorldToViewAt.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f );
	
	nOutCascadeSize = 0;

	//float flCascade0ShadowViewMaxZ = -1e-10f; 
	float flCascade0ShadowViewMinZ = 1e-10f; flCascade0ShadowViewMinZ;
	VPlane exclusionFrustumPlanes[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE][6];

	static_assert( CVolumeCuller::cMaxInclusionVolumePlanes >= MAX_INCLUSION_VOLUME_PLANES );
	
	struct CascadeParams_t
	{
		VMatrix m_ShadowWorldToView;
		VMatrix m_ShadowViewToProj;
		float m_flLeft;
		float m_flRight;
		float m_flTop;
		float m_flBottom;
		float m_flZNear;
		float m_flZFar;
	};

	CascadeParams_t cascadeParams[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE];

	float flCascadeShadowViewMinZ = 1e+10f;
	float flCascadeShadowViewMaxZ = -1e-10f;

	for ( int nCascadeIndex = 0; nCascadeIndex < nMaxCascadeSize; ++nCascadeIndex )
	{
		float flSceneSplitZNear = !nCascadeIndex ? flSceneZNear : flSplitPlaneDistances[nCascadeIndex - 1];
		float flSceneSplitZFar = flSplitPlaneDistances[nCascadeIndex];

		if ( nCascadeIndex )
		{
			// No need to expand the cascade beyond the max visible distance (we'll never see any shadows cast into invisible areas of the scene).
			if ( flSceneSplitZNear > flMaxVisibleDistance )
				break;
		}

		// 0=[left,bottom],1=[right,bottom],2=[left,top],3=[right,top]
		Vector vSubfrustumWorldPoints[ NUM_WORLD_FOCUS_POINTS ];
		ComputeFarPlaneCameraRelativePoints( vSubfrustumWorldPoints, flSceneSplitZNear, flSceneSplitZFar, sceneFrustum );
					
		// Determine bounds of orthographic projection of this subfrustum's cornerpoints.
		Vector shadowFocusRegionVerts[NUM_WORLD_FOCUS_POINTS];
		const float flVeryLarge = 9e+12;
		Vector vShadowFocusRegionMin( flVeryLarge, flVeryLarge, flVeryLarge );
		Vector vShadowFocusRegionMax( -flVeryLarge, -flVeryLarge, -flVeryLarge );
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i], shadowFocusRegionVerts[i] );
			vShadowFocusRegionMin = vShadowFocusRegionMin.Min( shadowFocusRegionVerts[i] );
			vShadowFocusRegionMax = vShadowFocusRegionMax.Max( shadowFocusRegionVerts[i] );
		}

		// Expand the subfrustum "up" towards the light to catch all shadow casters (even those outside of the scene's view frustum).
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i] + vDirToLight * flMaxDistanceBetweenAnyCasterAndReceiver, vLightPoint );
			vShadowFocusRegionMin.z = std::min( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = std::max( vShadowFocusRegionMax.z, vLightPoint.z );
		}

		// Now expand the Z near/far range so it fully encompasses the actual worldspace area that could be shadowed with this cascade's texture (this must take into account the shadow distance fading in the shader).
		// Note this is very wasteful of Z precision.
		for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vWorldFrustumVertsAtMaxShadowDist[i], vLightPoint );
			vShadowFocusRegionMin.z = std::min( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = std::max( vShadowFocusRegionMax.z, vLightPoint.z );
		}

		flCascadeShadowViewMinZ = std::min( vShadowFocusRegionMin.z, flCascadeShadowViewMinZ );
		flCascadeShadowViewMaxZ = std::max( vShadowFocusRegionMax.z, flCascadeShadowViewMaxZ );
	}

	// Important note: Source1 uses right handed ortho projection matrices, so negative viewspace Z is actually inside the frustum. This is opposite from the CSM code in Source2.
	for ( int nCascadeIndex = 0; nCascadeIndex < nMaxCascadeSize; ++nCascadeIndex )
	{
		pDebugInfo[nCascadeIndex].m_nNumWorldFocusVerts = NUM_WORLD_FOCUS_POINTS;

		float flSceneSplitZNear = !nCascadeIndex ? flSceneZNear : flSplitPlaneDistances[nCascadeIndex - 1];
		float flSceneSplitZFar = flSplitPlaneDistances[nCascadeIndex];
		
		if ( nCascadeIndex )
		{
			// No need to expand the cascade beyond the max visible distance (we'll never see any shadows cast into invisible areas of the scene).
			if ( flSceneSplitZNear > flMaxVisibleDistance )
				break;
		}
				
		pDebugInfo[nCascadeIndex].m_flSplitPlaneDistance = flSceneSplitZFar;
		
		// 0=[left,bottom],1=[right,bottom],2=[left,top],3=[right,top]
		Vector vSubfrustumWorldPoints[ NUM_WORLD_FOCUS_POINTS ];
		float flSubfrustumDiagonalLength = ComputeFarPlaneCameraRelativePoints( vSubfrustumWorldPoints, flSceneSplitZNear, flSceneSplitZFar, sceneFrustum );
		
		flSubfrustumDiagonalLength = std::ceil( flSubfrustumDiagonalLength * 100.0f ) / 100.0f;
				
		Vector vSubfrustumWorldCenter( ( vSubfrustumWorldPoints[0] + vSubfrustumWorldPoints[4 + 3] ) * .5f );
							
		V_memcpy( &pDebugInfo[nCascadeIndex].m_WorldFocusVerts, vSubfrustumWorldPoints, sizeof( vSubfrustumWorldPoints ) );
		
		// Determine bounds of orthographic projection of this subfrustum's cornerpoints.
		Vector shadowFocusRegionVerts[NUM_WORLD_FOCUS_POINTS];
		const float flVeryLarge = 9e+12;
		Vector vShadowFocusRegionMin( flVeryLarge, flVeryLarge, flVeryLarge );
		Vector vShadowFocusRegionMax( -flVeryLarge, -flVeryLarge, -flVeryLarge );
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i], shadowFocusRegionVerts[i] );
			vShadowFocusRegionMin = vShadowFocusRegionMin.Min( shadowFocusRegionVerts[i] );
			vShadowFocusRegionMax = vShadowFocusRegionMax.Max( shadowFocusRegionVerts[i] );
		}

		// Expand the subfrustum "up" towards the light to catch all shadow casters (even those outside of the scene's view frustum).
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i] + vDirToLight * flMaxDistanceBetweenAnyCasterAndReceiver, vLightPoint );
			vShadowFocusRegionMin.z = std::min( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = std::max( vShadowFocusRegionMax.z, vLightPoint.z );
		}
		
		// Now expand the Z near/far range so it fully encompasses the actual worldspace area that could be shadowed with this cascade's texture (this must take into account the shadow distance fading in the shader).
		// Note this is very wasteful of Z precision.
		for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vWorldFrustumVertsAtMaxShadowDist[i], vLightPoint );
			vShadowFocusRegionMin.z = std::min( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = std::max( vShadowFocusRegionMax.z, vLightPoint.z );
		}

		// Set all cascades to the same MinZ/MaxZ so we only need to transform into ortho Z once in the shader (not per cascade). This is doable with 24-bit Z.
		vShadowFocusRegionMin.z = flCascadeShadowViewMinZ;
		vShadowFocusRegionMax.z = flCascadeShadowViewMaxZ;
		
		// Push "back" the shadow camera towards the light, so the NearZ used to construct the projection matrix can be set to 0.
		// X, Y MUST be 0,0 here for the UV continuity solution to work.
		VMatrix shadowWorldToViewTrans;
		
		Vector vShadowCameraPos( 0.0f, 0.0f, vShadowFocusRegionMax.z );
		vShadowFocusRegionMin.z -= vShadowFocusRegionMax.z;
		vShadowFocusRegionMax.z = 0.0f;

		MatrixBuildTranslation( shadowWorldToViewTrans, -vShadowCameraPos );
		
		VMatrix shadowWorldToView( shadowWorldToViewTrans * shadowWorldToViewRot );

		float flShadowFocusRegionWidth = vShadowFocusRegionMax.x - vShadowFocusRegionMin.x;
		float flShadowFocusRegionHeight = vShadowFocusRegionMax.y - vShadowFocusRegionMin.y;

		float flShadowFocusRegionOffsetX = ( flSubfrustumDiagonalLength - flShadowFocusRegionWidth ) * .5f;
		float flShadowFocusRegionOffsetY = ( flSubfrustumDiagonalLength - flShadowFocusRegionHeight ) * .5f;

		// Expand the ortho projection so it is always large enough to contain the largest projection of the subfrustum in any orientation.
		vShadowFocusRegionMin.x -= flShadowFocusRegionOffsetX;
		vShadowFocusRegionMax.x += flShadowFocusRegionOffsetX;
		vShadowFocusRegionMin.y -= flShadowFocusRegionOffsetY;
		vShadowFocusRegionMax.y += flShadowFocusRegionOffsetY;

		// Eliminate any subpixel offset in the ortho projection parameters.
		float flWorldUnitsPerTexel = flSubfrustumDiagonalLength / m_nShadowBufferSize;
		
		if ( cl_csm_xlat_continuity.GetBool() )
		{
			vShadowFocusRegionMin.x = ((int)std::floor( vShadowFocusRegionMin.x / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;
			vShadowFocusRegionMax.x = ((int)floor( vShadowFocusRegionMax.x / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;
			vShadowFocusRegionMin.y = ((int)floor( vShadowFocusRegionMin.y / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;
			vShadowFocusRegionMax.y = ((int)floor( vShadowFocusRegionMax.y / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;

			vShadowFocusRegionMin.z = floor( vShadowFocusRegionMin.z );
			vShadowFocusRegionMax.z = ceil( vShadowFocusRegionMax.z );
		}
				
		// Build ortho projection matrix
		VMatrix shadowViewToProj;
		float left = vShadowFocusRegionMin.x, right = vShadowFocusRegionMax.x;
		float bottom = vShadowFocusRegionMax.y, top = vShadowFocusRegionMin.y;
		float zNear, zFar;
		
		// Flipped/negated because MatrixBuildOrtho() is RH (viewspace -Z is inside the frustum)
		zNear = vShadowFocusRegionMax.z ? -vShadowFocusRegionMax.z : 0.0f;
		zFar = -vShadowFocusRegionMin.z;
		
		// Beware MatrixBuildOrtho() actually flips top/bottom! (ARGH)
		MatrixBuildOrtho( shadowViewToProj, left, top, right, bottom, zNear, zFar );
						
		// Ensure m[2][3] is exactly 0.0f
		shadowViewToProj.m[2][3] = 0.0f;

		pDebugInfo[nCascadeIndex].m_flLeft = left;
		pDebugInfo[nCascadeIndex].m_flRight = right;
		pDebugInfo[nCascadeIndex].m_flTop = top;
		pDebugInfo[nCascadeIndex].m_flBottom = bottom;
		pDebugInfo[nCascadeIndex].m_flZNear = zNear;
		pDebugInfo[nCascadeIndex].m_flZFar = zFar;

		CascadeParams_t &params = cascadeParams[nCascadeIndex];
		params.m_ShadowWorldToView = shadowWorldToView;
		params.m_ShadowViewToProj = shadowViewToProj;
		params.m_flLeft = left;
		params.m_flRight = right;
		params.m_flTop = top;
		params.m_flBottom = bottom;
		params.m_flZNear = zNear;
		params.m_flZFar = zFar;

		nOutCascadeSize++;
	}

	VPlane cullingInclusionPlanes[CVolumeCuller::cMaxInclusionVolumePlanes];
	const uint nNumCullingInclusionPlanes = ComputeCullingVolumePlanes( cullingInclusionPlanes, vDirToLight, sceneFrustum, flMaxDistanceBetweenAnyCasterAndReceiver );
	
	// Phase 2 - create output frustum, matrices, and culling volumes.
	for ( uint nCascadeIndex = 0; nCascadeIndex < nOutCascadeSize; ++nCascadeIndex )
	{
		CascadeParams_t &params = cascadeParams[nCascadeIndex];

		// Build this cascade's output frustum.
		CFrustum *pOutFrustum = &pOutFrustums[nCascadeIndex];

		pOutFrustum->BuildShadowFrustum( params.m_ShadowWorldToView, params.m_ShadowViewToProj );

		// Compute exclusion frustum planes, taking into account smooth cascade transitioning.
		// The exclusion volume must completely fit inside the "inner" circle defining the beginning of the cascade transition region.
		float flLerpAmount = cl_csm_shadow_split_lerp_factor_range.GetFloat();
						
		VMatrix conservativeShadowViewToProj;
		MatrixBuildOrtho( conservativeShadowViewToProj, 
			Lerp( flLerpAmount, params.m_flLeft, params.m_flRight ),
			Lerp( flLerpAmount, params.m_flTop, params.m_flBottom ),
			Lerp( flLerpAmount, params.m_flRight, params.m_flLeft ),
			Lerp( flLerpAmount, params.m_flBottom, params.m_flTop ),
			params.m_flZNear, params.m_flZFar );
		
		VMatrix conservativeShadowWorldToProj( conservativeShadowViewToProj * params.m_ShadowWorldToView );
		// Extract world relative frustum planes (UpdateCameraAndFrustumFromOrthoViewProjMatrices computes camera worldspace relative planes assuming no world->view translation).
		VPlane *pFrustumPlanes = &exclusionFrustumPlanes[nCascadeIndex][0];
		
		ExtractClipPlanesFromNonTransposedMatrix( conservativeShadowWorldToProj, pFrustumPlanes );
						
		// Init this cascade's volume culler
		pOutVolumeCullers[nCascadeIndex].Clear();

		VPlane basePlanes[6];
		ExtractClipPlanesFromNonTransposedMatrix( pOutFrustum->GetViewProj(), basePlanes );
		pOutVolumeCullers[nCascadeIndex].SetBaseFrustumPlanes( basePlanes );

		if ( nCascadeIndex > 1 )
		{
			pOutVolumeCullers[nCascadeIndex].SetExclusionFrustumPlanes( &exclusionFrustumPlanes[nCascadeIndex - 1][0] );
		}
				
		if ( nNumCullingInclusionPlanes )
		{
			pOutVolumeCullers[nCascadeIndex].SetInclusionVolumePlanes( cullingInclusionPlanes, nNumCullingInclusionPlanes );
		}
	}
}

void CCSMParallelSplit::ComputeCascadeProjToTexMatrices( SunLightState_t &lightState )
{
	float fOffsetX = 0.5f + ( 0.5f / static_cast< float >( m_nShadowBufferSize ) );
	float fOffsetY = 0.5f + ( 0.5f / static_cast< float >( m_nShadowBufferSize ) );
	float frange   = 1.0f;
				
	static_assert( 4 == MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );

	float flBiases[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE];
	static float s_flDefBliases[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE] = { -0.000005f, -0.000005f, -0.000008f, -0.00009f };
	V_memcpy( flBiases, s_flDefBliases, sizeof( flBiases )	);
	
	const bool bInvertZ = false; 

	// Z is *not* inverted when rendering to R32F textures (because the Z flip is in viewport matrix created by rendersystem).
	if ( bInvertZ )
	{
		frange   = -1.0f;
	}
	
	for ( uint i = 0; i < lightState.m_nShadowCascadeSize; ++i )
	{	
		// Create projection to normalized texture space matrix used for shadow mapping.
		float flBiasVal = flBiases[i];
		if ( bInvertZ )
		{
			flBiasVal = 1.0f - flBiasVal;
		}

		VMatrix texScaleBiasMat( 
			0.5f,     0.0f,     0.0f,		fOffsetX,
			0.0f,    -0.5f,     0.0f,		fOffsetY,
			0.0f,     0.0f,     frange,		flBiasVal,
			0.0f,	   0.0f,     0.0f,	    1.0f );
		
		lightState.m_CascadeProjToTexMatrices[ i ] = texScaleBiasMat;
	}
}

float g_flDebugForcedMaxVisibleRadius;

bool CCSMParallelSplit::Update( const SunLightViewState_t &viewState )
{
	if ( !m_nShadowBufferSize )
	{
		m_bValid = false;
		return false;
	}

	m_viewState = viewState;
	
	m_lightState.m_nShadowCascadeSize = m_nShadowCascadeMaxSize;

	// Important note: The scaleU/scaleV params are currently assumed to be (.5, .5) in the shaders!
	static const Vector4D s_vecAtlasUVs[4] = 
	{
		// ofsU, ofsV, scaleU, scaleV
		Vector4D( 0.0f, 0.0f, 0.5f, 0.5f ),
		Vector4D( 0.5f, 0.0f, 0.5f, 0.5f ),
		Vector4D( 0.0f, 0.5f, 0.5f, 0.5f ),
		Vector4D( 0.5f, 0.5f, 0.5f, 0.5f )
	};
	
	// Initializing them all because the loop below reads them all (to guarantee ALL shader constants are all initialized to something).
	for ( int i = 0; i < MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE; ++i )
	{
		m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[i] = s_vecAtlasUVs[ ( i + viewState.m_nAtlasFirstCascadeIndex ) % MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	}
				
	Vector vDirToLight( 0, 0, 1 );
	vDirToLight = -m_viewState.m_Direction;
	
	Vector4D lightColor( m_viewState.m_LightColor.r / 255.0f, m_viewState.m_LightColor.g / 255.0f, m_viewState.m_LightColor.b / 255.0f, m_viewState.m_LightColorScale / 255.0f);

	// convert to linear space color for shader use, also save some shader instructions be pre-multiplying by 3.0f for bump normalized values stored in vertex/lightmap alpha for CSM blending
	lightColor.x = GammaToLinearFullRange( lightColor.x ) * lightColor.w * 3.0f;
	lightColor.y = GammaToLinearFullRange( lightColor.y ) * lightColor.w * 3.0f;
	lightColor.z = GammaToLinearFullRange( lightColor.z ) * lightColor.w * 3.0f;
	lightColor.w = 1.0f / g_pMaterialSystemHardwareConfig->GetLightMapScaleFactor();

	const CFrustum &sceneFrustum = m_viewState.m_frustum;;

	static bool bDebugBreak = false;
			
	float flMaxVisibleDist = m_viewState.m_flMaxVisibleDist;
	flMaxVisibleDist = std::min( flMaxVisibleDist, 7500.0f );	// arbitrary limit - where did I get this from?

	if ( g_flDebugForcedMaxVisibleRadius )
	{
		flMaxVisibleDist = g_flDebugForcedMaxVisibleRadius;
	}
	
	ComputeCascadeProjToTexMatrices( m_lightState );

	uint nMaxCascades = std::min( (int)m_nShadowCascadeMaxSize, (int)m_viewState.m_nMaxCascadeSize );
	if ( !nMaxCascades ) 
	{
		nMaxCascades = 1;
	}
	
	const float flMaxShadowDist = m_viewState.m_flMaxShadowDist;
	
	CalculateShadowFrustaParallelSplits( sceneFrustum, m_lightState.m_nShadowCascadeSize, m_lightState.m_CascadeFrustums, m_lightState.m_CascadeVolumeCullers, m_lightState.m_flActualMaxShadowDist, nMaxCascades, vDirToLight, flMaxShadowDist, flMaxVisibleDist, viewState.m_nCSMQualityLevel, m_lightState.m_DebugInfo );

	m_lightState.m_SunLightShaderParams.m_nNumCascades = m_lightState.m_nShadowCascadeSize;
	for ( uint nCascadeIndex = 0; nCascadeIndex < MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE; ++nCascadeIndex )
	{
		m_lightState.m_SunLightShaderParams.m_matWorldToShadowTexMatrices[nCascadeIndex] = m_lightState.m_CascadeProjToTexMatrices[nCascadeIndex] * m_lightState.m_CascadeFrustums[nCascadeIndex].GetViewProj();

		Rect_t &viewport = m_lightState.m_CascadeViewports[nCascadeIndex];
		viewport.x = m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[nCascadeIndex].x * m_nShadowAtlasWidth;
		viewport.y = m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[nCascadeIndex].y * m_nShadowAtlasWidth;
		viewport.width = m_nShadowBufferSize;
		viewport.height = m_nShadowBufferSize;

		ExtractClipPlanesFromNonTransposedMatrix( m_lightState.m_CascadeFrustums[nCascadeIndex].GetViewProj(), &m_lightState.m_CascadeFrustumPlanes[nCascadeIndex][0] );
	}		

	m_lightState.m_SunLightShaderParams.m_vLightDir = vDirToLight;
	m_lightState.m_SunLightShaderParams.m_vLightColor = lightColor;	
	
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flInvShadowTextureWidth = 1.0f / m_nShadowAtlasWidth;
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flInvShadowTextureHeight = 1.0f / m_nShadowAtlasHeight;
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flHalfInvShadowTextureWidth = .5f / m_nShadowAtlasWidth;
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flHalfInvShadowTextureHeight = .5f / m_nShadowAtlasHeight;
	
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flShadowTextureWidth = m_nShadowAtlasWidth;
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flShadowTextureHeight = m_nShadowAtlasHeight;
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flSplitLerpFactorBase = .5f - cl_csm_shadow_split_lerp_factor_range.GetFloat();
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flSplitLerpFactorInvRange = 1.0f / cl_csm_shadow_split_lerp_factor_range.GetFloat();

	// cheaper quadratic distance fade 
	float flZLerpStartDist = m_lightState.m_flActualMaxShadowDist * cl_csm_shadow_split_radial_dist_lerp_factor_multiplier.GetFloat();
	float flZLerpEndDist = m_lightState.m_flActualMaxShadowDist;
	float flQ = 1.0f / ( flZLerpEndDist * flZLerpEndDist - flZLerpStartDist * flZLerpStartDist );
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorBase = - ( ( flZLerpStartDist * flZLerpStartDist ) * flQ );
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorInvRange = flQ;

	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flUnused0 = 0.0f;
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flUnused1 = 0.0f;

	m_lightState.m_SunLightShaderParams.m_vCamPosition.Init( sceneFrustum.GetCameraPosition().x, sceneFrustum.GetCameraPosition().y, sceneFrustum.GetCameraPosition().z, 1.0f );
	
	m_lightState.m_flZLerpStartDist = flZLerpStartDist;
	m_lightState.m_flZLerpEndDist = m_lightState.m_flActualMaxShadowDist;

	if ( viewState.m_bSetAllCascadesToFirst )
	{
		// Copy cascade 0's world->shadow matrix and UV texture offsets to all other cascades (used for the viewmodel),
		// so the pixel shader can use any of them for viewmodel shadowing.
		for ( int i = 1; i < MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE; ++i )
		{
			m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[i] = m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[0];
			m_lightState.m_SunLightShaderParams.m_matWorldToShadowTexMatrices[ i ] = m_lightState.m_SunLightShaderParams.m_matWorldToShadowTexMatrices[ 0 ];
		}
	}
	
	m_bValid = true;

	return true;
}

class CBaseAnimating;
class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Material proxy to get the cycle from a CBaseAnimateing derived entity.

// identifier was truncated to '255' characters in the debug information

class CCycleMaterialProxy: public CEntityMaterialProxy
{
public:
	CCycleMaterialProxy()
	{
		m_pMaterial = nullptr;
		m_pResult = nullptr;
		m_bEaseIn = false;
		m_bEaseOut = false;
		m_fStart = 0.0f;
		m_fEnd = 1.0f;
	}
	virtual ~CCycleMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;

		const char *pResult = pKeyValues->GetString( "resultVar", nullptr  );
		if ( !pResult )
			return false;

		bool found;
		m_pResult = m_pMaterial->FindVar( pResult, &found );
		if ( !found )
		{
			m_pResult = nullptr;
			return false;
		}

		if ( !V_stricmp( pResult, "$alpha" ) )
		{
			pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
		}

		pResult = pKeyValues->GetString( "easein", nullptr );
		if( pResult && V_atoi( pResult ) != 0 )
		{
			m_bEaseIn = true;
		}

		pResult = pKeyValues->GetString( "easeout", nullptr );
		if( pResult && V_atoi( pResult ) != 0 )
		{
			m_bEaseOut = true;
		}

		pResult = pKeyValues->GetString( "start", nullptr );
		if( pResult )
		{
			m_fStart = V_atof( pResult );
		}

		pResult = pKeyValues->GetString( "end", nullptr );
		if( pResult )
		{
			m_fEnd = V_atof( pResult );
		}

		return true;
	}
	virtual void OnBind( CBaseEntity *pC_BaseEntity )
	{
		CBaseAnimating *pBaseAnimating = pC_BaseEntity ? pC_BaseEntity->GetBaseAnimating() : nullptr;
		if ( pBaseAnimating )
		{
			float fCycle = pBaseAnimating->GetCycle();
			float f = RemapValClamped( fCycle, m_fStart, m_fEnd, 0.0f, 1.0f );
			if ( m_bEaseIn && m_bEaseOut )
			{
				f = SimpleSpline( f );
			}
			else if ( m_bEaseIn )
			{
				f = std::sin( std::numbers::pi * f * 0.5f );
			}
			else if ( m_bEaseOut )
			{
				f = 1.0f - std::sin( std::numbers::pi * f * 0.5f + 0.5f * std::numbers::pi );
			}
			
			MaterialVarType_t resultType;
			int vecSize;
			ComputeResultType( resultType, vecSize );

			switch( resultType )
			{
			case MATERIAL_VAR_TYPE_VECTOR:
				{
					Vector4D vec( f, f, f, f );
					m_pResult->SetVecValue( vec.Base(), vecSize );
				}
				break;

			case MATERIAL_VAR_TYPE_FLOAT:
			case MATERIAL_VAR_TYPE_INT:
			default:
				m_pResult->SetFloatValue( f );
				break;
			}
		}
	}
	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

	void ComputeResultType( MaterialVarType_t& resultType, int& vecSize )
	{
		vecSize = 1;
		resultType = m_pResult->GetType();
		if (resultType == MATERIAL_VAR_TYPE_VECTOR)
		{
			vecSize = m_pResult->VectorSize();
		}
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pResult;
	bool m_bEaseIn;
	bool m_bEaseOut;
	float m_fStart;
	float m_fEnd;
};

EXPOSE_MATERIAL_PROXY( CCycleMaterialProxy, Cycle );
