// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Client handler implementations for instruction players how to play

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
#include "isaverestore.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"

#include "c_baselesson.h"
#include "c_gameinstructor.h"

#include "hud_locator_target.h"
#include "c_world.h"
#include "iinput.h"
#include "ammodef.h"
#include "view.h"

constexpr auto LESSON_PRIORITY_MAX = 1000;
constexpr auto LESSON_PRIORITY_NONE = 0;
constexpr auto LESSON_MIN_TIME_ON_SCREEN_TO_MARK_DISPLAYED = 1.5f;
constexpr auto LESSON_MIN_TIME_BEFORE_LOCK_ALLOWED = 0.1f;
constexpr auto LESSON_DISTANCE_UPDATE_RATE = 0.25f;

CGameInstructorSymbol::CGameInstructorSymbol( const char *pStr )
{
	// Function-local so symbols built during static init in other TUs find it constructed.
	static ks::intern_pool s_Pool;
	m_pString = pStr ? s_Pool.intern( pStr ).data() : nullptr;
}

extern ConVar gameinstructor_verbose;
extern ConVar gameinstructor_verbose_lesson;
extern ConVar gameinstructor_find_errors;

//
// CGameInstructorLesson
//

Color CBaseLesson::m_rgbaVerboseHeader = Color( 255, 128, 64, 255 );
Color CBaseLesson::m_rgbaVerbosePlain = Color( 64, 128, 255, 255 );
Color CBaseLesson::m_rgbaVerboseName = Color( 255, 255, 255, 255 );
Color CBaseLesson::m_rgbaVerboseOpen = Color( 0, 255, 0, 255 );
Color CBaseLesson::m_rgbaVerboseClose = Color( 255, 0, 0, 255 );
Color CBaseLesson::m_rgbaVerboseSuccess = Color( 255, 255, 0, 255 );
Color CBaseLesson::m_rgbaVerboseUpdate = Color( 255, 0, 255, 255  );


CBaseLesson::CBaseLesson( const char *pchName, bool bIsDefaultHolder, bool bIsOpenOpportunity, int nSplitScreenSlot )
{
	m_nSplitScreenSlot = nSplitScreenSlot;
	m_stringName = pchName;
	m_stringReplaceKey = "";
	m_bIsDefaultHolder = bIsDefaultHolder;
	m_bIsOpenOpportunity = bIsOpenOpportunity;

	Init();
}

CBaseLesson::~CBaseLesson( void )
{
	if ( m_pRoot )
	{
		// Remove from root's children list
		ks::erase_first( m_pRoot->m_OpenOpportunities, this );
	}
	else
	{
		for ( int i = 0; i < std::ssize( m_OpenOpportunities ); ++i )
		{
			// Remove from children if they are still around
			CBaseLesson *pLesson = m_OpenOpportunities[ i ];
			pLesson->m_pRoot = nullptr;
		}
	}
}

void CBaseLesson::AddPrerequisite( const char *pchLessonName )
{
	if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "\t%s: ", GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Adding prereq " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\"", pchLessonName );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
	}

	const CBaseLesson *pPrerequisite = GetGameInstructor().GetLesson( pchLessonName );
	if ( !pPrerequisite )
	{
		DevWarning( "Prerequisite %s added by lesson %s doesn't exist!\n", pchLessonName, GetName() );
		return;
	}

	m_Prerequisites.push_back( pPrerequisite );
}

void CBaseLesson::SetRoot( CBaseLesson *pRoot )
{
	m_pRoot = pRoot;

	if ( m_pRoot && ks::find_index( m_pRoot->m_OpenOpportunities, this ) == -1 )
	{
		m_pRoot->m_OpenOpportunities.push_back( this );
	}
}

bool CBaseLesson::ShouldShowSpew( void )
{
	if ( gameinstructor_verbose_lesson.GetString()[ 0 ] == '\0' )
		return false;

	return ( V_stristr( GetName(), gameinstructor_verbose_lesson.GetString() ) != nullptr );
}

bool CBaseLesson::NoPriority( void ) const
{
	return ( m_iPriority == LESSON_PRIORITY_NONE );
}

bool CBaseLesson::IsLocked( void ) const
{
	if ( m_fLockDuration == 0.0f )
		return false;

	if ( !IsInstructing() || !IsVisible() )
		return false;

	float fLockTime = m_fLockTime;

	if ( fLockTime == 0.0f )
	{
		fLockTime = m_fStartTime;
	}

	return ( gpGlobals->curtime > m_fStartTime + LESSON_MIN_TIME_BEFORE_LOCK_ALLOWED && gpGlobals->curtime < fLockTime + m_fLockDuration );
}

bool CBaseLesson::IsLearned( void ) const
{
	if ( m_iDisplayLimit > 0 && m_iDisplayCount >= m_iDisplayLimit )
		return true;

	if ( m_iSuccessLimit > 0 && m_iSuccessCount >= m_iSuccessLimit )
		return true;

	return false;
}

bool CBaseLesson::PrerequisitesHaveBeenMet( void ) const
{
	for ( int i = 0; i < std::ssize( m_Prerequisites ); ++i )
	{
		if ( !m_Prerequisites[ i ]->IsLearned() )
		{
			// Failed a prereq
			return false;
		}
	}

	// All prereqs passed
	return true;
}

bool CBaseLesson::IsTimedOut( void )
{
	CVProfScope _( "CBaseLesson::IsTimedOut", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	// Check for no timeout
	if ( m_fTimeout == 0.0f )
		return false;

	float fStartTime = m_fStartTime;

	if ( GetRoot()->IsLearned() )
	{
		if ( !m_bBumpWithTimeoutWhenLearned )
		{
			// Time out instantly if we've learned this and don't want to keep it open for priority bumping
			return true;
		}
		else
		{
			// It'll never be active, so lets use timeout based on when it was initialized
			fStartTime = m_fInitTime;
		}
	}

	if ( !fStartTime )
	{
		if ( !m_bCanTimeoutWhileInactive )
		{
			return false;
		}

		// Not active, so lets use timeout based on when it was initialized
		fStartTime = m_fInitTime;
	}

	bool bTimedOut = ( fStartTime + m_fTimeout < gpGlobals->curtime );

	if ( bTimedOut )
	{
		SetCloseReason( "Timed out." );
	}

	return bTimedOut;
}

void CBaseLesson::ResetDisplaysAndSuccesses( void )
{
	m_iDisplayCount = 0;

	m_bSuccessCounted = false;
	m_iSuccessCount = 0;
}

bool CBaseLesson::IncDisplayCount( void )
{
	if ( m_iDisplayCount < m_iDisplayLimit )
	{
		m_iDisplayCount++;
		return true;
	}

	return false;
}

bool CBaseLesson::IncSuccessCount( void )
{
	if ( m_iSuccessCount < m_iSuccessLimit )
	{
		m_iSuccessCount++;
		return true;
	}

	return false;
}

void CBaseLesson::Init( void )
{
	m_pRoot = nullptr;
	m_bSuccessCounted = false;

	SetCloseReason( "None given." );

	m_iPriority = LESSON_PRIORITY_MAX; // Set to invalid value to ensure that it is actually set later on
	m_iInstanceType = LESSON_INSTANCE_MULTIPLE;
	m_iFixedInstancesMax = 1;
	m_bReplaceOnlyWhenStopped = false;
	m_iTeam = TEAM_ANY;
	m_bOnlyKeyboard = false;
	m_bOnlyGamepad = false;
	m_bNoSplitscreen = false;

	m_iDisplayLimit = 0;
	m_iDisplayCount = 0;
	m_bWasDisplayed = false;

	m_iSuccessLimit = 0;
	m_iSuccessCount = 0;

	m_fLockDuration = 0.0f;
	m_bCanOpenWhenDead = false;
	m_bBumpWithTimeoutWhenLearned = false;
	m_bOnceLearnedNeverOpen = false;
	m_bCanTimeoutWhileInactive = false;
	m_bUsableInMidair = false;
	m_fTimeout = 0.0f;

	m_fInitTime = gpGlobals->curtime;
	m_fStartTime = 0.0f;
	m_fLockTime = 0.0f;

	m_fUpdateInterval = 0.5;

	m_bHasPlayedSound = false;

	m_szStartSound = "Instructor.LessonStart";
	m_szLessonGroup = "";

	m_iNumDelayedPlayerSwaps = 0;
}

void CBaseLesson::TakePlaceOf( CBaseLesson *pLesson )
{
	// Transfer over marked as displayed so a replaced lesson won't count as an extra display
	m_bWasDisplayed = pLesson->m_bWasDisplayed;
	pLesson->m_bWasDisplayed = false;
}

void CBaseLesson::MarkSucceeded( void )
{
	if ( !m_bSuccessCounted )
	{
		GetGameInstructor().MarkSucceeded( GetName() );
		m_bSuccessCounted = true;
	}
}

void CBaseLesson::CloseOpportunity( const char *pchReason )
{
	SetCloseReason( pchReason );
	m_bIsOpenOpportunity = false;
}

bool CBaseLesson::DoDelayedPlayerSwaps( void ) const
{
	// A bot has swapped places with a player or player with a bot...
	// At the time of the actual swap there was no client representation for the new player...
	// So that swap was queued up and now we're going to make things right!
	while ( m_iNumDelayedPlayerSwaps )
	{
		C_BasePlayer *pNewPlayer = UTIL_PlayerByUserId( m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps - 1 ].iNewUserID );

		if ( !pNewPlayer )
		{
			// There is still no client representation of the new player, we'll have to try again later
			if ( gameinstructor_verbose.GetInt() > 1 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tFailed delayed player swap!" );
			}
			return false;
		}

		if ( gameinstructor_verbose.GetInt() > 1 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tSuccessful delayed player swap!" );
		}

		m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps - 1 ].phHandleToChange->Set( pNewPlayer );

		m_iNumDelayedPlayerSwaps--;
	}

	return true;
}


//
// CTextLesson
//

void CTextLesson::Init( void )
{
	m_szDisplayText = "";
	m_szDisplayParamText = "";
	m_szBinding = "";
	m_szGamepadBinding = "";
}

void CTextLesson::Start( void )
{
	// TODO: Display some text
	//m_szDisplayText
}


void CTextLesson::Stop( void )
{
	// TODO: Clean up text
}


//
// CIconLesson
//

void CIconLesson::Init( void )
{
	m_hIconTarget = nullptr;
	m_szUITargetName = "";
	m_szUITargetLookup = "";
	m_nUITargetEdge = 0;

	m_hLocatorTarget = -1;
	m_bFixedPosition = false;
	m_bNoIconTarget = false;
	m_bAllowNodrawTarget = false;

	m_bVisible = true;
	m_bShowWhenOccluded = true;
	m_bNoOffscreen = false;
	m_bForceCaption = false;

	m_szOnscreenIcon = "";
	m_szOffscreenIcon = "";

	m_flUpOffset = 0.0f;
	m_flRelativeUpOffset = 0.0f;
	m_fFixedPositionX = 0.0f;
	m_fFixedPositionY = 0.0f;

	m_fRange = 0.0f;
	m_fCurrentDistance = 0.0f;

	m_fOnScreenStartTime = 0.0f;
	m_fUpdateDistanceTime = 0.0f;

	m_iFlags = LOCATOR_ICON_FX_NONE;

	m_szCaptionColor = "255,255,255";// Default to white
}

void CIconLesson::Start( void )
{
	if ( !DoDelayedPlayerSwaps() )
	{
		return;
	}

	// Display some text
	C_BaseEntity *pIconTarget = m_hIconTarget.Get();

	if ( !pIconTarget )	
	{
		if ( !m_bNoIconTarget )
		{
			// Wanted one, but couldn't get it
			CloseOpportunity( "Icon Target handle went invalid before the lesson started!" );
		}

		return;
	}
	else
	{
		if ( ( pIconTarget->IsEffectActive( EF_NODRAW ) || pIconTarget->IsDormant() ) && !m_bAllowNodrawTarget )
		{
			// We don't allow no draw entities
			CloseOpportunity( "Icon Target is using effect NODRAW and allow_nodraw_target is false!" );
			return;
		}
	}

	CLocatorTarget *pLocatorTarget = nullptr;

	if( m_hLocatorTarget != -1 )
	{
		// Lets try the handle that we've held on to
		pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );

		if ( !pLocatorTarget )
		{
			// It's gone stale, get a new target
			m_hLocatorTarget = Locator_AddTarget();
			pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );
		}
	}
	else
	{
		// Get a new target
		m_hLocatorTarget = Locator_AddTarget();
		pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );
	}

	if( m_hLocatorTarget == -1 || !pLocatorTarget )
	{
		CloseOpportunity( "Could not get a handle for new locator target. Too many targets in use!" );
		return;
	}

	pLocatorTarget->AddIconEffects( m_iFlags );

	pLocatorTarget->SetCaptionColor( GetCaptionColorString() );

	pLocatorTarget->SetIconNoTarget( m_bNoIconTarget );

	UpdateLocatorTarget( pLocatorTarget, pIconTarget );

	// Update occlusion data
	Locator_ComputeTargetIconPositionFromHandle( m_hLocatorTarget );
}


void CIconLesson::Stop( void )
{
	if ( !DoDelayedPlayerSwaps() )
	{
		return;
	}

	if ( m_hLocatorTarget != -1 )
	{
		Locator_RemoveTarget( m_hLocatorTarget );
	}

	m_fOnScreenStartTime = 0.0f;
}

void CIconLesson::Update( void )
{
	if ( !DoDelayedPlayerSwaps() )
	{
		return;
	}

	C_BaseEntity *pIconTarget = m_hIconTarget.Get();

	if ( !pIconTarget )
	{
		if ( !m_bNoIconTarget )
		{
			CloseOpportunity( "Lost our icon target handle returned NULL." );
		}

		return;
	}
	else
	{
		if ( ( pIconTarget->IsEffectActive( EF_NODRAW ) || pIconTarget->IsDormant() ) && !m_bAllowNodrawTarget )
		{
			// We don't allow no draw entities
			CloseOpportunity( "Icon Target is using effect NODRAW and allow_nodraw_target is false!" );
			return;
		}
	}

	CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );
	if ( !pLocatorTarget )
	{
		// Temp instrumentation to catch a bug - possibly calling Update without having called Start?
		Warning( "Problem in lesson %s: Locator_GetTargetFromHandle returned null for handle %d.\n IsInstanceActive: %s. IsInstructing: %s. IsLearned: %s\n",
				GetName(), m_hLocatorTarget, 
				(IsInstanceActive() ? "yes" : "no"),
				(IsInstructing() ? "yes" : "no"),
				(IsLearned() ? "yes" : "no") );
		CloseOpportunity( "Lost locator target handle." );
		return;
	}

	UpdateLocatorTarget( pLocatorTarget, pIconTarget );
	C_BasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();

	// Check if it has been onscreen long enough to count as being displayed
	if ( m_fOnScreenStartTime == 0.0f )
	{
		if ( pLocatorTarget->IsOnScreen() && ( IsPresentComplete() || ( pLocatorTarget->GetIconEffectsFlags() & LOCATOR_ICON_FX_STATIC ) ) )
		{
			// Is either static or has finished presenting and is on screen
			m_fOnScreenStartTime = gpGlobals->curtime;
		}
	}
	else
	{
		if ( !pLocatorTarget->IsOnScreen() )
		{
			// Was visible before, but it isn't now
			m_fOnScreenStartTime = 0.0f;
		}
		else if ( gpGlobals->curtime - m_fOnScreenStartTime >= LESSON_MIN_TIME_ON_SCREEN_TO_MARK_DISPLAYED )
		{
			// Lesson on screen long enough to be counted as displayed
			m_bWasDisplayed = true;
		}
	}

	if ( m_fUpdateDistanceTime < gpGlobals->curtime )
	{
		// Update it's distance from the local player
		C_BaseEntity *pTarget = m_hIconTarget.Get();

		if ( !pLocalPlayer || !pTarget || pLocalPlayer == pTarget )
		{
			m_fCurrentDistance = 0.0f;
		}
		else
		{
			m_fCurrentDistance = pLocalPlayer->ActivePlayerCombatCharacter()->EyePosition().DistTo( pTarget->WorldSpaceCenter() );
		}

		m_fUpdateDistanceTime = gpGlobals->curtime + LESSON_DISTANCE_UPDATE_RATE;
	}
}

void CIconLesson::UpdateInactive( void )
{
	if ( m_fUpdateDistanceTime < gpGlobals->curtime )
	{
		if ( !DoDelayedPlayerSwaps() )
		{
			return;
		}

		C_BaseEntity *pIconTarget = m_hIconTarget.Get();

		if ( !pIconTarget )
		{
			if ( !m_bNoIconTarget )
			{
				CloseOpportunity( "Lost our icon target handle returned NULL." );
			}

			m_fCurrentDistance = 0.0f;
			return;
		}
		else
		{
			if ( ( pIconTarget->IsEffectActive( EF_NODRAW ) || pIconTarget->IsDormant() ) && !m_bAllowNodrawTarget )
			{
				// We don't allow no draw entities
				CloseOpportunity( "Icon Target is using effect NODRAW and allow_nodraw_target is false!" );
				return;
			}
		}

		// Update it's distance from the local player
		C_BasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();

		if ( !pLocalPlayer || pLocalPlayer == pIconTarget )
		{
			m_fCurrentDistance = 0.0f;
		}
		else
		{
			m_fCurrentDistance = pLocalPlayer->ActivePlayerCombatCharacter()->EyePosition().DistTo( pIconTarget->WorldSpaceCenter() );
		}

		m_fUpdateDistanceTime = gpGlobals->curtime + LESSON_DISTANCE_UPDATE_RATE;
	}
}

bool CIconLesson::ShouldDisplay( void ) const
{
	CVProfScope _( "CIconLesson::ShouldDisplay", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	if ( !DoDelayedPlayerSwaps() )
	{
		return false;
	}

	if ( m_fRange > 0.0f && m_fCurrentDistance > m_fRange )
	{
		// Distance to target is more than the max range
		return false;
	}

	if ( !m_bShowWhenOccluded && m_hLocatorTarget >= 0 )
	{
		CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );

		if ( pLocatorTarget && pLocatorTarget->IsOccluded() )
		{
			// Target is occluded and doesn't want to be shown when occluded
			return false;
		}
	}

	C_BasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();
	if ( !IsUsableInMidair() )
	{
		if ( !pLocalPlayer )
		{
			return false;
		}
		else
		{
			float flAirTime = pLocalPlayer->GetAirTime();
			if ( flAirTime > 0.75f )
			{
				return false;
			}
		}
	}

	// Ok to display
	return true;
}

bool CIconLesson::IsVisible( void ) const
{
	CVProfScope _( "CIconLesson::IsVisible", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	if( m_hLocatorTarget == -1 )
	{
		// If it doesn't want a target, it's "visible" otherwise we'll have to call it invisible
		return m_bNoIconTarget;
	}

	CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );
	if ( !pLocatorTarget )
	{
		return false;
	}

	return pLocatorTarget->IsVisible();
}

void CIconLesson::SwapOutPlayers( int iOldUserID, int iNewUserID )
{
	BaseClass::SwapOutPlayers( iOldUserID, iNewUserID );

	if ( m_bNoIconTarget )
		return;

	// Get the player pointers from the user IDs
	C_BasePlayer *pOldPlayer = UTIL_PlayerByUserId( iOldUserID );
	C_BasePlayer *pNewPlayer = UTIL_PlayerByUserId( iNewUserID );

	if ( pOldPlayer == m_hIconTarget.Get() )
	{
		if ( pNewPlayer )
		{
			m_hIconTarget = pNewPlayer;
		}
		else
		{
			if ( m_iNumDelayedPlayerSwaps < MAX_DELAYED_PLAYER_SWAPS )
			{
				m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps ].phHandleToChange = &m_hIconTarget;
				m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps ].iNewUserID = iNewUserID;
				++m_iNumDelayedPlayerSwaps;
			}
		}
	}
}

void CIconLesson::TakePlaceOf( CBaseLesson *pLesson )
{
	BaseClass::TakePlaceOf( pLesson );

	const CIconLesson *pIconLesson = dynamic_cast<const CIconLesson*>( pLesson );

	if ( pIconLesson )
	{
		if ( pIconLesson->m_hLocatorTarget != -1 )
		{
			CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( pIconLesson->m_hLocatorTarget );

			if ( pLocatorTarget )
			{
				// This one draw right to the hud... use it's icon target handle
				m_hLocatorTarget = pIconLesson->m_hLocatorTarget;
			}
		}

		m_fOnScreenStartTime = pIconLesson->m_fOnScreenStartTime;
	}
}

void CIconLesson::SetLocatorBinding( CLocatorTarget * pLocatorTarget )
{
	if ( input->ControllerModeActive() )
	{
		// Try to use gamepad bindings first
		if ( m_szGamepadBinding.String()[ 0 ] != '\0' )
		{
			// Found gamepad binds!
			pLocatorTarget->SetBinding( m_szGamepadBinding.String() );
		}
		else
		{
			// No gamepad binding, so fallback to the regular binding
			pLocatorTarget->SetBinding( m_szBinding.String() );
		}
	}
	else
	{
		// Always use the regular binding when the gamepad is disabled
		pLocatorTarget->SetBinding( m_szBinding.String() );
	}
}

bool CIconLesson::IsPresentComplete( void )
{
	if ( m_hLocatorTarget == -1 )
		return false;

	CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );

	if ( !pLocatorTarget )
		return false;

	return !pLocatorTarget->IsPresenting();
}

void CIconLesson::PresentStart( void )
{
	if ( m_hLocatorTarget == -1 )
		return;

	CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );

	if ( !pLocatorTarget )
		return;

	pLocatorTarget->StartPresent();
}

void CIconLesson::PresentEnd( void )
{
	if ( m_hLocatorTarget == -1 )
		return;

	CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( m_hLocatorTarget );

	if ( !pLocatorTarget )
		return;

	pLocatorTarget->EndPresent();
}

void CIconLesson::UpdateLocatorTarget( CLocatorTarget *pLocatorTarget, C_BaseEntity *pIconTarget )
{
	if ( m_bFixedPosition )
	{
		pLocatorTarget->m_bOriginInScreenspace = true;
		pLocatorTarget->m_vecOrigin.x = m_fFixedPositionX;
		pLocatorTarget->m_vecOrigin.y = m_fFixedPositionY;
		pLocatorTarget->SetVguiTargetName( m_szUITargetName.String() );
		pLocatorTarget->SetVguiTargetLookup( m_szUITargetLookup.String() );
		pLocatorTarget->SetVguiTargetEdge( m_nUITargetEdge );
	}
	else
	{
		pLocatorTarget->m_bOriginInScreenspace = false;
		pLocatorTarget->m_vecOrigin = pIconTarget->EyePosition() + MainViewUp( 0 ) * m_flRelativeUpOffset + Vector( 0.0f, 0.0f, m_flUpOffset );
		pLocatorTarget->SetVguiTargetName( "" );
	}

	const char *pchDisplayParamText = m_szDisplayParamText.String();

	// Check if the parameter is the be the player display name
	if ( V_stricmp( pchDisplayParamText, "use_name" ) == 0 )
	{
		// Fix up the player display name
		C_BasePlayer *pPlayer = ToBasePlayer( pIconTarget );
		if ( pPlayer )
		{
			pchDisplayParamText = pPlayer->GetPlayerName();
		}
		else
		{
			bool bNoName = true;


			if ( bNoName )
			{
				// It's not a player!
				pchDisplayParamText = "";
			}
		}
	}

	pLocatorTarget->SetCaptionText( m_szDisplayText.String(), pchDisplayParamText );
	SetLocatorBinding( pLocatorTarget );
	pLocatorTarget->SetOnscreenIconTextureName( m_szOnscreenIcon.String() );
	pLocatorTarget->SetOffscreenIconTextureName( m_szOffscreenIcon.String() );
	pLocatorTarget->SetVisible( m_bVisible );

	C_BasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();

	if( !m_bFixedPosition && 
		( ( pLocalPlayer != nullptr && pLocalPlayer == m_hIconTarget ) || 
		  GetClientWorldEntity() == m_hIconTarget ) )
	{
		// Mark this icon as a static icon that draws in a fixed 
		// location on the hud rather than tracking an object
		// in 3D space.
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_STATIC );
	}
	else
	{
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_NONE );
	}

	if ( m_bNoOffscreen )
	{
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_NO_OFFSCREEN );
	}
	else
	{
		pLocatorTarget->RemoveIconEffects( LOCATOR_ICON_FX_NO_OFFSCREEN );
	}

	if( m_bForceCaption || IsLocked() )
	{
		pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_FORCE_CAPTION );
	}
	else
	{
		pLocatorTarget->RemoveIconEffects( LOCATOR_ICON_FX_FORCE_CAPTION );
	}

	pLocatorTarget->Update();

	if ( pLocatorTarget->m_bIsDrawing )
	{
		if ( !m_bHasPlayedSound )
		{
			GetGameInstructor().PlaySound( m_szStartSound.String() );
			m_bHasPlayedSound = true;
		}
	}
}

//
// CScriptedIconLesson
//

// Linking variables to scriptable entries is done here!
// The first parameter correlates to the case insensitive string name read from scripts.
// This macro generates code that passes this consistent variable data in to other macros


// Create keyvalues name symbol
#define LESSON_VARIABLE_SYMBOL( _varEnum, _varName, _varType ) static int g_n##_varEnum##Symbol;

#define LESSON_VARIABLE_INIT_SYMBOL( _varEnum, _varName, _varType ) g_n##_varEnum##Symbol = KeyValuesSystem()->GetSymbolForString( #_varEnum );

#define LESSON_SCRIPT_STRING_ADD_TO_MAP( _varEnum, _varName, _varType ) g_NameToTypeMap.emplace( #_varEnum, LESSON_VARIABLE_##_varEnum );

// Create enum value
#define LESSON_VARIABLE_ENUM( _varEnum, _varName, _varType ) LESSON_VARIABLE_##_varEnum,

// Init info call
#define LESSON_VARIABLE_INIT_INFO_CALL( _varEnum, _varName, _varType ) g_pLessonVariableInfo[ LESSON_VARIABLE_##_varEnum ].Init_##_varEnum();

// Init info
#define LESSON_VARIABLE_INIT_INFO( _varEnum, _varName, _varType ) \
	void Init_##_varEnum( void ) \
	{ \
		iOffset = __builtin_offsetof( CScriptedIconLesson, _varName ); \
		varType = LessonParamTypeFromString( #_varType ); \
	}

#define LESSON_VARIABLE_INIT_INFO_BOOL( _varEnum, _varName, _varType ) \
	void Init_##_varEnum( void ) \
	{ \
		iOffset = __builtin_offsetof( CScriptedIconLesson, _varName ); \
		varType = FIELD_BOOLEAN; \
	}

#define LESSON_VARIABLE_INIT_INFO_EHANDLE( _varEnum, _varName, _varType ) \
	void Init_##_varEnum( void ) \
	{ \
		iOffset = __builtin_offsetof( CScriptedIconLesson, _varName ); \
		varType = FIELD_EHANDLE; \
	}

#define LESSON_VARIABLE_INIT_INFO_STRING( _varEnum, _varName, _varType ) \
	void Init_##_varEnum( void ) \
	{ \
		iOffset = __builtin_offsetof( CScriptedIconLesson, _varName ); \
		varType = FIELD_STRING; \
	}

// Copy defaults into this scripted lesson into a new one
#define LESSON_VARIABLE_DEFAULT( _varEnum, _varName, _varType ) ( _varName = m_pDefaultHolder->_varName );

// Copy a variable from this scripted lesson into a new one
#define LESSON_VARIABLE_COPY( _varEnum, _varName, _varType ) ( pOpenLesson->_varName = _varName );

// Return the first param if pchName is the same as the second param
#define LESSON_SCRIPT_STRING( _type, _string ) \
	if ( V_stricmp( pchName, _string ) == 0 )\
	{\
		return _type;\
	}

// Wrapper for using this macro in the factory
#define LESSON_SCRIPT_STRING_GENERAL( _varEnum, _varName, _varType ) LESSON_SCRIPT_STRING( LESSON_VARIABLE_##_varEnum, #_varEnum )

// Process the element action on this variable 
#define PROCESS_LESSON_ACTION( _varEnum, _varName, _varType ) \
	case LESSON_VARIABLE_##_varEnum:\
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, #_varName, _varName, &pLessonElement->szParam, eventParam_float );

#define PROCESS_LESSON_ACTION_EHANDLE( _varEnum, _varName, _varType ) \
	case LESSON_VARIABLE_##_varEnum:\
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, #_varName, _varName, &pLessonElement->szParam, eventParam_float, eventParam_BaseEntity, eventParam_string );

#define PROCESS_LESSON_ACTION_STRING( _varEnum, _varName, _varType ) \
	case LESSON_VARIABLE_##_varEnum:\
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, #_varName, &_varName, &pLessonElement->szParam, eventParam_string );

// Init the variable from the script (or a convar)
#define LESSON_VARIABLE_INIT( _varEnum, _varName, _varType ) \
	else if ( g_n##_varEnum##Symbol == pSubKey->GetNameSymbol() ) \
	{ \
		const char *pchParam = pSubKey->GetString(); \
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) \
		{ \
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); \
			if ( tempCVar.IsValid() ) \
			{ \
				_varName = static_cast<_varType>( tempCVar.GetFloat() ); \
			} \
			else \
			{ \
				_varName = static_cast<_varType>( 0.0f ); \
			} \
		} \
		else \
		{ \
			_varName = static_cast<_varType>( pSubKey->GetFloat() ); \
		} \
	}

#define LESSON_VARIABLE_INIT_BOOL( _varEnum, _varName, _varType ) \
	else if ( V_stricmp( #_varEnum, pSubKey->GetName() ) == 0 ) \
	{ \
		_varName = pSubKey->GetBool(); \
	}

#define LESSON_VARIABLE_INIT_EHANDLE( _varEnum, _varName, _varType ) \
	else if ( g_n##_varEnum##Symbol == pSubKey->GetNameSymbol() ) \
	{ \
		DevWarning( "Can't initialize an EHANDLE from the instructor lesson script." ); \
	}

#define LESSON_VARIABLE_INIT_STRING( _varEnum, _varName, _varType ) \
	else if ( g_n##_varEnum##Symbol == pSubKey->GetNameSymbol() ) \
	{ \
		const char *pchParam = pSubKey->GetString(); \
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) \
		{ \
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); \
			if ( tempCVar.IsValid() ) \
			{ \
				_varName = tempCVar.GetString(); \
			} \
			else \
			{ \
				_varName = ""; \
			} \
		} \
		else \
		{ \
			_varName = pSubKey->GetString(); \
		} \
	}

// Gets a scripted variable by offset and casts it to the proper type
#define LESSON_VARIABLE_GET_FROM_OFFSET( _type, _offset ) *static_cast<_type*>( static_cast<void*>( static_cast<byte*>( static_cast<void*>( this ) ) + _offset ) )


// Enum of scripted variables
enum LessonVariable
{
	// Run enum macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
LESSON_VARIABLE_VOID,
LESSON_VARIABLE_LOCAL_PLAYER,
LESSON_VARIABLE_OUTPUT,
LESSON_VARIABLE_ENTITY1,
LESSON_VARIABLE_ENTITY2,
LESSON_VARIABLE_STRING1,
LESSON_VARIABLE_STRING2,
LESSON_VARIABLE_INTEGER1,
LESSON_VARIABLE_INTEGER2,
LESSON_VARIABLE_FLOAT1,
LESSON_VARIABLE_FLOAT2,
LESSON_VARIABLE_ICON_TARGET,
LESSON_VARIABLE_UI_TARGET_NAME,
LESSON_VARIABLE_UI_TARGET_LOOKUP,
LESSON_VARIABLE_UI_TARGET_EDGE,
LESSON_VARIABLE_FIXED_POSITION_X,
LESSON_VARIABLE_FIXED_POSITION_Y,
LESSON_VARIABLE_FIXED_POSITION,
LESSON_VARIABLE_NO_ICON_TARGET,
LESSON_VARIABLE_ALLOW_NODRAW_TARGET,
LESSON_VARIABLE_VISIBLE,
LESSON_VARIABLE_SHOW_WHEN_OCCLUDED,
LESSON_VARIABLE_NO_OFFSCREEN,
LESSON_VARIABLE_FORCE_CAPTION,
LESSON_VARIABLE_ONSCREEN_ICON,
LESSON_VARIABLE_OFFSCREEN_ICON,
LESSON_VARIABLE_ICON_OFFSET,
LESSON_VARIABLE_ICON_RELATIVE_OFFSET,
LESSON_VARIABLE_RANGE,
LESSON_VARIABLE_FLAGS,
LESSON_VARIABLE_CAPTION_COLOR,
LESSON_VARIABLE_GROUP,
LESSON_VARIABLE_CAPTION,
LESSON_VARIABLE_CAPTION_PARAM,
LESSON_VARIABLE_BINDING,
LESSON_VARIABLE_GAMEPAD_BINDING,
LESSON_VARIABLE_PRIORITY,
LESSON_VARIABLE_REPLACE_KEY,
LESSON_VARIABLE_LOCK_DURATION,
LESSON_VARIABLE_CAN_OPEN_WHEN_DEAD,
LESSON_VARIABLE_BUMP_WITH_TIMEOUT_WHEN_LEARNED,
LESSON_VARIABLE_ONCE_LEARNED_NEVER_OPEN,
LESSON_VARIABLE_CAN_TIMEOUT_WHILE_INACTIVE,
LESSON_VARIABLE_USABLE_IN_MIDAIR,
LESSON_VARIABLE_TIMEOUT,
LESSON_VARIABLE_UPDATE_INTERVAL,
LESSON_VARIABLE_START_SOUND,

	LESSON_VARIABLE_TOTAL
};

// Declare the keyvalues symbols for the keynames
static int g_nVOIDSymbol;
static int g_nLOCAL_PLAYERSymbol;
static int g_nOUTPUTSymbol;
static int g_nENTITY1Symbol;
static int g_nENTITY2Symbol;
static int g_nSTRING1Symbol;
static int g_nSTRING2Symbol;
static int g_nINTEGER1Symbol;
static int g_nINTEGER2Symbol;
static int g_nFLOAT1Symbol;
static int g_nFLOAT2Symbol;
static int g_nICON_TARGETSymbol;
static int g_nUI_TARGET_NAMESymbol;
static int g_nUI_TARGET_LOOKUPSymbol;
static int g_nUI_TARGET_EDGESymbol;
static int g_nFIXED_POSITION_XSymbol;
static int g_nFIXED_POSITION_YSymbol;
static int g_nFIXED_POSITIONSymbol;
static int g_nNO_ICON_TARGETSymbol;
static int g_nALLOW_NODRAW_TARGETSymbol;
static int g_nVISIBLESymbol;
static int g_nSHOW_WHEN_OCCLUDEDSymbol;
static int g_nNO_OFFSCREENSymbol;
static int g_nFORCE_CAPTIONSymbol;
static int g_nONSCREEN_ICONSymbol;
static int g_nOFFSCREEN_ICONSymbol;
static int g_nICON_OFFSETSymbol;
static int g_nICON_RELATIVE_OFFSETSymbol;
static int g_nRANGESymbol;
static int g_nFLAGSSymbol;
static int g_nCAPTION_COLORSymbol;
static int g_nGROUPSymbol;
static int g_nCAPTIONSymbol;
static int g_nCAPTION_PARAMSymbol;
static int g_nBINDINGSymbol;
static int g_nGAMEPAD_BINDINGSymbol;
static int g_nPRIORITYSymbol;
static int g_nREPLACE_KEYSymbol;
static int g_nLOCK_DURATIONSymbol;
static int g_nCAN_OPEN_WHEN_DEADSymbol;
static int g_nBUMP_WITH_TIMEOUT_WHEN_LEARNEDSymbol;
static int g_nONCE_LEARNED_NEVER_OPENSymbol;
static int g_nCAN_TIMEOUT_WHILE_INACTIVESymbol;
static int g_nUSABLE_IN_MIDAIRSymbol;
static int g_nTIMEOUTSymbol;
static int g_nUPDATE_INTERVALSymbol;
static int g_nSTART_SOUNDSymbol;

// String lookup prototypes
LessonVariable LessonVariableFromString( const char *pchName, bool bWarnOnInvalidNames = true );
_fieldtypes LessonParamTypeFromString( const char *pchName );
int LessonActionFromString( const char *pchName );


// This is used to get type info an variable offsets from the variable enumerated value
class LessonVariableInfo
{
public:

	LessonVariableInfo( void ) 
		: iOffset( 0 ), varType( FIELD_VOID )
	{
	}

	// Run init info macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)

	void Init_VOID( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_hLocalPlayer ); 
		varType = FIELD_EHANDLE; 
	}

	void Init_LOCAL_PLAYER( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_hLocalPlayer ); 
		varType = FIELD_EHANDLE; 
	}

	void Init_OUTPUT( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fOutput ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_ENTITY1( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_hEntity1 ); 
		varType = FIELD_EHANDLE; 
	}

	void Init_ENTITY2( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_hEntity2 ); 
		varType = FIELD_EHANDLE; 
	}

	void Init_STRING1( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szString1 ); 
		varType = FIELD_STRING; 
	}

	void Init_STRING2( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szString2 ); 
		varType = FIELD_STRING; 
	}

	void Init_INTEGER1( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_iInteger1 ); 
		varType = LessonParamTypeFromString( "int" ); 
	}

	void Init_INTEGER2( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_iInteger2 ); 
		varType = LessonParamTypeFromString( "int" ); 
	}

	void Init_FLOAT1( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fFloat1 ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_FLOAT2( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fFloat2 ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_ICON_TARGET( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_hIconTarget ); 
		varType = FIELD_EHANDLE; 
	}

	void Init_UI_TARGET_NAME( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szUITargetName ); 
		varType = FIELD_STRING; 
	}

	void Init_UI_TARGET_LOOKUP( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szUITargetLookup ); 
		varType = FIELD_STRING; 
	}

	void Init_UI_TARGET_EDGE( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_nUITargetEdge ); 
		varType = LessonParamTypeFromString( "int" ); 
	}

	void Init_FIXED_POSITION_X( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fFixedPositionX ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_FIXED_POSITION_Y( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fFixedPositionY ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_FIXED_POSITION( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bFixedPosition ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_NO_ICON_TARGET( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bNoIconTarget ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_ALLOW_NODRAW_TARGET( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bAllowNodrawTarget ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_VISIBLE( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bVisible ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_SHOW_WHEN_OCCLUDED( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bShowWhenOccluded ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_NO_OFFSCREEN( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bNoOffscreen ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_FORCE_CAPTION( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bForceCaption ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_ONSCREEN_ICON( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szOnscreenIcon ); 
		varType = FIELD_STRING; 
	}

	void Init_OFFSCREEN_ICON( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szOffscreenIcon ); 
		varType = FIELD_STRING; 
	}

	void Init_ICON_OFFSET( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_flUpOffset ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_ICON_RELATIVE_OFFSET( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_flRelativeUpOffset ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_RANGE( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fRange ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_FLAGS( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_iFlags ); 
		varType = LessonParamTypeFromString( "int" ); 
	}

	void Init_CAPTION_COLOR( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szCaptionColor ); 
		varType = FIELD_STRING; 
	}

	void Init_GROUP( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szLessonGroup ); 
		varType = FIELD_STRING; 
	}

	void Init_CAPTION( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szDisplayText ); 
		varType = FIELD_STRING; 
	}

	void Init_CAPTION_PARAM( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szDisplayParamText ); 
		varType = FIELD_STRING; 
	}

	void Init_BINDING( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szBinding ); 
		varType = FIELD_STRING; 
	}

	void Init_GAMEPAD_BINDING( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szGamepadBinding ); 
		varType = FIELD_STRING; 
	}

	void Init_PRIORITY( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_iPriority ); 
		varType = LessonParamTypeFromString( "int" ); 
	}

	void Init_REPLACE_KEY( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_stringReplaceKey ); 
		varType = FIELD_STRING; 
	}

	void Init_LOCK_DURATION( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fLockDuration ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_CAN_OPEN_WHEN_DEAD( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bCanOpenWhenDead ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_BUMP_WITH_TIMEOUT_WHEN_LEARNED( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bBumpWithTimeoutWhenLearned ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_ONCE_LEARNED_NEVER_OPEN( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bOnceLearnedNeverOpen ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_CAN_TIMEOUT_WHILE_INACTIVE( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bCanTimeoutWhileInactive ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_USABLE_IN_MIDAIR( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_bUsableInMidair ); 
		varType = FIELD_BOOLEAN; 
	}

	void Init_TIMEOUT( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fTimeout ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_UPDATE_INTERVAL( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_fUpdateInterval ); 
		varType = LessonParamTypeFromString( "float" ); 
	}

	void Init_START_SOUND( void ) 
	{ 
		iOffset = __builtin_offsetof( CScriptedIconLesson, m_szStartSound ); 
		varType = FIELD_STRING; 
	}

public:

	int iOffset;
	_fieldtypes varType;
};

LessonVariableInfo g_pLessonVariableInfo[ LESSON_VARIABLE_TOTAL ];


const LessonVariableInfo *GetLessonVariableInfo( int iLessonVariable )
{
	contract_assert( iLessonVariable >= 0 && iLessonVariable < LESSON_VARIABLE_TOTAL );

	if ( g_pLessonVariableInfo[ 0 ].varType == FIELD_VOID )
	{
		// Run init info call macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
g_pLessonVariableInfo[ LESSON_VARIABLE_VOID ].Init_VOID();
g_pLessonVariableInfo[ LESSON_VARIABLE_LOCAL_PLAYER ].Init_LOCAL_PLAYER();
g_pLessonVariableInfo[ LESSON_VARIABLE_OUTPUT ].Init_OUTPUT();
g_pLessonVariableInfo[ LESSON_VARIABLE_ENTITY1 ].Init_ENTITY1();
g_pLessonVariableInfo[ LESSON_VARIABLE_ENTITY2 ].Init_ENTITY2();
g_pLessonVariableInfo[ LESSON_VARIABLE_STRING1 ].Init_STRING1();
g_pLessonVariableInfo[ LESSON_VARIABLE_STRING2 ].Init_STRING2();
g_pLessonVariableInfo[ LESSON_VARIABLE_INTEGER1 ].Init_INTEGER1();
g_pLessonVariableInfo[ LESSON_VARIABLE_INTEGER2 ].Init_INTEGER2();
g_pLessonVariableInfo[ LESSON_VARIABLE_FLOAT1 ].Init_FLOAT1();
g_pLessonVariableInfo[ LESSON_VARIABLE_FLOAT2 ].Init_FLOAT2();
g_pLessonVariableInfo[ LESSON_VARIABLE_ICON_TARGET ].Init_ICON_TARGET();
g_pLessonVariableInfo[ LESSON_VARIABLE_UI_TARGET_NAME ].Init_UI_TARGET_NAME();
g_pLessonVariableInfo[ LESSON_VARIABLE_UI_TARGET_LOOKUP ].Init_UI_TARGET_LOOKUP();
g_pLessonVariableInfo[ LESSON_VARIABLE_UI_TARGET_EDGE ].Init_UI_TARGET_EDGE();
g_pLessonVariableInfo[ LESSON_VARIABLE_FIXED_POSITION_X ].Init_FIXED_POSITION_X();
g_pLessonVariableInfo[ LESSON_VARIABLE_FIXED_POSITION_Y ].Init_FIXED_POSITION_Y();
g_pLessonVariableInfo[ LESSON_VARIABLE_FIXED_POSITION ].Init_FIXED_POSITION();
g_pLessonVariableInfo[ LESSON_VARIABLE_NO_ICON_TARGET ].Init_NO_ICON_TARGET();
g_pLessonVariableInfo[ LESSON_VARIABLE_ALLOW_NODRAW_TARGET ].Init_ALLOW_NODRAW_TARGET();
g_pLessonVariableInfo[ LESSON_VARIABLE_VISIBLE ].Init_VISIBLE();
g_pLessonVariableInfo[ LESSON_VARIABLE_SHOW_WHEN_OCCLUDED ].Init_SHOW_WHEN_OCCLUDED();
g_pLessonVariableInfo[ LESSON_VARIABLE_NO_OFFSCREEN ].Init_NO_OFFSCREEN();
g_pLessonVariableInfo[ LESSON_VARIABLE_FORCE_CAPTION ].Init_FORCE_CAPTION();
g_pLessonVariableInfo[ LESSON_VARIABLE_ONSCREEN_ICON ].Init_ONSCREEN_ICON();
g_pLessonVariableInfo[ LESSON_VARIABLE_OFFSCREEN_ICON ].Init_OFFSCREEN_ICON();
g_pLessonVariableInfo[ LESSON_VARIABLE_ICON_OFFSET ].Init_ICON_OFFSET();
g_pLessonVariableInfo[ LESSON_VARIABLE_ICON_RELATIVE_OFFSET ].Init_ICON_RELATIVE_OFFSET();
g_pLessonVariableInfo[ LESSON_VARIABLE_RANGE ].Init_RANGE();
g_pLessonVariableInfo[ LESSON_VARIABLE_FLAGS ].Init_FLAGS();
g_pLessonVariableInfo[ LESSON_VARIABLE_CAPTION_COLOR ].Init_CAPTION_COLOR();
g_pLessonVariableInfo[ LESSON_VARIABLE_GROUP ].Init_GROUP();
g_pLessonVariableInfo[ LESSON_VARIABLE_CAPTION ].Init_CAPTION();
g_pLessonVariableInfo[ LESSON_VARIABLE_CAPTION_PARAM ].Init_CAPTION_PARAM();
g_pLessonVariableInfo[ LESSON_VARIABLE_BINDING ].Init_BINDING();
g_pLessonVariableInfo[ LESSON_VARIABLE_GAMEPAD_BINDING ].Init_GAMEPAD_BINDING();
g_pLessonVariableInfo[ LESSON_VARIABLE_PRIORITY ].Init_PRIORITY();
g_pLessonVariableInfo[ LESSON_VARIABLE_REPLACE_KEY ].Init_REPLACE_KEY();
g_pLessonVariableInfo[ LESSON_VARIABLE_LOCK_DURATION ].Init_LOCK_DURATION();
g_pLessonVariableInfo[ LESSON_VARIABLE_CAN_OPEN_WHEN_DEAD ].Init_CAN_OPEN_WHEN_DEAD();
g_pLessonVariableInfo[ LESSON_VARIABLE_BUMP_WITH_TIMEOUT_WHEN_LEARNED ].Init_BUMP_WITH_TIMEOUT_WHEN_LEARNED();
g_pLessonVariableInfo[ LESSON_VARIABLE_ONCE_LEARNED_NEVER_OPEN ].Init_ONCE_LEARNED_NEVER_OPEN();
g_pLessonVariableInfo[ LESSON_VARIABLE_CAN_TIMEOUT_WHILE_INACTIVE ].Init_CAN_TIMEOUT_WHILE_INACTIVE();
g_pLessonVariableInfo[ LESSON_VARIABLE_USABLE_IN_MIDAIR ].Init_USABLE_IN_MIDAIR();
g_pLessonVariableInfo[ LESSON_VARIABLE_TIMEOUT ].Init_TIMEOUT();
g_pLessonVariableInfo[ LESSON_VARIABLE_UPDATE_INTERVAL ].Init_UPDATE_INTERVAL();
g_pLessonVariableInfo[ LESSON_VARIABLE_START_SOUND ].Init_START_SOUND();
	}

	return &(g_pLessonVariableInfo[ iLessonVariable ]);
}

static ks::idict< LessonVariable > g_NameToTypeMap;
static ks::idict< fieldtype_t > g_TypeToParamTypeMap;
ks::idict< int > CScriptedIconLesson::LessonActionMap;


IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CScriptedIconLesson )


CScriptedIconLesson::~CScriptedIconLesson( void )
{
	if ( m_pDefaultHolder )
	{
		delete m_pDefaultHolder;
		m_pDefaultHolder = nullptr;
	}
}


void CScriptedIconLesson::Init( void )
{
	m_hLocalPlayer.Set( nullptr );
	m_fOutput = 0.0f;
	m_hEntity1.Set( nullptr );
	m_hEntity2.Set( nullptr );
	m_szString1 = "";
	m_szString2 = "";
	m_iInteger1 = 0;
	m_iInteger2 = 0;
	m_fFloat1 = 0.0f;
	m_fFloat2 = 0.0f;

	m_fUpdateEventTime = 0.0f;
	m_pDefaultHolder = nullptr;
	
	m_iScopeDepth = 0;

	if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Initializing scripted lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
	}

	if ( !IsDefaultHolder() )
	{
		if ( !IsOpenOpportunity() )
		{
			// Initialize from the key value file
			InitFromKeys( GetGameInstructor().GetScriptKeys() );

			if ( m_iPriority >= LESSON_PRIORITY_MAX )
			{
				DevWarning( "Priority level not set for lesson: %s\n", GetName() );
			}

			// We use this to remember variable defaults to be reset before each open attempt
			m_pDefaultHolder = new CScriptedIconLesson( GetName(), true, false, m_nSplitScreenSlot );
			CScriptedIconLesson *pOpenLesson = m_pDefaultHolder;

			// Run copy macros on all default scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
( pOpenLesson->m_hLocalPlayer = m_hLocalPlayer );
( pOpenLesson->m_hLocalPlayer = m_hLocalPlayer );
( pOpenLesson->m_fOutput = m_fOutput );
( pOpenLesson->m_hEntity1 = m_hEntity1 );
( pOpenLesson->m_hEntity2 = m_hEntity2 );
( pOpenLesson->m_szString1 = m_szString1 );
( pOpenLesson->m_szString2 = m_szString2 );
( pOpenLesson->m_iInteger1 = m_iInteger1 );
( pOpenLesson->m_iInteger2 = m_iInteger2 );
( pOpenLesson->m_fFloat1 = m_fFloat1 );
( pOpenLesson->m_fFloat2 = m_fFloat2 );
( pOpenLesson->m_hIconTarget = m_hIconTarget );
( pOpenLesson->m_szUITargetName = m_szUITargetName );
( pOpenLesson->m_szUITargetLookup = m_szUITargetLookup );
( pOpenLesson->m_nUITargetEdge = m_nUITargetEdge );
( pOpenLesson->m_fFixedPositionX = m_fFixedPositionX );
( pOpenLesson->m_fFixedPositionY = m_fFixedPositionY );
( pOpenLesson->m_bFixedPosition = m_bFixedPosition );
( pOpenLesson->m_bNoIconTarget = m_bNoIconTarget );
( pOpenLesson->m_bAllowNodrawTarget = m_bAllowNodrawTarget );
( pOpenLesson->m_bVisible = m_bVisible );
( pOpenLesson->m_bShowWhenOccluded = m_bShowWhenOccluded );
( pOpenLesson->m_bNoOffscreen = m_bNoOffscreen );
( pOpenLesson->m_bForceCaption = m_bForceCaption );
( pOpenLesson->m_szOnscreenIcon = m_szOnscreenIcon );
( pOpenLesson->m_szOffscreenIcon = m_szOffscreenIcon );
( pOpenLesson->m_flUpOffset = m_flUpOffset );
( pOpenLesson->m_flRelativeUpOffset = m_flRelativeUpOffset );
( pOpenLesson->m_fRange = m_fRange );
( pOpenLesson->m_iFlags = m_iFlags );
( pOpenLesson->m_szCaptionColor = m_szCaptionColor );
( pOpenLesson->m_szLessonGroup = m_szLessonGroup );
( pOpenLesson->m_szDisplayText = m_szDisplayText );
( pOpenLesson->m_szDisplayParamText = m_szDisplayParamText );
( pOpenLesson->m_szBinding = m_szBinding );
( pOpenLesson->m_szGamepadBinding = m_szGamepadBinding );
( pOpenLesson->m_iPriority = m_iPriority );
( pOpenLesson->m_stringReplaceKey = m_stringReplaceKey );
( pOpenLesson->m_fLockDuration = m_fLockDuration );
( pOpenLesson->m_bCanOpenWhenDead = m_bCanOpenWhenDead );
( pOpenLesson->m_bBumpWithTimeoutWhenLearned = m_bBumpWithTimeoutWhenLearned );
( pOpenLesson->m_bOnceLearnedNeverOpen = m_bOnceLearnedNeverOpen );
( pOpenLesson->m_bCanTimeoutWhileInactive = m_bCanTimeoutWhileInactive );
( pOpenLesson->m_bUsableInMidair = m_bUsableInMidair );
( pOpenLesson->m_fTimeout = m_fTimeout );
( pOpenLesson->m_fUpdateInterval = m_fUpdateInterval );
( pOpenLesson->m_szStartSound = m_szStartSound );

			// Listen for open events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( m_OpenEvents ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(m_OpenEvents[ iLessonEvent ]);
				ListenForGameEvent( pLessonEvent->szEventName.String() );

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tListen for open event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\"", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
				}
			}

			// Listen for close events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( m_CloseEvents ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(m_CloseEvents[ iLessonEvent ]);
				ListenForGameEvent( pLessonEvent->szEventName.String() );

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tListen for close event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\"", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
				}
			}

			// Listen for success events
			for ( int iLessonEvent = 0; iLessonEvent < std::ssize( m_SuccessEvents ); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(m_SuccessEvents[ iLessonEvent ]);
				ListenForGameEvent( pLessonEvent->szEventName.String());

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tListen for success event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "\"%s\"", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
				}
			}
		}
		else
		{
			// This is an open lesson! Get the root for reference
			const CScriptedIconLesson *pLesson = static_cast<const CScriptedIconLesson *>( GetGameInstructor().GetLesson( GetName() ) );
			SetRoot( const_cast<CScriptedIconLesson *>( pLesson ) );
		}
	}
}

void CScriptedIconLesson::InitPrerequisites( void )
{
	if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Initializing prereqs for scripted lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\"", GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
	}

	for ( int iPrerequisit = 0; iPrerequisit < std::ssize( m_PrerequisiteNames ); ++iPrerequisit )
	{
		const char *pPrerequisiteName = m_PrerequisiteNames[ iPrerequisit ].String();
		AddPrerequisite( pPrerequisiteName );
	}
}

void CScriptedIconLesson::OnOpen( void )
{
	CVProfScope _( "CScriptedIconLesson::OnOpen", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	if ( !DoDelayedPlayerSwaps() )
	{
		return;
	}

	const CScriptedIconLesson *pLesson = static_cast<const CScriptedIconLesson *>( GetRoot() );

	// Process all update events
	for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->m_OnOpenEvents ); ++iLessonEvent )
	{
		const LessonEvent_t *pLessonEvent = &(pLesson->m_OnOpenEvents[ iLessonEvent ]);

		if ( gameinstructor_verbose.GetInt() > 1 && ShouldShowSpew() )
		{
			ConColorMsg( Color( 255, 128, 64, 255 ), "GAME INSTRUCTOR: " );
			ConColorMsg( Color( 64, 128, 255, 255 ), "OnOpen event " );
			ConColorMsg( Color( 0, 255, 0, 255 ), "\"%s\"", pLessonEvent->szEventName.String());
			ConColorMsg( Color( 64, 128, 255, 255 ), "received for lesson \"%s\"...\n", GetName() );
		}

		ProcessElements( nullptr, &(pLessonEvent->elements) );
	}

	BaseClass::OnOpen();
}

void CScriptedIconLesson::Update( void )
{
	CVProfScope _( "CScriptedIconLesson::Update", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	if ( !DoDelayedPlayerSwaps() )
	{
		return;
	}

	const CScriptedIconLesson *pLesson = static_cast<const CScriptedIconLesson *>( GetRoot() );

	if ( gpGlobals->curtime >= m_fUpdateEventTime )
	{
		bool bShowSpew = ( gameinstructor_verbose.GetInt() > 1 && ShouldShowSpew() );

		int iVerbose = gameinstructor_verbose.GetInt();
		if ( gameinstructor_verbose.GetInt() == 1 )
		{
			// Force the verbose level from 1 to 0 for update events
			gameinstructor_verbose.SetValue( 0 );
		}

		// Process all update events
		for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pLesson->m_UpdateEvents ); ++iLessonEvent )
		{
			const LessonEvent_t *pLessonEvent = &(pLesson->m_UpdateEvents[ iLessonEvent ]);

			if ( bShowSpew )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Update event " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseUpdate, "\"%s\"", pLessonEvent->szEventName.String());
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "received for lesson \"%s\"...\n", GetName() );
			}

			ProcessElements( nullptr, &(pLessonEvent->elements) );
		}

		gameinstructor_verbose.SetValue( iVerbose );

		// Wait before doing update events again
		m_fUpdateEventTime = gpGlobals->curtime + m_fUpdateInterval;
	}

	BaseClass::Update();
}

void CScriptedIconLesson::SwapOutPlayers( int iOldUserID, int iNewUserID )
{
	BaseClass::SwapOutPlayers( iOldUserID, iNewUserID );

	// Get the player pointers from the user IDs
	C_BasePlayer *pOldPlayer = UTIL_PlayerByUserId( iOldUserID );
	C_BasePlayer *pNewPlayer = UTIL_PlayerByUserId( iNewUserID );

	if ( pOldPlayer == m_hEntity1.Get() )
	{
		if ( pNewPlayer )
		{
			m_hEntity1 = pNewPlayer;
		}
		else
		{
			if ( m_iNumDelayedPlayerSwaps < MAX_DELAYED_PLAYER_SWAPS )
			{
				m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps ].phHandleToChange = &m_hEntity1;
				m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps ].iNewUserID = iNewUserID;
				++m_iNumDelayedPlayerSwaps;
			}
		}
	}

	if ( pOldPlayer == m_hEntity2.Get() )
	{
		if ( pNewPlayer )
		{
			m_hEntity2 = pNewPlayer;
		}
		else
		{

			if ( m_iNumDelayedPlayerSwaps < MAX_DELAYED_PLAYER_SWAPS )
			{
				m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps ].phHandleToChange = &m_hEntity2;
				m_pDelayedPlayerSwap[ m_iNumDelayedPlayerSwaps ].iNewUserID = iNewUserID;
				++m_iNumDelayedPlayerSwaps;
			}
		}
	}
}

void CScriptedIconLesson::FireGameEvent( IGameEvent *event )
{
	CVProfScope _( "CScriptedIconLesson::FireGameEvent", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GetSplitScreenSlot() );

	if ( m_bDisabled )
		return;

	if ( !DoDelayedPlayerSwaps() )
	{
		return;
	}

	if ( !C_BasePlayer::GetLocalPlayer() )
		return;

	// Check that this lesson is allowed for the current input device
	if( m_bOnlyKeyboard && input->ControllerModeActive() )
		return;

	if( m_bOnlyGamepad && !input->ControllerModeActive() )
		return;

	if( m_bNoSplitscreen && XBX_GetNumGameUsers() > 1 )
		return;

	// Check that this lesson is for the proper team
	CBasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();

	if ( m_iTeam != TEAM_ANY && pLocalPlayer && pLocalPlayer->GetTeamNumber() != m_iTeam )
	{
		// This lesson is intended for a different team
		return;
	}

	const char *name = event->GetName();

	// Open events run on the root
	ProcessOpenGameEvents( this, name, event );

	// Close and success events run on the children
	const std::vector< CBaseLesson * > *pChildren = GetChildren();
	for ( int iChild = 0; iChild < std::ssize( *pChildren ); ++iChild )
	{
		CScriptedIconLesson *pScriptedChild = dynamic_cast<CScriptedIconLesson*>( (*pChildren)[ iChild ] );
		
		pScriptedChild->ProcessCloseGameEvents( this, name, event );
		pScriptedChild->ProcessSuccessGameEvents( this, name, event );
	}
}

void CScriptedIconLesson::ProcessOpenGameEvents( const CScriptedIconLesson *pRootLesson, const char *name, IGameEvent *event )
{
	if ( pRootLesson->InstanceType() == LESSON_INSTANCE_SINGLE_OPEN && GetGameInstructor().IsLessonOfSameTypeOpen( this ) )
	{
		// We don't want more than one of this type, and there is already one open
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pRootLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "open events NOT processed (there is already an open lesson of this type).\n" );
		}

		return;
	}

	for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pRootLesson->m_OpenEvents ); ++iLessonEvent )
	{
		const LessonEvent_t *pLessonEvent = &(pRootLesson->m_OpenEvents[ iLessonEvent ]);

		if ( V_strcmp( name, pLessonEvent->szEventName.String()) == 0 )
		{
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Open event " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\"", pLessonEvent->szEventName.String());
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "received for lesson \"%s\"...\n", GetName() );
			}

			if ( m_pDefaultHolder )
			{
				// Run copy from default macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
( m_hLocalPlayer = m_pDefaultHolder->m_hLocalPlayer );
( m_hLocalPlayer = m_pDefaultHolder->m_hLocalPlayer );
( m_fOutput = m_pDefaultHolder->m_fOutput );
( m_hEntity1 = m_pDefaultHolder->m_hEntity1 );
( m_hEntity2 = m_pDefaultHolder->m_hEntity2 );
( m_szString1 = m_pDefaultHolder->m_szString1 );
( m_szString2 = m_pDefaultHolder->m_szString2 );
( m_iInteger1 = m_pDefaultHolder->m_iInteger1 );
( m_iInteger2 = m_pDefaultHolder->m_iInteger2 );
( m_fFloat1 = m_pDefaultHolder->m_fFloat1 );
( m_fFloat2 = m_pDefaultHolder->m_fFloat2 );
( m_hIconTarget = m_pDefaultHolder->m_hIconTarget );
( m_szUITargetName = m_pDefaultHolder->m_szUITargetName );
( m_szUITargetLookup = m_pDefaultHolder->m_szUITargetLookup );
( m_nUITargetEdge = m_pDefaultHolder->m_nUITargetEdge );
( m_fFixedPositionX = m_pDefaultHolder->m_fFixedPositionX );
( m_fFixedPositionY = m_pDefaultHolder->m_fFixedPositionY );
( m_bFixedPosition = m_pDefaultHolder->m_bFixedPosition );
( m_bNoIconTarget = m_pDefaultHolder->m_bNoIconTarget );
( m_bAllowNodrawTarget = m_pDefaultHolder->m_bAllowNodrawTarget );
( m_bVisible = m_pDefaultHolder->m_bVisible );
( m_bShowWhenOccluded = m_pDefaultHolder->m_bShowWhenOccluded );
( m_bNoOffscreen = m_pDefaultHolder->m_bNoOffscreen );
( m_bForceCaption = m_pDefaultHolder->m_bForceCaption );
( m_szOnscreenIcon = m_pDefaultHolder->m_szOnscreenIcon );
( m_szOffscreenIcon = m_pDefaultHolder->m_szOffscreenIcon );
( m_flUpOffset = m_pDefaultHolder->m_flUpOffset );
( m_flRelativeUpOffset = m_pDefaultHolder->m_flRelativeUpOffset );
( m_fRange = m_pDefaultHolder->m_fRange );
( m_iFlags = m_pDefaultHolder->m_iFlags );
( m_szCaptionColor = m_pDefaultHolder->m_szCaptionColor );
( m_szLessonGroup = m_pDefaultHolder->m_szLessonGroup );
( m_szDisplayText = m_pDefaultHolder->m_szDisplayText );
( m_szDisplayParamText = m_pDefaultHolder->m_szDisplayParamText );
( m_szBinding = m_pDefaultHolder->m_szBinding );
( m_szGamepadBinding = m_pDefaultHolder->m_szGamepadBinding );
( m_iPriority = m_pDefaultHolder->m_iPriority );
( m_stringReplaceKey = m_pDefaultHolder->m_stringReplaceKey );
( m_fLockDuration = m_pDefaultHolder->m_fLockDuration );
( m_bCanOpenWhenDead = m_pDefaultHolder->m_bCanOpenWhenDead );
( m_bBumpWithTimeoutWhenLearned = m_pDefaultHolder->m_bBumpWithTimeoutWhenLearned );
( m_bOnceLearnedNeverOpen = m_pDefaultHolder->m_bOnceLearnedNeverOpen );
( m_bCanTimeoutWhileInactive = m_pDefaultHolder->m_bCanTimeoutWhileInactive );
( m_bUsableInMidair = m_pDefaultHolder->m_bUsableInMidair );
( m_fTimeout = m_pDefaultHolder->m_fTimeout );
( m_fUpdateInterval = m_pDefaultHolder->m_fUpdateInterval );
( m_szStartSound = m_pDefaultHolder->m_szStartSound );
			}

			if ( ProcessElements( event, &(pLessonEvent->elements) ) )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\tAll elements returned true. Opening!\n" );
				}

				CScriptedIconLesson *pOpenLesson = new CScriptedIconLesson( GetName(), false, true, GetSplitScreenSlot() );

				// Run copy macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
( pOpenLesson->m_hLocalPlayer = m_hLocalPlayer );
( pOpenLesson->m_hLocalPlayer = m_hLocalPlayer );
( pOpenLesson->m_fOutput = m_fOutput );
( pOpenLesson->m_hEntity1 = m_hEntity1 );
( pOpenLesson->m_hEntity2 = m_hEntity2 );
( pOpenLesson->m_szString1 = m_szString1 );
( pOpenLesson->m_szString2 = m_szString2 );
( pOpenLesson->m_iInteger1 = m_iInteger1 );
( pOpenLesson->m_iInteger2 = m_iInteger2 );
( pOpenLesson->m_fFloat1 = m_fFloat1 );
( pOpenLesson->m_fFloat2 = m_fFloat2 );
( pOpenLesson->m_hIconTarget = m_hIconTarget );
( pOpenLesson->m_szUITargetName = m_szUITargetName );
( pOpenLesson->m_szUITargetLookup = m_szUITargetLookup );
( pOpenLesson->m_nUITargetEdge = m_nUITargetEdge );
( pOpenLesson->m_fFixedPositionX = m_fFixedPositionX );
( pOpenLesson->m_fFixedPositionY = m_fFixedPositionY );
( pOpenLesson->m_bFixedPosition = m_bFixedPosition );
( pOpenLesson->m_bNoIconTarget = m_bNoIconTarget );
( pOpenLesson->m_bAllowNodrawTarget = m_bAllowNodrawTarget );
( pOpenLesson->m_bVisible = m_bVisible );
( pOpenLesson->m_bShowWhenOccluded = m_bShowWhenOccluded );
( pOpenLesson->m_bNoOffscreen = m_bNoOffscreen );
( pOpenLesson->m_bForceCaption = m_bForceCaption );
( pOpenLesson->m_szOnscreenIcon = m_szOnscreenIcon );
( pOpenLesson->m_szOffscreenIcon = m_szOffscreenIcon );
( pOpenLesson->m_flUpOffset = m_flUpOffset );
( pOpenLesson->m_flRelativeUpOffset = m_flRelativeUpOffset );
( pOpenLesson->m_fRange = m_fRange );
( pOpenLesson->m_iFlags = m_iFlags );
( pOpenLesson->m_szCaptionColor = m_szCaptionColor );
( pOpenLesson->m_szLessonGroup = m_szLessonGroup );
( pOpenLesson->m_szDisplayText = m_szDisplayText );
( pOpenLesson->m_szDisplayParamText = m_szDisplayParamText );
( pOpenLesson->m_szBinding = m_szBinding );
( pOpenLesson->m_szGamepadBinding = m_szGamepadBinding );
( pOpenLesson->m_iPriority = m_iPriority );
( pOpenLesson->m_stringReplaceKey = m_stringReplaceKey );
( pOpenLesson->m_fLockDuration = m_fLockDuration );
( pOpenLesson->m_bCanOpenWhenDead = m_bCanOpenWhenDead );
( pOpenLesson->m_bBumpWithTimeoutWhenLearned = m_bBumpWithTimeoutWhenLearned );
( pOpenLesson->m_bOnceLearnedNeverOpen = m_bOnceLearnedNeverOpen );
( pOpenLesson->m_bCanTimeoutWhileInactive = m_bCanTimeoutWhileInactive );
( pOpenLesson->m_bUsableInMidair = m_bUsableInMidair );
( pOpenLesson->m_fTimeout = m_fTimeout );
( pOpenLesson->m_fUpdateInterval = m_fUpdateInterval );
( pOpenLesson->m_szStartSound = m_szStartSound );

				if ( GetGameInstructor().OpenOpportunity( pOpenLesson ) )
				{
					pOpenLesson->OnOpen();

					if ( pRootLesson->InstanceType() == LESSON_INSTANCE_SINGLE_OPEN )
					{
						// This one is open and we only want one! So, we're done.
						// Other open events may be listening for the same events... skip them!
						return;
					}
				}
			}
		}
	}
}

void CScriptedIconLesson::ProcessCloseGameEvents( const CScriptedIconLesson *pRootLesson, const char *name, IGameEvent *event )
{
	for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pRootLesson->m_CloseEvents ) && IsOpenOpportunity(); ++iLessonEvent )
	{
		const LessonEvent_t *pLessonEvent = &(pRootLesson->m_CloseEvents[ iLessonEvent ]);

		if ( V_strcmp( name, pLessonEvent->szEventName.String()) == 0 )
		{
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Close event " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\"", pLessonEvent->szEventName.String());
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "received for lesson \"%s\"...\n", GetName() );
			}

			if ( ProcessElements( event, &(pLessonEvent->elements) ) )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tAll elements returned true. Closing!\n" );
				}

				CloseOpportunity( "Close event elements completed." );
			}
		}
	}
}

void CScriptedIconLesson::ProcessSuccessGameEvents( const CScriptedIconLesson *pRootLesson, const char *name, IGameEvent *event )
{
	for ( int iLessonEvent = 0; iLessonEvent < std::ssize( pRootLesson->m_SuccessEvents ); ++iLessonEvent )
	{
		const LessonEvent_t *pLessonEvent = &(pRootLesson->m_SuccessEvents[ iLessonEvent ]);

		if ( V_strcmp( name, pLessonEvent->szEventName.String()) == 0 )
		{
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Success event " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "\"%s\"", pLessonEvent->szEventName.String());
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "received for lesson \"%s\"...\n", GetName() );
			}

			if ( ProcessElements( event, &(pLessonEvent->elements) ) )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "\tAll elements returned true. Succeeding!\n" );
				}

				MarkSucceeded();
			}
		}
	}
}

LessonVariable LessonVariableFromString( const char *pchName, bool bWarnOnInvalidNames )
{
	auto it = g_NameToTypeMap.find( pchName );
	if ( it != g_NameToTypeMap.end() )
		return it->second;

	if ( bWarnOnInvalidNames )
	{
		contract_assert( false && "Invalid scripted lesson variable!" );
		DevWarning( "Invalid scripted lesson variable: %s\n", pchName );
	}
	
	return LESSON_VARIABLE_TOTAL;
}

_fieldtypes LessonParamTypeFromString( const char *pchName )
{
	if ( g_TypeToParamTypeMap.empty() )
	{
		CScriptedIconLesson::PreReadLessonsFromFile();
	}

	auto it = g_TypeToParamTypeMap.find( pchName );
	if ( it != g_TypeToParamTypeMap.end() )
		return it->second;

	DevWarning( "Invalid scripted lesson variable/param type: %s\n", pchName );
	return FIELD_VOID;
}

int LessonActionFromString( const char *pchName )
{
	auto it = CScriptedIconLesson::LessonActionMap.find( pchName );
	if ( it != CScriptedIconLesson::LessonActionMap.end() )
		return it->second;

	DevWarning( "Invalid scripted lesson action: %s\n", pchName );
	return LESSON_ACTION_NONE;
}

void CScriptedIconLesson::InitElementsFromKeys( std::vector< LessonElement_t > *pLessonElements, KeyValues *pKey )
{
	KeyValues *pSubKey = nullptr;
	for ( pSubKey = pKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
	{
		char szSubKeyName[ 256 ];
		V_strcpy_safe( szSubKeyName, pSubKey->GetName() );

		char *pchToken = strtok( szSubKeyName, " " );
		LessonVariable iVariable = LessonVariableFromString( pchToken );

		pchToken = strtok ( nullptr, "" );
		int iAction = LESSON_ACTION_NONE;
		bool bNot = false;
		bool bOptionalParam = false;
		
		if ( !pchToken || pchToken[ 0 ] == '\0' )
		{
			DevWarning( "No action specified for variable: \"%s\"\n", pSubKey->GetName() );
		}
		else
		{
			if ( pchToken[ 0 ] == '?' )
			{
				pchToken++;
				bOptionalParam = true;
			}

			if ( pchToken[ 0 ] == '!' )
			{
				pchToken++;
				bNot = true;
			}
			
			iAction = LessonActionFromString( pchToken );
		}

		V_strcpy_safe( szSubKeyName, pSubKey->GetString() );

		pchToken = strtok( szSubKeyName, " " );
		_fieldtypes paramType = LessonParamTypeFromString( pchToken );

		const char *pchParam = "";

		if ( paramType != FIELD_VOID )
		{
			pchToken = strtok ( nullptr, "" );
			pchParam = pchToken;
		}

		if ( !pchParam )
		{
			DevWarning( "No parameter specified for action: \"%s\"\n", pSubKey->GetName() );
		}
		else
		{
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t\tElement \"%s %s\" added.\n", pSubKey->GetName(), pSubKey->GetString() );
			}

			// See if our param is a scripted var
			LessonVariable iParamVarIndex = LessonVariableFromString( pchParam, false );

			pLessonElements->push_back( LessonElement_t( iVariable, iAction, bNot, bOptionalParam, pchParam, iParamVarIndex, paramType ) );
		}
	}
}

void CScriptedIconLesson::InitElementsFromElements( std::vector< LessonElement_t > *pLessonElements, const std::vector< LessonElement_t > *pLessonElements2 )
{
	for ( int i = 0; i < std::ssize( *pLessonElements2 ); ++i )
	{
		pLessonElements->push_back( LessonElement_t( (*pLessonElements2)[ i ] ) );
	}
}

void CScriptedIconLesson::InitFromKeys( KeyValues *pKey )
{
	if ( !pKey )
		return;

	static int s_nInstanceTypeSymbol = KeyValuesSystem()->GetSymbolForString( "instance_type" );
	static int s_nReplaceKeySymbol = KeyValuesSystem()->GetSymbolForString( "replace_key" );
	static int s_nFixedInstancesMaxSymbol = KeyValuesSystem()->GetSymbolForString( "fixed_instances_max" );
	static int s_nReplaceOnlyWhenStopped = KeyValuesSystem()->GetSymbolForString( "replace_only_when_stopped" );
	static int s_nTeamSymbol = KeyValuesSystem()->GetSymbolForString( "team" );
	static int s_nOnlyKeyboardSymbol = KeyValuesSystem()->GetSymbolForString( "only_keyboard" );
	static int s_nOnlyGamepadSymbol = KeyValuesSystem()->GetSymbolForString( "only_gamepad" );
	static int s_nNoSplitscreenSymbol = KeyValuesSystem()->GetSymbolForString( "no_splitscreen" );
	static int s_nDisplayLimitSymbol = KeyValuesSystem()->GetSymbolForString( "display_limit" );
	static int s_nSuccessLimitSymbol = KeyValuesSystem()->GetSymbolForString( "success_limit" );
	static int s_nPreReqSymbol = KeyValuesSystem()->GetSymbolForString( "prereq" );
	static int s_nOpenSymbol = KeyValuesSystem()->GetSymbolForString( "open" );
	static int s_nCloseSymbol = KeyValuesSystem()->GetSymbolForString( "close" );
	static int s_nSuccessSymbol = KeyValuesSystem()->GetSymbolForString( "success" );
	static int s_nOnOpenSymbol = KeyValuesSystem()->GetSymbolForString( "onopen" );
	static int s_nUpdateSymbol = KeyValuesSystem()->GetSymbolForString( "update" );

	KeyValues *pSubKey = nullptr;
	for ( pSubKey = pKey->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )
	{
		if ( pSubKey->GetNameSymbol() == s_nInstanceTypeSymbol )
		{
			m_iInstanceType = LessonInstanceType( pSubKey->GetInt() );
		}
		else if ( pSubKey->GetNameSymbol() == s_nReplaceKeySymbol )
		{
			m_stringReplaceKey = pSubKey->GetString();
		}
		else if ( pSubKey->GetNameSymbol() == s_nFixedInstancesMaxSymbol )
		{
			m_iFixedInstancesMax = pSubKey->GetInt();
		}
		else if ( pSubKey->GetNameSymbol() == s_nReplaceOnlyWhenStopped )
		{
			m_bReplaceOnlyWhenStopped = pSubKey->GetBool();
		}
		else if ( pSubKey->GetNameSymbol() == s_nTeamSymbol )
		{
			m_iTeam = pSubKey->GetInt();
		}
		else if ( pSubKey->GetNameSymbol() == s_nOnlyKeyboardSymbol )
		{
			m_bOnlyKeyboard = pSubKey->GetBool();
		}
		else if ( pSubKey->GetNameSymbol() == s_nOnlyGamepadSymbol )
		{
			m_bOnlyGamepad = pSubKey->GetBool();
		}
		else if ( pSubKey->GetNameSymbol() == s_nNoSplitscreenSymbol )
		{
			m_bNoSplitscreen = pSubKey->GetBool();
		}
		else if ( pSubKey->GetNameSymbol() == s_nDisplayLimitSymbol )
		{
			m_iDisplayLimit = pSubKey->GetInt();
		}
		else if ( pSubKey->GetNameSymbol() == s_nSuccessLimitSymbol )
		{
			m_iSuccessLimit = pSubKey->GetInt();
		}
		else if ( pSubKey->GetNameSymbol() == s_nPreReqSymbol )
		{
			CGameInstructorSymbol pName;
			pName = pSubKey->GetString();
			m_PrerequisiteNames.push_back( pName );
		}
		else if ( pSubKey->GetNameSymbol() == s_nOpenSymbol )
		{
			KeyValues *pEventKey = nullptr;
			for ( pEventKey = pSubKey->GetFirstTrueSubKey(); pEventKey; pEventKey = pEventKey->GetNextTrueSubKey() )
			{
				LessonEvent_t *pLessonEvent = AddOpenEvent();
				pLessonEvent->szEventName = pEventKey->GetName();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tAdding open event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
				}

				InitElementsFromKeys( &(pLessonEvent->elements), pEventKey );
			}
		}
		else if ( pSubKey->GetNameSymbol() == s_nCloseSymbol )
		{
			KeyValues *pEventKey = nullptr;
			for ( pEventKey = pSubKey->GetFirstTrueSubKey(); pEventKey; pEventKey = pEventKey->GetNextTrueSubKey() )
			{
				LessonEvent_t *pLessonEvent = AddCloseEvent();
				pLessonEvent->szEventName = pEventKey->GetName();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tAdding close event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
				}

				InitElementsFromKeys( &(pLessonEvent->elements), pEventKey );
			}
		}
		else if ( pSubKey->GetNameSymbol() == s_nSuccessSymbol )
		{
			KeyValues *pEventKey = nullptr;
			for ( pEventKey = pSubKey->GetFirstTrueSubKey(); pEventKey; pEventKey = pEventKey->GetNextTrueSubKey() )
			{
				LessonEvent_t *pLessonEvent = AddSuccessEvent();
				pLessonEvent->szEventName = pEventKey->GetName();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tAdding success event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "\"%s\" ", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
				}

				InitElementsFromKeys( &(pLessonEvent->elements), pEventKey );
			}
		}
		else if ( pSubKey->GetNameSymbol() == s_nOnOpenSymbol )
		{
			KeyValues *pEventKey = nullptr;
			for ( pEventKey = pSubKey->GetFirstTrueSubKey(); pEventKey; pEventKey = pEventKey->GetNextTrueSubKey() )
			{
				LessonEvent_t *pLessonEvent = AddOnOpenEvent();
				pLessonEvent->szEventName = pEventKey->GetName();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tAdding onopen event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
				}

				InitElementsFromKeys( &(pLessonEvent->elements), pEventKey );
			}
		}
		else if ( pSubKey->GetNameSymbol() == s_nUpdateSymbol )
		{
			KeyValues *pEventKey = nullptr;
			for ( pEventKey = pSubKey->GetFirstTrueSubKey(); pEventKey; pEventKey = pEventKey->GetNextTrueSubKey() )
			{
				LessonEvent_t *pLessonEvent = AddUpdateEvent();
				pLessonEvent->szEventName = pEventKey->GetName();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tAdding update event " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseUpdate, "\"%s\" ", pLessonEvent->szEventName.String());
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "...\n" );
				}

				InitElementsFromKeys( &(pLessonEvent->elements), pEventKey );
			}
		}

		// Run intialize from key macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)

	else if ( g_nVOIDSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		DevWarning( "Can't initialize an EHANDLE from the instructor lesson script." ); 
	}

	else if ( g_nLOCAL_PLAYERSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		DevWarning( "Can't initialize an EHANDLE from the instructor lesson script." ); 
	}

	else if ( g_nOUTPUTSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fOutput = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fOutput = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fOutput = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nENTITY1Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		DevWarning( "Can't initialize an EHANDLE from the instructor lesson script." ); 
	}

	else if ( g_nENTITY2Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		DevWarning( "Can't initialize an EHANDLE from the instructor lesson script." ); 
	}

	else if ( g_nSTRING1Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szString1 = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szString1 = ""; 
			} 
		} 
		else 
		{ 
			m_szString1 = pSubKey->GetString(); 
		} 
	}

	else if ( g_nSTRING2Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szString2 = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szString2 = ""; 
			} 
		} 
		else 
		{ 
			m_szString2 = pSubKey->GetString(); 
		} 
	}

	else if ( g_nINTEGER1Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_iInteger1 = static_cast<int>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_iInteger1 = static_cast<int>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_iInteger1 = static_cast<int>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nINTEGER2Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_iInteger2 = static_cast<int>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_iInteger2 = static_cast<int>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_iInteger2 = static_cast<int>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nFLOAT1Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fFloat1 = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fFloat1 = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fFloat1 = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nFLOAT2Symbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fFloat2 = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fFloat2 = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fFloat2 = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nICON_TARGETSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		DevWarning( "Can't initialize an EHANDLE from the instructor lesson script." ); 
	}

	else if ( g_nUI_TARGET_NAMESymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szUITargetName = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szUITargetName = ""; 
			} 
		} 
		else 
		{ 
			m_szUITargetName = pSubKey->GetString(); 
		} 
	}

	else if ( g_nUI_TARGET_LOOKUPSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szUITargetLookup = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szUITargetLookup = ""; 
			} 
		} 
		else 
		{ 
			m_szUITargetLookup = pSubKey->GetString(); 
		} 
	}

	else if ( g_nUI_TARGET_EDGESymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_nUITargetEdge = static_cast<int>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_nUITargetEdge = static_cast<int>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_nUITargetEdge = static_cast<int>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nFIXED_POSITION_XSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fFixedPositionX = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fFixedPositionX = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fFixedPositionX = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nFIXED_POSITION_YSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fFixedPositionY = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fFixedPositionY = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fFixedPositionY = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( V_stricmp( "FIXED_POSITION", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bFixedPosition = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "NO_ICON_TARGET", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bNoIconTarget = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "ALLOW_NODRAW_TARGET", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bAllowNodrawTarget = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "VISIBLE", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bVisible = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "SHOW_WHEN_OCCLUDED", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bShowWhenOccluded = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "NO_OFFSCREEN", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bNoOffscreen = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "FORCE_CAPTION", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bForceCaption = pSubKey->GetBool(); 
	}

	else if ( g_nONSCREEN_ICONSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szOnscreenIcon = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szOnscreenIcon = ""; 
			} 
		} 
		else 
		{ 
			m_szOnscreenIcon = pSubKey->GetString(); 
		} 
	}

	else if ( g_nOFFSCREEN_ICONSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szOffscreenIcon = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szOffscreenIcon = ""; 
			} 
		} 
		else 
		{ 
			m_szOffscreenIcon = pSubKey->GetString(); 
		} 
	}

	else if ( g_nICON_OFFSETSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_flUpOffset = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_flUpOffset = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_flUpOffset = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nICON_RELATIVE_OFFSETSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_flRelativeUpOffset = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_flRelativeUpOffset = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_flRelativeUpOffset = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nRANGESymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fRange = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fRange = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fRange = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nFLAGSSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_iFlags = static_cast<int>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_iFlags = static_cast<int>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_iFlags = static_cast<int>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nCAPTION_COLORSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szCaptionColor = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szCaptionColor = ""; 
			} 
		} 
		else 
		{ 
			m_szCaptionColor = pSubKey->GetString(); 
		} 
	}

	else if ( g_nGROUPSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szLessonGroup = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szLessonGroup = ""; 
			} 
		} 
		else 
		{ 
			m_szLessonGroup = pSubKey->GetString(); 
		} 
	}

	else if ( g_nCAPTIONSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szDisplayText = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szDisplayText = ""; 
			} 
		} 
		else 
		{ 
			m_szDisplayText = pSubKey->GetString(); 
		} 
	}

	else if ( g_nCAPTION_PARAMSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szDisplayParamText = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szDisplayParamText = ""; 
			} 
		} 
		else 
		{ 
			m_szDisplayParamText = pSubKey->GetString(); 
		} 
	}

	else if ( g_nBINDINGSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szBinding = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szBinding = ""; 
			} 
		} 
		else 
		{ 
			m_szBinding = pSubKey->GetString(); 
		} 
	}

	else if ( g_nGAMEPAD_BINDINGSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szGamepadBinding = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szGamepadBinding = ""; 
			} 
		} 
		else 
		{ 
			m_szGamepadBinding = pSubKey->GetString(); 
		} 
	}

	else if ( g_nPRIORITYSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_iPriority = static_cast<int>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_iPriority = static_cast<int>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_iPriority = static_cast<int>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nREPLACE_KEYSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_stringReplaceKey = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_stringReplaceKey = ""; 
			} 
		} 
		else 
		{ 
			m_stringReplaceKey = pSubKey->GetString(); 
		} 
	}

	else if ( g_nLOCK_DURATIONSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fLockDuration = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fLockDuration = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fLockDuration = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( V_stricmp( "CAN_OPEN_WHEN_DEAD", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bCanOpenWhenDead = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "BUMP_WITH_TIMEOUT_WHEN_LEARNED", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bBumpWithTimeoutWhenLearned = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "ONCE_LEARNED_NEVER_OPEN", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bOnceLearnedNeverOpen = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "CAN_TIMEOUT_WHILE_INACTIVE", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bCanTimeoutWhileInactive = pSubKey->GetBool(); 
	}

	else if ( V_stricmp( "USABLE_IN_MIDAIR", pSubKey->GetName() ) == 0 ) 
	{ 
		m_bUsableInMidair = pSubKey->GetBool(); 
	}

	else if ( g_nTIMEOUTSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fTimeout = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fTimeout = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fTimeout = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nUPDATE_INTERVALSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_fUpdateInterval = static_cast<float>( tempCVar.GetFloat() ); 
			} 
			else 
			{ 
				m_fUpdateInterval = static_cast<float>( 0.0f ); 
			} 
		} 
		else 
		{ 
			m_fUpdateInterval = static_cast<float>( pSubKey->GetFloat() ); 
		} 
	}

	else if ( g_nSTART_SOUNDSymbol == pSubKey->GetNameSymbol() ) 
	{ 
		const char *pchParam = pSubKey->GetString(); 
		if ( pchParam && StringHasPrefix( pchParam, "convar " ) ) 
		{ 
			ConVarRef tempCVar( pchParam + V_strlen( "convar " ) ); 
			if ( tempCVar.IsValid() ) 
			{ 
				m_szStartSound = tempCVar.GetString(); 
			} 
			else 
			{ 
				m_szStartSound = ""; 
			} 
		} 
		else 
		{ 
			m_szStartSound = pSubKey->GetString(); 
		} 
	}
	}
}

bool CScriptedIconLesson::ProcessElements( IGameEvent *event, const std::vector< LessonElement_t > *pElements )
{
	CVProfScope _( "CScriptedIconLesson::ProcessElements", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	m_hLocalPlayer = GetGameInstructor().GetLocalPlayer();

	bool bSuccess = true;
	int nContinueScope = -1;
	m_iScopeDepth = 0;

	if ( gameinstructor_find_errors.GetBool() )
	{
		// Just run them all to check for errors!
		for ( int iElement = 0; iElement < std::ssize( *pElements ); ++iElement )
		{
			ProcessElement( event, &((*pElements)[ iElement ] ), false );
		}

		return false;
	}

	// Process each element until a step fails
	for ( int iElement = 0; iElement < std::ssize( *pElements ); ++iElement )
	{
		if ( nContinueScope == m_iScopeDepth )
		{
			nContinueScope = -1;
		}

		if ( !ProcessElement( event, &((*pElements)[ iElement ]), nContinueScope != -1 ) )
		{
			// This element failed
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tPrevious element returned false.\n" );
			}

			nContinueScope = m_iScopeDepth - 1;

			if ( nContinueScope < 0 )
			{
				// No outer scope to worry about, we're done
				bSuccess = false;
				break;
			}
		}
	}

	return bSuccess;
}

bool CScriptedIconLesson::ProcessElement( IGameEvent *event, const LessonElement_t *pLessonElement, bool bInFailedScope )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GetSplitScreenSlot() );

	CVProfScope _( "CScriptedIconLesson::ProcessElement", 0, "GameInstructor", false, BUDGETFLAG_OTHER );

	if ( pLessonElement->iAction == LESSON_ACTION_SCOPE_IN )
	{
		// Special case for closing (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tScopeIn()\n" );
		}

		m_iScopeDepth++;
		return true;
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_SCOPE_OUT )
	{
		// Special case for closing (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tScopeOut()\n" );
		}

		m_iScopeDepth--;
		return true;
	}

	if ( bInFailedScope )
	{
		// Only scope upkeep is done when we're in a failing scope... bail!
		return true;
	}

	if ( pLessonElement->iAction == LESSON_ACTION_CLOSE )
	{
		// Special case for closing (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tCloseOpportunity()\n" );
		}

		CloseOpportunity( "Close action." );
		return true;
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_SUCCESS )
	{
		// Special case for succeeding (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tMarkSucceeded()\n" );
		}

		MarkSucceeded();
		return true;
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_LOCK )
	{
		// Special case for setting the starting point for the lesson to stay locked from (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tm_fLockTime = gpGlobals->curtime\n" );
		}

		m_fLockTime = gpGlobals->curtime;
		return true;
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_PRESENT_COMPLETE )
	{
		// Special case for checking presentation status (we don't need variables for this)
		bool bPresentComplete = IsPresentComplete();

		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tIsPresentComplete() " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bPresentComplete ) ? ( "true" ) : ( "false" ) );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( pLessonElement->bNot ) ? ( "!= true\n" ) : ( "== true\n" ) );
		}

		return ( pLessonElement->bNot ) ? ( !bPresentComplete ) : ( bPresentComplete );
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_PRESENT_START )
	{
		// Special case for setting presentation status (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tPresentStart()\n" );
		}

		PresentStart();
		return true;
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_PRESENT_END )
	{
		// Special case for setting presentation status (we don't need variables for this)
		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tPresentEnd()\n" );
		}

		PresentEnd();
		return true;
	}
	else if ( pLessonElement->iAction == LESSON_ACTION_IS_MULTIPLAYER )
	{
		// Special case for checking if the gamerules are multiplayer
		bool bIsMultiplayer = ( GameRules() && GameRules()->IsMultiplayer() );

		if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tGameRules()->IsMultiplayer() " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bIsMultiplayer ) ? ( "true" ) : ( "false" ) );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( pLessonElement->bNot ) ? ( "!= true\n" ) : ( "== true\n" ) );
		}

		return ( pLessonElement->bNot ) ? ( !bIsMultiplayer ) : ( bIsMultiplayer );
	}

	// These values temporarily hold the parameter's value
	const char *pParamName = pLessonElement->szParam.String();
	float eventParam_float = 0.0f;
	char eventParam_string[ 256 ];
	eventParam_string[ 0 ] = '\0';
	C_BaseEntity *eventParam_BaseEntity = nullptr;

	// Get the value from the event parameter based on its type
	switch ( pLessonElement->paramType )
	{
	case FIELD_FLOAT:
		if ( pLessonElement->iParamVarIndex < LESSON_VARIABLE_TOTAL )
		{
			// The parameter is a scripted var
			const LessonVariableInfo *pInfo = GetLessonVariableInfo( pLessonElement->iParamVarIndex );

			switch ( pInfo->varType )
			{
			case FIELD_FLOAT:
				eventParam_float = LESSON_VARIABLE_GET_FROM_OFFSET( float, pInfo->iOffset );
				break;
			case FIELD_INTEGER:
				eventParam_float = static_cast<float>( LESSON_VARIABLE_GET_FROM_OFFSET( int, pInfo->iOffset ) );
				break;
			case FIELD_BOOLEAN:
				eventParam_float = static_cast<float>( LESSON_VARIABLE_GET_FROM_OFFSET( bool, pInfo->iOffset ) );
				break;
			case FIELD_STRING:
				eventParam_float = static_cast<float>( atoi( &LESSON_VARIABLE_GET_FROM_OFFSET( CGameInstructorSymbol, pInfo->iOffset )->String() ) );
				break;
			case FIELD_EHANDLE:
			case FIELD_FUNCTION:
				DevWarning( "Can't use this variable type with this parameter type in lesson script.\n" );
				break;
			}
		}
		else if ( event && !(event->IsEmpty( pParamName )) )
		{
			eventParam_float = event->GetFloat( pParamName );
		}
		else if ( pLessonElement->bOptionalParam )
		{
			// We don't want to interpret this and not finding the param is still ok
			return true;
		}
		else if ( ( pParamName[ 0 ] >= '0' && pParamName[ 0 ] <= '9' ) || pParamName[ 0 ] == '-' || pParamName[ 0 ] == '.' )
		{
			// This param doesn't exist, try parsing the string
			eventParam_float = V_atof( pParamName );
		}
		else
		{
			DevWarning( "Invalid event field name and not a float \"%s\".\n", pParamName );
			return false;
		}
		break;

	case FIELD_INTEGER:
		if ( pLessonElement->iParamVarIndex < LESSON_VARIABLE_TOTAL )
		{
			// The parameter is a scripted var
			const LessonVariableInfo *pInfo = GetLessonVariableInfo( pLessonElement->iParamVarIndex );

			switch ( pInfo->varType )
			{
			case FIELD_FLOAT:
				eventParam_float = static_cast<int>( LESSON_VARIABLE_GET_FROM_OFFSET( float, pInfo->iOffset ) );
				break;
			case FIELD_INTEGER:
				eventParam_float = LESSON_VARIABLE_GET_FROM_OFFSET( int, pInfo->iOffset );
				break;
			case FIELD_BOOLEAN:
				eventParam_float = static_cast<int>( LESSON_VARIABLE_GET_FROM_OFFSET( bool, pInfo->iOffset ) );
				break;
			case FIELD_STRING:
				eventParam_float = atof( &LESSON_VARIABLE_GET_FROM_OFFSET( CGameInstructorSymbol, pInfo->iOffset )->String() );
				break;
			case FIELD_EHANDLE:
			case FIELD_FUNCTION:
				DevWarning( "Can't use this variable type with this parameter type in lesson script.\n" );
				break;
			}
		}
		else if ( event && !(event->IsEmpty( pParamName )) )
		{
			eventParam_float = static_cast<float>( event->GetInt( pParamName ) );
		}
		else if ( pLessonElement->bOptionalParam )
		{
			// We don't want to interpret this and not finding the param is still ok
			return true;
		}
		else if ( ( pParamName[ 0 ] >= '0' && pParamName[ 0 ] <= '9' ) || pParamName[ 0 ] == '-' )
		{
			// This param doesn't exist, try parsing the string
			eventParam_float = static_cast<float>( V_atoi( pParamName ) );
		}
		else
		{
			DevWarning( "Invalid event field name and not an integer \"%s\".\n", pParamName );
			return false;
		}
		break;

	case FIELD_STRING:
		if ( pLessonElement->iParamVarIndex < LESSON_VARIABLE_TOTAL )
		{
			// The parameter is a scripted var
			const LessonVariableInfo *pInfo = GetLessonVariableInfo( pLessonElement->iParamVarIndex );

			switch ( pInfo->varType )
			{
			case FIELD_STRING:
				V_strncpy( eventParam_string, &LESSON_VARIABLE_GET_FROM_OFFSET( CGameInstructorSymbol, pInfo->iOffset )->String(), sizeof( eventParam_string ) );
				break;
			case FIELD_FLOAT:
				V_snprintf( eventParam_string, sizeof( eventParam_string ), "%f", LESSON_VARIABLE_GET_FROM_OFFSET( float, pInfo->iOffset ) );
				break;
			case FIELD_INTEGER:
				V_snprintf( eventParam_string, sizeof( eventParam_string ), "%i", LESSON_VARIABLE_GET_FROM_OFFSET( int, pInfo->iOffset ) );
				break;
			case FIELD_BOOLEAN:
			case FIELD_EHANDLE:
			case FIELD_FUNCTION:
				DevWarning( "Can't use this variable type with this parameter type in lesson script.\n" );
				break;
			}
		}
		else
		{
			const char *pchEventString = nullptr;

			if ( event && !(event->IsEmpty( pParamName )) )
			{
				pchEventString = event->GetString( pParamName );
			}

			if ( pchEventString && pchEventString[0] )
			{
				V_strcpy_safe( eventParam_string, pchEventString );
			}
			else if ( pLessonElement->bOptionalParam )
			{
				// We don't want to interpret this and not finding the param is still ok
				return true;
			}
			else
			{
				// This param doesn't exist, try parsing the string
				V_strncpy( eventParam_string, pParamName, sizeof( eventParam_string ) );
			}
		}
		break;

	case FIELD_BOOLEAN:
		if ( pLessonElement->iParamVarIndex < LESSON_VARIABLE_TOTAL )
		{
			// The parameter is a scripted var
			const LessonVariableInfo *pInfo = GetLessonVariableInfo( pLessonElement->iParamVarIndex );

			switch ( pInfo->varType )
			{
			case FIELD_FLOAT:
				eventParam_float = ( ( LESSON_VARIABLE_GET_FROM_OFFSET( float, pInfo->iOffset ) ) ? ( 1.0f ) : ( 0.0f ) );
				break;
			case FIELD_INTEGER:
				eventParam_float = ( ( LESSON_VARIABLE_GET_FROM_OFFSET( int, pInfo->iOffset ) ) ? ( 1.0f ) : ( 0.0f ) );
				break;
			case FIELD_BOOLEAN:
				eventParam_float = ( ( LESSON_VARIABLE_GET_FROM_OFFSET( bool, pInfo->iOffset ) ) ? ( 1.0f ) : ( 0.0f ) );
				break;
			case FIELD_EHANDLE:
			case FIELD_STRING:
			case FIELD_FUNCTION:
				DevWarning( "Can't use this variable type with this parameter type in lesson script.\n" );
				break;
			}
		}
		else if ( event && !(event->IsEmpty( pParamName )) )
		{
			eventParam_float = ( event->GetBool( pParamName ) ) ? ( 1.0f ) : ( 0.0f );
		}
		else if ( pLessonElement->bOptionalParam )
		{
			// We don't want to interpret this and not finding the param is still ok
			return true;
		}
		else if ( pParamName[ 0 ] == '0' || pParamName[ 0 ] == '1' )
		{
			// This param doesn't exist, try parsing the string
			eventParam_float = ( V_atof( pParamName ) != 0.0f ) ? 1.0f : 0.0f;
		}
		else
		{
			DevWarning( "Invalid event field name and not an boolean \"%s\".\n", pParamName );
			return false;
		}
		break;

	case FIELD_CUSTOM:
		if ( pLessonElement->iParamVarIndex < LESSON_VARIABLE_TOTAL )
		{
			// The parameter is a scripted var
			const LessonVariableInfo *pInfo = GetLessonVariableInfo( pLessonElement->iParamVarIndex );

			switch ( pInfo->varType )
			{
			case FIELD_EHANDLE:
				eventParam_BaseEntity = ( LESSON_VARIABLE_GET_FROM_OFFSET( EHANDLE, pInfo->iOffset ) ).Get();
				if ( !eventParam_BaseEntity )
				{
					if ( pLessonElement->bOptionalParam )
					{
						// Not having an entity is fine
						return true;
					}

					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tPlayer param \"%s\" returned NULL.\n", pParamName );
					}
					return false;
				}
				break;
			case FIELD_FLOAT:
			case FIELD_INTEGER:
			case FIELD_BOOLEAN:
			case FIELD_STRING:
			case FIELD_FUNCTION:
				DevWarning( "Can't use this variable type with this parameter type in lesson script.\n" );
				break;
			}
		}
		else if ( event && !(event->IsEmpty( pParamName )) )
		{
			eventParam_BaseEntity = UTIL_PlayerByUserId( event->GetInt( pParamName ) );
			if ( !eventParam_BaseEntity )
			{
				if ( pLessonElement->bOptionalParam )
				{
					// Not having an entity is fine
					return true;
				}

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tPlayer param \"%s\" returned NULL.\n", pParamName );
				}
				return false;
			}
		}
		else if ( pLessonElement->bOptionalParam )
		{
			// We don't want to interpret this and not finding the param is still ok
			return true;
		}
		else if ( V_stricmp( pParamName, "null" ) == 0 )
		{
			// They explicitly want a null pointer
			eventParam_BaseEntity = nullptr;
		}
		else
		{
			DevWarning( "Invalid event field name \"%s\".\n", pParamName );
			return false;
		}
		break;

	case FIELD_EHANDLE:
		if ( pLessonElement->iParamVarIndex < LESSON_VARIABLE_TOTAL )
		{
			// The parameter is a scripted var
			const LessonVariableInfo *pInfo = GetLessonVariableInfo( pLessonElement->iParamVarIndex );

			switch ( pInfo->varType )
			{
			case FIELD_EHANDLE:
				eventParam_BaseEntity = ( LESSON_VARIABLE_GET_FROM_OFFSET( EHANDLE, pInfo->iOffset ) ).Get();
				if ( !eventParam_BaseEntity )
				{
					if ( pLessonElement->bOptionalParam )
					{
						// Not having an entity is fine
						return true;
					}

					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tEntity param \"%s\" returned NULL.\n", pParamName );
					}
					return false;
				}
				break;
			case FIELD_FLOAT:
			case FIELD_INTEGER:
			case FIELD_BOOLEAN:
			case FIELD_STRING:
			case FIELD_FUNCTION:
				DevWarning( "Can't use this variable type with this parameter type in lesson script.\n" );
				break;
			}
		}
		else if ( event && !(event->IsEmpty( pParamName ))  )
		{
			int iEntID = event->GetInt( pParamName );
			if ( iEntID >= NUM_ENT_ENTRIES )
			{
				contract_assert( false && "Invalid entity ID used in game event field!" );
				DevWarning( "Invalid entity ID used in game event (%s) for param (%s).", event->GetName(), pParamName );
				return false;
			}

			eventParam_BaseEntity = C_BaseEntity::Instance( iEntID );
			if ( !eventParam_BaseEntity )
			{
				if ( pLessonElement->bOptionalParam )
				{
					// Not having an entity is fine
					return true;
				}

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tEntity param \"%s\" returned NULL.\n", pParamName );
				}
				return false;
			}
		}
		else if ( pLessonElement->bOptionalParam )
		{
			// We don't want to interpret this and not finding the param is still ok
			return true;
		}
		else if ( V_stricmp( pParamName, "null" ) == 0 )
		{
			// They explicitly want a null pointer
			eventParam_BaseEntity = nullptr;
		}
		else if ( V_stricmp( pParamName, "world" ) == 0 )
		{
			// They explicitly want the world
			eventParam_BaseEntity = GetClientWorldEntity();
		}
		else
		{
			DevWarning( "Invalid event field name \"%s\".\n", pParamName );
			return false;
		}
		break;

	case FIELD_EMBEDDED:
		{
			// The parameter is a convar
			ConVarRef tempCVar( pParamName );
			if ( tempCVar.IsValid() )
			{
				eventParam_float = tempCVar.GetFloat();
				V_strncpy( eventParam_string, tempCVar.GetString(), sizeof( eventParam_string ) );
			}
			else
			{
				DevWarning( "Invalid convar name \"%s\".\n", pParamName );
				return false;
			}
		}
		break;
	}

	// Do the action to the specified variable
	switch ( pLessonElement->iVariable )
	{
		// Run process action macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)

	case LESSON_VARIABLE_VOID:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_hLocalPlayer", m_hLocalPlayer, &pLessonElement->szParam, eventParam_float, eventParam_BaseEntity, eventParam_string );

	case LESSON_VARIABLE_LOCAL_PLAYER:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_hLocalPlayer", m_hLocalPlayer, &pLessonElement->szParam, eventParam_float, eventParam_BaseEntity, eventParam_string );

	case LESSON_VARIABLE_OUTPUT:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fOutput", m_fOutput, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_ENTITY1:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_hEntity1", m_hEntity1, &pLessonElement->szParam, eventParam_float, eventParam_BaseEntity, eventParam_string );

	case LESSON_VARIABLE_ENTITY2:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_hEntity2", m_hEntity2, &pLessonElement->szParam, eventParam_float, eventParam_BaseEntity, eventParam_string );

	case LESSON_VARIABLE_STRING1:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szString1", &m_szString1, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_STRING2:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szString2", &m_szString2, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_INTEGER1:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_iInteger1", m_iInteger1, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_INTEGER2:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_iInteger2", m_iInteger2, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FLOAT1:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fFloat1", m_fFloat1, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FLOAT2:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fFloat2", m_fFloat2, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_ICON_TARGET:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_hIconTarget", m_hIconTarget, &pLessonElement->szParam, eventParam_float, eventParam_BaseEntity, eventParam_string );

	case LESSON_VARIABLE_UI_TARGET_NAME:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szUITargetName", &m_szUITargetName, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_UI_TARGET_LOOKUP:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szUITargetLookup", &m_szUITargetLookup, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_UI_TARGET_EDGE:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_nUITargetEdge", m_nUITargetEdge, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FIXED_POSITION_X:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fFixedPositionX", m_fFixedPositionX, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FIXED_POSITION_Y:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fFixedPositionY", m_fFixedPositionY, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FIXED_POSITION:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bFixedPosition", m_bFixedPosition, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_NO_ICON_TARGET:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bNoIconTarget", m_bNoIconTarget, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_ALLOW_NODRAW_TARGET:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bAllowNodrawTarget", m_bAllowNodrawTarget, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_VISIBLE:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bVisible", m_bVisible, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_SHOW_WHEN_OCCLUDED:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bShowWhenOccluded", m_bShowWhenOccluded, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_NO_OFFSCREEN:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bNoOffscreen", m_bNoOffscreen, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FORCE_CAPTION:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bForceCaption", m_bForceCaption, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_ONSCREEN_ICON:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szOnscreenIcon", &m_szOnscreenIcon, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_OFFSCREEN_ICON:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szOffscreenIcon", &m_szOffscreenIcon, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_ICON_OFFSET:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_flUpOffset", m_flUpOffset, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_ICON_RELATIVE_OFFSET:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_flRelativeUpOffset", m_flRelativeUpOffset, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_RANGE:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fRange", m_fRange, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_FLAGS:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_iFlags", m_iFlags, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_CAPTION_COLOR:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szCaptionColor", &m_szCaptionColor, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_GROUP:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szLessonGroup", &m_szLessonGroup, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_CAPTION:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szDisplayText", &m_szDisplayText, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_CAPTION_PARAM:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szDisplayParamText", &m_szDisplayParamText, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_BINDING:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szBinding", &m_szBinding, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_GAMEPAD_BINDING:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szGamepadBinding", &m_szGamepadBinding, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_PRIORITY:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_iPriority", m_iPriority, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_REPLACE_KEY:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_stringReplaceKey", &m_stringReplaceKey, &pLessonElement->szParam, eventParam_string );

	case LESSON_VARIABLE_LOCK_DURATION:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fLockDuration", m_fLockDuration, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_CAN_OPEN_WHEN_DEAD:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bCanOpenWhenDead", m_bCanOpenWhenDead, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_BUMP_WITH_TIMEOUT_WHEN_LEARNED:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bBumpWithTimeoutWhenLearned", m_bBumpWithTimeoutWhenLearned, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_ONCE_LEARNED_NEVER_OPEN:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bOnceLearnedNeverOpen", m_bOnceLearnedNeverOpen, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_CAN_TIMEOUT_WHILE_INACTIVE:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bCanTimeoutWhileInactive", m_bCanTimeoutWhileInactive, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_USABLE_IN_MIDAIR:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_bUsableInMidair", m_bUsableInMidair, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_TIMEOUT:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fTimeout", m_fTimeout, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_UPDATE_INTERVAL:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_fUpdateInterval", m_fUpdateInterval, &pLessonElement->szParam, eventParam_float );

	case LESSON_VARIABLE_START_SOUND:
		return ProcessElementAction( pLessonElement->iAction, pLessonElement->bNot, "m_szStartSound", &m_szStartSound, &pLessonElement->szParam, eventParam_string );
	}

	return true;
}

bool CScriptedIconLesson::ProcessElementAction( int iAction, bool bNot, const char *pchVarName, float &fVar, const CGameInstructorSymbol *pchParamName, float fParam )
{
	switch ( iAction )
	{
		case LESSON_ACTION_SET:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = [%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			fVar = fParam;
			return true;

		case LESSON_ACTION_ADD:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] += [%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			fVar += fParam;
			return true;

		case LESSON_ACTION_SUBTRACT:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] -= [%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			fVar -= fParam;
			return true;

		case LESSON_ACTION_MULTIPLY:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] *= [%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			fVar *= fParam;
			return true;

		case LESSON_ACTION_IS:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f ", fVar );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "!= [%s] " ) : ( "== [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			return ( bNot ) ? ( fVar != fParam ) : ( fVar == fParam );

		case LESSON_ACTION_LESS_THAN:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f ", fVar );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			return ( bNot ) ? ( fVar >= fParam ) : ( fVar < fParam );

		case LESSON_ACTION_HAS_BIT:
		{
			int iTemp1 = static_cast<int>( fVar );
			int iTemp2 = ( 1 << static_cast<int>( fParam ) );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t([%s] ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "0x%X ", iTemp1 );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "& [%s] ", pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "0x%X", iTemp2 );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ") == 0\n" ) : ( ") != 0\n" ) );
			}

			return ( bNot ) ? ( ( iTemp1 & iTemp2 ) == 0 ) : ( ( iTemp1 & iTemp2 ) != 0 );
		}

		case LESSON_ACTION_BIT_COUNT_IS:
		{
			int iTemp1 = UTIL_CountNumBitsSet( static_cast<unsigned int>( fVar ) );
			int iTemp2 = static_cast<int>( fParam );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tUTIL_CountNumBitsSet([%s]) ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", iTemp1 );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%s] " ) : ( " == [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i\n", iTemp2 );
			}

			return ( bNot ) ? ( iTemp1 != iTemp2 ) : ( iTemp1 == iTemp2 );
		}

		case LESSON_ACTION_BIT_COUNT_LESS_THAN:
		{
			int iTemp1 = UTIL_CountNumBitsSet( static_cast<unsigned int>( fVar ) );
			int iTemp2 = static_cast<int>( fParam );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tUTIL_CountNumBitsSet([%s]) ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", iTemp1 );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " >= [%s] " ) : ( " < [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i\n", iTemp2 );
			}

			return ( bNot ) ? ( iTemp1 >= iTemp2 ) : ( iTemp1 < iTemp2 );
		}
	}

	DevWarning( "Invalid lesson action type used with \"%s\" variable type.\n", pchVarName );

	return false;
}

bool CScriptedIconLesson::ProcessElementAction( int iAction, bool bNot, const char *pchVarName, int &iVar, const CGameInstructorSymbol *pchParamName, float fParam )
{
	float fTemp = static_cast<float>( iVar );
	bool bRetVal = ProcessElementAction( iAction, bNot, pchVarName, fTemp, pchParamName, fParam );

	iVar = static_cast<int>( fTemp );
	return bRetVal;
}

bool CScriptedIconLesson::ProcessElementAction( int iAction, bool bNot, const char *pchVarName, bool &bVar, const CGameInstructorSymbol *pchParamName, float fParam )
{
	float fTemp = ( bVar ) ? ( 1.0f ) : ( 0.0f );
	bool bRetVal = ProcessElementAction( iAction, bNot, pchVarName, fTemp, pchParamName, fParam );

	bVar = ( fTemp != 0.0f );
	return bRetVal;
}

bool CScriptedIconLesson::ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, C_BaseEntity *pParam, const char *pchParam )
{
	// First try to let the mod act on the action
	bool bModHandled = false;
	bool bModReturn = Mod_ProcessElementAction( iAction, bNot, pchVarName, hVar, pchParamName, fParam, pParam, pchParam, bModHandled );

	if ( bModHandled )
	{
		return bModReturn;
	}

	C_BaseEntity *pVar = hVar.Get();

	switch ( iAction )
	{
		case LESSON_ACTION_SET:
		{
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = [%s]\n", pchVarName, pchParamName->String() );
			}

			hVar = pParam;
			return true;
		}

		case LESSON_ACTION_IS:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t[%s] != [%s]\n" ) : ( "\t[%s] == [%s]\n" ), pchVarName, pchParamName->String() );
			}

			return ( bNot ) ? ( pVar != pParam ) : ( pVar == pParam );

		case LESSON_ACTION_GET_DISTANCE:
		{
			if ( !pVar || !pParam )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->DistTo( [%s] )", pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "...\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle or Param handle returned NULL!\n" );
				}

				return false;
			}
			
			C_BasePlayer *pVarPlayer = ( pVar->IsPlayer() ? static_cast< C_BasePlayer* >( pVar ) : nullptr );
			C_BasePlayer *pParamPlayer = ( pParam->IsPlayer() ? static_cast< C_BasePlayer* >( pParam ) : nullptr );

			Vector vVarPos = ( pVarPlayer ? pVarPlayer->ActivePlayerCombatCharacter()->EyePosition() : pVar->WorldSpaceCenter() );
			Vector vParamPos = ( pParamPlayer ? pParamPlayer->ActivePlayerCombatCharacter()->EyePosition() : pParam->WorldSpaceCenter() );

			m_fOutput = vVarPos.DistTo( vParamPos );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->DistTo( [%s] ) ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", m_fOutput );
			}

			return true;
		}

		case LESSON_ACTION_GET_ANGULAR_DISTANCE:
		{
			if ( !pVar || !pParam )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->AngularDistTo( [%s] )", pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "...\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle or Param handle returned NULL!\n" );
				}

				return false;
			}

			C_BasePlayer *pVarPlayer = ( pVar->IsPlayer() ? static_cast< C_BasePlayer* >( pVar ) : nullptr );
			C_BasePlayer *pParamPlayer = ( pParam->IsPlayer() ? static_cast< C_BasePlayer* >( pParam ) : nullptr );

			Vector vVarPos = ( pVarPlayer ? pVarPlayer->ActivePlayerCombatCharacter()->EyePosition() : pVar->WorldSpaceCenter() );
			Vector vParamPos = ( pParamPlayer ? pParamPlayer->ActivePlayerCombatCharacter()->EyePosition() : pParam->WorldSpaceCenter() );

			Vector vVarToParam = vParamPos - vVarPos;
			VectorNormalize( vVarToParam );

			Vector vVarForward;

			if ( pVar->IsPlayer() )
			{
				AngleVectors( static_cast< C_BasePlayer* >( pVar )->ActivePlayerCombatCharacter()->EyeAngles(), &vVarForward, nullptr, nullptr );
			}
			else
			{
				pVar->GetVectors( &vVarForward, nullptr, nullptr );
			}

			// Set the distance in degrees
			m_fOutput = ( vVarToParam.Dot( vVarForward ) - 1.0f ) * -90.0f;

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->AngularDistTo( [%s] ) ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", m_fOutput );
			}

			return true;
		}

		case LESSON_ACTION_GET_PLAYER_DISPLAY_NAME:
		{
			int iTemp = static_cast<int>( fParam );

			if ( iTemp <= 0 || iTemp > 2 )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcpy( [stringINVALID], [%s]->GetPlayerName() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam selecting string is out of range!\n" );
				}

				return false;
			}

			// Use string2 if it was specified, otherwise, use string1
			CGameInstructorSymbol *pString;
			char const *pchParamNameTemp = nullptr;

			if ( iTemp == 2 )
			{
				pString = &m_szString2;
				pchParamNameTemp = "string2";
			}
			else
			{
				pString = &m_szString1;
				pchParamNameTemp = "string1";
			}

			if ( !pVar )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcpy( [%s], [%s]->GetPlayerName() ", pchParamNameTemp, pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle returned NULL!\n" );
				}

				return false;
			}

			*pString = pVar->GetPlayerName();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcpy( [%s], [%s]->GetPlayerName() ", pchParamNameTemp, pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pString->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
			}

			return true;
		}

		case LESSON_ACTION_CLASSNAME_IS:
		{
			if ( !pVar )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t!FClassnameIs( [%s] " ) : ( "\tFClassnameIs( [%s] " ), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "..." );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String()  );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );

					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t!FClassnameIs( [%s] " ) : ( "\tFClassnameIs( [%s] " ), pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s", pVar->GetClassname() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String()  );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
			}

			return ( bNot ) ? ( !FClassnameIs( pVar, pchParam ) ) : ( FClassnameIs( pVar, pchParam ) );
		}

		case LESSON_ACTION_TEAM_IS:
		{
			int iTemp = static_cast<int>( fParam );

			if ( !pVar )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetTeamNumber() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "!= [%s] " ) : ( "== [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i\n", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetTeamNumber() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", pVar->GetTeamNumber() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "!= [%s] " ) : ( "== [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i\n", iTemp );
			}

			return ( bNot ) ? ( pVar->GetTeamNumber() != iTemp ) : ( pVar->GetTeamNumber() == iTemp );
		}

		case LESSON_ACTION_MODELNAME_IS:
		{
			C_BaseAnimating *pBaseAnimating = dynamic_cast<C_BaseAnimating *>( pVar );

			if ( !pBaseAnimating )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_stricmp( [%s]->ModelName() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "..." );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String()  );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ") != 0\n" ) : ( ") == 0\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseAnimating returned NULL!\n" );
				}

				return false;
			}

			const char *pchModelName = "-no model-";
			CStudioHdr *pModel = pBaseAnimating->GetModelPtr();
			if ( pModel )
			{
				const studiohdr_t *pRenderHDR = pModel->GetRenderHdr();
				if ( pRenderHDR )
				{
					pchModelName = pRenderHDR->name;
				}
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_stricmp( [%s]->ModelName() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s", pchModelName );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String()  );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ") != 0\n" ) : ( ") == 0\n" ) );
			}

			return ( bNot ) ? ( V_stricmp( pchModelName, pchParam ) != 0 ) : ( V_stricmp( pchModelName, pchParam ) == 0 );
		}
		
		case LESSON_ACTION_HEALTH_LESS_THAN:
		{
			int iTemp = static_cast<int>( fParam );

			if ( !pVar )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetHealth() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i\n", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetHealth() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", pVar->GetHealth() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i\n", iTemp );
			}

			return ( bNot ) ? ( pVar->GetHealth() >= iTemp ) : ( pVar->GetHealth() < iTemp );
		}

		case LESSON_ACTION_HEALTH_PERCENTAGE_LESS_THAN:
		{
			if ( !pVar )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HealthFraction() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HealthFraction() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f ", pVar->HealthFraction() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", fParam );
			}

			float fHealthPercentage = 1.0f;
			
			if ( pVar->GetMaxHealth() != 0.0f )
			{
				fHealthPercentage = pVar->HealthFraction();
			}

			return ( bNot ) ? ( fHealthPercentage >= fParam ) : ( fHealthPercentage < fParam );
		}

		case LESSON_ACTION_GET_ACTIVE_WEAPON:
		{
			int iTemp = static_cast<int>( fParam );

			if ( iTemp <= 0 || iTemp > 2 )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[entityINVALID] = [%s]->GetActiveWeapon()\n", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam selecting string is out of range!\n" );
				}

				return false;
			}

			// Use entity2 if it was specified, otherwise, use entity1
			CHandle<C_BaseEntity> *pHandle;

			char const *pchParamNameTemp = nullptr;

			if ( iTemp == 2 )
			{
				pHandle = &m_hEntity2;
				pchParamNameTemp = "entity2";
			}
			else
			{
				pHandle = &m_hEntity1;
				pchParamNameTemp = "entity1";
			}

			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = [%s]->GetActiveWeapon()", pchParamNameTemp, pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			pHandle->Set( pBaseCombatCharacter->GetActiveWeapon() );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = [%s]->GetActiveWeapon()", pchParamNameTemp, pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"\n", pchParam );
			}

			return true;
		}

		case LESSON_ACTION_WEAPON_IS:
		{
			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetActiveWeapon()->GetName() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "!= [%s] " ) : ( "== [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"\n", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			CBaseCombatWeapon *pBaseCombatWeapon = pBaseCombatCharacter->GetActiveWeapon();

			if ( !pBaseCombatWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetActiveWeapon()->GetName() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "!= [%s] " ) : ( "== [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"\n", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar GetActiveWeapon returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetActiveWeapon()->GetName() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pBaseCombatWeapon->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "!= [%s] " ) : ( "== [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"\n", pchParam );
			}

			return ( bNot ) ? ( V_stricmp( pBaseCombatWeapon->GetName(), pchParam ) != 0 ) : ( V_stricmp( pBaseCombatWeapon->GetName(), pchParam ) == 0 );
		}

		case LESSON_ACTION_WEAPON_HAS:
		{
			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->Weapon_OwnsThisType([%s] " ) : ( "\t[%s]->Weapon_OwnsThisType([%s] " ), pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->Weapon_OwnsThisType([%s] " ) : ( "\t[%s]->Weapon_OwnsThisType([%s] " ), pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
			}

			return ( bNot ) ? ( pBaseCombatCharacter->Weapon_OwnsThisType( pchParam ) == nullptr ) : ( pBaseCombatCharacter->Weapon_OwnsThisType( pchParam ) != nullptr );
		}

		case LESSON_ACTION_GET_ACTIVE_WEAPON_SLOT:
		{
			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->Weapon_GetActiveSlot() ...\n", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			C_BaseCombatWeapon *pWeapon = pBaseCombatCharacter->GetActiveWeapon();

			if ( !pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->Weapon_GetActiveSlot() ...\n", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar GetActiveWeapon returned NULL!\n" );
				}

				return false;
			}

			m_fOutput = pWeapon->GetSlot();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->Weapon_GetSlot() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", m_fOutput );
			}

			return true;
		}

		case LESSON_ACTION_GET_WEAPON_SLOT:
		{
			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->Weapon_GetSlot([%s] ", pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ") ...\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			C_BaseCombatWeapon *pWeapon = pBaseCombatCharacter->Weapon_OwnsThisType( pchParam );
			m_fOutput = (pWeapon != nullptr) ? pWeapon->GetSlot() : 0.0f;

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[output] = [%s]->Weapon_GetSlot([%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ") " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%f\n", m_fOutput );
			}

			return true;
		}

		case LESSON_ACTION_GET_WEAPON_IN_SLOT:
		{
			int nTemp = static_cast<int>( fParam );

			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[entity1] = [%s]->GetWeapon([%s] ", pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%i\"", nTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			m_hEntity1 = pBaseCombatCharacter->GetWeapon( nTemp );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[entity1] = [%s]->GetWeapon([%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%i\"", nTemp );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
			}

			return true;
		}

		case LESSON_ACTION_CLIP_PERCENTAGE_LESS_THAN:
		{
			C_BaseCombatCharacter *pBaseCombatCharacter = nullptr;

			if ( pVar )
			{
				pBaseCombatCharacter = pVar->MyCombatCharacterPointer();
			}

			if ( !pBaseCombatCharacter )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetActiveWeapon()->Clip1Percentage() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%.1f\n", fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter returned NULL!\n" );
				}

				return false;
			}

			CBaseCombatWeapon *pBaseCombatWeapon = pBaseCombatCharacter->GetActiveWeapon();

			if ( !pBaseCombatWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetActiveWeapon()->Clip1Percentage() ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%.1f\n", fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar GetActiveWeapon returned NULL!\n" );
				}

				return false;
			}

			float fClip1Percentage = 100.0f;

			if ( pBaseCombatWeapon->UsesClipsForAmmo1() )
			{
				fClip1Percentage = 100.0f * ( static_cast<float>( pBaseCombatWeapon->Clip1() ) / static_cast<float>( pBaseCombatWeapon->GetMaxClip1() ) );
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetActiveWeapon()->Clip1Percentage() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%.1f ", fClip1Percentage );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ">= [%s] " ) : ( "< [%s] " ), pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%.1f\n", fParam );
			}

			return ( bNot ) ? ( fClip1Percentage >= fParam ) : ( fClip1Percentage < fParam );
		}

		case LESSON_ACTION_WEAPON_AMMO_LOW:
		{
			int iTemp = static_cast<int>( fParam );

			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetWeaponInSlot( ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ")->AmmoPercentage() >= 30\n" ) : ( ")->AmmoPercentage() < 30\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BasePlayer returned NULL!\n" );
				}

				return false;
			}

			CBaseCombatWeapon *pBaseCombatWeapon = nullptr;

			// Get the weapon in variable slot
			for ( int iWeapon = 0; iWeapon < MAX_WEAPONS; iWeapon++ )
			{
				CBaseCombatWeapon *pBaseCombatWeaponTemp = pBasePlayer->ActivePlayerCombatCharacter()->GetWeapon( iWeapon );
				if ( pBaseCombatWeaponTemp )
				{
					if ( pBaseCombatWeaponTemp->GetSlot() == iTemp )
					{
						pBaseCombatWeapon = pBaseCombatWeaponTemp;
						break;
					}
				}
			}

			if ( !pBaseCombatWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetWeaponInSlot( ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ")->AmmoPercentage() >= 30\n" ) : ( ")->AmmoPercentage() < 30\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar GetActiveWeapon returned NULL!\n" );
				}

				return false;
			}

			// Check if the ammo is full
			int iMaxAmmo = pBaseCombatWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY );
			int iPlayerAmmo = pBaseCombatWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );


			bool bAmmoLow = ( iPlayerAmmo < ( iMaxAmmo / 3 ) );

			if ( bNot )
			{
				bAmmoLow = !bAmmoLow;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetWeaponInSlot( ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%i ", iTemp );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ")->AmmoPercentage() >= 30 " ) : ( ")->AmmoPercentage() < 30 " ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bAmmoLow ) ? ( "true\n" ) : ( "false\n" ) );
			}

			return bAmmoLow;
		}

		case LESSON_ACTION_WEAPON_AMMO_FULL:
		{
			int iTemp = static_cast<int>( fParam );

			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->GetWeaponInSlot( " ) : ( "\t[%s]->GetWeaponInSlot( " ), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "%i ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")->AmmoFull()\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BasePlayer returned NULL!\n" );
				}

				return false;
			}

			CBaseCombatWeapon *pBaseCombatWeapon = nullptr;

			// Get the weapon in variable slot
			for ( int iWeapon = 0; iWeapon < MAX_WEAPONS; iWeapon++ )
			{
				CBaseCombatWeapon *pBaseCombatWeaponTemp = pBasePlayer->ActivePlayerCombatCharacter()->GetWeapon( iWeapon );
				if ( pBaseCombatWeaponTemp )
				{
					if ( pBaseCombatWeaponTemp->GetSlot() == iTemp )
					{
						pBaseCombatWeapon = pBaseCombatWeaponTemp;
						break;
					}
				}
			}

			if ( !pBaseCombatWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->GetWeaponInSlot( " ) : ( "\t[%s]->GetWeaponInSlot( " ), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "%i ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")->AmmoFull()\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar GetWeaponInSlot returned NULL!\n" );
				}

				return false;
			}

			// Check if the ammo is full
			int iMaxAmmo = pBaseCombatWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY );
			int iPlayerAmmo = pBaseCombatWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

			bool bAmmoFull = ( iPlayerAmmo >= iMaxAmmo );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->GetWeaponInSlot( " ) : ( "\t[%s]->GetWeaponInSlot( " ), pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "%i ", iTemp );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")->AmmoFull() " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, ( bAmmoFull ) ? ( "true\n" ) : ( "false\n" ) );
			}

			return ( bNot ) ? ( !bAmmoFull ) : ( bAmmoFull );
		}

		case LESSON_ACTION_WEAPON_AMMO_EMPTY:
		{
			int iTemp = static_cast<int>( fParam );

			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->GetWeaponInSlot( " ) : ( "\t[%s]->GetWeaponInSlot( " ), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "%i ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")->AmmoEmpty()\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BasePlayer returned NULL!\n" );
				}

				return false;
			}

			CBaseCombatWeapon *pBaseCombatWeapon = nullptr;

			// Get the weapon in variable slot
			for ( int iWeapon = 0; iWeapon < MAX_WEAPONS; iWeapon++ )
			{
				CBaseCombatWeapon *pBaseCombatWeaponTemp = pBasePlayer->ActivePlayerCombatCharacter()->GetWeapon( iWeapon );
				if ( pBaseCombatWeaponTemp )
				{
					if ( pBaseCombatWeaponTemp->GetSlot() == iTemp )
					{
						pBaseCombatWeapon = pBaseCombatWeaponTemp;
						break;
					}
				}
			}

			if ( !pBaseCombatWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->GetWeaponInSlot( " ) : ( "\t[%s]->GetWeaponInSlot( " ), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "%i ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")->AmmoEmpty()\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar GetWeaponInSlot returned NULL!\n" );
				}

				return false;
			}

			// Check if the ammo is empty
			int iPlayerAmmo = pBaseCombatWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

			bool bAmmoEmpty = ( iPlayerAmmo <= 0 );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->GetWeaponInSlot( " ) : ( "\t[%s]->GetWeaponInSlot( " ), pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "%i ", iTemp );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")->AmmoEmpty() " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, ( bAmmoEmpty ) ? ( "true" ) : ( "false" ) );
				ConColorMsg(CBaseLesson::m_rgbaVerbosePlain, " )\n" );
			}

			return ( bNot ) ? ( !bAmmoEmpty ) : ( bAmmoEmpty );
		}

		case LESSON_ACTION_WEAPON_CAN_USE:
		{
			C_BaseCombatWeapon *pBaseCombatWeapon = dynamic_cast<C_BaseCombatWeapon*>( pParam );
			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->Weapon_CanUse([%s])\n" ) : ( "\t[%s]->Weapon_CanUse([%s])\n" ), pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BasePlayer returned NULL!\n" );
				}

				return false;
			}

			if ( !pBaseCombatWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->Weapon_CanUse([%s])\n" ) : ( "\t[%s]->Weapon_CanUse([%s])\n" ), pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam BaseCombatWeapon returned NULL!\n" );
				}

				return false;
			}

			bool bCanEquip = pBasePlayer->Weapon_CanUse( pBaseCombatWeapon );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->Weapon_CanUse([%s]) " ) : ( "\t[%s]->Weapon_CanUse([%s]) " ), pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, ( bCanEquip ) ? ( "true\n" ) : ( "false\n" ) );
			}

			return ( bNot ) ? ( !bCanEquip ) : ( bCanEquip );
		}

		case LESSON_ACTION_USE_TARGET_IS:
		{
			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\tC_BaseEntity::Instance([%s]->GetUseEntity()) != [%s]\n" ) : ( "\tC_BaseEntity::Instance([%s]->GetUseEntity()) == [%s]\n" ), pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as Player returned NULL!\n" );
				}

				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\tC_BaseEntity::Instance([%s]->GetUseEntity()) != [%s]\n" ) : ( "\tC_BaseEntity::Instance([%s]->GetUseEntity()) == [%s]\n" ), pchVarName, pchParamName->String() );
			}

			return ( bNot ) ? ( C_BaseEntity::Instance( pBasePlayer->GetUseEntity() ) != pParam ) : ( C_BaseEntity::Instance( pBasePlayer->GetUseEntity() ) == pParam );
		}

		case LESSON_ACTION_GET_USE_TARGET:
		{
			int iTemp = static_cast<int>( fParam );

			if ( iTemp <= 0 || iTemp > 2 )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[entityINVALID] = C_BaseEntity::Instance([%s]->GetUseEntity())\n", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam selecting string is out of range!\n" );
				}

				return false;
			}

			// Use entity2 if it was specified, otherwise, use entity1
			CHandle<C_BaseEntity> *pHandle;
			char const *pchParamNameTemp = nullptr;

			if ( iTemp == 2 )
			{
				pHandle = &m_hEntity2;
				pchParamNameTemp = "entity2";
			}
			else
			{
				pHandle = &m_hEntity1;
				pchParamNameTemp = "entity1";
			}

			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = C_BaseEntity::Instance([%s]->GetUseEntity())\n", pchParamNameTemp, pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as Player returned NULL!\n" );
				}

				return false;
			}

			pHandle->Set( C_BaseEntity::Instance( pBasePlayer->GetUseEntity() ) );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = C_BaseEntity::Instance([%s]->GetUseEntity())\n", pchParamNameTemp, pchVarName );
			}

			return true;
		}

		case LESSON_ACTION_GET_POTENTIAL_USE_TARGET:
		{
			int iTemp = static_cast<int>( fParam );

			if ( iTemp <= 0 || iTemp > 2 )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[entityINVALID] = C_BaseEntity::Instance([%s]->GetPotentialUseEntity())\n", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam selecting string is out of range!\n" );
				}

				return false;
			}

			// Use entity2 if it was specified, otherwise, use entity1
			CHandle<C_BaseEntity> *pHandle;
			char const *pchParamNameTemp = nullptr;

			if ( iTemp == 2 )
			{
				pHandle = &m_hEntity2;
				pchParamNameTemp = "entity2";
			}
			else
			{
				pHandle = &m_hEntity1;
				pchParamNameTemp = "entity1";
			}

			C_BasePlayer *pBasePlayer = ToBasePlayer( pVar );

			if ( !pBasePlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = C_BaseEntity::Instance([%s]->GetPotentialUseEntity())\n", pchParamNameTemp, pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as Player returned NULL!\n" );
				}

				return false;
			}

			pHandle->Set( C_BaseEntity::Instance( pBasePlayer->GetPotentialUseEntity() ) );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = C_BaseEntity::Instance([%s]->GetPotentialUseEntity())\n", pchParamNameTemp, pchVarName );
			}

			return true;
		}
	}

	DevWarning( "Invalid lesson action type used with \"%s\" variable type.\n", pchVarName );

	return false;
}

bool CScriptedIconLesson::ProcessElementAction( int iAction, bool bNot, const char *pchVarName, CGameInstructorSymbol *pchVar, const CGameInstructorSymbol *pchParamName, const char *pchParam )
{
	switch ( iAction )
	{
		case LESSON_ACTION_REFERENCE_OPEN:
		{
			const CBaseLesson *pLesson = GetGameInstructor().GetLesson( pchParamName->String() );
			if ( !pLesson )
			{
				DevWarning( "Invalid lesson specified: \"%s\".", pchParamName->String() );
				return false;
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( Color( 64, 128, 255, 255 ), ( bNot ) ? ( "\t!( [\"%s\"]->IsInstanceActive() " ) : ( "\t( [\"%s\"]->IsInstanceActive() " ), pchParamName->String() );
				ConColorMsg( Color( 255, 255, 255, 255 ), "\"%s\"", (pLesson->IsInstanceActive() ? "true" : "false") );
				ConColorMsg( Color( 64, 128, 255, 255 ), " )\n" );
			}

			return ( bNot ) ? ( !pLesson->IsInstanceActive() ) : ( pLesson->IsInstanceActive() );
		}

		case LESSON_ACTION_SET:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcpy([%s], [%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
			}

			*pchVar = pchParam;
			return true;

		case LESSON_ACTION_ADD:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcat([%s], [%s] ", pchVarName, pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
			}

			char szTemp[ 256 ];
			V_strncpy( szTemp, pchVar->String(), sizeof( szTemp ) );
			V_strncat( szTemp, pchParam, sizeof( szTemp ) );

			*pchVar = szTemp;
			return true;

		case LESSON_ACTION_IS:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcmp([%s] ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchVar->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ") != 0\n" ) : ( ") == 0\n" ) );
			}

			return ( bNot ) ? ( V_strcmp( pchVar->String(), pchParam ) != 0 ) : ( V_strcmp( pchVar->String(), pchParam ) == 0 );

		case LESSON_ACTION_HAS_PREFIX:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tStringHasPrefix([%s] ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchVar->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ") == false\n" ) : ( ") == true\n" ) );
			}

			return ( bNot ) ? ( !StringHasPrefix( pchVar->String(), pchParam ) ) : ( StringHasPrefix( pchVar->String(), pchParam ) );

		case LESSON_ACTION_LESS_THAN:
			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tQ_strcmp([%s] ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\"%s\"", pchVar->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\"%s\"", pchParam );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( ") >= 0\n" ) : ( ") < 0\n" ) );
			}

			return ( bNot ) ? ( V_strcmp( pchVar->String(), pchParam ) >= 0 ) : ( V_strcmp( pchVar->String(), pchParam ) < 0 );
	}

	DevWarning( "Invalid lesson action type used with \"%s\" variable type.\n", pchVarName );

	return false;
}

LessonEvent_t * CScriptedIconLesson::AddOpenEvent( void )
{
	int iNewLessonEvent = std::ssize( m_OpenEvents ); m_OpenEvents.emplace_back();
	return &(m_OpenEvents[ iNewLessonEvent ]);
}

LessonEvent_t * CScriptedIconLesson::AddCloseEvent( void )
{
	int iNewLessonEvent = std::ssize( m_CloseEvents ); m_CloseEvents.emplace_back();
	return &(m_CloseEvents[ iNewLessonEvent ]);
}

LessonEvent_t * CScriptedIconLesson::AddSuccessEvent( void )
{
	int iNewLessonEvent = std::ssize( m_SuccessEvents ); m_SuccessEvents.emplace_back();
	return &(m_SuccessEvents[ iNewLessonEvent ]);
}

LessonEvent_t * CScriptedIconLesson::AddOnOpenEvent( void )
{
	int iNewLessonEvent = std::ssize( m_OnOpenEvents ); m_OnOpenEvents.emplace_back();
	return &(m_OnOpenEvents[ iNewLessonEvent ]);
}

LessonEvent_t * CScriptedIconLesson::AddUpdateEvent( void )
{
	int iNewLessonEvent = std::ssize( m_UpdateEvents ); m_UpdateEvents.emplace_back();
	return &(m_UpdateEvents[ iNewLessonEvent ]);
}

// Static method to init the keyvalues symbols used for comparisons
void CScriptedIconLesson::PreReadLessonsFromFile()
{
	static bool bFirstTime = true;
	if ( !bFirstTime )
		return;
	bFirstTime = false;

	// Run init info call macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
g_nVOIDSymbol = KeyValuesSystem()->GetSymbolForString( "VOID" );
g_nLOCAL_PLAYERSymbol = KeyValuesSystem()->GetSymbolForString( "LOCAL_PLAYER" );
g_nOUTPUTSymbol = KeyValuesSystem()->GetSymbolForString( "OUTPUT" );
g_nENTITY1Symbol = KeyValuesSystem()->GetSymbolForString( "ENTITY1" );
g_nENTITY2Symbol = KeyValuesSystem()->GetSymbolForString( "ENTITY2" );
g_nSTRING1Symbol = KeyValuesSystem()->GetSymbolForString( "STRING1" );
g_nSTRING2Symbol = KeyValuesSystem()->GetSymbolForString( "STRING2" );
g_nINTEGER1Symbol = KeyValuesSystem()->GetSymbolForString( "INTEGER1" );
g_nINTEGER2Symbol = KeyValuesSystem()->GetSymbolForString( "INTEGER2" );
g_nFLOAT1Symbol = KeyValuesSystem()->GetSymbolForString( "FLOAT1" );
g_nFLOAT2Symbol = KeyValuesSystem()->GetSymbolForString( "FLOAT2" );
g_nICON_TARGETSymbol = KeyValuesSystem()->GetSymbolForString( "ICON_TARGET" );
g_nUI_TARGET_NAMESymbol = KeyValuesSystem()->GetSymbolForString( "UI_TARGET_NAME" );
g_nUI_TARGET_LOOKUPSymbol = KeyValuesSystem()->GetSymbolForString( "UI_TARGET_LOOKUP" );
g_nUI_TARGET_EDGESymbol = KeyValuesSystem()->GetSymbolForString( "UI_TARGET_EDGE" );
g_nFIXED_POSITION_XSymbol = KeyValuesSystem()->GetSymbolForString( "FIXED_POSITION_X" );
g_nFIXED_POSITION_YSymbol = KeyValuesSystem()->GetSymbolForString( "FIXED_POSITION_Y" );
g_nFIXED_POSITIONSymbol = KeyValuesSystem()->GetSymbolForString( "FIXED_POSITION" );
g_nNO_ICON_TARGETSymbol = KeyValuesSystem()->GetSymbolForString( "NO_ICON_TARGET" );
g_nALLOW_NODRAW_TARGETSymbol = KeyValuesSystem()->GetSymbolForString( "ALLOW_NODRAW_TARGET" );
g_nVISIBLESymbol = KeyValuesSystem()->GetSymbolForString( "VISIBLE" );
g_nSHOW_WHEN_OCCLUDEDSymbol = KeyValuesSystem()->GetSymbolForString( "SHOW_WHEN_OCCLUDED" );
g_nNO_OFFSCREENSymbol = KeyValuesSystem()->GetSymbolForString( "NO_OFFSCREEN" );
g_nFORCE_CAPTIONSymbol = KeyValuesSystem()->GetSymbolForString( "FORCE_CAPTION" );
g_nONSCREEN_ICONSymbol = KeyValuesSystem()->GetSymbolForString( "ONSCREEN_ICON" );
g_nOFFSCREEN_ICONSymbol = KeyValuesSystem()->GetSymbolForString( "OFFSCREEN_ICON" );
g_nICON_OFFSETSymbol = KeyValuesSystem()->GetSymbolForString( "ICON_OFFSET" );
g_nICON_RELATIVE_OFFSETSymbol = KeyValuesSystem()->GetSymbolForString( "ICON_RELATIVE_OFFSET" );
g_nRANGESymbol = KeyValuesSystem()->GetSymbolForString( "RANGE" );
g_nFLAGSSymbol = KeyValuesSystem()->GetSymbolForString( "FLAGS" );
g_nCAPTION_COLORSymbol = KeyValuesSystem()->GetSymbolForString( "CAPTION_COLOR" );
g_nGROUPSymbol = KeyValuesSystem()->GetSymbolForString( "GROUP" );
g_nCAPTIONSymbol = KeyValuesSystem()->GetSymbolForString( "CAPTION" );
g_nCAPTION_PARAMSymbol = KeyValuesSystem()->GetSymbolForString( "CAPTION_PARAM" );
g_nBINDINGSymbol = KeyValuesSystem()->GetSymbolForString( "BINDING" );
g_nGAMEPAD_BINDINGSymbol = KeyValuesSystem()->GetSymbolForString( "GAMEPAD_BINDING" );
g_nPRIORITYSymbol = KeyValuesSystem()->GetSymbolForString( "PRIORITY" );
g_nREPLACE_KEYSymbol = KeyValuesSystem()->GetSymbolForString( "REPLACE_KEY" );
g_nLOCK_DURATIONSymbol = KeyValuesSystem()->GetSymbolForString( "LOCK_DURATION" );
g_nCAN_OPEN_WHEN_DEADSymbol = KeyValuesSystem()->GetSymbolForString( "CAN_OPEN_WHEN_DEAD" );
g_nBUMP_WITH_TIMEOUT_WHEN_LEARNEDSymbol = KeyValuesSystem()->GetSymbolForString( "BUMP_WITH_TIMEOUT_WHEN_LEARNED" );
g_nONCE_LEARNED_NEVER_OPENSymbol = KeyValuesSystem()->GetSymbolForString( "ONCE_LEARNED_NEVER_OPEN" );
g_nCAN_TIMEOUT_WHILE_INACTIVESymbol = KeyValuesSystem()->GetSymbolForString( "CAN_TIMEOUT_WHILE_INACTIVE" );
g_nUSABLE_IN_MIDAIRSymbol = KeyValuesSystem()->GetSymbolForString( "USABLE_IN_MIDAIR" );
g_nTIMEOUTSymbol = KeyValuesSystem()->GetSymbolForString( "TIMEOUT" );
g_nUPDATE_INTERVALSymbol = KeyValuesSystem()->GetSymbolForString( "UPDATE_INTERVAL" );
g_nSTART_SOUNDSymbol = KeyValuesSystem()->GetSymbolForString( "START_SOUND" );

	// And build the map of variable name to enum
	// Run string to int macros on all scriptable variables (see: LESSON_VARIABLE_FACTORY definition)
g_NameToTypeMap.emplace( "VOID", LESSON_VARIABLE_VOID );
g_NameToTypeMap.emplace( "LOCAL_PLAYER", LESSON_VARIABLE_LOCAL_PLAYER );
g_NameToTypeMap.emplace( "OUTPUT", LESSON_VARIABLE_OUTPUT );
g_NameToTypeMap.emplace( "ENTITY1", LESSON_VARIABLE_ENTITY1 );
g_NameToTypeMap.emplace( "ENTITY2", LESSON_VARIABLE_ENTITY2 );
g_NameToTypeMap.emplace( "STRING1", LESSON_VARIABLE_STRING1 );
g_NameToTypeMap.emplace( "STRING2", LESSON_VARIABLE_STRING2 );
g_NameToTypeMap.emplace( "INTEGER1", LESSON_VARIABLE_INTEGER1 );
g_NameToTypeMap.emplace( "INTEGER2", LESSON_VARIABLE_INTEGER2 );
g_NameToTypeMap.emplace( "FLOAT1", LESSON_VARIABLE_FLOAT1 );
g_NameToTypeMap.emplace( "FLOAT2", LESSON_VARIABLE_FLOAT2 );
g_NameToTypeMap.emplace( "ICON_TARGET", LESSON_VARIABLE_ICON_TARGET );
g_NameToTypeMap.emplace( "UI_TARGET_NAME", LESSON_VARIABLE_UI_TARGET_NAME );
g_NameToTypeMap.emplace( "UI_TARGET_LOOKUP", LESSON_VARIABLE_UI_TARGET_LOOKUP );
g_NameToTypeMap.emplace( "UI_TARGET_EDGE", LESSON_VARIABLE_UI_TARGET_EDGE );
g_NameToTypeMap.emplace( "FIXED_POSITION_X", LESSON_VARIABLE_FIXED_POSITION_X );
g_NameToTypeMap.emplace( "FIXED_POSITION_Y", LESSON_VARIABLE_FIXED_POSITION_Y );
g_NameToTypeMap.emplace( "FIXED_POSITION", LESSON_VARIABLE_FIXED_POSITION );
g_NameToTypeMap.emplace( "NO_ICON_TARGET", LESSON_VARIABLE_NO_ICON_TARGET );
g_NameToTypeMap.emplace( "ALLOW_NODRAW_TARGET", LESSON_VARIABLE_ALLOW_NODRAW_TARGET );
g_NameToTypeMap.emplace( "VISIBLE", LESSON_VARIABLE_VISIBLE );
g_NameToTypeMap.emplace( "SHOW_WHEN_OCCLUDED", LESSON_VARIABLE_SHOW_WHEN_OCCLUDED );
g_NameToTypeMap.emplace( "NO_OFFSCREEN", LESSON_VARIABLE_NO_OFFSCREEN );
g_NameToTypeMap.emplace( "FORCE_CAPTION", LESSON_VARIABLE_FORCE_CAPTION );
g_NameToTypeMap.emplace( "ONSCREEN_ICON", LESSON_VARIABLE_ONSCREEN_ICON );
g_NameToTypeMap.emplace( "OFFSCREEN_ICON", LESSON_VARIABLE_OFFSCREEN_ICON );
g_NameToTypeMap.emplace( "ICON_OFFSET", LESSON_VARIABLE_ICON_OFFSET );
g_NameToTypeMap.emplace( "ICON_RELATIVE_OFFSET", LESSON_VARIABLE_ICON_RELATIVE_OFFSET );
g_NameToTypeMap.emplace( "RANGE", LESSON_VARIABLE_RANGE );
g_NameToTypeMap.emplace( "FLAGS", LESSON_VARIABLE_FLAGS );
g_NameToTypeMap.emplace( "CAPTION_COLOR", LESSON_VARIABLE_CAPTION_COLOR );
g_NameToTypeMap.emplace( "GROUP", LESSON_VARIABLE_GROUP );
g_NameToTypeMap.emplace( "CAPTION", LESSON_VARIABLE_CAPTION );
g_NameToTypeMap.emplace( "CAPTION_PARAM", LESSON_VARIABLE_CAPTION_PARAM );
g_NameToTypeMap.emplace( "BINDING", LESSON_VARIABLE_BINDING );
g_NameToTypeMap.emplace( "GAMEPAD_BINDING", LESSON_VARIABLE_GAMEPAD_BINDING );
g_NameToTypeMap.emplace( "PRIORITY", LESSON_VARIABLE_PRIORITY );
g_NameToTypeMap.emplace( "REPLACE_KEY", LESSON_VARIABLE_REPLACE_KEY );
g_NameToTypeMap.emplace( "LOCK_DURATION", LESSON_VARIABLE_LOCK_DURATION );
g_NameToTypeMap.emplace( "CAN_OPEN_WHEN_DEAD", LESSON_VARIABLE_CAN_OPEN_WHEN_DEAD );
g_NameToTypeMap.emplace( "BUMP_WITH_TIMEOUT_WHEN_LEARNED", LESSON_VARIABLE_BUMP_WITH_TIMEOUT_WHEN_LEARNED );
g_NameToTypeMap.emplace( "ONCE_LEARNED_NEVER_OPEN", LESSON_VARIABLE_ONCE_LEARNED_NEVER_OPEN );
g_NameToTypeMap.emplace( "CAN_TIMEOUT_WHILE_INACTIVE", LESSON_VARIABLE_CAN_TIMEOUT_WHILE_INACTIVE );
g_NameToTypeMap.emplace( "USABLE_IN_MIDAIR", LESSON_VARIABLE_USABLE_IN_MIDAIR );
g_NameToTypeMap.emplace( "TIMEOUT", LESSON_VARIABLE_TIMEOUT );
g_NameToTypeMap.emplace( "UPDATE_INTERVAL", LESSON_VARIABLE_UPDATE_INTERVAL );
g_NameToTypeMap.emplace( "START_SOUND", LESSON_VARIABLE_START_SOUND );

	// Set up mapping of field types
	g_TypeToParamTypeMap.emplace( "float", FIELD_FLOAT );
	g_TypeToParamTypeMap.emplace( "string", FIELD_STRING );
	g_TypeToParamTypeMap.emplace( "int", FIELD_INTEGER );
	g_TypeToParamTypeMap.emplace( "integer", FIELD_INTEGER );
	g_TypeToParamTypeMap.emplace( "short", FIELD_INTEGER );
	g_TypeToParamTypeMap.emplace( "long", FIELD_INTEGER );
	g_TypeToParamTypeMap.emplace( "bool", FIELD_BOOLEAN );
	g_TypeToParamTypeMap.emplace( "player", FIELD_CUSTOM );
	g_TypeToParamTypeMap.emplace( "entity", FIELD_EHANDLE );
	g_TypeToParamTypeMap.emplace( "convar", FIELD_EMBEDDED );
	g_TypeToParamTypeMap.emplace( "void", FIELD_VOID );

	// Set up the lesson action map
	
	CScriptedIconLesson::LessonActionMap.emplace( "scope in", LESSON_ACTION_SCOPE_IN );
	CScriptedIconLesson::LessonActionMap.emplace( "scope out", LESSON_ACTION_SCOPE_OUT );
	CScriptedIconLesson::LessonActionMap.emplace( "close", LESSON_ACTION_CLOSE );
	CScriptedIconLesson::LessonActionMap.emplace( "success", LESSON_ACTION_SUCCESS );
	CScriptedIconLesson::LessonActionMap.emplace( "lock", LESSON_ACTION_LOCK );
	CScriptedIconLesson::LessonActionMap.emplace( "present complete", LESSON_ACTION_PRESENT_COMPLETE );
	CScriptedIconLesson::LessonActionMap.emplace( "present start", LESSON_ACTION_PRESENT_START );
	CScriptedIconLesson::LessonActionMap.emplace( "present end", LESSON_ACTION_PRESENT_END );

	CScriptedIconLesson::LessonActionMap.emplace( "reference open", LESSON_ACTION_REFERENCE_OPEN );

	CScriptedIconLesson::LessonActionMap.emplace( "is multiplayer", LESSON_ACTION_IS_MULTIPLAYER );

	CScriptedIconLesson::LessonActionMap.emplace( "set", LESSON_ACTION_SET );
	CScriptedIconLesson::LessonActionMap.emplace( "add", LESSON_ACTION_ADD );
	CScriptedIconLesson::LessonActionMap.emplace( "subtract", LESSON_ACTION_SUBTRACT );
	CScriptedIconLesson::LessonActionMap.emplace( "multiply", LESSON_ACTION_MULTIPLY );
	CScriptedIconLesson::LessonActionMap.emplace( "is", LESSON_ACTION_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "less than", LESSON_ACTION_LESS_THAN );
	CScriptedIconLesson::LessonActionMap.emplace( "has prefix", LESSON_ACTION_HAS_PREFIX );
	CScriptedIconLesson::LessonActionMap.emplace( "has bit", LESSON_ACTION_HAS_BIT );
	CScriptedIconLesson::LessonActionMap.emplace( "bit count is", LESSON_ACTION_BIT_COUNT_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "bit count less than", LESSON_ACTION_BIT_COUNT_LESS_THAN );

	CScriptedIconLesson::LessonActionMap.emplace( "get distance", LESSON_ACTION_GET_DISTANCE );
	CScriptedIconLesson::LessonActionMap.emplace( "get angular distance", LESSON_ACTION_GET_ANGULAR_DISTANCE );
	CScriptedIconLesson::LessonActionMap.emplace( "get player display name", LESSON_ACTION_GET_PLAYER_DISPLAY_NAME );
	CScriptedIconLesson::LessonActionMap.emplace( "classname is", LESSON_ACTION_CLASSNAME_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "modelname is", LESSON_ACTION_MODELNAME_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "team is", LESSON_ACTION_TEAM_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "health less than", LESSON_ACTION_HEALTH_LESS_THAN );
	CScriptedIconLesson::LessonActionMap.emplace( "health percentage less than", LESSON_ACTION_HEALTH_PERCENTAGE_LESS_THAN );
	CScriptedIconLesson::LessonActionMap.emplace( "get active weapon", LESSON_ACTION_GET_ACTIVE_WEAPON );
	CScriptedIconLesson::LessonActionMap.emplace( "weapon is", LESSON_ACTION_WEAPON_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "weapon has", LESSON_ACTION_WEAPON_HAS );
	CScriptedIconLesson::LessonActionMap.emplace( "get active weapon slot", LESSON_ACTION_GET_ACTIVE_WEAPON_SLOT );
	CScriptedIconLesson::LessonActionMap.emplace( "get weapon slot", LESSON_ACTION_GET_WEAPON_SLOT );
	CScriptedIconLesson::LessonActionMap.emplace( "get weapon in slot", LESSON_ACTION_GET_WEAPON_IN_SLOT );
	CScriptedIconLesson::LessonActionMap.emplace( "clip percentage less than", LESSON_ACTION_CLIP_PERCENTAGE_LESS_THAN);
	CScriptedIconLesson::LessonActionMap.emplace( "weapon ammo low", LESSON_ACTION_WEAPON_AMMO_LOW );
	CScriptedIconLesson::LessonActionMap.emplace( "weapon ammo full", LESSON_ACTION_WEAPON_AMMO_FULL );
	CScriptedIconLesson::LessonActionMap.emplace( "weapon ammo empty", LESSON_ACTION_WEAPON_AMMO_EMPTY );
	CScriptedIconLesson::LessonActionMap.emplace( "weapon can use", LESSON_ACTION_WEAPON_CAN_USE );
	CScriptedIconLesson::LessonActionMap.emplace( "use target is", LESSON_ACTION_USE_TARGET_IS );
	CScriptedIconLesson::LessonActionMap.emplace( "get use target", LESSON_ACTION_GET_USE_TARGET );
	CScriptedIconLesson::LessonActionMap.emplace( "get potential use target", LESSON_ACTION_GET_POTENTIAL_USE_TARGET );

	// Add mod actions to the map
	Mod_PreReadLessonsFromFile();
}
