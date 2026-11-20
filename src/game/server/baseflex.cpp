// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "choreoactor.h"
#include "choreoscene.h"
#include "eiface.h"
#include "irecipientfilter.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "vscript/ivscript.h"
#include "ai_criteria.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "reflect_annotations.h"
#include "predictable_entity.h"
#include "shake.h"
#include "baseentity.h"
#include "edict.h"
#include "igameevents.h"
#include "usermessages_schema.h"
#include "declare_class.h"
#include "engine/IEngineTrace.h"
#include "vphysics/player_controller.h"
#include "baseplayer_shared.h"
#include "shareddefs.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.platform;
import ks.core.time;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :fish;
import :baseentity;
import ks.game.ai_activity;
import :player;
import :vscript;
import :sharedvar;
import :nav_4;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.game.tempentity;
import ks.core.types;
import ks.tier1.playerid;
import ks.core.scratcharena;
import ks.game.in_buttons;
import ks.math.transform;
import ks.math.vector2d;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :cstrike15.hostage.cs_simple_hostage;
import :econ.econ_item_schema_minimal;

class CBaseAnimating;
class CBaseEntity;
class CBaseFlex;
class CFlexCycler;
class CSceneEventInfo;
class CTakeDamageInfo;

using namespace ::ks::reflect;

static ConVar scene_showlook( side_registry(), "scene_showlook", "0", FCVAR_ARCHIVE, "When playing back, show the directions of look events." );
static ConVar scene_showmoveto( side_registry(), "scene_showmoveto", "0", FCVAR_ARCHIVE, "When moving, show the end location." );
static ConVar scene_showunlock( side_registry(), "scene_showunlock", "0", FCVAR_ARCHIVE, "Show when a vcd is playing but normal AI is running." );

// static ConVar scene_checktagposition( "scene_checktagposition", "0", FCVAR_ARCHIVE, "When playing back a choreographed scene, check the current position of the tags relative to where they were authored." );

// Fake layer # to force HandleProcessSceneEvent to actually allocate the layer during npc think time instead of in between.
static constexpr auto REQUEST_DEFERRED_LAYER_ALLOCATION = -2;

extern bool g_bClientFlex;

// ---------------------------------------------------------------------
//
// CBaseFlex -- physically simulated brush rectangular solid
//
// ---------------------------------------------------------------------

void* SendProxy_FlexWeights( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Don't any flexweights to client unless scene_clientflex.GetBool() is false
	if ( !g_bClientFlex )
		return (void*)pVarData;
	else
		return nullptr;
}

REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_FlexWeights );

// SendTable stuff.
IMPLEMENT_REFLECT_SERVERCLASS( CBaseFlex, DT_BaseFlex )

IMPLEMENT_REFLECT_DATAMAP( CBaseFlex )

BEGIN_ENT_SCRIPTDESC( CBaseFlex, CBaseAnimating, "Animated characters who have vertex flex capability." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetOldestScene, "GetCurrentScene", "Returns the instance of the oldest active scene entity (if any)." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetSceneByIndex, "GetSceneByIndex", "Returns the instance of the scene entity at the specified index." )
END_SCRIPTDESC();

LINK_ENTITY_TO_CLASS( funCBaseFlex, CBaseFlex ); // meaningless independant class!!

CBaseFlex::CBaseFlex( void )
{
	m_bUpdateLayerPriorities = true;
	m_flLastFlexAnimationTime = 0.0;
}

CBaseFlex::~CBaseFlex( void )
{
	m_LocalToGlobal.clear();
	// If a player is forcibly removed while speaking a response .vcd we can get into here w/ some events waiting to
	//  be closed out.  It's benign and we just discard the events.  The CSceneEntity uses EHANDLEs so it'll wipe the
	//  scene out on next think anyway.
	m_SceneEvents.clear();
}

void CBaseFlex::SetModel_Impl( const char *szModelName )
{
	MDLCACHE_CRITICAL_SECTION();

	BaseClass::SetModel_Impl( szModelName );

	for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
	{
		SetFlexWeight( i, 0.0f );
	}
}

CStudioHdr* CBaseFlex::OnNewModel_Impl()
{
	return BaseClass::OnNewModel_Impl();
}

void CBaseFlex::SetViewtarget( const Vector &viewtarget )
{
	m_viewtarget = viewtarget;	// bah
}

void CBaseFlex::SetFlexWeight( LocalFlexController_t index, float value )
{
	if (index >= 0 && index < GetNumFlexControllers())
	{
		CStudioHdr *pstudiohdr = GetModelPtr( );
		if (! pstudiohdr)
			return;

		mstudioflexcontroller_t *pflexcontroller = pstudiohdr->pFlexcontroller( index );

		if (pflexcontroller->max != pflexcontroller->min)
		{
			value = (value - pflexcontroller->min) / (pflexcontroller->max - pflexcontroller->min);
			value = clamp( value, 0.0, 1.0 );
		}

		m_flexWeight.Set( index, value );
	}
}

float CBaseFlex::GetFlexWeight( LocalFlexController_t index )
{
	if (index >= 0 && index < GetNumFlexControllers())
	{
		CStudioHdr *pstudiohdr = GetModelPtr( );
		if (! pstudiohdr)
			return 0;

		mstudioflexcontroller_t *pflexcontroller = pstudiohdr->pFlexcontroller( index );

		if (pflexcontroller->max != pflexcontroller->min)
		{
			return m_flexWeight[index] * (pflexcontroller->max - pflexcontroller->min) + pflexcontroller->min;
		}

		return m_flexWeight[index];
	}
	return 0.0;
}

LocalFlexController_t CBaseFlex::FindFlexController( const char *szName )
{
	for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
	{
		if (stricmp( GetFlexControllerName( i ), szName ) == 0)
		{
			return i;
		}
	}

	// AssertMsg( 0, UTIL_VarArgs( "flexcontroller %s couldn't be mapped!!!\n", szName ) );
	return LocalFlexController_t(0);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFlex::StartChoreoScene( CChoreoScene *scene )
{
	if ( ks::find_index( m_ActiveChoreoScenes, scene ) != -1 )
	{
		return;
	}

	m_ActiveChoreoScenes.push_back( scene );
	m_bUpdateLayerPriorities = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFlex::RemoveChoreoScene( CChoreoScene *scene, bool canceled )
{
	// Assert( m_ActiveChoreoScenes.Find( scene ) != m_ActiveChoreoScenes.InvalidIndex() );

	ks::erase_first( m_ActiveChoreoScenes, scene );
	m_bUpdateLayerPriorities = true;

	if (canceled)
	{
		
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

int CBaseFlex::GetScenePriority( CChoreoScene *scene )
{
	int iPriority = 0;
	int c = std::ssize( m_ActiveChoreoScenes );
	// count number of channels in scenes older than current
	for ( int i = 0; i < c; i++ )
	{
		CChoreoScene *pScene = m_ActiveChoreoScenes[ i ];
		if ( !pScene )
		{
			continue;
		}

		if ( pScene == scene )
		{
			break;
		}

		iPriority += pScene->GetNumChannels( );
	}
	return iPriority;
}

//-----------------------------------------------------------------------------
// Purpose: Remove all active SceneEvents
//-----------------------------------------------------------------------------
void CBaseFlex::ClearSceneEvents( CChoreoScene *scene, bool canceled )
{
	if ( !scene )
	{
		m_SceneEvents.clear();
		return;
	}

	for ( int i = std::ssize( m_SceneEvents ) - 1; i >= 0; i-- )
	{
		CSceneEventInfo *info = &m_SceneEvents[ i ];

		contract_assert( info );
		contract_assert( info->m_pScene );
		contract_assert( info->m_pEvent );

		if ( info->m_pScene != scene )
			continue;

		if ( !ClearSceneEvent( info, false, canceled ))
		{
			// unknown expression to clear!!
			contract_assert( 0 );
		}

		// Free this slot
		info->m_pEvent		= nullptr;
		info->m_pScene		= nullptr;
		info->m_bStarted	= false;

		m_SceneEvents.erase( m_SceneEvents.begin() + i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop specifics of expression
//-----------------------------------------------------------------------------

bool CBaseFlex::ClearSceneEvent( CSceneEventInfo *info, bool fastKill, bool canceled )
{
	contract_assert( info );
	contract_assert( info->m_pScene );
	contract_assert( info->m_pEvent );

	// FIXME: this code looks duplicated
	switch ( info->m_pEvent->GetType() )
	{
	case CChoreoEvent::GESTURE:
	case CChoreoEvent::SEQUENCE:
		{
			if (info->m_iLayer >= 0)
			{
				if ( fastKill )
				{
					FastRemoveLayer( info->m_iLayer );
				}
				else if (info->m_pEvent->GetType() == CChoreoEvent::GESTURE)
				{
					if (canceled)
					{
						// remove slower if interrupted
						RemoveLayer( info->m_iLayer, 0.5 );
					}
					else
					{
						RemoveLayer( info->m_iLayer, 0.1 );
					}
				}
				else
				{
					RemoveLayer( info->m_iLayer, 0.3 );
				}
			}
		}
		return true;

	case CChoreoEvent::MOVETO:
		{
			return true;

			// cancel moveto if it's distance based, of if the event was part of a canceled vcd
			if (IsMoving() && (canceled || info->m_pEvent->GetDistanceToTarget() > 0.0))
			{
				if (!info->m_bHasArrived)
				{
					if (info->m_pScene)
					{
						Scene_Printf( "%s : %8.2f: MOVETO canceled but actor %s not at goal\n", info->m_pScene->GetFilename(), info->m_pScene->GetTime(), info->m_pEvent->GetActor()->GetName() );
					}
				}
			}
		}
		return true;
	case CChoreoEvent::FACE:
	case CChoreoEvent::FLEXANIMATION:
	case CChoreoEvent::EXPRESSION:
	case CChoreoEvent::LOOKAT:
	case CChoreoEvent::GENERIC:
	case CChoreoEvent::CAMERA:
	case CChoreoEvent::SCRIPT:
		{
			// no special rules
		}
		return true;
	case CChoreoEvent::SPEAK:
		{
			// Tracker 15420:  Issue stopsound if we need to cut this short...
			if ( canceled )
			{
				StopSound( info->m_pEvent->GetParameters() );

			}
		}
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Add string indexed scene/expression/duration to list of active SceneEvents
// Input  : scenefile -
//			expression -
//			duration -
//-----------------------------------------------------------------------------
void CBaseFlex::AddSceneEvent( CChoreoScene *scene, CChoreoEvent *event, CBaseEntity *pTarget /*= NULL*/, CSceneEntity* pSceneEntity /*= NULL*/ )
{
	if ( !scene || !event )
	{
		Msg( "CBaseFlex::AddSceneEvent:  scene or event was NULL!!!\n" );
		return;
	}

	CChoreoActor *actor = event->GetActor();
	if ( !actor )
	{
		Msg( "CBaseFlex::AddSceneEvent:  event->GetActor() was NULL!!!\n" );
		return;
	}

	CSceneEventInfo info;

	std::memset( (void *)&info, 0, sizeof( info ) );

	info.m_pEvent		= event;
	info.m_pScene		= scene;
	info.m_hTarget		= pTarget;
	info.m_bStarted		= false;
	info.m_hSceneEntity = pSceneEntity;

	if (StartSceneEvent( &info, scene, event, actor, pTarget ))
	{
		m_SceneEvents.push_back( info );
	}
	else
	{
		Scene_Printf( "CBaseFlex::AddSceneEvent:  event failed\n" );
		// Assert( 0 ); // expression failed to start
	}
}

//-----------------------------------------------------------------------------
// Starting various expression types
//-----------------------------------------------------------------------------

bool CBaseFlex::RequestStartSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	info->m_nSequence = LookupSequence( event->GetParameters() );

	// make sure sequence exists
	if (info->m_nSequence < 0)
	{
		Warning( "CSceneEntity %s :\"%s\" unable to find sequence \"%s\"\n", STRING(GetEntityName()), actor->GetName(), event->GetParameters() );
		return false;
	}

	// This is a bit of a hack, but we need to defer the actual allocation until Process which will sync the layer allocation
	//  to the NPCs think/m_flAnimTime instead of some arbitrary tick
	info->m_iLayer = REQUEST_DEFERRED_LAYER_ALLOCATION;
	info->m_pActor = actor;
	return true;
}

bool CBaseFlex::RequestStartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	info->m_nSequence = LookupSequence( event->GetParameters() );

	// make sure sequence exists
	if (info->m_nSequence < 0)
	{
		Warning( "CSceneEntity %s :\"%s\" unable to find gesture \"%s\"\n", STRING(GetEntityName()), actor->GetName(), event->GetParameters() );
		return false;
	}

	// This is a bit of a hack, but we need to defer the actual allocation until Process which will sync the layer allocation
	//  to the NPCs think/m_flAnimTime instead of some arbitrary tick
	info->m_iLayer = REQUEST_DEFERRED_LAYER_ALLOCATION;
	info->m_pActor = actor;
	return true;
}

bool CBaseFlex::HandleStartSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor )
{
	contract_assert( info->m_iLayer == REQUEST_DEFERRED_LAYER_ALLOCATION );

	info->m_nSequence = LookupSequence( event->GetParameters() );
	info->m_iLayer = -1;

	if (info->m_nSequence < 0)
	{
		Warning( "CSceneEntity %s :\"%s\" unable to find sequence \"%s\"\n", STRING(GetEntityName()), actor->GetName(), event->GetParameters() );
		return false;
	}

	if (!EnterSceneSequence( scene, event ))
	{
		if (!event->GetPlayOverScript())
		{
			// this has failed to start
			Warning( "CSceneEntity %s :\"%s\" failed to start sequence \"%s\"\n", STRING(GetEntityName()), actor->GetName(), event->GetParameters() );
			return false;
		}
		// Start anyways, just use normal no-movement, must be in IDLE rules
	}

	info->m_iPriority = actor->FindChannelIndex( event->GetChannel() );
	info->m_iLayer = AddLayeredSequence( info->m_nSequence, info->m_iPriority + GetScenePriority( scene ) );
	SetLayerNoRestore( info->m_iLayer, true );
	SetLayerWeight( info->m_iLayer, 0.0 );

	bool looping = ((GetSequenceFlags( GetModelPtr(), info->m_nSequence ) & STUDIO_LOOPING) != 0);
	if (!looping)
	{
		// figure out the animtime when this was frame 0
		float dt =  scene->GetTime() - event->GetStartTime();
		float seq_duration = SequenceDuration( info->m_nSequence );
		float flCycle = dt / seq_duration;
		flCycle = flCycle - (int)flCycle; // loop
		SetLayerCycle( info->m_iLayer, flCycle, flCycle );

		SetLayerPlaybackRate( info->m_iLayer, 0.0 );
	}
	else
	{
		SetLayerPlaybackRate( info->m_iLayer, 1.0 );
	}

	if (IsMoving())
	{
		info->m_flWeight = 0.0;
	}
	else
	{
		info->m_flWeight = 1.0;
	}

	return true;
}

bool CBaseFlex::HandleStartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor )
{
	contract_assert( info->m_iLayer == REQUEST_DEFERRED_LAYER_ALLOCATION );

	info->m_nSequence = LookupSequence( event->GetParameters() );
	info->m_iLayer = -1;

	if (info->m_nSequence < 0)
	{
		Warning( "CSceneEntity %s :\"%s\" unable to find gesture \"%s\"\n", STRING(GetEntityName()), actor->GetName(), event->GetParameters() );
		return false;
	}

	// FIXME: this seems like way too much code
	info->m_bIsGesture = false;
	KeyValues *seqKeyValues = GetSequenceKeyValues( info->m_nSequence );
	if (seqKeyValues)
	{
		// Do we have a build point section?
		KeyValues *pkvAllFaceposer = seqKeyValues->FindKey("faceposer");
		if ( pkvAllFaceposer )
		{
			KeyValues *pkvType = pkvAllFaceposer->FindKey("type");

			if (pkvType)
			{
				info->m_bIsGesture = (stricmp( pkvType->GetString(), "gesture" ) == 0) ? true : false;
			}
		}

		// FIXME: fixup tags that should be set as "linear", should be done in faceposer
		char szStartLoop[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH] = { "loop" };
		char szEndLoop[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH] = { "end" };

		// check in the tag indexes
		KeyValues *pkvFaceposer;
		for ( pkvFaceposer = pkvAllFaceposer->GetFirstSubKey(); pkvFaceposer; pkvFaceposer = pkvFaceposer->GetNextKey() )
		{
			if (!stricmp( pkvFaceposer->GetName(), "startloop" ))
			{
				std::strcpy( szStartLoop, pkvFaceposer->GetString() );
			}
			else if (!stricmp( pkvFaceposer->GetName(), "endloop" ))
			{
				std::strcpy( szEndLoop, pkvFaceposer->GetString() );
			}
		}

		CEventAbsoluteTag *ptag;
		ptag = event->FindAbsoluteTag( CChoreoEvent::ORIGINAL, szStartLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}
		ptag = event->FindAbsoluteTag( CChoreoEvent::PLAYBACK, szStartLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}
		ptag = event->FindAbsoluteTag( CChoreoEvent::ORIGINAL, szEndLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}
		ptag = event->FindAbsoluteTag( CChoreoEvent::PLAYBACK, szEndLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}

		if ( pkvAllFaceposer )
		{
			CStudioHdr *pstudiohdr = GetModelPtr();

			mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( info->m_nSequence );
			mstudioanimdesc_t &animdesc = pstudiohdr->pAnimdesc( pstudiohdr->iRelativeAnim( info->m_nSequence, seqdesc.anim(0,0) ) );

			// check in the tag indexes
			KeyValues *pkvFaceposer;
			for ( pkvFaceposer = pkvAllFaceposer->GetFirstSubKey(); pkvFaceposer; pkvFaceposer = pkvFaceposer->GetNextKey() )
			{
				if (!stricmp( pkvFaceposer->GetName(), "tags" ))
				{
					KeyValues *pkvTags;
					for ( pkvTags = pkvFaceposer->GetFirstSubKey(); pkvTags; pkvTags = pkvTags->GetNextKey() )
					{
						int maxFrame = animdesc.numframes - 2; // FIXME: this is off by one!

						if ( maxFrame > 0)
						{
							float percentage = (float)pkvTags->GetInt() / maxFrame;

							CEventAbsoluteTag *ptag = event->FindAbsoluteTag( CChoreoEvent::ORIGINAL, pkvTags->GetName() );
							if (ptag)
							{
								if (fabs(ptag->GetPercentage() - percentage) > 0.05)
								{
									DevWarning("%s repositioned tag: %s : %.3f -> %.3f (%s:%s:%s)\n", scene->GetFilename(), pkvTags->GetName(), ptag->GetPercentage(), percentage, scene->GetFilename(), actor->GetName(), event->GetParameters() );
									// reposition tag
									ptag->SetPercentage( percentage );
								}
							}
						}
					}
				}
			}

			if (!event->VerifyTagOrder())
			{
				DevWarning("out of order tags : %s : (%s:%s:%s)\n", scene->GetFilename(), actor->GetName(), event->GetName(), event->GetParameters() );
			}
		}

		seqKeyValues->deleteThis();
	}

	// initialize posture suppression
	// FIXME: move priority of base animation so that layers can be inserted before
	// FIXME: query stopping, post idle layer to figure out correct weight
	//			GetIdleLayerWeight()?
	if (!info->m_bIsGesture && IsMoving())
	{
		info->m_flWeight = 0.0;
	}
	else
	{
		info->m_flWeight = 1.0;
	}

	// this happens before StudioFrameAdvance()
	info->m_iPriority = actor->FindChannelIndex( event->GetChannel() );
	info->m_iLayer = AddLayeredSequence( info->m_nSequence, info->m_iPriority + GetScenePriority( scene ) );
	SetLayerNoRestore( info->m_iLayer, true );
	SetLayerDuration( info->m_iLayer, event->GetDuration() );
	SetLayerWeight( info->m_iLayer, 0.0 );

	bool looping = ((GetSequenceFlags( GetModelPtr(), info->m_nSequence ) & STUDIO_LOOPING) != 0);
	if ( looping )
	{
		DevMsg( 1, "vcd error, gesture %s of model %s is marked as STUDIO_LOOPING!\n",
				event->GetParameters(), STRING(GetModelName()) );
	}

	SetLayerLooping( info->m_iLayer, false ); // force to not loop

	float duration = event->GetDuration( );

	// figure out the animtime when this was frame 0
	float flEventCycle = (scene->GetTime() - event->GetStartTime()) / duration;
	float flCycle = event->GetOriginalPercentageFromPlaybackPercentage( flEventCycle );
	SetLayerCycle( info->m_iLayer, flCycle, 0.0 );
	SetLayerPlaybackRate( info->m_iLayer, 0.0 );

	return true;
}

bool CBaseFlex::StartFacingSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	if ( pTarget )
	{
		// Don't allow FACE commands while sitting in the vehicle
		

		info->m_bIsMoving = false;
		return true;
	}
	return false;
}

bool CBaseFlex::StartMoveToSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	if (pTarget)
	{
		info->m_bIsMoving = false;
		info->m_bHasArrived = false;
		{
			return false;
		}

		EnterSceneSequence( scene, event, true );

		// If they're already moving, stop them
		//
		// Don't stop them during restore because that will set a stopping path very
		// nearby, causing us to signal arrival prematurely in CheckSceneEventCompletion.
		// BEWARE: the behavior of this bug depended on the order in which the entities were restored!!
		

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseFlex::StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	switch ( event->GetType() )
	{
	case CChoreoEvent::SEQUENCE:
		return RequestStartSequenceSceneEvent( info, scene, event, actor, pTarget );

	case CChoreoEvent::GESTURE:
		return RequestStartGestureSceneEvent( info, scene, event, actor, pTarget );

	case CChoreoEvent::FACE:
		return StartFacingSceneEvent( info, scene, event, actor, pTarget );

	// FIXME: move this to an CBaseActor
	case CChoreoEvent::MOVETO:
		return StartMoveToSceneEvent( info, scene, event, actor, pTarget );

	case CChoreoEvent::LOOKAT:
		info->m_hTarget = pTarget;
		return true;

	case CChoreoEvent::FLEXANIMATION:
		info->InitWeight( this );
		return true;

	case CChoreoEvent::SPEAK:
		return true;

	case CChoreoEvent::EXPRESSION: // These are handled client-side
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Remove expression
// Input  : scenefile -
//			expression -
//-----------------------------------------------------------------------------
void CBaseFlex::RemoveSceneEvent( CChoreoScene *scene, CChoreoEvent *event, bool fastKill )
{
	contract_assert( event );

	for ( int i = 0 ; i < std::ssize( m_SceneEvents ); i++ )
	{
		CSceneEventInfo *info = &m_SceneEvents[ i ];

		contract_assert( info );
		contract_assert( info->m_pEvent );

		if ( info->m_pScene != scene )
			continue;

		if ( info->m_pEvent != event)
			continue;

		if (ClearSceneEvent( info, fastKill, false ))
		{
			// Free this slot
			info->m_pEvent		= nullptr;
			info->m_pScene		= nullptr;
			info->m_bStarted	= false;

			m_SceneEvents.erase( m_SceneEvents.begin() + i );
			return;
		}
	}

	// many events refuse to start due to bogus parameters
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the event should be considered "completed"
//-----------------------------------------------------------------------------
bool CBaseFlex::CheckSceneEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	for ( int i = 0 ; i < std::ssize( m_SceneEvents ); i++ )
	{
		CSceneEventInfo *info = &m_SceneEvents[ i ];

		contract_assert( info );
		contract_assert( info->m_pEvent );

		if ( info->m_pScene != scene )
			continue;

		if ( info->m_pEvent != event)
			continue;

		return CheckSceneEventCompletion( info, currenttime, scene, event );
	}
	return true;
}

bool CBaseFlex::CheckSceneEventCompletion( CSceneEventInfo *info, float currenttime, CChoreoScene *scene, CChoreoEvent *event )
{
	switch ( event->GetType() )
	{
	case CChoreoEvent::MOVETO:
		{

			
		}
		break;
	default:
		break;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Default implementation
//-----------------------------------------------------------------------------
void CBaseFlex::ProcessSceneEvents( void )
{
	CVProfScope _( "CBaseFlex::ProcessSceneEvents", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// slowly decay to netural expression
	for ( LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
	{
		SetFlexWeight( i, GetFlexWeight( i ) * 0.95 );
	}

	bool bHasForegroundEvents = false;
	// Iterate SceneEvents and look for active slots
	for ( int i = 0; i < std::ssize( m_SceneEvents ); i++ )
	{
		CSceneEventInfo *info = &m_SceneEvents[ i ];
		contract_assert( info );

		// FIXME:  Need a safe handle to m_pEvent in case of memory deletion?
		CChoreoEvent *event = info->m_pEvent;
		contract_assert( event );

		CChoreoScene *scene = info->m_pScene;
		contract_assert( scene );

		if ( scene && !scene->IsBackground() )
		{
			bHasForegroundEvents = true;
		}

		if (ProcessSceneEvent( info, scene, event ))
		{
			info->m_bStarted = true;
		}
	}

	if ( bHasForegroundEvents && scene_showunlock.GetBool())
	{
		
	}

	// any needed layer priorites have now been reset
	m_bUpdateLayerPriorities = false;
}

class CFlexSceneFileManager : CAutoGameSystem
{
public:

	CFlexSceneFileManager( char const *name ) : CAutoGameSystem( name )
	{
	}

	virtual bool InitRecursive( const char *pFolder )
	{
		if ( pFolder == nullptr )
		{
			pFolder = "expressions";

		}

		char directory[ MAX_PATH ];
		V_snprintf( directory, sizeof( directory ), "%s/*.*", pFolder );

		FileFindHandle_t fh;
		const char *fn;

		for (fn = g_pFullFileSystem->FindFirst( directory, &fh ); fn; fn = g_pFullFileSystem->FindNext( fh ) )
		{
			if ( !stricmp( fn, ".") || !stricmp( fn, "..") )
			{
				continue;
			}

			if ( g_pFullFileSystem->FindIsDirectory( fh ) )
			{
				char folderpath[MAX_PATH];
				V_snprintf( folderpath, sizeof( folderpath ), "%s/%s", pFolder, fn );

				InitRecursive( folderpath );
				continue;
			}

			const char *pExt = V_GetFileExtension( fn );
			if ( pExt && V_stricmp( pExt, "vfe" ) )
			{
				continue;
			}

			char fullFileName[MAX_PATH];
			V_snprintf( fullFileName, sizeof(fullFileName), "%s/%s", pFolder, fn );

			// strip default folder and extension
			int index = V_strlen( "expressions/" );
			char vfeName[ MAX_PATH ];
			V_StripExtension( &fullFileName[index], vfeName, sizeof( vfeName ) );
			V_FixSlashes( vfeName );

			FindSceneFile( nullptr, vfeName, true );
		}
		return true;
	}

	virtual bool Init()
	{
		// Trakcer 16692:  Preload these at startup to avoid hitch first time we try to load them during actual gameplay
		FindSceneFile( nullptr, "phonemes", true );
		FindSceneFile( nullptr, "phonemes_weak", true );
		FindSceneFile( nullptr, "phonemes_strong", true );

		InitRecursive( nullptr );
		return true;
	}

	// Tracker 14992:  We used to load 18K of .vfes for every CBaseFlex who lipsynced, but now we only load those files once globally.
	// Note, we could wipe these between levels, but they don't ever load more than the weak/normal/strong phoneme classes that I can tell
	//  so I'll just leave them loaded forever for now
	virtual void Shutdown()
	{
		DeleteSceneFiles();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Sets up translations
	// Input  : *instance -
	//			*pSettinghdr -
	// Output : 	void
	//-----------------------------------------------------------------------------
	void EnsureTranslations( CBaseFlex *instance, const flexsettinghdr_t *pSettinghdr )
	{
		// The only time instance is NULL is in Init() above, where we're just loading the .vfe files off of the hard disk.
		if ( instance )
		{
			instance->EnsureTranslations( pSettinghdr );
		}
	}

	const void *FindSceneFile( CBaseFlex *instance, const char *filename, bool allowBlockingIO )
	{
		char szFilename[MAX_PATH];
		V_strcpy( szFilename, filename );

		V_FixSlashes( szFilename );

		// See if it's already loaded
		int i;
		for ( i = 0; i < std::ssize( m_FileList ); i++ )
		{
			CFlexSceneFile *file = m_FileList[ i ];
			if ( file && !V_stricmp( file->filename, szFilename ) )
			{
				// Make sure translations (local to global flex controller) are set up for this instance
				EnsureTranslations( instance, ( const flexsettinghdr_t * )file->buffer );
				return file->buffer;
			}
		}

		if ( !allowBlockingIO )
		{
			return nullptr;
		}

		// Load file into memory
		void *buffer = nullptr;
		int len = filesystem->ReadFileEx( UTIL_VarArgs( "expressions/%s.vfe", szFilename ), "GAME", &buffer, false, true );

		if ( !len )
			return nullptr;

		// Create scene entry
		CFlexSceneFile *pfile = new CFlexSceneFile;
		// Remember filename
		V_strncpy( pfile->filename, szFilename, sizeof( pfile->filename ) );
		// Remember data pointer
		pfile->buffer = buffer;
		// Add to list
		m_FileList.push_back( pfile );

		// Swap the entire file

		// Fill in translation table
		EnsureTranslations( instance, ( const flexsettinghdr_t * )pfile->buffer );

		// Return data
		return pfile->buffer;
	}

private:

	void DeleteSceneFiles()
	{
		while ( !m_FileList.empty() )
		{
			CFlexSceneFile *file = m_FileList[ 0 ];
			m_FileList.erase( m_FileList.begin() + 0 );
			filesystem->FreeOptimalReadBuffer( file->buffer );
			delete file;
		}
	}

	std::vector< CFlexSceneFile * > m_FileList;
};

// Singleton manager
CFlexSceneFileManager g_FlexSceneFileManager( "CFlexSceneFileManager" );

//-----------------------------------------------------------------------------
// Purpose: Since everyone shared a pSettinghdr now, we need to set up the localtoglobal mapping per entity, but
//  we just do this in memory with an array of integers (could be shorts, I suppose)
// Input  : *pSettinghdr -
//-----------------------------------------------------------------------------
void CBaseFlex::EnsureTranslations( const flexsettinghdr_t *pSettinghdr )
{

	if ( m_LocalToGlobal.contains( pSettinghdr ) )
		return;

	contract_assert( pSettinghdr->numkeys > 0 );
	std::vector< LocalFlexController_t > mapping( pSettinghdr->numkeys );

	for ( int i = 0; i < pSettinghdr->numkeys; ++i )
	{
		mapping[ i ] = FindFlexController( pSettinghdr->pLocalName( i ) );
	}

	m_LocalToGlobal.emplace( pSettinghdr, std::move( mapping ) );
}

//-----------------------------------------------------------------------------
// Purpose: Look up instance specific mapping
// Input  : *pSettinghdr -
//			key -
// Output : int
//-----------------------------------------------------------------------------
LocalFlexController_t CBaseFlex::FlexControllerLocalToGlobal( const flexsettinghdr_t *pSettinghdr, int key )
{
	auto idx = m_LocalToGlobal.find( pSettinghdr );
	if ( idx == m_LocalToGlobal.end() )
	{
		// This should never happen!!!
		contract_assert( 0 );
		Warning( "Unable to find mapping for flexcontroller %i, settings %p on %i/%s\n", key, pSettinghdr, entindex(), GetClassname() );
		EnsureTranslations( pSettinghdr );
		idx = m_LocalToGlobal.find( pSettinghdr );
		if ( idx == m_LocalToGlobal.end() )
		{
			Error( "CBaseFlex::FlexControllerLocalToGlobal failed!\n" );
		}
	}

	const std::vector< LocalFlexController_t >& result = idx->second;
	// Validate lookup
	contract_assert( !result.empty() && key < std::ssize( result ) );
	LocalFlexController_t index = result[ key ];
	return index;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *filename -
//-----------------------------------------------------------------------------
const void *CBaseFlex::FindSceneFile( const char *filename )
{
	// Ask manager to get the globally cached scene instead.

	// hunt up the tree for the filename starting with our model name prepended as the path
	static char szExtendedPath[MAX_PATH];
	if ( StringHasPrefix( STRING( GetModelName() ), "models" ) )
	{
		std::strcpy( szExtendedPath, StringAfterPrefix( STRING( GetModelName() ), "models" ) + 1 );
	}
	else
	{
		std::strcpy( szExtendedPath, STRING( GetModelName() ) );
	}
	V_StripExtension( szExtendedPath, szExtendedPath, sizeof( szExtendedPath ) );
	V_FixupPathName( szExtendedPath, sizeof( szExtendedPath ), szExtendedPath );

	const void *pSceneFile = nullptr;
	// FIXME: V_StripLastDir returns "./" path when it strips out the last one.  That don't resolve on FindSceneFile
	while ( V_strlen( szExtendedPath ) > 2 )
	{
		static char szExtendedName[MAX_PATH];

		V_ComposeFileName( szExtendedPath, filename, szExtendedName, sizeof( szExtendedName ) );

		pSceneFile = g_FlexSceneFileManager.FindSceneFile( this, szExtendedName, false );

		if ( pSceneFile )
			break;

		if ( !V_StripLastDir( szExtendedPath, sizeof( szExtendedPath ) ) )
			break;
	}

	if ( !pSceneFile )
	{
		// just ask for it by name
		pSceneFile = g_FlexSceneFileManager.FindSceneFile( this, filename, false );
	}

	return pSceneFile;
}

ConVar	ai_expression_optimization( side_registry(), "ai_expression_optimization", "0", FCVAR_NONE, "Disable npc background expressions when you can't see them." );
ConVar	ai_expression_frametime( side_registry(), "ai_expression_frametime", "0.05", FCVAR_NONE, "Maximum frametime to still play background expressions." );

//-----------------------------------------------------------------------------
// Various methods to process facial SceneEvents:
//-----------------------------------------------------------------------------
bool CBaseFlex::ProcessFlexAnimationSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	CVProfScope _( "CBaseFlex::ProcessFlexAnimationSceneEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( event->HasEndTime() )
	{
		// don't bother with flex animation if the player can't see you
		

		AddFlexAnimation( info );
	}
	return true;
}

bool CBaseFlex::ProcessFlexSettingSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	// Flexanimations have to have an end time!!!
	if ( !event->HasEndTime() )
		return true;

	CVProfScope _( "CBaseFlex::ProcessFlexSettingSceneEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Look up the actual strings
	const char *scenefile	= event->GetParameters();
	const char *name		= event->GetParameters2();

	// Have to find both strings
	if ( scenefile && name )
	{
		if ( info->m_pExpHdr == nullptr)
		{
			info->m_pExpHdr = ( const flexsettinghdr_t * )FindSceneFile( scenefile );
		}

		if ( info->m_pExpHdr )
		{
			float scenetime = scene->GetTime();

			float scale = event->GetIntensity( scenetime );

			// Add the named expression
			AddFlexSetting( name, scale, info->m_pExpHdr, !info->m_bStarted );
		}
	}

	return true;
}

bool CBaseFlex::ProcessFacingSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	// make sure target exists
	if (info->m_hTarget == nullptr)
		return false;

	CVProfScope _( "CBaseFlex::ProcessFacingSceneEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// make sure we're still able to play this command
	if (!EnterSceneSequence( scene, event, true ))
	{
		return false;
	}

	if (!info->m_bStarted)
	{
		info->m_flInitialYaw = GetLocalAngles().y;
	}

	// lock in place if aiming at self
	if (info->m_hTarget == this)
	{
		return true;
	}

	
	return false;
}

bool CBaseFlex::ProcessMoveToSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	// Moving to a target is what NPCs did; nothing here moves on its own.
	return false;
}

bool CBaseFlex::ProcessLookAtSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	CVProfScope _( "CBaseFlex::ProcessLookAtSceneEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseFlex::ProcessSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	CVProfScope _( "CBaseFlex::ProcessSceneEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	switch ( event->GetType() )
	{
	case CChoreoEvent::FLEXANIMATION:
		return ProcessFlexAnimationSceneEvent( info, scene, event );

	case CChoreoEvent::EXPRESSION:
		return ProcessFlexSettingSceneEvent( info, scene, event );

	case CChoreoEvent::SEQUENCE:
		return ProcessSequenceSceneEvent( info, scene, event );

	case CChoreoEvent::GESTURE:
		return ProcessGestureSceneEvent( info, scene, event );

	case CChoreoEvent::FACE:
		return ProcessFacingSceneEvent( info, scene, event );

	case CChoreoEvent::MOVETO:
		return ProcessMoveToSceneEvent( info, scene, event );

	case CChoreoEvent::LOOKAT:
		return ProcessLookAtSceneEvent( info, scene, event );

	case CChoreoEvent::SPEAK:
		return true;

	default:
		{
			Msg( "unknown type %d in ProcessSceneEvent()\n", event->GetType() );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseFlex::IsRunningSceneMoveToEvent()
{
	for ( int i = std::ssize( m_SceneEvents ) - 1; i >= 0; i-- )
	{
		CSceneEventInfo *info = &m_SceneEvents[ i ];
		CChoreoEvent *event = info->m_pEvent;
		if ( event && event->GetType() == CChoreoEvent::MOVETO )
			return true;
	}

	return false;
}

flexsetting_t const *CBaseFlex::FindNamedSetting( flexsettinghdr_t const *pSettinghdr, const char *expr )
{
	int i;
	const flexsetting_t *pSetting = nullptr;

	for ( i = 0; i < pSettinghdr->numflexsettings; i++ )
	{
		pSetting = pSettinghdr->pSetting( i );
		if ( !pSetting )
			continue;

		const char *name = pSetting->pszName();

		if ( !stricmp( name, expr ) )
			break;
	}

	if ( i>=pSettinghdr->numflexsettings )
	{
		return nullptr;
	}

	return pSetting;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *event -
//-----------------------------------------------------------------------------
void CBaseFlex::AddFlexAnimation( CSceneEventInfo *info )
{
	if ( !info )
		return;

	// don't bother with flex animation if the player can't see you
	

	CChoreoEvent *event = info->m_pEvent;
	if ( !event )
		return;

	CChoreoScene *scene = info->m_pScene;
	if ( !scene )
		return;

	if ( !event->GetTrackLookupSet() )
	{
		// Create lookup data
		for ( int i = 0; i < event->GetNumFlexAnimationTracks(); i++ )
		{
			CFlexAnimationTrack *track = event->GetFlexAnimationTrack( i );
			if ( !track )
				continue;

			if ( track->IsComboType() )
			{
				char name[ 512 ];
				V_strncpy( name, "right_" ,sizeof(name));
				V_strncat( name, track->GetFlexControllerName(),sizeof(name), COPY_ALL_CHARACTERS );

				track->SetFlexControllerIndex( FindFlexController( name ), 0, 0 );


				V_strncpy( name, "left_" ,sizeof(name));
				V_strncat( name, track->GetFlexControllerName(),sizeof(name), COPY_ALL_CHARACTERS );

				track->SetFlexControllerIndex( FindFlexController( name ), 0, 1 );

			}
			else
			{
				track->SetFlexControllerIndex( FindFlexController( (char *)track->GetFlexControllerName() ), 0 );

				// Only non-combo tracks can be server side
				track->SetServerSide( false );
			}
		}

		event->SetTrackLookupSet( true );
	}

	float scenetime = scene->GetTime();
	// decay if this is a background scene and there's other flex animations playing
	float weight = event->GetIntensity( scenetime ) * info->UpdateWeight( this );
	{
	CVProfScope _( "AddFlexAnimation_SetFlexWeight", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Compute intensity for each track in animation and apply
	// Iterate animation tracks
	for ( int i = 0; i < event->GetNumFlexAnimationTracks(); i++ )
	{
		CFlexAnimationTrack *track = event->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		// Disabled
		if ( !track->IsTrackActive() )
			continue;

		// If we are doing client side flexing, skip all tracks which are not server side
		if ( g_bClientFlex && !track->IsServerSide() )
			continue;

		// Map track flex controller to global name
		if ( track->IsComboType() )
		{
			for ( int side = 0; side < 2; side++ )
			{
				LocalFlexController_t controller = track->GetRawFlexControllerIndex( side );

				// Get spline intensity for controller
				float flIntensity = track->GetIntensity( scenetime, side );
				if ( controller >= LocalFlexController_t(0) )
				{
					float orig = GetFlexWeight( controller );
					SetFlexWeight( controller, orig * (1 - weight) + flIntensity * weight );
				}
			}
		}
		else
		{
			LocalFlexController_t controller = track->GetRawFlexControllerIndex( 0 );

			// Get spline intensity for controller
			float flIntensity = track->GetIntensity( scenetime, 0 );
			if ( controller >= LocalFlexController_t(0) )
			{
				float orig = GetFlexWeight( controller );
				SetFlexWeight( controller, orig * (1 - weight) + flIntensity * weight );
			}
		}
	}
	}

	info->m_bStarted = true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *expr -
//			scale -
//			*pSettinghdr -
//			newexpression -
//-----------------------------------------------------------------------------
void CBaseFlex::AddFlexSetting( const char *expr, float scale,
	const flexsettinghdr_t *pSettinghdr, bool newexpression )
{
	int i;
	const flexsetting_t *pSetting = nullptr;

	// Find the named setting in the base
	for ( i = 0; i < pSettinghdr->numflexsettings; i++ )
	{
		pSetting = pSettinghdr->pSetting( i );
		if ( !pSetting )
			continue;

		const char *name = pSetting->pszName();

		if ( !stricmp( name, expr ) )
			break;
	}

	if ( i>=pSettinghdr->numflexsettings )
	{
		return;
	}

	flexweight_t *pWeights = nullptr;
	int truecount = pSetting->psetting( (byte *)pSettinghdr, 0, &pWeights );
	if ( !pWeights )
		return;

	for (i = 0; i < truecount; i++, pWeights++)
	{
		// Translate to local flex controller
		// this is translating from the settings's local index to the models local index
		LocalFlexController_t index = FlexControllerLocalToGlobal( pSettinghdr, pWeights->key );

		// blend scaled weighting in to total
		float s = clamp( scale * pWeights->influence, 0.0f, 1.0f );
		float value = GetFlexWeight( index ) * (1.0f - s ) + pWeights->weight * s;
		SetFlexWeight( index, value );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *actor -
//			*parameters -
//-----------------------------------------------------------------------------
bool CBaseFlex::ProcessGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !info || !event || !scene )
		return false;

	if ( info->m_iLayer == REQUEST_DEFERRED_LAYER_ALLOCATION )
	{
		HandleStartGestureSceneEvent( info, scene, event, info->m_pActor );
	}

	if (info->m_iLayer >= 0)
	{
		// this happens after StudioFrameAdvance()
		// FIXME; this needs to be adjusted by npc offset to scene time? Known?
		// FIXME: what should this do when the scene loops?
		float duration = event->GetDuration( );
		float flEventCycle = (scene->GetTime() - event->GetStartTime()) / duration;
		float flCycle = event->GetOriginalPercentageFromPlaybackPercentage( flEventCycle );

		SetLayerCycle( info->m_iLayer, flCycle );

		float flWeight = event->GetIntensity( scene->GetTime() );

		/*
		if (stricmp( event->GetParameters(), "m_g_arms_crossed" ) == 0)
		{
			Msg("%.2f (%.2f) : %s : %.3f (%.3f) %.2f\n", scene->GetTime(), scene->GetTime() - event->GetStartTime(), event->GetParameters(), flCycle, flEventCycle, flWeight );
		}
		*/

		// fade out/in if npc is moving
		if (!info->m_bIsGesture)
		{
			if (IsMoving())
			{
				info->m_flWeight = std::max( info->m_flWeight - 0.2, 0.0 );
			}
			else
			{
				info->m_flWeight = std::min( info->m_flWeight + 0.2, 1.0 );
			}
		}

		// 3x^2-2x^3
		float spline = 3 * info->m_flWeight * info->m_flWeight - 2 * info->m_flWeight * info->m_flWeight * info->m_flWeight;
		SetLayerWeight( info->m_iLayer, flWeight * spline );

		// update layer priority
		if (m_bUpdateLayerPriorities)
		{
			SetLayerPriority( info->m_iLayer, info->m_iPriority + GetScenePriority( scene ) );
		}

		/*
		Msg( "%d : %.2f (%.2f) : %.3f %.3f : %.3f\n",
			info->m_iLayer,
			scene->GetTime(),
			(scene->GetTime() - event->GetStartTime()) / duration,
			flCycle,
			flNextCycle,
			rate );
		*/
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *actor -
//			*parameters -
//-----------------------------------------------------------------------------
bool CBaseFlex::ProcessSequenceSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event )
{
	if ( !info  || !event || !scene )
		return false;

	bool bNewlyAllocated = false;
	if ( info->m_iLayer == REQUEST_DEFERRED_LAYER_ALLOCATION )
	{
		bool result = HandleStartSequenceSceneEvent( info, scene, event, info->m_pActor );
		if (!result)
			return false;
		bNewlyAllocated = true;
	}

	if (info->m_iLayer >= 0)
	{
		float flWeight = event->GetIntensity( scene->GetTime() );

		// force layer to zero weight in newly allocated, fixed bug with inter-think spawned sequences blending in badly
		if (bNewlyAllocated)
			flWeight = 0.0;

		// fade out/in if npc is moving
		bool bFadeOut = IsMoving();
		

		if (bFadeOut)
		{
			info->m_flWeight = std::max( info->m_flWeight - 0.2, 0.0 );
		}
		else
		{
			info->m_flWeight = std::min( info->m_flWeight + 0.2, 1.0 );
		}

		float spline = 3 * info->m_flWeight * info->m_flWeight - 2 * info->m_flWeight * info->m_flWeight * info->m_flWeight;
		SetLayerWeight( info->m_iLayer, flWeight * spline );

		bool looping = ((GetSequenceFlags( GetModelPtr(), info->m_nSequence ) & STUDIO_LOOPING) != 0);
		if (!looping)
		{
			float dt =  scene->GetTime() - event->GetStartTime();
			float seq_duration = SequenceDuration( info->m_nSequence );
			float flCycle = dt / seq_duration;
			flCycle = clamp( flCycle, 0, 1.0 );
			SetLayerCycle( info->m_iLayer, flCycle );
		}

		

		// update layer priority
		if (m_bUpdateLayerPriorities)
		{
			SetLayerPriority( info->m_iLayer, info->m_iPriority + GetScenePriority( scene ) );
		}
	}

	// FIXME: clean up cycle index from restart
	return true;

}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the actor is not currently in a scene OR if the actor
//  is in a scene (checked externally), but a PERMIT_RESPONSES event is active and
//  the permit time period has enough time remaining to handle the response in full.
// Input  : response_length -
//-----------------------------------------------------------------------------
bool CBaseFlex::PermitResponse( float response_length )
{
	// Nothing set, disallow it
	if ( m_flAllowResponsesEndTime <= 0.0f )
	{
		return false;
	}

	// If response ends before end of allow time, then that's okay
	if ( gpGlobals->curtime + response_length <= m_flAllowResponsesEndTime )
	{
		return true;
	}

	// Disallow responses for now
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Set response end time (0 to clear response blocking)
// Input  : endtime -
//-----------------------------------------------------------------------------
void CBaseFlex::SetPermitResponse( float endtime )
{
	// Mark time after which we'll be occupied again (if in a scene)
	// Note a value of <= 0.0f means unset (always allow actor to speak if not in a scene,
	//  and always disallow if in a scene)
	m_flAllowResponsesEndTime = endtime;
}

//-----------------------------------------------------------------------------
// Purpose: Play a one-shot scene
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CBaseFlex::PlayScene( const char *pszScene, float flDelay, AI_Response *response, IRecipientFilter *filter /* = NULL */ )
{
	return InstancedScriptedScene( this, pszScene, nullptr, flDelay, false, response, false, filter );
}

float CBaseFlex::ScriptPlayScene( const char* pszScene, float flDelay )
{
	return InstancedScriptedScene( this, pszScene, nullptr, flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: Generate a one-shot scene in memory with one track which is to play the named sound on the actor
// Input  : *soundname -
// Output : float
//-----------------------------------------------------------------------------
float CBaseFlex::PlayAutoGeneratedSoundScene( const char *soundname )
{
	return InstancedAutoGeneratedSoundScene( this, soundname );
}

//--------------------------------------------------------------------------------------------------
// Returns the script instance of the scene entity associated with our oldest ("top level") scene event
//--------------------------------------------------------------------------------------------------
HSCRIPT CBaseFlex::ScriptGetOldestScene( void )
{
	if ( !m_SceneEvents.empty() )
	{
		CSceneEventInfo curScene = m_SceneEvents.front();
		return ToHScript( (CBaseEntity*)(curScene.m_hSceneEntity.Get()) );
	}
	else
	{
		return nullptr;
	}
}

//--------------------------------------------------------------------------------------------------
// Returns the script instance of the scene at the specified index, or null if index >= count
//--------------------------------------------------------------------------------------------------
HSCRIPT CBaseFlex::ScriptGetSceneByIndex( int index )
{
	if ( ks::is_valid_index( m_SceneEvents, index ) )
	{
		CSceneEventInfo curScene = m_SceneEvents[ index ];
		return ToHScript( (CBaseEntity*)(curScene.m_hSceneEntity.Get()) );
	}
	else
	{
		return nullptr;
	}
}

// FIXME: move to CBaseActor
bool CBaseFlex::EnterSceneSequence( CChoreoScene *scene, CChoreoEvent *event, bool bRestart )
{

	return false;

	// 2 seconds past current event, or 0.2 seconds past end of scene, whichever is shorter
	float flDuration = std::min( 2.0, std::min( event->GetEndTime() - scene->GetTime() + 2.0, scene->FindStopTime() - scene->GetTime() + 0.2 ) );

	

	// for now, don't interrupt sequences that don't understand being interrupted
	

	

	return false;
}

bool CBaseFlex::ExitSceneSequence( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: keep track of last valid flex animation time and returns if the current info should play theirs
//-----------------------------------------------------------------------------

bool CBaseFlex::IsSuppressedFlexAnimation( CSceneEventInfo *info )
{
	// check for suppression if the current info is a background
	if (info->m_pScene && info->m_pScene->IsBackground())
	{
		// allow for slight jitter
		return m_flLastFlexAnimationTime > gpGlobals->curtime - GetAnimTimeInterval() * 1.5;
	}
	// keep track of last non-suppressable flex animation
	m_flLastFlexAnimationTime = gpGlobals->curtime;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out body lean states that are invalidated with Teleport
//-----------------------------------------------------------------------------

void CBaseFlex::Teleport_Impl( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts )
{
	BaseClass::Teleport_Impl( newPosition, newAngles, newVelocity, bUseSlowHighAccuracyContacts );
}

//-----------------------------------------------------------------------------
// Purpose: keep track of accel/decal and lean the body
//-----------------------------------------------------------------------------

void CBaseFlex::DoBodyLean( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: initialize weight for background events
//-----------------------------------------------------------------------------

void CSceneEventInfo::InitWeight( CBaseFlex *pActor )
{
	if (pActor->IsSuppressedFlexAnimation( this ))
	{
		m_flWeight = 0.0;
	}
	else
	{
		m_flWeight = 1.0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: update weight for background events.  Only call once per think
//-----------------------------------------------------------------------------

float CSceneEventInfo::UpdateWeight( CBaseFlex *pActor )
{
	// decay if this is a background scene and there's other flex animations playing
	if (pActor->IsSuppressedFlexAnimation( this ))
	{
		m_flWeight = std::max( m_flWeight - 0.2, 0.0 );
	}
	else
	{
		m_flWeight = std::min( m_flWeight + 0.1, 1.0 );
	}
	return m_flWeight;
}

IMPLEMENT_REFLECT_DATAMAP( CFlexCycler )

LINK_ENTITY_TO_CLASS( cycler_flex, CGenericFlexCycler );

ConVar	flex_expression( side_registry(), "flex_expression","-" );
ConVar	flex_talk( side_registry(), "flex_talk","0" );

// Cycler member functions

void CFlexCycler::GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax)
{
	if (!szModel || !*szModel)
	{
		Warning( "cycler at %.0f %.0f %0.f missing modelname\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	PrecacheModel( szModel );
	SetModel( szModel );

	CFlexCycler::Spawn_Impl( );

	UTIL_SetSize(this, vecMin, vecMax);

	Vector vecEyeOffset;
	GetEyePosition( GetModelPtr(), vecEyeOffset );
	SetViewOffset( vecEyeOffset );

	InitBoneControllers();

	if (GetNumFlexControllers() < 5)
		Warning( "cycler_flex used on model %s without enough flexes.\n", szModel );
}

void CFlexCycler::Spawn_Impl( )
{
	Precache();
	/*
	if ( m_spawnflags & FCYCLER_NOTSOLID )
	{
		SetSolid( SOLID_NOT );
	}
	else
	{
		SetSolid( SOLID_SLIDEBOX );
	}
	*/

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_NONE );
	m_takedamage		= DAMAGE_YES;
	m_iHealth			= 80000;// no cycler should die

	m_flPlaybackRate	= 1.0f;
	m_flGroundSpeed		= 0;

	SetNextThink( gpGlobals->curtime + 1.0f );

	ResetSequenceInfo( );

	m_flCycle = random_valve->RandomFloat( 0, 1.0 );
}

const char *predef_flexcontroller_names[] = {
	"right_lid_raiser",
	"left_lid_raiser",
	"right_lid_tightener",
	"left_lid_tightener",
	"right_lid_droop",
	"left_lid_droop",
	"right_inner_raiser",
	"left_inner_raiser",
	"right_outer_raiser",
	"left_outer_raiser",
	"right_lowerer",
	"left_lowerer",
	"right_cheek_raiser",
	"left_cheek_raiser",
	"wrinkler",
	"right_upper_raiser",
	"left_upper_raiser",
	"right_corner_puller",
	"left_corner_puller",
	"corner_depressor",
	"chin_raiser",
	"right_puckerer",
	"left_puckerer",
	"right_funneler",
	"left_funneler",
	"tightener",
	"jaw_clencher",
	"jaw_drop",
	"right_mouth_drop",
	"left_mouth_drop",
	nullptr };

float predef_flexcontroller_values[7][30] = {
/* 0 */	{ 0.700,0.560,0.650,0.650,0.650,0.585,0.000,0.000,0.400,0.040,0.000,0.000,0.450,0.450,0.000,0.000,0.000,0.750,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.150,1.000,0.000,0.000,0.000 },
/* 1 */	{ 0.450,0.450,0.450,0.450,0.000,0.000,0.000,0.000,0.300,0.300,0.000,0.000,0.250,0.250,0.000,0.000,0.000,0.750,0.750,0.000,0.000,0.000,0.000,0.400,0.400,0.000,1.000,0.000,0.050,0.050 },
/* 2 */	{ 0.200,0.200,0.500,0.500,0.150,0.150,0.100,0.100,0.150,0.150,0.000,0.000,0.700,0.700,0.000,0.000,0.000,0.750,0.750,0.000,0.200,0.000,0.000,0.000,0.000,0.000,0.850,0.000,0.000,0.000 },
/* 3 */	{ 0.000,0.000,0.000,0.000,0.000,0.000,0.300,0.300,0.000,0.000,0.000,0.000,0.000,0.000,0.100,0.000,0.000,0.000,0.000,0.700,0.300,0.000,0.000,0.200,0.200,0.000,0.000,0.300,0.000,0.000 },
/* 4 */	{ 0.450,0.450,0.000,0.000,0.450,0.450,0.000,0.000,0.000,0.000,0.450,0.450,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.300,0.000,0.000,0.000,0.000 },
/* 5 */	{ 0.000,0.000,0.350,0.350,0.150,0.150,0.300,0.300,0.450,0.450,0.000,0.000,0.200,0.200,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.200,0.200,0.000,0.000,0.300,0.000,0.000,0.000,0.000 },
/* 6 */	{ 0.000,0.000,0.650,0.650,0.750,0.750,0.000,0.000,0.000,0.000,0.300,0.300,0.000,0.000,0.000,0.250,0.250,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000,0.000 }
};

//-----------------------------------------------------------------------------
// Purpose: Changes sequences when shot
//-----------------------------------------------------------------------------
int CFlexCycler::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	int nSequence = GetSequence() + 1;
	if (!IsValidSequence( nSequence ))
	{
		nSequence = 0;
	}

	ResetSequence( nSequence );
	m_flCycle = 0;

	return 0;
}

void CFlexCycler::SetFlexTarget( LocalFlexController_t flexnum )
{
	m_flextarget[flexnum] = random_valve->RandomFloat( 0.5, 1.0 );

	const char *pszType = GetFlexControllerType( flexnum );

	// zero out all other flexes of the same type
	for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
	{
		if (i != flexnum)
		{
			const char *pszOtherType = GetFlexControllerType( i );
			if (stricmp( pszType, pszOtherType ) == 0)
			{
				m_flextarget[i] = 0;
			}
		}
	}

	// HACK, for now, consider then linked is named "right_" or "left_"
	if ( StringHasPrefixCaseSensitive( GetFlexControllerName( flexnum ), "right_" ) )
	{
		m_flextarget[flexnum+1] = m_flextarget[flexnum];
	}
	else if ( StringHasPrefixCaseSensitive( GetFlexControllerName( flexnum ), "left_" ) )
	{
		m_flextarget[flexnum-1] = m_flextarget[flexnum];
	}
}

LocalFlexController_t CFlexCycler::LookupFlex( const char *szTarget  )
{
	for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
	{
		const char *pszFlex = GetFlexControllerName( i );
		if (stricmp( szTarget, pszFlex ) == 0)
		{
			return i;
		}
	}
	return LocalFlexController_t(-1);
}

void CFlexCycler::Think_Impl( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	StudioFrameAdvance ( );

	if (IsSequenceFinished() && !SequenceLoops())
	{
		// ResetSequenceInfo();
		// hack to avoid reloading model every frame
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 1.0;
		m_bSequenceFinished = false;
		m_flLastEventCheck = 0;
		m_flCycle = 0;
	}

	// only do this if they have more than eyelid movement
	if (GetNumFlexControllers() > 2)
	{
		const char *pszExpression = flex_expression.GetString();

		if (pszExpression && pszExpression[0] == '+' && pszExpression[1] != '\0')
		{
			int i;
			int j = std::atoi( &pszExpression[1] );
			for ( i = 0; i < GetNumFlexControllers(); i++)
			{
				m_flextarget[m_flexnum] = 0;
			}

			for (i = 0; i < 35 && predef_flexcontroller_names[i]; i++)
			{
				m_flexnum = LookupFlex( predef_flexcontroller_names[i] );
				m_flextarget[m_flexnum] = predef_flexcontroller_values[j][i];
				// Msg( "%s %.3f\n", predef_flexcontroller_names[i], predef_flexcontroller_values[j][i] );
			}
		}
		else if ( pszExpression && (pszExpression[0] == '1') && (pszExpression[1] == '\0') ) // 1 for maxed controller values
		{
			for ( LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++ )
			{
				// Max everything out...
				m_flextarget[i] = 1.0f;
				SetFlexWeight( i, m_flextarget[i] );
			}
		}
		else if ( pszExpression && (pszExpression[0] == '^') && (pszExpression[1] == '\0') ) // ^ for sine wave
		{
			for ( LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++ )
			{
				// Throw a differently offset sine wave on all of the flex controllers
				float fFlexTime = i * (1.0f / (float)GetNumFlexControllers()) + gpGlobals->curtime;
				m_flextarget[i] = std::sinf( fFlexTime ) * 0.5f + 0.5f;
				SetFlexWeight( i, m_flextarget[i] );
			}
		}
		else if (pszExpression && pszExpression[0] != '\0' && std::strcmp(pszExpression, "+") != 0)
		{
			char szExpression[128];
			char szTemp[32];

			V_strncpy( szExpression, pszExpression ,sizeof(szExpression));
			char *pszExpression = szExpression;

			while (*pszExpression != '\0')
			{
				if (*pszExpression == '+')
					*pszExpression = ' ';

				pszExpression++;
			}

			pszExpression = szExpression;

			while (*pszExpression)
			{
				if (*pszExpression != ' ')
				{
					if (*pszExpression == '-')
					{
						for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
						{
							m_flextarget[i] = 0;
						}
					}
					else if (*pszExpression == '?')
					{
						for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
						{
							Msg( "\"%s\" ", GetFlexControllerName( i ) );
						}
						Msg( "\n" );
						flex_expression.SetValue( "" );
					}
					else
					{
						if (std::sscanf( pszExpression, "%31s", szTemp ) == 1)
						{
							m_flexnum = LookupFlex( szTemp );

							if (m_flexnum != -1 && m_flextarget[m_flexnum] != 1)
							{
								m_flextarget[m_flexnum] = 1.0;
								// SetFlexTarget( m_flexnum );
							}
							pszExpression += std::strlen( szTemp ) - 1;
						}
					}
				}
				pszExpression++;
			}
		}
		else if (m_flextime < gpGlobals->curtime)
		{
			// m_flextime = gpGlobals->curtime + 1.0; // RandomFloat( 0.1, 0.5 );
			m_flextime = gpGlobals->curtime + random_valve->RandomFloat( 0.3, 0.5 ) * (30.0 / GetNumFlexControllers());
			m_flexnum = (LocalFlexController_t)random_valve->RandomInt( 0, GetNumFlexControllers() - 1 );

			// m_flexnum = (pflex->num + 1) % r_psubmodel->numflexes;

			if (m_flextarget[m_flexnum] == 1)
			{
				m_flextarget[m_flexnum] = 0;
				// pflex->time = cl.time + 0.1;
			}
			else if (stricmp( GetFlexControllerType( m_flexnum ), "phoneme" ) != 0)
			{
				if (std::strstr( GetFlexControllerName( m_flexnum ), "upper_raiser" ) == nullptr)
				{
					Msg( "%s:%s\n", GetFlexControllerType( m_flexnum ), GetFlexControllerName( m_flexnum ) );
					SetFlexTarget( m_flexnum );
				}
			}

		}

		// slide it up.
		for (LocalFlexController_t i = LocalFlexController_t(0); i < GetNumFlexControllers(); i++)
		{
			float weight = GetFlexWeight( i );

			if (weight != m_flextarget[i])
			{
				weight = weight + (m_flextarget[i] - weight) / random_valve->RandomFloat( 2.0, 4.0 );
			}
			weight = clamp( weight, 0.0f, 1.0f );
			SetFlexWeight( i, weight );
		}

		if (flex_talk.GetInt() == -1)
		{
			m_istalking = 1;
			char pszSentence[256];
			V_snprintf( pszSentence,sizeof(pszSentence), "%s%d", STRING(m_iszSentence), m_sentence++ );
			int sentenceIndex = engine->SentenceIndexFromName( pszSentence );
			if (sentenceIndex >= 0)
			{
				Msg( "%d : %s\n", sentenceIndex, pszSentence );
				CPASAttenuationFilter filter( this );
				CBaseEntity::EmitSentenceByIndex( filter, entindex(), CHAN_VOICE, sentenceIndex, 1, SNDLVL_TALKING, 0, PITCH_NORM );
			}
			else
			{
				m_sentence = 0;
			}

			flex_talk.SetValue( "0" );
		}
		else if (!FStrEq( flex_talk.GetString(), "0") )
		{
			int sentenceIndex = engine->SentenceIndexFromName( flex_talk.GetString() );
			if (sentenceIndex >= 0)
			{
				CPASAttenuationFilter filter( this );
				CBaseEntity::EmitSentenceByIndex( filter, entindex(), CHAN_VOICE, sentenceIndex, 1, SNDLVL_TALKING, 0, PITCH_NORM );
			}
			flex_talk.SetValue( "0" );
		}

		// blink
		if (m_blinktime < gpGlobals->curtime)
		{
			Blink();
			m_blinktime = gpGlobals->curtime + random_valve->RandomFloat( 1.5, 4.5 );
		}
	}

	Vector forward, right, up;
	GetVectors( &forward, &right, &up );

	CBaseEntity *pPlayer = (CBaseEntity *)UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		if (pPlayer->GetSmoothedVelocity().Length() != 0 && DotProduct( forward, pPlayer->EyePosition() - EyePosition()) > 0.5)
		{
			m_lookTarget = pPlayer->EyePosition();
			m_looktime = gpGlobals->curtime + random_valve->RandomFloat(2.0,4.0);
		}
		else if (m_looktime < gpGlobals->curtime)
		{
			if ((!m_istalking) && random_valve->RandomInt( 0, 1 ) == 0)
			{
				m_lookTarget = EyePosition() + forward * 128 + right * random_valve->RandomFloat(-64,64) + up * random_valve->RandomFloat(-32,32);
				m_looktime = gpGlobals->curtime + random_valve->RandomFloat(0.3,1.0);

				if (m_blinktime - 0.5 < gpGlobals->curtime)
				{
					Blink();
				}
			}
			else
			{
				m_lookTarget = pPlayer->EyePosition();
				m_looktime = gpGlobals->curtime + random_valve->RandomFloat(1.0,4.0);
			}
		}

		SetViewtarget( m_lookTarget );
	}

	// Handle any facial animation from scene playback
	// FIXME: do we still actually need flex cyclers?
	// AddSceneSceneEvents();
}

void CFlexCycler::ProcessSceneEvents( void )
{
	// Don't do anything since we handle facial stuff in Think()
}

//-----------------------------------------------------------------------------
// For toggling blinking
//-----------------------------------------------------------------------------
void CBaseFlex::Blink()
{
	m_blinktoggle = !m_blinktoggle;
}

//-----------------------------------------------------------------------------
// Do we have active expressions?
//-----------------------------------------------------------------------------
bool CBaseFlex::HasSceneEvents() const
{
	return !m_SceneEvents.empty();
}

//-----------------------------------------------------------------------------
// Other inlines
//-----------------------------------------------------------------------------
const Vector &CBaseFlex::GetViewtarget( ) const
{
	return m_viewtarget.Get();	// bah
}

void CBaseFlex::SetFlexWeight( char *szName, float value )
{
	SetFlexWeight( FindFlexController( szName ), value );
}

float CBaseFlex::GetFlexWeight( char *szName )
{
	return GetFlexWeight( FindFlexController( szName ) );
}

KS_ENTITY_ABSTRACT_KIND( CFlexCycler )

class CBaseCombatCharacter;
class CBaseEntity;
class CBaseGrenade;
class CBasePlayer;
class CTakeDamageInfo;

using namespace ::ks::reflect;

extern int	g_sModelIndexFireball;		// (in combatweapon.cpp) holds the index for the fireball 
extern int	g_sModelIndexWExplosion;	// (in combatweapon.cpp) holds the index for the underwater explosion
extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud
extern ConVar    sk_plr_dmg_grenade;

// Global Savedata for friction modifier
IMPLEMENT_REFLECT_DATAMAP( CBaseGrenade )

void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID);

IMPLEMENT_NETWORKCLASS_ALIASED( BaseGrenade, DT_BaseGrenade )

IMPLEMENT_REFLECT_TABLE( CBaseGrenade, DT_BaseGrenade );

LINK_ENTITY_TO_CLASS_ALIASED( grenade, BaseGrenade );

// Grenades flagged with this will be triggered when the owner calls detonateSatchelCharges
static constexpr auto SF_DETONATE = 0x0001;

static constexpr auto MAX_WATER_SURFACE_DISTANCE = 512;

// UNDONE: temporary scorching for PreAlpha - find a less sleazy permenant solution.
void CBaseGrenade::Explode( trace_t *pTrace, int bitsDamageType )
{
	
	SetModelName( NULL_STRING );//invisible
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->fraction != 1.0 )
	{
		SetAbsOrigin( pTrace->endpos + (pTrace->plane.normal * 0.6) );
	}

	Vector vecAbsOrigin = GetAbsOrigin();
	int contents = UTIL_PointContents ( vecAbsOrigin, MASK_ALL );

	// Try using the new particle system instead of temp ents
	surfacedata_t *pSurfaceData = physprops->GetSurfaceData( pTrace->surface.surfaceProps );
	const char *pEffectName = GetParticleSystemName( contents, pSurfaceData );
	if ( pEffectName != nullptr )
	{
		Vector vecParticleOrigin = vecAbsOrigin;

		if ( contents & MASK_WATER )
		{
			// Find our water surface by tracing up till we're out of the water
			trace_t tr;
			Vector vecTrace( 0, 0, MAX_WATER_SURFACE_DISTANCE );
			UTIL_TraceLine( vecParticleOrigin, vecParticleOrigin + vecTrace, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );

			// If we didn't start in water, we're above it
			if ( tr.startsolid == false )
			{
				// Look downward to find the surface
				vecTrace.Init( 0, 0, -MAX_WATER_SURFACE_DISTANCE );
				UTIL_TraceLine( vecParticleOrigin, vecParticleOrigin + vecTrace, MASK_WATER, nullptr, COLLISION_GROUP_NONE, &tr );

				// If we hit it, setup the explosion
				if ( tr.fraction < 1.0f )
				{
					vecParticleOrigin = tr.endpos;
				}
			}
			else if ( tr.fractionleftsolid )
			{
				// Otherwise we came out of the water at this point
				vecParticleOrigin = vecParticleOrigin + (vecTrace * tr.fractionleftsolid);
			}
		}

		QAngle	vecAngles;
		DispatchParticleEffect( pEffectName, vecParticleOrigin, vecAngles );
	}
	else
	{
		if ( pTrace->fraction != 1.0 )
		{
			Vector vecNormal = pTrace->plane.normal;
			surfacedata_t *pdata = physprops->GetSurfaceData( pTrace->surface.surfaceProps );	
			CPASFilter filter( vecAbsOrigin );

			te->Explosion( filter, -1.0, // don't apply cl_interp delay
				vecAbsOrigin,
				!( contents & MASK_WATER ) ? g_sModelIndexFireball : g_sModelIndexWExplosion,
				m_DmgRadius * .03, 
				25,
				TE_EXPLFLAG_NONE,
				m_DmgRadius,
				m_flDamage,
				&vecNormal,
				(char) pdata->game.material );
		}
		else
		{
			CPASFilter filter( vecAbsOrigin );
			te->Explosion( filter, -1.0, // don't apply cl_interp delay
				vecAbsOrigin, 
				!( contents & MASK_WATER ) ? g_sModelIndexFireball : g_sModelIndexWExplosion,
				m_DmgRadius * .03, 
				25,
				TE_EXPLFLAG_NONE,
				m_DmgRadius,
				m_flDamage );
		}
	}

	CSoundEnt::InsertSound ( SOUND_COMBAT, GetAbsOrigin(), BASEGRENADE_EXPLOSION_VOLUME, 3.0 );

	// Use the thrower's position as the reported position
	Vector vecReported = m_hThrower ? m_hThrower->GetAbsOrigin() : vec3_origin;
	
	EmitSound( "BaseGrenade.Explode" );
	CTakeDamageInfo info( this, m_hThrower, GetBlastForce(), GetAbsOrigin(), m_flDamage, bitsDamageType, 0, &vecReported );

	RadiusDamage( info, GetAbsOrigin(), m_DmgRadius, CLASS_NONE, nullptr );

	UTIL_DecalTrace( pTrace, "Scorch" );

	SetThink( &CBaseGrenade::SUB_Remove );
	SetTouch( nullptr );
	SetSolid( SOLID_NONE );
	
	AddEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );

	SetNextThink( gpGlobals->curtime );

}

void CBaseGrenade::Smoke( void )
{
	Vector vecAbsOrigin = GetAbsOrigin();
	if ( UTIL_PointContents ( vecAbsOrigin, MASK_WATER ) & MASK_WATER )
	{
		UTIL_Bubbles( vecAbsOrigin - Vector( 64, 64, 64 ), vecAbsOrigin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		CPVSFilter filter( vecAbsOrigin );

		te->Smoke( filter, 0.0, 
			&vecAbsOrigin, g_sModelIndexSmoke,
			m_DmgRadius * 0.03,
			24 );
	}
	SetThink ( &CBaseGrenade::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

void CBaseGrenade::Event_Killed_Impl( const CTakeDamageInfo &info )
{
	Detonate( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGrenade::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Support player pickup
	if ( useType == USE_TOGGLE )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( pPlayer )
		{
			pPlayer->PickupObject( this );
			return;
		}
	}

	// Pass up so we still call any custom Use function
	BaseClass::Use_Impl( pActivator, pCaller, useType, value );
}

//-----------------------------------------------------------------------------
// Purpose: Timed grenade, this think is called when time runs out.
//-----------------------------------------------------------------------------
void CBaseGrenade::DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CBaseGrenade::Detonate );
	SetNextThink( gpGlobals->curtime );
}

void CBaseGrenade::PreDetonate( void )
{
	CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this );

	SetThink( &CBaseGrenade::Detonate );
	SetNextThink( gpGlobals->curtime + 1.5 );
}

void CBaseGrenade::Detonate( void )
{
	trace_t		tr;
	Vector		vecSpot;// trace starts here!

	SetThink( nullptr );

	vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -32 ), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);

	if( tr.startsolid )
	{
		// Since we blindly moved the explosion origin vertically, we may have inadvertently moved the explosion into a solid,
		// in which case nothing is going to be harmed by the grenade's explosion because all subsequent traces will startsolid.
		// If this is the case, we do the downward trace again from the actual origin of the grenade. (sjb) 3/8/2007  (for ep2_outland_09)
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -32), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
	}

	Explode( &tr, DMG_BLAST );

	if ( GetShakeAmplitude() )
	{
		UTIL_ScreenShake( GetAbsOrigin(), GetShakeAmplitude(), 150.0, 1.0, GetShakeRadius(), SHAKE_START );
	}
}

//
// Contact grenade, explode when it touches something
// 
void CBaseGrenade::ExplodeTouch( CBaseEntity *pOther )
{
	trace_t		tr;
	Vector		vecSpot;// trace starts here!

	contract_assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	Vector velDir = GetAbsVelocity();
	VectorNormalize( velDir );
	vecSpot = GetAbsOrigin() - velDir * 32;
	UTIL_TraceLine( vecSpot, vecSpot + velDir * 64, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

	Explode( &tr, DMG_BLAST );
}

void CBaseGrenade::DangerSoundThink( void )
{
	if (!IsInWorld())
	{
		Remove( );
		return;
	}

	CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, GetAbsVelocity().Length( ), 0.2, this );

	SetNextThink( gpGlobals->curtime + 0.2 );

	if (GetWaterLevel() != WL_NotInWater)
	{
		SetAbsVelocity( GetAbsVelocity() * 0.5 );
	}
}

void CBaseGrenade::BounceTouch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS) )
		return;

	// don't hit the guy that launched this grenade
	if ( pOther == GetThrower() )
		return;

	// only do damage if we're moving fairly fast
	if ( (pOther->m_takedamage != DAMAGE_NO) && (m_flNextAttack < gpGlobals->curtime && GetAbsVelocity().Length() > 100))
	{
		if (m_hThrower)
		{
			trace_t tr;
			tr = CBaseEntity::GetTouchTrace( );
			ClearMultiDamage( );
			Vector forward;
			AngleVectors( GetLocalAngles(), &forward, nullptr, nullptr );
			CTakeDamageInfo info( this, m_hThrower, 1, DMG_CLUB );
			CalculateMeleeDamageForce( &info, GetAbsVelocity(), GetAbsOrigin() );
			pOther->DispatchTraceAttack( info, forward, &tr ); 
			ApplyMultiDamage();
		}
		m_flNextAttack = gpGlobals->curtime + 1.0; // debounce
	}

	Vector vecTestVelocity;
	// m_vecAngVelocity = Vector (300, 300, 300);

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity. 
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = GetAbsVelocity(); 
	vecTestVelocity.z *= 0.45;

	if ( !m_bHasWarnedAI && vecTestVelocity.Length() <= 60 )
	{
		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
		// emit the danger sound.
		
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), m_flDamage / 0.4, 0.3, this );
		m_bHasWarnedAI = true;
	}

	if (GetFlags() & FL_ONGROUND)
	{
		// add a bit of static friction
//		SetAbsVelocity( GetAbsVelocity() * 0.8 );

		// SetSequence( random->RandomInt( 1, 1 ) ); // FIXME: missing tumble animations
	}
	else
	{
		// play bounce sound
		BounceSound();
	}
	m_flPlaybackRate = GetAbsVelocity().Length() / 200.0;
	if (GetPlaybackRate() > 1.0)
		m_flPlaybackRate = 1;
	else if (GetPlaybackRate() < 0.5)
		m_flPlaybackRate = 0;

}

void CBaseGrenade::SlideTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther == GetThrower() )
		return;

	// m_vecAngVelocity = Vector (300, 300, 300);

	if (GetFlags() & FL_ONGROUND)
	{
		// add a bit of static friction
//		SetAbsVelocity( GetAbsVelocity() * 0.95 );  

		if (GetAbsVelocity().x != 0 || GetAbsVelocity().y != 0)
		{
			// maintain sliding sound
		}
	}
	else
	{
		BounceSound();
	}
}

void CBaseGrenade ::BounceSound( void )
{
	// Doesn't need to do anything anymore! Physics makes the sound.
}

void CBaseGrenade ::TumbleThink( void )
{
	if (!IsInWorld())
	{
		Remove( );
		return;
	}

	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );

	//
	// Emit a danger sound one second before exploding.
	//
	if (m_flDetonateTime - 1 < gpGlobals->curtime)
	{
		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * (m_flDetonateTime - gpGlobals->curtime), 400, 0.1, this );
	}

	if (m_flDetonateTime <= gpGlobals->curtime)
	{
		SetThink( &CBaseGrenade::Detonate );
	}

	if (GetWaterLevel() != WL_NotInWater)
	{
		SetAbsVelocity( GetAbsVelocity() * 0.5 );
		m_flPlaybackRate = 0.2;
	}
}

void CBaseGrenade::Precache_Impl( void )
{
	BaseClass::Precache_Impl( );

	PrecacheScriptSound( "BaseGrenade.Explode" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatCharacter
//-----------------------------------------------------------------------------
CBaseCombatCharacter *CBaseGrenade::GetThrower( void )
{
	CBaseCombatCharacter *pResult = ToBaseCombatCharacter( m_hThrower );
	if ( !pResult && GetOwnerEntity() != nullptr )
	{
		pResult = ToBaseCombatCharacter( GetOwnerEntity() );
	}
	return pResult;
}

//-----------------------------------------------------------------------------

void CBaseGrenade::SetThrower( CBaseCombatCharacter *pThrower )
{
	m_hThrower = pThrower;

	// if this is the first thrower, set it as the original thrower
	if ( nullptr == m_hOriginalThrower )
	{
		m_hOriginalThrower = pThrower;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseGrenade::~CBaseGrenade(void)
{
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseGrenade::CBaseGrenade(void)
{
	m_hThrower			= nullptr;
	m_hOriginalThrower	= nullptr;
	m_bIsLive			= false;
	m_DmgRadius			= 100;
	m_flDetonateTime	= 0;
	m_bHasWarnedAI		= false;

	SetSimulatedEveryTick( true );
};

class CAI_Expresser;
class CBaseMultiplayerPlayer;
class CBasePlayer;

using namespace ::ks::reflect;

ConVar sv_allchat(side_registry(), "sv_allchat", "1", FCVAR_NOTIFY, "Players can receive all other players' text chat, team restrictions apply");

CBaseMultiplayerPlayer::CBaseMultiplayerPlayer()
{
	m_iCurrentConcept = MP_CONCEPT_NONE;
	m_flLastForcedChangeTeamTime = -1;
	m_iBalanceScore = 0;
	m_flConnectionTime = gpGlobals->curtime;
	m_pExpresser = nullptr;

	// per life achievement counters
	m_pAchievementKV = new KeyValues( "achievement_counts" );

	m_flAreaCaptureScoreAccumulator = 0.0f;
}

CBaseMultiplayerPlayer::~CBaseMultiplayerPlayer()
{
	m_pAchievementKV->deleteThis();
	delete m_pExpresser;
}

BEGIN_ENT_SCRIPTDESC( CBaseMultiplayerPlayer, CBasePlayer, "Player" )
END_SCRIPTDESC();

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CAI_Expresser *CBaseMultiplayerPlayer::CreateExpresser( void )
{
	m_pExpresser = new CMultiplayer_Expresser(this);
	if ( !m_pExpresser)
		return nullptr;

	m_pExpresser->Connect(this);
	return m_pExpresser;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseMultiplayerPlayer::PostConstructor_Impl( const char *szClassname )
{
	BaseClass::PostConstructor_Impl( szClassname );
	CreateExpresser();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseMultiplayerPlayer::ModifyOrAppendCriteria_Impl( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria_Impl( criteriaSet );

	ModifyOrAppendPlayerCriteria( criteriaSet );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseMultiplayerPlayer::SpeakIfAllowed( AIConcept_t conc, SpeechPriorityType priority, const char *modifiers, char *pszOutResponseChosen, std::size_t bufsize, IRecipientFilter *filter )
{
	if ( !IsAlive() )
		return false;

	//if ( IsAllowedToSpeak( conc, bRespondingToPlayer ) )
	return Speak( conc, modifiers, pszOutResponseChosen, bufsize, filter );
}

//-----------------------------------------------------------------------------
// Purpose: Fill out given response with the appropriate one for this concept
//-----------------------------------------------------------------------------
void CBaseMultiplayerPlayer::SpeakConcept( AI_Response &outResponse, int iConcept )
{
	m_iCurrentConcept = iConcept;
	AIConcept_t conc( g_pszMPConcepts[iConcept] );
	FindResponse( outResponse, conc );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseMultiplayerPlayer::SpeakConceptIfAllowed( int iConcept, const char *modifiers, char *pszOutResponseChosen, std::size_t bufsize, IRecipientFilter *filter )
{
	// Save the current concept.
	m_iCurrentConcept = iConcept;
	return SpeakIfAllowed( g_pszMPConcepts[iConcept], SPEECH_PRIORITY_NORMAL, modifiers, pszOutResponseChosen, bufsize, filter );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseMultiplayerPlayer::CanHearAndReadChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console unless we're ignoring every chat
	if ( !pPlayer )
		return m_iIgnoreGlobalChat != CHAT_IGNORE_EVERYTHING;

	// check if we're ignoring all chat
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_BROADCAST_AND_TEAM )
		return false;

	// check if we're ignoring all but teammates
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_BROADCAST && g_pGameRules->PlayerRelationship( this, pPlayer ) != GR_TEAMMATE )
		return false;

	// can't hear dead players if we're alive
	if ( pPlayer->m_lifeState != LIFE_ALIVE && m_lifeState == LIFE_ALIVE )
		return false;

	return true;
}

bool CBaseMultiplayerPlayer::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];

	if ( FStrEq( pcmd, "ignoremsg" ) )
	{
		m_iIgnoreGlobalChat = (m_iIgnoreGlobalChat + 1) % 4;
		switch( m_iIgnoreGlobalChat )
		{
		case CHAT_IGNORE_NONE:
			ClientPrint( this, HUD_PRINTTALK, "#Accept_All_Messages" );
			break;
		case CHAT_IGNORE_BROADCAST:
			ClientPrint( this, HUD_PRINTTALK, "#Ignore_Broadcast_Messages" );
			break;
		case CHAT_IGNORE_BROADCAST_AND_TEAM:
			ClientPrint( this, HUD_PRINTTALK, "#Ignore_Broadcast_Team_Messages" );
			break;
		case CHAT_IGNORE_EVERYTHING:
			ClientPrint( this, HUD_PRINTTALK, "#Ignore_All_Messages" );
			break;
		default:
			break;
		}
		return true;
	}

	return BaseClass::ClientCommand( args );
}

bool CBaseMultiplayerPlayer::ShouldShowVoiceSubtitleToEnemy( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// calculate a score for this player. higher is more likely to be switched
//-----------------------------------------------------------------------------
int	CBaseMultiplayerPlayer::CalculateTeamBalanceScore( void )
{
	// base score is 0 - ( seconds on server )
	float flTimeConnected = gpGlobals->curtime - m_flConnectionTime;
	int iScore = 0 - (int)flTimeConnected;

	// if we were switched recently, score us way down
	float flLastSwitchedTime = GetLastForcedChangeTeamTime();
	if ( flLastSwitchedTime > 0 && ( gpGlobals->curtime - flLastSwitchedTime ) < 300 )
	{
		iScore -= 10000;
	}
	return iScore;
}

void CBaseMultiplayerPlayer::Spawn_Impl( void )
{
	ResetPerLifeCounters();

	BaseClass::Spawn_Impl();
}

void CBaseMultiplayerPlayer::AwardAchievement( int iAchievement, int iCount )
{
	contract_assert( iAchievement >= 0 && iAchievement < 0xFFFF );		// must fit in short

	CSingleUserRecipientFilter filter( this );

	int userID = GetPlayerInfo()->GetUserID();

	ks::net::CCSUsrMsg_AchievementEvent msg;
	msg.achievement = iAchievement;
	msg.count = iCount;
	msg.user_id = userID;
	SendUserMessage( filter, ks::net::CS_UM_AchievementEvent, msg );
}

int	CBaseMultiplayerPlayer::GetPerLifeCounterKV( const char *name )
{
	return m_pAchievementKV->GetInt( name, 0 );
}

void CBaseMultiplayerPlayer::SetPerLifeCounterKV( const char *name, int value )
{
	m_pAchievementKV->SetInt( name, value );
}

void CBaseMultiplayerPlayer::ResetPerLifeCounters( void )
{
	m_pAchievementKV->Clear();
}

inline constexpr const char *ESCORT_SCORE_CONTEXT = "AreaScoreContext";
static constexpr auto ESCORT_SCORE_INTERVAL = 0.1;

//-----------------------------------------------------------------------------
// Purpose: think to accumulate and award points for escorting the train
//-----------------------------------------------------------------------------
void CBaseMultiplayerPlayer::EscortScoringThink( void )
{
	m_flAreaCaptureScoreAccumulator += ESCORT_SCORE_INTERVAL;

	if ( m_flCapPointScoreRate > 0 )
	{
		float flTimeForOnePoint = 1.0f / m_flCapPointScoreRate;

		int iPoints = 0;

		while ( m_flAreaCaptureScoreAccumulator >= flTimeForOnePoint )
		{
			m_flAreaCaptureScoreAccumulator -= flTimeForOnePoint;
			iPoints++;
		}

		if ( iPoints > 0 )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "player_escort_score" );
			if ( event )
			{
				event->SetInt( "player", entindex() );
				event->SetInt( "points", iPoints );
				gameeventmanager->FireEvent( event, true /* only to server */ );
			}
		}
	}

	SetContextThink( &CBaseMultiplayerPlayer::EscortScoringThink, gpGlobals->curtime + ESCORT_SCORE_INTERVAL, ESCORT_SCORE_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: We're escorting the train, start giving points
//-----------------------------------------------------------------------------
void CBaseMultiplayerPlayer::StartScoringEscortPoints( float flRate )
{
	contract_assert( flRate > 0.0f );
	m_flCapPointScoreRate = flRate;
	SetContextThink( &CBaseMultiplayerPlayer::EscortScoringThink, gpGlobals->curtime + ESCORT_SCORE_INTERVAL, ESCORT_SCORE_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: Stopped escorting the train
//-----------------------------------------------------------------------------
void CBaseMultiplayerPlayer::StopScoringEscortPoints( void )
{
	SetContextThink( nullptr, 0, ESCORT_SCORE_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseMultiplayerPlayer::GetPlayerID( CPlayerId *pID ) const
{
	const CPlayerId *pClientID = engine->GetClientPlayerID( edict() );
	if ( pClientID )
	{
		*pID = *pClientID;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
uint64 CBaseMultiplayerPlayer::GetPlayerIDAsUInt64( void ) const
{
	CPlayerId playerIDForPlayer;
	if ( GetPlayerID( &playerIDForPlayer ) )
		return playerIDForPlayer.ConvertToUint64();
	return 0;
}

void CBaseMultiplayerPlayer::Precache_Impl( void)
{
	PrecacheParticleSystem( "achieved" );

	BaseClass::Precache_Impl();
}

class CBaseEntity;

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( BaseParticleEntity, DT_BaseParticleEntity )

IMPLEMENT_REFLECT_TABLE( CBaseParticleEntity, DT_BaseParticleEntity );

CBaseParticleEntity::CBaseParticleEntity( void )
{
}

CBaseParticleEntity::~CBaseParticleEntity( void )
{
}

int CBaseParticleEntity::UpdateTransmitState_Impl( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return SetTransmitState( FL_EDICT_DONTSEND );

	if ( IsEFlagSet( EFL_IN_SKYBOX ) )
		return SetTransmitState( FL_EDICT_ALWAYS );

	// cull against PVS
	return SetTransmitState( FL_EDICT_PVSCHECK );
}

void CBaseParticleEntity::Activate_Impl()
{
	BaseClass::Activate_Impl();
}	

void CBaseParticleEntity::Think_Impl()
{
	Remove( );
}

void CBaseParticleEntity::FollowEntity(CBaseEntity *pEntity)
{
	BaseClass::FollowEntity( pEntity );
	SetLocalOrigin( vec3_origin );
}

void CBaseParticleEntity::SetLifetime(float lifetime)
{
	if(lifetime == -1)
		SetNextThink( TICK_NEVER_THINK );
	else
		SetNextThink( gpGlobals->curtime + lifetime );
}

KS_ENTITY_ABSTRACT_KIND( CBaseMultiplayerPlayer )

KS_ENTITY_ABSTRACT_KIND( CBaseParticleEntity )

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CCSPlayer;
class CRecipientFilter;
class CUserCmd;
class CWeaponCSBase;
struct fogparams_t;

using namespace ::ks::reflect;

// Purpose: Implements shared baseplayer class functionality

	extern int TrainSpeed(int iSpeed, int iMax);
	
//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
static constexpr auto VPHYS_MAX_DISTANCE = 2.0;
static constexpr auto VPHYS_MAX_VEL = 10;
inline constexpr auto VPHYS_MAX_DISTSQR = (VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE);
inline constexpr auto VPHYS_MAX_VELSQR = (VPHYS_MAX_VEL*VPHYS_MAX_VEL);

ConVar sv_infinite_ammo( side_registry(), "sv_infinite_ammo", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Player's active weapon will never run out of ammo. If set to 2 then player has infinite total ammo but still has to reload the magazine." );

ConVar view_punch_decay( side_registry(), "view_punch_decay", "18", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor exponent for view punch" );
ConVar view_recoil_tracking( side_registry(), "view_recoil_tracking", "0.45", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "How closely the view tracks with the aim punch from weapon recoil" );

ConVar sv_footstep_sound_frequency( side_registry(), "sv_footstep_sound_frequency", "0.97", FCVAR_CHEAT | FCVAR_REPLICATED, "How frequent to hear the player's step sound or how fast they appear to be running from first person." );

	static ConVar physicsshadowupdate_render( side_registry(), "physicsshadowupdate_render", "0" );

	extern ConVar sv_pushaway_max_force;
	extern ConVar sv_pushaway_force;
	extern ConVar sv_turbophysics;

	class CUsePushFilter : public CTraceFilterEntitiesOnly
	{
	public:
		bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
		{
			CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

			// Static prop case...
			if ( !pEntity )
				return false;

			// Only impact on physics objects
			if ( !pEntity->VPhysicsGetObject() )
				return false;

			return g_pGameRules->CanEntityBeUsePushed( pEntity );
		}
	};

extern ConVar sv_turbophysics;
extern CMoveData *g_pMoveData;

extern ConVar sv_coaching_enabled;

bool UseHWMorphModels()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBasePlayer::GetTimeBase( void ) const
{
	return m_nTickBase * TICK_INTERVAL;
}

float CBasePlayer::GetPlayerMaxSpeed()
{
	// player max speed is the lower limit of m_flMaxSpeed and sv_maxspeed
	float fMaxSpeed = sv_maxspeed.GetFloat();
	if ( MaxSpeed() > 0.0f && MaxSpeed() < fMaxSpeed )
		fMaxSpeed = MaxSpeed();

	return fMaxSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: Called every usercmd by the player PreThink
//-----------------------------------------------------------------------------
void CBasePlayer::ItemPreFrame()
{
	// Handle use events
	PlayerUse();

	CBaseCombatWeapon *pActive = GetActiveWeapon();

	// Allow all the holstered weapons to update
	for ( int i = 0; i < WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );

		if ( pWeapon == nullptr )
			continue;

		if ( pActive == pWeapon )
			continue;

		pWeapon->ItemHolsterFrame();
	}

    if ( gpGlobals->curtime < m_flNextAttack )
		return;

	if (!pActive)
		return;

	pActive->ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.


//-----------------------------------------------------------------------------
// Purpose: Called every usercmd by the player PostThink
//-----------------------------------------------------------------------------
extern ConVar sv_infinite_ammo;
void CBasePlayer::ItemPostFrame()
{
	CVProfScope _( "CBasePlayer::ItemPostFrame", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Put viewmodels into basically correct place based on new player origin
	CalcViewModelView( EyePosition(), EyeAngles() );

	// Don't process items while in a vehicle.
	

	// check if the player is using something
	if ( m_hUseEntity != nullptr )
	{
		contract_assert( !nullptr );
		ImpulseCommands();// this will call playerUse
		return;
	}

    if ( gpGlobals->curtime < m_flNextAttack )
	{
		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->ItemBusyFrame();
		}
	}
	else
	{
		if ( GetActiveWeapon() )
		{

			{
				GetActiveWeapon()->ItemPostFrame( );
			}
		}
	}

	ImpulseCommands();

	if( ( sv_infinite_ammo.GetInt() == 1 ) && (GetActiveWeapon() != nullptr) )
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();

		pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		pWeapon->m_iClip2 = pWeapon->GetMaxClip2();

 		if ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE )
		{
			int iPrimaryAmmoType = pWeapon->GetPrimaryAmmoType();
			if ( iPrimaryAmmoType >= 0 )
				SetAmmoCount( GetAmmoDef()->MaxCarry( iPrimaryAmmoType, this ), iPrimaryAmmoType );

			int iSecondaryAmmoType = pWeapon->GetSecondaryAmmoType();
			if ( iSecondaryAmmoType >= 0 )
				SetAmmoCount( GetAmmoDef()->MaxCarry( iSecondaryAmmoType, this ), iSecondaryAmmoType );
		}
		else
		{
			pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, pWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY ), true );
			pWeapon->SetReserveAmmoCount( AMMO_POSITION_SECONDARY, pWeapon->GetReserveAmmoMax( AMMO_POSITION_SECONDARY ), true );
		}

	}
}

//-----------------------------------------------------------------------------
// Eye angles
//-----------------------------------------------------------------------------
const QAngle &CBasePlayer::EyeAngles_Impl( )
{
	// NOTE: Viewangles are measured *relative* to the parent's coordinate system
	CBaseEntity *pMoveParent = const_cast<CBasePlayer*>(this)->GetMoveParent();

	if ( !pMoveParent )
	{
		// if in camera mode, use that
		if ( GetViewEntity() != nullptr )
		{
			return GetViewEntity()->EyeAngles();
		}
		return pl.v_angle;
	}

	// FIXME: Cache off the angles?
	matrix3x4_t eyesToParent, eyesToWorld;
	AngleMatrix( pl.v_angle, eyesToParent );
	ConcatTransforms( pMoveParent->EntityToWorldTransform(), eyesToParent, eyesToWorld );

	static QAngle angEyeWorld;
	MatrixAngles( eyesToWorld, angEyeWorld );
	return angEyeWorld;
}

const QAngle &CBasePlayer::LocalEyeAngles_Impl()
{
	return pl.v_angle;
}

//-----------------------------------------------------------------------------
// Actual Eye position + angles
//-----------------------------------------------------------------------------
Vector CBasePlayer::EyePosition_Impl( )
{
	{
		// if in camera mode, use that
		if ( GetViewEntity() != nullptr )
		{
			return GetViewEntity()->EyePosition();
		}

		return BaseClass::EyePosition_Impl();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector CBasePlayer::GetPlayerMins( void ) const
{
	if ( IsObserver() )
	{
		return VEC_OBS_HULL_MIN;	
	}
	else
	{
		if ( GetFlags() & FL_DUCKING )
		{
			return VEC_DUCK_HULL_MIN;
		}
		else
		{
			return VEC_HULL_MIN;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector CBasePlayer::GetPlayerMaxs( void ) const
{	
	if ( IsObserver() )
	{
		return VEC_OBS_HULL_MAX;	
	}
	else
	{
		if ( GetFlags() & FL_DUCKING )
		{
			return VEC_DUCK_HULL_MAX;
		}
		else
		{
			return VEC_HULL_MAX;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateCollisionBounds( void )
{
	if ( GetFlags() & FL_DUCKING )
	{
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	}
	else
	{
		SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the vehicle view, or simply return the cached position and angles


//-----------------------------------------------------------------------------
// Returns eye vectors
//-----------------------------------------------------------------------------
void CBasePlayer::EyeVectors( Vector *pForward, Vector *pRight, Vector *pUp )
{
	{
		AngleVectors( EyeAngles(), pForward, pRight, pUp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the eye position and angle vectors.
//-----------------------------------------------------------------------------
void CBasePlayer::EyePositionAndVectors( Vector *pPosition, Vector *pForward,
										 Vector *pRight, Vector *pUp )
{
	// Handle the view in the vehicle
	{
		VectorCopy( EyePosition(), *pPosition );
		AngleVectors( EyeAngles(), pForward, pRight, pUp );
	}
}

surfacedata_t *CBasePlayer::GetLadderSurface( const Vector &origin )
{
	return physprops->GetSurfaceData( physprops->GetSurfaceIndex( "ladder" ) );
}

void CBasePlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
{
	bool bWalking;
	float fvol;
	Vector knee;
	Vector feet;
	float height;
	float speed;
	float velrun;
	float velwalk;
	bool fLadder;

	if ( m_flStepSoundTime > 0 )
	{
		m_flStepSoundTime -= 1000.0f * gpGlobals->frametime;
		if ( m_flStepSoundTime < 0 )
		{
			m_flStepSoundTime = 0;
		}
	}

	if ( m_flStepSoundTime > 0 )
		return;

	if ( GetFlags() & (FL_FROZEN|FL_ATCONTROLS))
		return;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return;

	if ( !sv_footsteps.GetFloat() )
		return;

	speed = VectorLength( vecVelocity );
	float groundspeed = Vector2DLength( vecVelocity.AsVector2D() );

	// determine if we are on a ladder
	fLadder = ( GetMoveType() == MOVETYPE_LADDER );

	GetStepSoundVelocities( &velwalk, &velrun );

	bool onground = ( GetFlags() & FL_ONGROUND );
	bool movingalongground = ( groundspeed > 0.0001f );
	bool moving_fast_enough =  ( speed >= velwalk );

	// To hear step sounds you must be either on a ladder or moving along the ground AND
	// You must be moving fast enough

	if ( !moving_fast_enough || !(fLadder || ( onground && movingalongground )) )
			return;

//	MoveHelper()->PlayerSetAnimation( PLAYER_WALK );

	bWalking = speed < velrun;

	VectorCopy( vecOrigin, knee );
	VectorCopy( vecOrigin, feet );

	height = GetPlayerMaxs()[ 2 ] - GetPlayerMins()[ 2 ];

	knee[2] = vecOrigin[2] + 0.2 * height;

	// find out what we're stepping in or on...
	if ( fLadder )
	{
		psurface = GetLadderSurface(vecOrigin);
		fvol = 0.5;

		SetStepSoundTime( STEPSOUNDTIME_ON_LADDER, bWalking );
	}
	else if ( enginetrace->GetPointContents( knee ) & MASK_WATER )  // we want to use the knee for Cstrike, not the waist
	{
		static int iSkipStep = 0;

		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			return;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "wade" ) );
		fvol = 0.65;
		SetStepSoundTime( STEPSOUNDTIME_WATER_KNEE, bWalking );
	}
	else if ( GetWaterLevel() == WL_Feet )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "water" ) );
		fvol = bWalking ? 0.2 : 0.5;

		SetStepSoundTime( STEPSOUNDTIME_WATER_FOOT, bWalking );
	}
	else
	{
		if ( !psurface )
			return;

		SetStepSoundTime( STEPSOUNDTIME_NORMAL, bWalking );

		switch ( psurface->game.material )
		{
		default:
		case CHAR_TEX_CONCRETE:						
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_METAL:	
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_DIRT:
			fvol = bWalking ? 0.25 : 0.55;
			break;

		case CHAR_TEX_VENT:	
			fvol = bWalking ? 0.4 : 0.7;
			break;

		case CHAR_TEX_GRATE:
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_TILE:	
			fvol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_SLOSH:
			fvol = bWalking ? 0.2 : 0.5;
			break;
		}
	}
	
	// play the sound
	// 65% volume if ducking
	if ( GetFlags() & FL_DUCKING )
	{
		fvol *= 0.65;
	}

	PlayStepSound( feet, psurface, fvol, false );
}

ConVar sv_max_distance_transmit_footsteps( side_registry(), "sv_max_distance_transmit_footsteps", "1250.0", FCVAR_REPLICATED, "Maximum distance to transmit footstep sound effects." );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
extern ISoundEmitterSystemBase *soundemitterbase;
void CBasePlayer::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool bForce )
{
	ks::ScratchArena<> scratch;
	if ( gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat() )
		return;

	if ( !psurface )
		return;

	int nSide = m_Local.m_nStepside;
	unsigned short stepSoundName = nSide ? psurface->sounds.runStepLeft : psurface->sounds.runStepRight;
	if ( !stepSoundName )
		return;

	m_Local.m_nStepside = !nSide;

	CSoundParameters params;

	contract_assert( nSide == 0 || nSide == 1 );

	if ( m_StepSoundCache[ nSide ].m_usSoundNameIndex == stepSoundName )
	{
		params = m_StepSoundCache[ nSide ].m_SoundParameters;
	}
	else
	{
		IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
		
// footstep sounds
		const char *pRawSoundName = physprops->GetString( stepSoundName );
		const char *pSoundName = nullptr;
		int const nStepCopyLen = V_strlen(pRawSoundName) + 4;
		char *szStep = ( char * ) scratch.AllocBytes( nStepCopyLen );
		if ( GetTeamNumber() == TEAM_CT )
		{
			V_snprintf(szStep, nStepCopyLen, "ct_%s", pRawSoundName);
		}
		else
		{
			V_snprintf(szStep, nStepCopyLen, "t_%s", pRawSoundName);
		}

		pSoundName = szStep;
		if ( !CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
		{
			DevMsg( "Can't find specific footstep sound! (%s) - Using the default instead. (%s)\n", pSoundName, pRawSoundName );
			pSoundName = pRawSoundName;
		}
		if ( !CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
			return;

		// Only cache if there's one option.  Otherwise we'd never here any other sounds
		if ( params.count == 1 )
		{
			m_StepSoundCache[ nSide ].m_usSoundNameIndex = stepSoundName;
			m_StepSoundCache[ nSide ].m_SoundParameters = params;
		}
	}

	CRecipientFilter filter;
	
	if( !bForce )
	{
		filter.AddRecipientsByPAS( vecOrigin );
	}

	// in MP, server removes all players in the vecOrigin's PVS, these players generate the footsteps client side
	if ( gpGlobals->maxClients > 1 && !bForce )
	{
		filter.RemoveRecipientsByPVS( vecOrigin );
	}
	
	if( bForce )
	{
		filter.AddAllPlayers();
	}
	// the client plays it's own sound
	filter.RemoveRecipient( this );

	// Don't transmit footsteps if they are outside maximum footstep transmission range.
	for ( int i = 0; i < filter.GetRecipientCount(); ++i )
	{
		int entIndex = filter.GetRecipientIndex( i );
		IHandleEntity* entity = gEntList.LookupEntityByNetworkIndex( entIndex );
		if ( entity )
		{
			CBasePlayer* player = dynamic_cast<CBasePlayer*>( gEntList.GetBaseEntity( entity->GetRefEHandle() ) );
			if ( player != nullptr )
			{
				float dist = vecOrigin.DistTo( player->EyePosition() );
				if ( dist > sv_max_distance_transmit_footsteps.GetFloat() )
				{
					filter.RemoveRecipient( player );
				}
			}
		}
	}

// #if defined( CLIENT_DLL )
// 	Msg( "CLIENT_DLL: (PlayStepSound) filter recipients = %d\n", filter.GetRecipientCount() );
// #else
// 	Msg( "GAME_DLL: (PlayStepSound) filter recipients = %d\n", filter.GetRecipientCount() );
// #endif

	EmitSound_t ep;
	ep.m_nChannel = CHAN_BODY;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = fvol;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;
	ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
	ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

	EmitSound( filter, entindex(), ep );

	// Step Suit
	if (CCSPlayer *pThisCsPlayer = dynamic_cast<CCSPlayer *>(this))
	{
		if (pThisCsPlayer->IsBot() && pThisCsPlayer->HasHeavyArmor())
		{
			static const char * const k_HeavyStepSoundName = "Heavy.Step";
			static HSOUNDSCRIPTHASH const k_HeavyStepSoundHash = soundemitterbase->HashSoundName( k_HeavyStepSoundName );
			ep.m_pSoundName = k_HeavyStepSoundName;
			ep.m_hSoundScriptHash = k_HeavyStepSoundHash;
			EmitSound(filter, entindex(), ep);
		}
	}
	CSoundParameters paramsSuitSound;
	if (!CBaseEntity::GetParametersForSound((GetTeamNumber() == TEAM_CT) ? "CT_Default.Suit" : "T_Default.Suit", paramsSuitSound, nullptr))
		return;

	EmitSound_t epSuitSound;
	epSuitSound.m_nChannel = CHAN_AUTO;
	epSuitSound.m_pSoundName = paramsSuitSound.soundname;
	epSuitSound.m_flVolume = fvol;
	epSuitSound.m_SoundLevel = paramsSuitSound.soundlevel;
	epSuitSound.m_nFlags = 0;
	epSuitSound.m_nPitch = paramsSuitSound.pitch;
	epSuitSound.m_pOrigin = &vecOrigin;
	epSuitSound.m_hSoundScriptHash = paramsSuitSound.m_hSoundScriptHash;
	epSuitSound.m_nSoundEntryVersion = paramsSuitSound.m_nSoundEntryVersion;

	EmitSound(filter, entindex(), epSuitSound);

}

void CBasePlayer::UpdateButtonState( int nUserCmdButtonMask )
{
	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = m_nButtons;

	// Get button states
	m_nButtons = nUserCmdButtonMask;
 	int buttonsChanged = m_afButtonLast ^ m_nButtons;
	
	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed =  buttonsChanged & m_nButtons;		// The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & (~m_nButtons);	// The ones not down are "released"
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		*velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		*velrun = 80;		
	}
	else
	{
		*velwalk = 90;
		*velrun = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking )
{
	switch ( iStepSoundTime )
	{
	case STEPSOUNDTIME_NORMAL:
	case STEPSOUNDTIME_WATER_FOOT:
		m_flStepSoundTime = bWalking ? 400 : 300;
		break;

	case STEPSOUNDTIME_ON_LADDER:
		m_flStepSoundTime = 200;
		break;

	case STEPSOUNDTIME_WATER_KNEE:
		m_flStepSoundTime = 600;
		break;

	default:
		contract_assert( 0 );
		break;
	}

	m_flStepSoundTime *= sv_footstep_sound_frequency.GetFloat();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		m_flStepSoundTime += 100;
	}
}

Vector CBasePlayer::Weapon_ShootPosition( )
{
	return EyePosition();
}

bool CBasePlayer::Weapon_CanUse( CBaseCombatWeapon *pWeapon )
{
	return true;
}

void CBasePlayer::SetAnimationExtension( const char *pExtension )
{
	V_strncpy( m_szAnimExtension, pExtension, sizeof(m_szAnimExtension) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the weapon to switch to when the player uses the 'lastinv' command
//-----------------------------------------------------------------------------
void CBasePlayer::Weapon_SetLast( CBaseCombatWeapon *pWeapon )
{
	m_hLastWeapon = pWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Override to clear dropped weapon from the hud
//-----------------------------------------------------------------------------
void CBasePlayer::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	bool bWasActiveWeapon = false;
	if ( pWeapon == GetActiveWeapon() )
	{
		bWasActiveWeapon = true;
	}

	if ( pWeapon )
	{
		if ( bWasActiveWeapon )
		{
			pWeapon->SendWeaponAnim( ACT_VM_IDLE );
		}
	}
	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );

	if ( bWasActiveWeapon )
	{
		if (!SwitchToNextBestWeapon( nullptr ))
		{
			CBaseViewModel *vm = GetViewModel();
			if ( vm )
			{
				vm->AddEffects( EF_NODRAW );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override base class so player can reset autoaim
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CBasePlayer::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	CBaseCombatWeapon *pLastWeapon = GetActiveWeapon();

	if ( BaseClass::Weapon_Switch( pWeapon, viewmodelindex ))
	{
		if ( pLastWeapon && Weapon_ShouldSetLast( pLastWeapon, GetActiveWeapon() ) )
		{
			Weapon_SetLast( pLastWeapon->GetLastWeapon() );
		}

		CBaseViewModel *pViewModel = GetViewModel( viewmodelindex );
		contract_assert( pViewModel );
		if ( pViewModel )
			pViewModel->RemoveEffects( EF_NODRAW );
		ResetAutoaim( );
		OnSwitchWeapons( pWeapon );
		return true;
	}
	return false;
}

void CBasePlayer::SelectLastItem(void)
{
	if ( m_hLastWeapon.Get() == nullptr )
		return;

	if ( GetActiveWeapon() && !GetActiveWeapon()->CanHolster() )
		return;

	SelectItem( m_hLastWeapon.Get()->GetClassname(), m_hLastWeapon.Get()->GetSubType() );
}

//-----------------------------------------------------------------------------
// Purpose: Abort any reloads we're in
//-----------------------------------------------------------------------------
void CBasePlayer::AbortReload( void )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->AbortReload();
	}
}

void CBasePlayer::AddToPlayerSimulationList( CBaseEntity *other )
{
	CHandle< CBaseEntity > h;
	h = other;
	// Already in list
	if ( ks::find_index( m_SimulatedByThisPlayer, h ) != -1 )
		return;

	contract_assert( other->IsPlayerSimulated() );

	m_SimulatedByThisPlayer.push_back( h );
}

//-----------------------------------------------------------------------------
// Purpose: Fixme, this should occur if the player fails to drive simulation
//  often enough!!!
// Input  : *other - 
//-----------------------------------------------------------------------------
void CBasePlayer::RemoveFromPlayerSimulationList( CBaseEntity *other )
{
	if ( !other )
		return;

	contract_assert( other->IsPlayerSimulated() );
	contract_assert( other->GetSimulatingPlayer() == this );

	CHandle< CBaseEntity > h;
	h = other;

	ks::erase_first( m_SimulatedByThisPlayer, h );
}

void CBasePlayer::SimulatePlayerSimulatedEntities( void )
{
	int c = std::ssize( m_SimulatedByThisPlayer );
	int i;

	for ( i = c - 1; i >= 0; i-- )
	{
		CHandle< CBaseEntity > h;
		
		h = m_SimulatedByThisPlayer[ i ];
		CBaseEntity *e = h;

		if ( !e || !e->IsPlayerSimulated() )
		{
			m_SimulatedByThisPlayer.erase( m_SimulatedByThisPlayer.begin() + i );
			continue;
		}

		contract_assert( e->IsPlayerSimulated() );
		contract_assert( e->GetSimulatingPlayer() == this );

		e->PhysicsSimulate();
	}

	// Loop through all entities again, checking their untouch if flagged to do so
	c = std::ssize( m_SimulatedByThisPlayer );

	for ( i = c - 1; i >= 0; i-- )
	{
		CHandle< CBaseEntity > h;
		
		h = m_SimulatedByThisPlayer[ i ];

		CBaseEntity *e = h;
		if ( !e || !e->IsPlayerSimulated() )
		{
			m_SimulatedByThisPlayer.erase( m_SimulatedByThisPlayer.begin() + i );
			continue;
		}

		contract_assert( e->IsPlayerSimulated() );
		contract_assert( e->GetSimulatingPlayer() == this );

		if ( !e->GetCheckUntouch() )
			continue;

		e->PhysicsCheckForEntityUntouch();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ClearPlayerSimulationList( void )
{
	int c = std::ssize( m_SimulatedByThisPlayer );
	int i;

	for ( i = c - 1; i >= 0; i-- )
	{
		CHandle< CBaseEntity > h;
		
		h = m_SimulatedByThisPlayer[ i ];
		CBaseEntity *e = h;
		if ( e )
		{
			e->UnsetPlayerSimulated();
		}
	}

	m_SimulatedByThisPlayer.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should allow selection of the specified item
//-----------------------------------------------------------------------------
bool CBasePlayer::Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon )
{
	return ( pWeapon != GetActiveWeapon() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::SelectItem( const char *pstr, int iSubType )
{
	if (!pstr)
		return;

	CBaseCombatWeapon *pItem = Weapon_OwnsThisType( pstr, iSubType );

	if (!pItem)
		return;

	if( GetObserverMode() != OBS_MODE_NONE )
		return;// Observers can't select things.

	if ( !Weapon_ShouldSelectItem( pItem ) )
		return;

	// FIX, this needs to queue them up and delay
	// Make sure the current weapon can be holstered
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return;

		ResetAutoaim( );
	}

	Weapon_Switch( pItem );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConVar sv_debug_player_use( side_registry(), "sv_debug_player_use", "0", FCVAR_REPLICATED, "Visualizes +use logic. Green cross=trace success, Red cross=trace too far, Green box=radius success" );
float IntervalDistance( float x, float x0, float x1 )
{
	// swap so x0 < x1
	if ( x0 > x1 )
	{
		float tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	if ( x < x0 )
		return x0-x;
	else if ( x > x1 )
		return x - x1;
	return 0;
}

CBaseEntity *CBasePlayer::FindUseEntity()
{
	Vector forward, up;
	// NOTE: This doesn't handle the case when the player is in a vehicle.
	AngleVectors( GetFinalAimAngle(), &forward, nullptr, &up );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	useableContents = (MASK_NPCSOLID_BRUSHONLY | MASK_OPAQUE_AND_NPCS) & ~CONTENTS_OPAQUE;

	CBaseEntity *pFoundByTrace = nullptr;

	// UNDONE: Might be faster to just fold this range into the sphere query
	CBaseEntity *pObject = nullptr;

	float nearestDist = std::numeric_limits<float>::max();
	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = nullptr;

	const int NUM_TANGENTS = 8;

	const int NUM_TRACES = 1;
	// trace a box at successive angles down
	//							forward, 45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
	const float tangents[NUM_TANGENTS] = { 0, 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
	for ( int i = 0; i < NUM_TRACES; i++ )
	{
		if ( i == 0 )
		{
			UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
		}
		else
		{
			Vector down = forward - tangents[i]*up;
			VectorNormalize(down);
			UTIL_TraceHull( searchCenter, searchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		}
		pObject = tr.Ent<CBaseEntity>();

		pFoundByTrace = pObject;
		bool bUsable = IsUseableEntity(pObject, 0);
		while ( pObject && !bUsable && pObject->GetMoveParent() )
		{
			pObject = pObject->GetMoveParent();
			bUsable = IsUseableEntity(pObject, 0);
		}

		if ( bUsable )
		{
			Vector delta = tr.endpos - tr.startpos;
			float centerZ = CollisionProp()->WorldSpaceCenter().z;
			delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
			float dist = delta.Length();
			CCSPlayer *pPlayer = dynamic_cast<CCSPlayer*>( pObject );
			if ( (pPlayer && pPlayer->IsBot() && dist < PLAYER_USE_BOT_RADIUS) || dist < PLAYER_USE_RADIUS )
			{

				if ( sv_debug_player_use.GetBool() )
				{
					NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
					NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
				}

				
				if ( sv_debug_player_use.GetBool() )
				{
					Msg( "Trace using: %s\n", pObject ? pObject->GetDebugName() : "no usable entity found" );
				}

				pNearest = pObject;
				
				// if this is directly under the cursor just return it now
				if ( i == 0 )
					return pObject;
			}
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
	}
	if ( pNearest )
	{
		// estimate nearest object by distance from the view vector
		Vector point;
		pNearest->CollisionProp()->CalcNearestPoint( searchCenter, &point );
		nearestDist = CalcDistanceToLine( point, searchCenter, forward );
		if ( sv_debug_player_use.GetBool() )
		{
			Msg("Trace found %s, dist %.2f\n", pNearest->GetClassname(), nearestDist );
		}
	}

	CCSPlayer* pPlayer = ToCSPlayer( this );
	const float MIN_DOT_FOR_WEAPONS = 0.99f;

	for ( CEntitySphereQuery sphere( searchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != nullptr; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( searchCenter, &point );

		float fMinimumDot = pObject->GetUseLookAtAngle();

		CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>( pObject );
		CSWeaponType nWepType = WEAPONTYPE_UNKNOWN;
		if ( pWeapon )
		{
			nWepType = pWeapon->GetWeaponType();

			if ( pPlayer->IsPrimaryOrSecondaryWeapon( nWepType ) )
				fMinimumDot = MIN_DOT_FOR_WEAPONS;
		}

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, forward );

		// Need to be looking at the object more or less
		if ( dot < fMinimumDot )
			continue;

		float dist = CalcDistanceToLine( point, searchCenter, forward );

		if ( sv_debug_player_use.GetBool() )
		{
			Msg("Radius found %s, dist %.2f\n", pObject->GetClassname(), dist );
		}

		if ( dist < nearestDist )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.Ent<CBaseEntity>() == pObject )
			{
				pNearest = pObject;
				nearestDist = dist;
			}
		}
	}


	

	if ( sv_debug_player_use.GetBool() )
	{
		if ( !pNearest )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 255, 0, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 255, 0, 0, true, 30 );
		}
		else if ( pNearest == pFoundByTrace )
		{
			NDebugOverlay::Line( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
			NDebugOverlay::Cross3D( tr.endpos, 16, 0, 255, 0, true, 30 );
		}
		else
		{
			NDebugOverlay::Box( pNearest->WorldSpaceCenter(), Vector(-8, -8, -8), Vector(8, 8, 8), 0, 255, 0, true, 30 );
		}
	}

	if ( sv_debug_player_use.GetBool() )
	{
		Msg( "Radial using: %s\n", pNearest ? pNearest->GetDebugName() : "no usable entity found" );
	}

	return pNearest;
}

//-----------------------------------------------------------------------------
// Purpose: Handles USE keypress
//-----------------------------------------------------------------------------
void CBasePlayer::PlayerUse ( void )
{
	// Was use pressed or released?
	if ( ! ((m_nButtons | m_afButtonPressed | m_afButtonReleased) & IN_USE) )
		return;

	if ( IsObserver() )
	{
		// do special use operation in oberserver mode
		if ( m_afButtonPressed & IN_USE )
			ObserverUse( true );
		else if ( m_afButtonReleased & IN_USE )
			ObserverUse( false );
		
		return;
	}

	// push objects in turbo physics mode
	if ( (m_nButtons & IN_USE) && sv_turbophysics.GetBool() )
	{
		Vector forward, up;
		EyeVectors( &forward, nullptr, &up );

		trace_t tr;
		// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
		Vector searchCenter = EyePosition();

		CUsePushFilter filter;

		UTIL_TraceLine( searchCenter, searchCenter + forward * 96.0f, MASK_SOLID, &filter, &tr );

		// try the hit entity if there is one, or the ground entity if there isn't.
		CBaseEntity *entity = tr.Ent<CBaseEntity>();

		if ( entity )
		{
			IPhysicsObject *pObj = entity->VPhysicsGetObject();

			if ( pObj )
			{
				Vector vPushAway = (entity->WorldSpaceCenter() - WorldSpaceCenter());
				vPushAway.z = 0;

				float flDist = VectorNormalize( vPushAway );
				flDist = std::max<std::common_type_t<float, int>>( flDist, 1 );

				float flForce = sv_pushaway_force.GetFloat() / flDist;
				flForce = std::min( flForce, sv_pushaway_max_force.GetFloat() );

				pObj->ApplyForceOffset( vPushAway * flForce, WorldSpaceCenter() );
			}
		}
	}

	if ( m_afButtonPressed & IN_USE )
	{
		// Controlling some latched entity?
		if ( ClearUseEntity() )
		{
			return;
		}
		else
		{
			if ( m_afPhysicsFlags & PFLAG_DIROVERRIDE )
			{
				m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
			else
			{	// Start controlling the train!
				CBaseEntity *pTrain = GetGroundEntity();
				if ( pTrain && !(m_nButtons & IN_JUMP) && (GetFlags() & FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(this) )
				{
					m_afPhysicsFlags |= PFLAG_DIROVERRIDE;
					m_iTrain = TrainSpeed(pTrain->m_flSpeed, ((CFuncTrackTrain*)pTrain)->GetMaxSpeed());
					m_iTrain |= TRAIN_NEW;
					EmitSound( "Player.UseTrain" );
					return;
				}
			}
		}
	}

	CBaseEntity *pUseEntity = FindUseEntity();

	// in counterstrike 15, we need to allow the buy menu to open more easily
	// The old code defaulted to using whatever you were pointing at.
	// This code first checks to see if you're in a buy zone.  If that's true, 
	// then it ignores any weapon you may be pointing at. (the planted c4 is
	// not a weapon).

	if ( m_afButtonPressed & IN_USE ) 
	{
		CCSPlayer* pPlayer = ToCSPlayer( this );
		CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>( pUseEntity );
		CSWeaponType nWepType = WEAPONTYPE_UNKNOWN;
		if ( pWeapon )
			nWepType = pWeapon->GetWeaponType();

		bool bOpenBuyWithUse = true;
		if ( pPlayer )
		{
			const char *cl_useopensbuymenu = engine->GetClientConVarValue( ENTINDEX( pPlayer->edict() ), "cl_use_opens_buy_menu" );
			if ( cl_useopensbuymenu && std::atoi( cl_useopensbuymenu ) <= 0 )
				bOpenBuyWithUse = false;
		}

		if ( pWeapon && pWeapon->CanBePickedUp() && pPlayer->IsPrimaryOrSecondaryWeapon( nWepType ) )
		{
			bool bPickupIsPrimary = IsPrimaryWeapon( pWeapon->GetCSWeaponID() );
			CBaseCombatWeapon *pPlayerWeapon = nullptr;

			if ( bPickupIsPrimary )
			{
				pPlayerWeapon = pPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE );
			}
			else
			{
				pPlayerWeapon = pPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL );
			}

			// if the player has a weapon in the slot that occupies the weapon that they'd like to pick up
			// AND they are able to drop it, pick up the new weapon
			// OR if they don't have a weapon in that slot, go ahead and pick up the new weapon
			if ( (pPlayerWeapon && pPlayer->HandleDropWeapon( pPlayerWeapon, true )) || !pPlayerWeapon )
			{	
				pWeapon->Touch( this );
			}		
		}
		else if ( pPlayer && bOpenBuyWithUse && pPlayer->IsInBuyZone() && pPlayer->CanPlayerBuy( false ) && !CSGameRules()->IsPlayingGunGame() )
		{
			bool bItemIsNullOrWeapon = true;

			if ( pUseEntity )
			{
				bItemIsNullOrWeapon = ( pWeapon != 0 );
			}

			if ( bItemIsNullOrWeapon )
			{
				engine->ClientCommand( edict(), "buymenu\n" );
				return;
			}
		}
		else if( !pUseEntity && pPlayer && pPlayer->HasC4() && pPlayer->GetActiveCSWeapon() )
		{
			if( pPlayer->m_bInBombZone && !( pPlayer->GetActiveCSWeapon()->IsA( WEAPON_C4 ) ) )
			{
				// we're in a bomb zone with C4, but it's not equipped.  Equip it.
				CWeaponCSBase	*pC4Weapon = nullptr;

				//Search for the c4 weapon to use
				for ( int i = 0; i < pPlayer->WeaponCount(); i++ )
				{
					CBaseCombatWeapon* pWeaponBase = pPlayer->GetWeapon(i);
					CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* > ( pWeaponBase );

					if ( pWeapon == nullptr )
					{
						continue;
					}

					if ( !pWeapon->IsA( WEAPON_C4 ) )
					{
						continue;
					}

					// Must be eligible for switching to.
					if ( !pPlayer->Weapon_CanSwitchTo( pWeapon ) )
					{
						continue;
					}

					pC4Weapon = pWeapon;
				}

				if ( pC4Weapon != nullptr )
				{
					//pPlayer->SetLastWeaponBeforeAutoSwitchToC4( pPlayer->GetActiveCSWeapon() );
					pPlayer->Weapon_Switch(pC4Weapon);

					static_cast<CC4*>(pC4Weapon)->m_bIsPlantingViaUse = true;
				}
			}
			else if( pPlayer->GetActiveCSWeapon()->IsA( WEAPON_C4 ) )
			{
				static_cast<CC4*>(pPlayer->GetActiveWeapon())->m_bIsPlantingViaUse = true;
			}
		}
		else if ( pUseEntity && pUseEntity->IsPlayer() )
		{
			// Bots can give their C4 to the requesting human.

			CCSPlayer* pPlayer = ToCSPlayer( this );
			CCSPlayer* pBot = ToCSPlayer( pUseEntity );

			if ( pPlayer && pPlayer->IsAlive() && pBot && pBot->IsBot() && pBot->IsOtherSameTeam(pPlayer->GetTeamNumber()) && pBot->HasC4() )
			{
			
				//distance check is implicit in +use, but check it anyway
				if ( (pBot->WorldSpaceCenter() - pPlayer->WorldSpaceCenter()).Length() < 200 )
				{
					CBaseCombatWeapon *pC4 = pBot->Weapon_OwnsThisType( "weapon_c4" );
					if ( pC4 )
					{
						pBot->SetBombDroppedTime( gpGlobals->curtime );
						pBot->CSWeaponDrop( pC4, WorldSpaceCenter(), false );
						pBot->Radio( "Radio.YouTakeThePoint",   "#Cstrike_TitlesTXT_Game_afk_bomb_drop" );
					}
				}
				
			}

		}
	}

	if ( pUseEntity )
	{
		//!!!UNDONE: traceline here to prevent +USEing buttons through walls			

		int caps = pUseEntity->ObjectCaps();
		variant_t emptyVariant;
		if ( ( (m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) || ( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
			{
				m_afPhysicsFlags |= PFLAG_USING;
			}

			if ( pUseEntity->ObjectCaps() & FCAP_ONOFF_USE )
			{
				pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_ON );
			}
			else
			{
				pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );
			}
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ( (m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
		{
			pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_OFF );
		}
	}
}

ConVar	sv_suppress_viewpunch( side_registry(), "sv_suppress_viewpunch", "0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ViewPunch( const QAngle &angleOffset )
{
	//See if we're suppressing the view punching
	if ( sv_suppress_viewpunch.GetBool() )
		return;

	// We don't allow view kicks in the vehicle
	

	m_Local.m_viewPunchAngle += angleOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ViewPunchReset( float tolerance )
{
	if ( tolerance != 0 )
	{
		tolerance *= tolerance;	// square
		float check = m_Local.m_viewPunchAngle->LengthSqr();
		if ( check > tolerance )
			return;
	}
	m_Local.m_viewPunchAngle = vec3_angle;
}

static ConVar smoothstairs( side_registry(), "smoothstairs", "1", FCVAR_REPLICATED, "Smooth player eye z coordinate when traversing stairs." );

//-----------------------------------------------------------------------------
// Handle view smoothing when going up or down stairs
//-----------------------------------------------------------------------------
void CBasePlayer::SmoothViewOnStairs( Vector& eyeOrigin )
{
	CBaseEntity *pGroundEntity = GetGroundEntity();
	float flCurrentPlayerZ = GetLocalOrigin().z;
	float flCurrentPlayerViewOffsetZ = GetViewOffset().z;

	// Smooth out stair step ups
	// NOTE: Don't want to do this when the ground entity is moving the player
	if ( ( pGroundEntity != nullptr && pGroundEntity->GetMoveType() == MOVETYPE_NONE ) && ( flCurrentPlayerZ != m_flOldPlayerZ ) && smoothstairs.GetBool() &&
		 m_flOldPlayerViewOffsetZ == flCurrentPlayerViewOffsetZ )
	{
		int dir = ( flCurrentPlayerZ > m_flOldPlayerZ ) ? 1 : -1;

		float steptime = gpGlobals->frametime;
		if (steptime < 0)
		{
			steptime = 0;
		}

		m_flOldPlayerZ += steptime * 150 * dir;

		const float stepSize = 18.0f;

		if ( dir > 0 )
		{
			if (m_flOldPlayerZ > flCurrentPlayerZ)
			{
				m_flOldPlayerZ = flCurrentPlayerZ;
			}
			if (flCurrentPlayerZ - m_flOldPlayerZ > stepSize)
			{
				m_flOldPlayerZ = flCurrentPlayerZ - stepSize;
			}
		}
		else
		{
			if (m_flOldPlayerZ < flCurrentPlayerZ)
			{
				m_flOldPlayerZ = flCurrentPlayerZ;
			}
			if (flCurrentPlayerZ - m_flOldPlayerZ < -stepSize)
			{
				m_flOldPlayerZ = flCurrentPlayerZ + stepSize;
			}
		}

		eyeOrigin[2] += m_flOldPlayerZ - flCurrentPlayerZ;
	}
	else
	{
		m_flOldPlayerZ = flCurrentPlayerZ;
		m_flOldPlayerViewOffsetZ = flCurrentPlayerViewOffsetZ;
	}
}

static bool IsWaterContents( int contents )
{
	if ( contents & MASK_WATER )
		return true;

//	if ( contents & CONTENTS_TESTFOGVOLUME )
//		return true;

	return false;
}

void CBasePlayer::ResetObserverMode()
{
	m_hObserverTarget.Set( 0 );
	m_iObserverMode = (int)OBS_MODE_NONE;

	m_iObserverLastMode = OBS_MODE_ROAMING;
	m_bForcedObserverMode = false;
	m_afPhysicsFlags &= ~PFLAG_OBSERVER;
	m_bIsSpecLerping = false;
	m_vecSpecLerpIdealPos = Vector( 0, 0, 0 );
	m_angSpecLerpIdealAng = QAngle( 0, 0, 0 );
	m_vecSpecLerpOldPos = Vector( 0, 0, 0 );
	m_angSpecLerpOldAng = QAngle( 0, 0, 0 );
	m_flSpecLerpEndTime = 0.0f;
	m_flSpecLerpTime = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eyeOrigin - 
//			eyeAngles - 
//			zNear - 
//			zFar - 
//			fov - 
//-----------------------------------------------------------------------------
extern Vector g_cameraFollowPos;
void CBasePlayer::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	if ( IsObserver() )
	{
		CalcObserverView( eyeOrigin, eyeAngles, fov );
	}
	else
	{
		CalcPlayerView( eyeOrigin, eyeAngles, fov );
	}

}

void CBasePlayer::CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles)
{
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;
	
		vm->CalcViewModelView( this, eyeOrigin, eyeAngles );
	}
}

void CBasePlayer::CalcPlayerView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{

	// TrackIR
	if ( IsHeadTrackingEnabled() )
	{
		VectorCopy( EyePosition() + GetEyeOffset(), eyeOrigin );
	}
	else
	{
		VectorCopy( EyePosition(), eyeOrigin );
	}
	
	VectorCopy( EyeAngles(), eyeAngles );

	{
		SmoothViewOnStairs( eyeOrigin );
	}

	// Snack off the origin before bob + water offset are applied
	Vector vecBaseEyePosition = eyeOrigin;
	QAngle baseEyeAngles = eyeAngles;

	CalcViewBob( eyeOrigin );
	CalcViewRoll( eyeAngles );

	CalcAddViewmodelCameraAnimation( eyeOrigin, eyeAngles );

	// Apply punch angles
	VectorAdd( eyeAngles, m_Local.m_viewPunchAngle, eyeAngles );

	// TODO[pmf]: apply a scaling factor to this
	VectorAdd( eyeAngles, GetAimPunchAngle() * view_recoil_tracking.GetFloat(), eyeAngles );

	// calc current FOV
	fov = GetFOV();
}

//-----------------------------------------------------------------------------
// Purpose: The main view setup function for vehicles


void CBasePlayer::CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	// on server just copy target postions, final view positions will be calculated on client
	VectorCopy( EyePosition(), eyeOrigin );
	VectorCopy( EyeAngles(), eyeAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Compute roll angle for a particular lateral velocity
// Input  : angles - 
//			velocity - 
//			rollangle - 
//			rollspeed - 
// Output : float CViewRender::CalcRoll
//-----------------------------------------------------------------------------
float CBasePlayer::CalcRoll (const QAngle& angles, const Vector& velocity, float rollangle, float rollspeed)
{
    float   sign;
    float   side;
    float   value;
	
	Vector  forward, right, up;
	
    AngleVectors (angles, &forward, &right, &up);
	
	// Get amount of lateral movement
    side = DotProduct( velocity, right );
	// Right or left side?
    sign = side < 0 ? -1 : 1;
    side = fabs(side);
    
	value = rollangle;
	// Hit 100% of rollangle at rollspeed.  Below that get linear approx.
    if ( side < rollspeed )
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}

	// Scale by right/left sign
    return side*sign;
}

//-----------------------------------------------------------------------------
// Purpose: Allow the viewmodel to layer in artist-authored additive camera animation (to make some first-person anims 'punchier')
//-----------------------------------------------------------------------------
static constexpr auto CAM_DRIVER_RETURN_TO_NORMAL = 0.25f;
static constexpr auto CAM_DRIVER_RETURN_TO_NORMAL_GAIN = 0.8f;
void CBasePlayer::CalcAddViewmodelCameraAnimation( Vector& eyeOrigin, QAngle& eyeAngles )
{
}

//-----------------------------------------------------------------------------
// Purpose: Determine view roll, including data kick
//-----------------------------------------------------------------------------
void CBasePlayer::CalcViewRoll( QAngle& eyeAngles )
{
	if ( GetMoveType() == MOVETYPE_NOCLIP )
		return;

	float side = CalcRoll( GetAbsAngles(), GetAbsVelocity(), sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
	eyeAngles[ROLL] += side;
}

void CBasePlayer::CalcViewBob( Vector& eyeOrigin )
{
}

void CBasePlayer::DoMuzzleFlash()
{
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;

		vm->DoMuzzleFlash();
	}

	BaseClass::DoMuzzleFlash();
}

float CBasePlayer::GetFOVDistanceAdjustFactor()
{
	float defaultFOV	= (float)GetDefaultFOV();
	float localFOV		= (float)GetFOV();

	if ( localFOV == defaultFOV || defaultFOV < 0.001f )
	{
		return 1.0f;
	}

	// If FOV is lower, then we're "zoomed" in and this will give a factor < 1 so apparent LOD distances can be
	//  shorted accordingly
	return localFOV / defaultFOV;

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecTracerSrc - 
//			&tr - 
//			iTracerType - 
//-----------------------------------------------------------------------------
void CBasePlayer::MakeTracer_Impl( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->MakeTracer( vecTracerSrc, tr, iTracerType );
		return;
	}

	BaseClass::MakeTracer_Impl( vecTracerSrc, tr, iTracerType );
}

void CBasePlayer::SharedSpawn()
{
	SetMoveType( MOVETYPE_WALK );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetFriction( 1.0f );

	pl.deadflag	= false;
	m_lifeState	= LIFE_ALIVE;
	m_iHealth = 100;
	m_takedamage		= DAMAGE_YES;

	m_Local.m_bDrawViewmodel = true;
	m_Local.m_flStepSize = sv_stepsize.GetFloat();
	m_Local.m_bAllowAutoMovement = true;

	SetRenderFX( kRenderFxNone );
	m_flNextAttack	= gpGlobals->curtime;
	m_flMaxspeed		= 0.0f;

	MDLCACHE_CRITICAL_SECTION();
	int iIdleSequence = SelectWeightedSequence( ACT_IDLE );
	if( iIdleSequence < 0 )
	{
		iIdleSequence = 0;
	}
	SetSequence( iIdleSequence );

	if ( GetFlags() & FL_DUCKING ) 
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		SetCollisionBounds( VEC_HULL_MIN, VEC_HULL_MAX );

	// dont let uninitialized value here hurt the player
	m_Local.m_flFallVelocity = 0;

	SetBloodColor( BLOOD_COLOR_RED );

	m_hUseEntity = nullptr;

	m_flDuckAmount = 0;
	m_flDuckSpeed = CS_PLAYER_DUCK_SPEED_IDEAL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBasePlayer::GetDefaultFOV( void ) const
{

	int iFOV = ( m_iDefaultFOV == 0 ) ? g_pGameRules->DefaultFOV() : m_iDefaultFOV;

	return iFOV;
}

void CBasePlayer::AvoidPhysicsProps( CUserCmd *pCmd )
{
	// Don't avoid if noclipping or in movetype none
	switch ( GetMoveType() )
	{
	case MOVETYPE_NOCLIP:
	case MOVETYPE_NONE:
	case MOVETYPE_OBSERVER:
		return;
	default:
		break;
	}

	if ( GetObserverMode() != OBS_MODE_NONE || !IsAlive() )
		return;

	AvoidPushawayProps( this, pCmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBasePlayer::GetTracerType_Impl( void )
{
	if ( GetActiveWeapon() )
	{
		return GetActiveWeapon()->GetTracerType();
	}

	return BaseClass::GetTracerType_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::ClearZoomOwner( void )
{
	m_hZoomOwner = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the FOV of the client, doing interpolation between old and new if requested
// Input  : FOV - New FOV
//			zoomRate - Amount of time (in seconds) to move between old and new FOV
//-----------------------------------------------------------------------------
bool CBasePlayer::SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate, int iZoomStart /* = 0 */ )
{
	//NOTENOTE: You MUST specify who is requesting the zoom change
	contract_assert( pRequester != nullptr );
	if ( pRequester == nullptr )
		return false;

	// If we already have an owner, we only allow requests from that owner
	if ( ( m_hZoomOwner.Get() != nullptr ) && ( m_hZoomOwner.Get() != pRequester ) )
	{
		if ( CanOverrideEnvZoomOwner( m_hZoomOwner.Get() ) == false )
			return false;
	}
	else
	{
		//FIXME: Maybe do this is as an accessor instead
		if ( FOV == 0 )
		{
			m_hZoomOwner = nullptr;
		}
		else
		{
			m_hZoomOwner = pRequester;
		}
	}

	// Setup our FOV and our scaling time

	if ( iZoomStart > 0 )
	{
		m_iFOVStart = iZoomStart;
	}
	else
	{
		m_iFOVStart = GetFOV();
	}

	m_flFOVTime = gpGlobals->curtime;
	m_iFOV = FOV;

	m_Local.m_flFOVRate	= zoomRate;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayer::UpdateUnderwaterState( void )
{
	if ( GetWaterLevel() == WL_Eyes )
	{
		if ( IsPlayerUnderwater() == false )
		{
			SetPlayerUnderwater( true );
		}
		return;
	}

	if ( IsPlayerUnderwater() )
	{
		SetPlayerUnderwater( false );
	}

	if ( GetWaterLevel() == WL_NotInWater )
	{
		if ( GetFlags() & FL_INWATER )
		{
			if ( m_iHealth > 0 && IsAlive() )
			{
				if ( GetAbsVelocity().Length() >= 135 )
				{
					EmitSound( "Player.Wade" );
				}				
			}
			RemoveFlag( FL_INWATER );
		}
	}
	else if ( !(GetFlags() & FL_INWATER) )
	{
		// player enter water sound
		if (GetWaterType() == CONTENTS_WATER)
		{
			if ( GetAbsVelocity().Length() >= 135 )
			{
				EmitSound( "Player.Wade" );
			}	
		}

		AddFlag( FL_INWATER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
// ensure that for every emitsound there is a matching stopsound
//-----------------------------------------------------------------------------
void CBasePlayer::SetPlayerUnderwater( bool state )
{
	if ( m_bPlayerUnderwater != state )
	{
		m_bPlayerUnderwater = state;

	}
}

void CBasePlayer::SetPreviouslyPredictedOrigin( const Vector &vecAbsOrigin )
{
	m_vecPreviouslyPredictedOrigin = vecAbsOrigin;
}

const Vector &CBasePlayer::GetPreviouslyPredictedOrigin() const
{
	return m_vecPreviouslyPredictedOrigin;
}

bool fogparams_t::operator !=( const fogparams_t& other ) const
{
	if ( this->enable != other.enable ||
		this->blend != other.blend ||
		!VectorsAreEqual(this->dirPrimary, other.dirPrimary, 0.01f ) || 
		this->colorPrimary.Get() != other.colorPrimary.Get() ||
		this->colorSecondary.Get() != other.colorSecondary.Get() ||
		this->start != other.start ||
		this->end != other.end ||
		this->farz != other.farz ||
		this->maxdensity != other.maxdensity ||
		this->colorPrimaryLerpTo.Get() != other.colorPrimaryLerpTo.Get() ||
		this->colorSecondaryLerpTo.Get() != other.colorSecondaryLerpTo.Get() ||
		this->startLerpTo != other.startLerpTo ||
		this->endLerpTo != other.endLerpTo ||
		this->maxdensityLerpTo != other.maxdensityLerpTo ||
		this->lerptime != other.lerptime ||
		this->duration != other.duration ||
		this->HDRColorScale != other.HDRColorScale ||
		this->ZoomFogScale != other.ZoomFogScale)
		return true;

	return false;
}

void CBasePlayer::IncrementEFNoInterpParity()
{
	// Only matters in multiplayer
	if ( gpGlobals->maxClients == 1 )
		return;
	m_ubEFNoInterpParity = (m_ubEFNoInterpParity + 1) % NOINTERP_PARITY_MAX;
}

int CBasePlayer::GetEFNoInterpParity() const
{
	return (int)m_ubEFNoInterpParity;
}

void CBasePlayer::AddSplitScreenPlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	if ( ks::find_index( m_hSplitScreenPlayers, h ) == -1 )
	{
		m_hSplitScreenPlayers.push_back( h );
	}

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

void CBasePlayer::RemoveSplitScreenPlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	ks::erase_first( m_hSplitScreenPlayers, h );

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

std::vector< CHandle< CBasePlayer > > &CBasePlayer::GetSplitScreenPlayers()
{
	return m_hSplitScreenPlayers;
}

bool CBasePlayer::HasAttachedSplitScreenPlayers() const
{
	return ( !m_hSplitScreenPlayers.empty() );
}

void CBasePlayer::AddPictureInPicturePlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	if ( ks::find_index( m_hPipPlayers, h ) == -1 )
	{
		m_hPipPlayers.push_back( h );
	}

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

void CBasePlayer::RemovePictureInPicturePlayer( CBasePlayer *pOther )
{
	CHandle< CBasePlayer > h;
	h = pOther;
	ks::erase_first( m_hPipPlayers, h );

	UpdateSplitScreenAndPictureInPicturePlayerList();
}

std::vector< CHandle< CBasePlayer > > &CBasePlayer::GetSplitScreenAndPictureInPicturePlayers()
{
	return m_hSplitScreenAndPipPlayers;
}

std::vector< CHandle< CBasePlayer > >& CBasePlayer::GetPictureInPicturePlayers( void )
{
	return m_hPipPlayers;
}

void CBasePlayer::UpdateSplitScreenAndPictureInPicturePlayerList()
{
	// Make m_hSplitScreenAndPipPlayers the union of m_hSplitScreenPlayers and m_hPipPlayers
	m_hSplitScreenAndPipPlayers.clear();
	m_hSplitScreenAndPipPlayers.append_range( m_hSplitScreenPlayers );

	for ( int i = 0; i < std::ssize( m_hPipPlayers ); i++ )
	{
		CHandle< CBasePlayer > h = m_hPipPlayers[i];
		if ( ks::find_index( m_hSplitScreenAndPipPlayers, h ) == -1 )
		{
			m_hSplitScreenAndPipPlayers.push_back( h );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Strips off IN_xxx flags from the player's input
//-----------------------------------------------------------------------------
void CBasePlayer::ForceButtons( int nButtons )
{
	m_afButtonForced |= nButtons;
}

//-----------------------------------------------------------------------------
// Purpose: Re-enables stripped IN_xxx flags to the player's input
//-----------------------------------------------------------------------------
void CBasePlayer::UnforceButtons( int nButtons )
{
	m_afButtonForced &= ~nButtons;
}

CBaseEntity* CBasePlayer::GetSoundscapeListener()
{
	return this;
}

void CBasePlayer::SetUseEntity( CBaseEntity *pUseEntity ) 
{ 
	m_hUseEntity = pUseEntity; 
}

bool CBasePlayer::ClearUseEntity()
{
	if ( m_hUseEntity != nullptr )
	{

		if( !m_bDropEnabled )
		{
			return false;
		}

		// Stop controlling the train/object
		// TODO: Send HUD Update
		GetUseEntity()->Use( this, this, USE_OFF, 0 );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBasePlayer::CanPickupObject( CBaseEntity *pObject, float massLimit, float sizeLimit )
{
	// UNDONE: Make this virtual and move to HL2 player
	return false;
}

float CBasePlayer::GetHeldObjectMass( IPhysicsObject *pHeldObject )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlayer::VPhysicsShadowUpdate_Impl( IPhysicsObject *pPhysics )
{
	if ( sv_turbophysics.GetBool() )
		return;

	Vector newPosition;

	bool physicsUpdated = m_pPhysicsController->GetShadowPosition( &newPosition, nullptr ) > 0 ? true : false;

	// UNDONE: If the player is penetrating, but the player's game collisions are not stuck, teleport the physics shadow to the game position
	if ( pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING )
	{
		std::vector< CBaseEntity * > list;
		PhysGetListOfPenetratingEntities( this, list );
		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			// filter out anything that isn't simulated by vphysics
			// UNDONE: Filter out motion disabled objects?
			if ( list[i]->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				// I'm currently stuck inside a moving object, so allow vphysics to 
				// apply velocity to the player in order to separate these objects
				m_bTouchedPhysObject = true;
			}
		}
	}

	bool bCheckStuck = false;
	if ( m_afPhysicsFlags & PFLAG_GAMEPHYSICS_ROTPUSH )
	{
		bCheckStuck = true;
		m_afPhysicsFlags &= ~PFLAG_GAMEPHYSICS_ROTPUSH;
	}
	uint32 nContactState = m_pPhysicsController->GetContactState( FVPHYSICS_PUSH_PLAYER );
	if ( (nContactState & PLAYER_CONTACT_PHYSICS) || (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER) )
	{
		m_bTouchedPhysObject = true;
	}

	if ( IsFollowingPhysics() )
	{
		m_bTouchedPhysObject = true;
	}

	if ( GetMoveType() == MOVETYPE_NOCLIP || pl.deadflag )
	{
		m_oldOrigin = GetAbsOrigin();
		return;
	}

	if ( phys_timescale.GetFloat() == 0.0f )
	{
		physicsUpdated = false;
	}

	if ( !physicsUpdated )
		return;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	Vector newVelocity;
	pPhysics->GetPosition( &newPosition, 0 );
	m_pPhysicsController->GetShadowVelocity( &newVelocity );
	
	// assume vphysics gave us back a position without penetration
	Vector lastValidPosition = newPosition;

	if ( physicsshadowupdate_render.GetBool() )
	{
		NDebugOverlay::Box( GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 24, 15.0f );
		NDebugOverlay::Box( newPosition, WorldAlignMins(), WorldAlignMaxs(), 0,0,255, 24, 15.0f);
		//	NDebugOverlay::Box( newPosition, WorldAlignMins(), WorldAlignMaxs(), 0,0,255, 24, .01f);
	}

	Vector tmp = GetAbsOrigin() - newPosition;
	if ( !m_bTouchedPhysObject && !(GetFlags() & FL_ONGROUND) )
	{
		tmp.z *= 0.5f;	// don't care about z delta as much
	}

	float dist = tmp.LengthSqr();
	float deltaV = (newVelocity - GetAbsVelocity()).LengthSqr();

	float maxDistErrorSqr = VPHYS_MAX_DISTSQR;
	float maxVelErrorSqr = VPHYS_MAX_VELSQR;
	if ( IsRideablePhysics(pPhysGround) )
	{
		maxDistErrorSqr *= 0.25;
		maxVelErrorSqr *= 0.25;
	}

	// player's physics was frozen, try moving to the game's simulated position if possible
	if ( m_pPhysicsController->WasFrozen() )
	{
		m_bPhysicsWasFrozen = true;
		// check my position (physics object could have simulated into my position
		// physics is not very far away, check my position
		trace_t trace;
		UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( !trace.startsolid )
			return;

		// The physics shadow position is probably not in solid, try to move from there to the desired position
		UTIL_TraceEntity( this, newPosition, GetAbsOrigin(), PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( !trace.startsolid )
		{
			// found a valid position between the two?  take it.
			SetAbsOrigin( trace.endpos );
			UpdateVPhysicsPosition(trace.endpos, vec3_origin, 0);
			return;
		}

	}
	if ( dist >= maxDistErrorSqr || deltaV >= maxVelErrorSqr || (pPhysGround && !m_bTouchedPhysObject) )
	{
		if ( m_bTouchedPhysObject || pPhysGround )
		{
			// BUGBUG: Rewrite this code using fixed timestep
			if ( deltaV >= maxVelErrorSqr && !m_bPhysicsWasFrozen )
			{
				if ( !IsRideablePhysics(pPhysGround) )
				{
					Vector dir = GetAbsVelocity();
					float len = VectorNormalize(dir);
					float dot = DotProduct( newVelocity, dir );
					if ( dot > len )
					{
						dot = len;
					}
					else if ( dot < -len )
					{
						dot = -len;
					}

					VectorMA( newVelocity, -dot, dir, newVelocity );

					if ( m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER )
					{
						float val = Lerp( 0.1f, len, dot );
						VectorMA( newVelocity, val - len, dir, newVelocity );
					}

					if ( !(m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER ) && IsSimulatingOnAlternateTicks() )
					{
						newVelocity *= 0.5f;
					}
	
					ApplyAbsVelocityImpulse( newVelocity );
				}
				else
				{
					Vector forward, right, up;

					AngleVectors (g_pMoveData->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

					// Copy movement amounts
					float fmove = g_pMoveData->m_flForwardMove;
					float smove = g_pMoveData->m_flSideMove;

					// Keep movement vectors in our plane of movement
					forward -= Vector(0,0,-1) * DotProduct( forward, Vector(0,0,-1) );
					VectorNormalize( forward );

					right -= Vector(0,0,-1) * DotProduct( right, Vector(0,0,-1) );
					VectorNormalize( right );

					// Determine velocity
					Vector wishvel = fmove * forward + smove * right;

					// HACK!:  This code let's the player walk around with a little more control when on a func_physbox.
					//		   They still slide around more than physical objects, but at least you can actually
					//		   control yourself.
					// Trying to walk around
					if( wishvel.Length2DSqr() )
					{
						Vector vVel = -newVelocity;
						vVel += GetAbsVelocity() + wishvel;

						// Clamp or else this can get wildy huge and I don't know why
						for( int i=0; i<3; ++i )
						{
							vVel[i] = clamp( vVel[i], newVelocity[i] - 180, newVelocity[i] + 180 );
						}

						SetAbsVelocity( vVel );
					}

				}
			}

			trace_t trace;
			UTIL_TraceEntity( this, newPosition, newPosition, PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( !trace.allsolid && !trace.startsolid )
			{
				SetAbsOrigin( newPosition );
			}
		}
		else
		{
			{
				bCheckStuck = true;
			}
		}
	}
	else
	{
		if ( m_bTouchedPhysObject || (nContactState & PLAYER_CONTACT_GAMEOBJECT) )
		{
			// check my position (physics object could have simulated into my position
			// physics is not very far away, check my position
			trace_t trace;
			UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(),
				PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

			// is current position ok?
			if ( trace.allsolid || trace.startsolid )
			{
				// no use the final stuck check to move back to old if this stuck fix didn't work
				bCheckStuck = true;
				lastValidPosition = m_oldOrigin;
				SetAbsOrigin( newPosition );
			}
		}
	}

	if ( bCheckStuck )
	{
		trace_t trace;
		UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		// current position is not ok, fixup
		if ( trace.allsolid || trace.startsolid )
		{
			// STUCK!?!?!
			//Warning( "Checkstuck failed.  Stuck on %s!!\n", trace.Ent<CBaseEntity>()->GetClassname() );
			SetAbsOrigin( lastValidPosition );
		}
	}
	m_oldOrigin = GetAbsOrigin();
	m_bPhysicsWasFrozen = false;

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlayer::PostThinkVPhysics( void )
{
	// Check to see if things are initialized!
	if ( !m_pPhysicsController )
	{
		return;
	}

	Vector newPosition = GetAbsOrigin();
	float frametime = gpGlobals->frametime;
	if ( frametime <= 0 || frametime > 0.1f )
		frametime = 0.1f;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	if ( !pPhysGround && m_bTouchedPhysObject && g_pMoveData->m_outStepHeight <= 0.f && (GetFlags() & FL_ONGROUND) )
	{
		newPosition = m_oldOrigin + frametime * g_pMoveData->m_outWishVel;
		newPosition = (GetAbsOrigin() * 0.5f) + (newPosition * 0.5f);
	}

	int collisionState = VPHYS_WALK;
	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
	{
		collisionState = VPHYS_NOCLIP;
	}
	else if ( GetFlags() & FL_DUCKING )
	{
		collisionState = VPHYS_CROUCH;
	}

	if ( collisionState != m_vphysicsCollisionState )
	{
		SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), collisionState );
	}

	if ( !(TouchedPhysics() || pPhysGround) )
	{
		float maxSpeed = m_flMaxspeed > 0.0f ? m_flMaxspeed : sv_maxspeed.GetFloat();
		g_pMoveData->m_outWishVel.Init( maxSpeed, maxSpeed, maxSpeed );
	}

	// teleport the physics object up by stepheight (game code does this - reflect in the physics)
	if ( g_pMoveData->m_outStepHeight > 0.1f )
	{
		if ( g_pMoveData->m_outStepHeight > 4.0f )
		{
			VPhysicsGetObject()->SetPosition( GetAbsOrigin(), vec3_angle, true );
		}
		else
		{
			// don't ever teleport into solid
			Vector position, end;
			VPhysicsGetObject()->GetPosition( &position, nullptr );
			end = position;
			end.z += g_pMoveData->m_outStepHeight;
			trace_t trace;
			UTIL_TraceEntity( this, position, end, PhysicsSolidMaskForEntity(), this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
			if ( trace.DidHit() )
			{
				g_pMoveData->m_outStepHeight = trace.endpos.z - position.z;
			}
			m_pPhysicsController->StepUp( g_pMoveData->m_outStepHeight );
		}
		m_pPhysicsController->Jump();
	}
	g_pMoveData->m_outStepHeight = 0.0f;

	// Store these off because after running the usercmds, it'll pass them
	// to UpdateVPhysicsPosition.	
	m_vNewVPhysicsPosition = newPosition;
	m_vNewVPhysicsVelocity = g_pMoveData->m_outWishVel;

	m_oldOrigin = GetAbsOrigin();
}

IPhysicsObject *CBasePlayer::GetGroundVPhysics()
{
	CBaseEntity *pGroundEntity = GetGroundEntity();
	if ( pGroundEntity && pGroundEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhysGround = pGroundEntity->VPhysicsGetObject();
		if ( pPhysGround && pPhysGround->IsMoveable() )
			return pPhysGround;
	}
	return nullptr;
}

// UNDONE: Look and see if the ground entity is in hierarchy with a MOVETYPE_VPHYSICS?
// Behavior in that case is not as good currently when the parent is rideable
bool CBasePlayer::IsRideablePhysics( IPhysicsObject *pPhysics )
{
	if ( pPhysics )
	{
		if ( pPhysics->GetMass() > (VPhysicsGetObject()->GetMass()*2) )
			return true;
	}

	return false;
}

void CBasePlayer::UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival )
{
	if( !m_pPhysicsController )
		return;

	bool onground = (GetFlags() & FL_ONGROUND) ? true : false;
	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	// if the object is much heavier than the player, treat it as a local coordinate system
	// the player controller will solve movement differently in this case.
	if ( !IsRideablePhysics(pPhysGround) )
	{
		pPhysGround = nullptr;
	}

	m_pPhysicsController->Update( position, velocity, secondsToArrival, onground, pPhysGround );
}

// used by the physics gun and game physics... is there a better interface?
void CBasePlayer::SetPhysicsFlag( int nFlag, bool bSet )
{
	if (bSet)
		m_afPhysicsFlags |= nFlag;
	else
		m_afPhysicsFlags &= ~nFlag;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBasePlayer::SetVCollisionState( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, int collisionState )
{
	m_vphysicsCollisionState = collisionState;
	if( m_pShadowStand == nullptr )
		return;

	switch( collisionState )
	{
	case VPHYS_WALK:
		m_pShadowStand->SetPosition( vecAbsOrigin, vec3_angle, true );
		m_pShadowStand->SetVelocity( &vecAbsVelocity, nullptr );
		m_pShadowCrouch->EnableCollisions( false );
		m_pPhysicsController->SetObject( m_pShadowStand );
		VPhysicsSwapObject( m_pShadowStand );
		m_pShadowStand->EnableCollisions( true );
		break;

	case VPHYS_CROUCH:
		m_pShadowCrouch->SetPosition( vecAbsOrigin, vec3_angle, true );
		m_pShadowCrouch->SetVelocity( &vecAbsVelocity, nullptr );
		m_pShadowStand->EnableCollisions( false );
		m_pPhysicsController->SetObject( m_pShadowCrouch );
		VPhysicsSwapObject( m_pShadowCrouch );
		m_pShadowCrouch->EnableCollisions( true );
		break;

	case VPHYS_NOCLIP:
		m_pShadowCrouch->EnableCollisions( false );
		m_pShadowStand->EnableCollisions( false );
		break;
	}
}

void CBasePlayer::UpdatePhysicsShadowToCurrentPosition()
{
	UpdateVPhysicsPosition( GetAbsOrigin(), vec3_origin, gpGlobals->frametime );
}

void CBasePlayer::UpdatePhysicsShadowToPosition( const Vector &vecAbsOrigin )
{
	UpdateVPhysicsPosition( vecAbsOrigin, vec3_origin, gpGlobals->frametime );
}

const char *CBasePlayer::GetPlayerModelName( void )
{
	return "models/player.mdl";
}

float CBasePlayer::GetAirTime( void )
{
	return m_flTimeLastTouchedGround == 0.0f ? 0.0f : gpGlobals->curtime - m_flTimeLastTouchedGround;
}

const Vector& CBasePlayer::GetEyeOffset() const 
{ 
	return m_vecEyeOffset; 
}

void CBasePlayer::SetEyeOffset( const Vector& v ) 
{ 
	m_vecEyeOffset = v; 
}

const QAngle & CBasePlayer::GetEyeAngleOffset() const 
{ 
	return m_EyeAngleOffset; 
}

void CBasePlayer::SetEyeAngleOffset( const QAngle& qa ) 
{ 
	m_EyeAngleOffset = qa; 
}

const Vector & CBasePlayer::GetAimDirection() const
{
	return m_AimDirection;
}

void CBasePlayer::SetAimDirection( const Vector& v )
{
	m_AimDirection = v; 
}

bool CBasePlayer::IsCoach( void ) const
{
	return ( GetCoachingTeam() != 0 );
}

int CBasePlayer::GetCoachingTeam( void ) const
{
	if ( sv_coaching_enabled.GetBool() && ( GetTeamNumber() == TEAM_SPECTATOR ) )
	{
		return m_iCoachingTeam;
	}
	else
	{ 
		return 0;
	}
}

// returns the player's team or, if coach, coaching team.
int CBasePlayer::GetAssociatedTeamNumber( void ) const
{
	return ( IsCoach() ? GetCoachingTeam() : GetTeamNumber() ); 
}

// is a true spectator, i.e. NOT a team coach
bool CBasePlayer::IsSpectator( void ) const
{
	return ( GetTeamNumber() == TEAM_SPECTATOR && !IsCoach() );
}

autoaim_params_t::autoaim_params_t()
{
	m_fScale = 0;
	m_fMaxDist = 0;
	m_fMaxDeflection = -1.0f;
	m_bOnTargetQueryOnly = false;
}

KS_ENTITY_ABSTRACT_KIND( CBasePlayer )
