// SPDX-License-Identifier: None
// Copyright (c) 2007 Turtle Rock Studios, Inc.
// Copyright 2006 Turtle Rock Studios, Inc.

module;
#include "client_class.h"
#include "edict.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "ScreenSpaceEffects.h"
module ks.client;

import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :basetoggle;
import :util;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :baseentity;
import ks.game.gamemovement;
import :particlemgr;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;

class CBasePlayer;

using namespace ::ks::reflect;

//--------------------------------------------------------------------------------------------------------

IMPLEMENT_REFLECT_CLIENTCLASS( CBaseToggle, DT_BaseToggle, CBaseToggle )

IMPLEMENT_REFLECT_PREDMAP( CBaseToggle );

ConVar cl_predict_basetoggles(side_registry(), "cl_predict_basetoggles", "1" );

//--------------------------------------------------------------------------------------------------------
// Returns the velocity imparted to players standing on us.
void CBaseToggle::GetGroundVelocityToApply( Vector &vecGroundVel )
{
	vecGroundVel = GetLocalVelocity();
	if( !GetPredictable() )
	{
		vecGroundVel.z = 0.0f; // don't give upward velocity, or it could predict players into the air.
	}
}

bool CBaseToggle::ShouldPredict( void )
{
	return cl_predict_basetoggles.GetBool() && (m_movementType == MOVE_TOGGLE_LINEAR);
}

CBasePlayer *CBaseToggle::GetPredictionOwner( void )
{
	CBasePlayer *pPlayer = (CBasePlayer *)m_hPredictionOwner.Get();

	if( pPlayer && pPlayer->IsLocalPlayer() )
		return pPlayer;

	return nullptr;
}

bool CBaseToggle::PredictionIsPhysicallySimulated( void )
{
	return (m_movementType == MOVE_TOGGLE_LINEAR);
}

void CBaseToggle::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );
	
	m_vLastNetworked = GetNetworkOrigin();
	m_fLastNetworkedTime = gpGlobals->curtime;

	if( GetPredictionEligible() && m_bWasPredictingMotion && (m_movementType == MOVE_TOGGLE_NONE) )
	{
		SetPredictionEligible( false );
		ResetLatched();
	}
	m_bWasPredictingMotion = (m_movementType != MOVE_TOGGLE_NONE);
}

void CBaseToggle::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();

	if( GetPredictable() )
	{
		Vector vOrigin = PredictPosition( GetPredictionOwner()->PredictedServerTime() );

		if( vOrigin == m_vecFinalDest )
		{
			SetLocalVelocity( vec3_origin );
		}
		SetLocalOrigin( vOrigin );
		SetNetworkOrigin( vOrigin );
	}	
}

Vector CBaseToggle::PredictPosition( float fCurTime )
{
	if( (m_movementType != MOVE_TOGGLE_LINEAR) || m_flMoveTargetTime <= 0.0f )
		return GetNetworkOrigin();

	if( m_flMoveTargetTime > fCurTime )
	{
		float fTargetTime = m_flMoveTargetTime;
		float fTimeRemaining = fTargetTime - fCurTime;
		float fTotalTime = fTargetTime - m_fLastNetworkedTime;

		if( fTotalTime <= 0.0f )
			return m_vecFinalDest;

		float fInterp = fTimeRemaining / fTotalTime;

		return (m_vLastNetworked * fInterp) + (m_vecFinalDest * (1.0f - fInterp));
	}
	else
	{
		return m_vecFinalDest;
	}
}

//--------------------------------------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CBaseButton, DT_BaseButton, CBaseButton )

//--------------------------------------------------------------------------------------------------------
bool CBaseButton::IsPotentiallyUsable( void )
{
	return true;
}

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CFuncBrush, DT_FuncBrush, CFuncBrush )

void CFuncBrush::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	VPhysicsShadowDataChanged(bCreate, this);
}

class IScreenSpaceEffect;

using namespace ::ks::reflect;

//------------------------------------------------------------------------------
// CScreenSpaceEffectRegistration code
// Used to register and effect with the IScreenSpaceEffectManager
//------------------------------------------------------------------------------
CScreenSpaceEffectRegistration *CScreenSpaceEffectRegistration::s_pHead = nullptr;

CScreenSpaceEffectRegistration::CScreenSpaceEffectRegistration( const char *pName, IScreenSpaceEffect *pEffect )
{
	m_pEffectName = pName;
	m_pEffect = pEffect;
	m_pNext = s_pHead;
	s_pHead = this;
}

//------------------------------------------------------------------------------
// CScreenSpaceEffectManager - Implementation of IScreenSpaceEffectManager
//------------------------------------------------------------------------------
class CScreenSpaceEffectManager : public IScreenSpaceEffectManager
{
public:

	virtual void InitScreenSpaceEffects( );
	virtual void ShutdownScreenSpaceEffects( );

	virtual IScreenSpaceEffect *GetScreenSpaceEffect( const char *pEffectName );

	virtual void SetScreenSpaceEffectParams( const char *pEffectName, KeyValues *params );
	virtual void SetScreenSpaceEffectParams( IScreenSpaceEffect *pEffect, KeyValues *params );
    
	virtual void EnableScreenSpaceEffect( const char *pEffectName );
	virtual void EnableScreenSpaceEffect( IScreenSpaceEffect *pEffect );

	virtual void DisableScreenSpaceEffect( const char *pEffectName );
	virtual void DisableScreenSpaceEffect( IScreenSpaceEffect *pEffect );

	virtual void DisableAllScreenSpaceEffects( );

	virtual void RenderEffects( int x, int y, int w, int h );
};

CScreenSpaceEffectManager g_ScreenSpaceEffectManager;
IScreenSpaceEffectManager *g_pScreenSpaceEffects = &g_ScreenSpaceEffectManager;

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::InitScreenSpaceEffects - Initialise all registered effects
//---------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::InitScreenSpaceEffects( )
{
	if ( CommandLine()->FindParm( "-filmgrain" ) )
	{
		GetScreenSpaceEffect( "filmgrain" )->Enable( true );
	}

	for( CScreenSpaceEffectRegistration *pReg=CScreenSpaceEffectRegistration::s_pHead; pReg!=nullptr; pReg=pReg->m_pNext )
	{
		IScreenSpaceEffect *pEffect = pReg->m_pEffect;
		if( pEffect )
		{
			bool bIsEnabled = pEffect->IsEnabled( );
			pEffect->Init( );
			pEffect->Enable( bIsEnabled );
		}
	}
}

//----------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::ShutdownScreenSpaceEffects - Shutdown all registered effects
//----------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::ShutdownScreenSpaceEffects( )
{
	for( CScreenSpaceEffectRegistration *pReg=CScreenSpaceEffectRegistration::s_pHead; pReg!=nullptr; pReg=pReg->m_pNext )
	{
		IScreenSpaceEffect *pEffect = pReg->m_pEffect;
		if( pEffect )
		{
			pEffect->Shutdown( );
		}
	}
}

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::GetScreenSpaceEffect - Returns a point to the named effect
//---------------------------------------------------------------------------------------
IScreenSpaceEffect *CScreenSpaceEffectManager::GetScreenSpaceEffect( const char *pEffectName )
{
	for( CScreenSpaceEffectRegistration *pReg=CScreenSpaceEffectRegistration::s_pHead; pReg!=nullptr; pReg=pReg->m_pNext )
	{
		if( !V_stricmp( pReg->m_pEffectName, pEffectName ) )
		{
			IScreenSpaceEffect *pEffect = pReg->m_pEffect;
			return pEffect;
		}
	}

	Warning( "Could not find screen space effect %s\n", pEffectName );

	return nullptr;
}

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::SetScreenSpaceEffectParams 
//	- Assign parameters to the specified effect
//---------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::SetScreenSpaceEffectParams( const char *pEffectName, KeyValues *params )
{
	IScreenSpaceEffect *pEffect = GetScreenSpaceEffect( pEffectName );
	if( pEffect )
		SetScreenSpaceEffectParams( pEffect, params );
}

void CScreenSpaceEffectManager::SetScreenSpaceEffectParams( IScreenSpaceEffect *pEffect, KeyValues *params )
{
	if( pEffect )
		pEffect->SetParameters( params );
}

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::EnableScreenSpaceEffect
//	- Enables the specified effect
//---------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::EnableScreenSpaceEffect( const char *pEffectName )
{
	IScreenSpaceEffect *pEffect = GetScreenSpaceEffect( pEffectName );
	if( pEffect )
		EnableScreenSpaceEffect( pEffect );
}

void CScreenSpaceEffectManager::EnableScreenSpaceEffect( IScreenSpaceEffect *pEffect )
{
	if( pEffect )
		pEffect->Enable( true );
}

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::DisableScreenSpaceEffect
//	- Disables the specified effect
//---------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::DisableScreenSpaceEffect( const char *pEffectName )
{
	IScreenSpaceEffect *pEffect = GetScreenSpaceEffect( pEffectName );
	if( pEffect )
		DisableScreenSpaceEffect( pEffect );
}

void CScreenSpaceEffectManager::DisableScreenSpaceEffect( IScreenSpaceEffect *pEffect )
{
	if( pEffect )
		pEffect->Enable( false );
}

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::DisableAllScreenSpaceEffects
//	- Disables all registered screen space effects
//---------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::DisableAllScreenSpaceEffects( )
{
	for( CScreenSpaceEffectRegistration *pReg=CScreenSpaceEffectRegistration::s_pHead; pReg!=nullptr; pReg=pReg->m_pNext )
	{
		IScreenSpaceEffect *pEffect = pReg->m_pEffect;
		if( pEffect )
		{
			pEffect->Enable( false );
		}
	}
}

//---------------------------------------------------------------------------------------
// CScreenSpaceEffectManager::RenderEffects
//	- Renders all registered screen space effects
//---------------------------------------------------------------------------------------
void CScreenSpaceEffectManager::RenderEffects( int x, int y, int w, int h )
{
	for( CScreenSpaceEffectRegistration *pReg=CScreenSpaceEffectRegistration::s_pHead; pReg!=nullptr; pReg=pReg->m_pNext )
	{
		IScreenSpaceEffect *pEffect = pReg->m_pEffect;
		if( pEffect )
		{
			pEffect->Render( x, y, w, h );
		}
	}
}

//------------------------------------------------------------------------------
// Example post-processing effect
//------------------------------------------------------------------------------
class CExampleEffect : public IScreenSpaceEffect
{
public:
	CExampleEffect( );
   ~CExampleEffect( );

	void Init( );
	void Shutdown( );

	void SetParameters( KeyValues *params );

	void Render( int x, int y, int w, int h );

	void Enable( bool bEnable );
	bool IsEnabled( );

private:

	bool				m_bEnable;

	CMaterialReference	m_Material;
};

ADD_SCREENSPACE_EFFECT( CExampleEffect, exampleeffect );

//------------------------------------------------------------------------------
// CExampleEffect constructor
//------------------------------------------------------------------------------
CExampleEffect::CExampleEffect( )
{
	m_bEnable = false;
}

//------------------------------------------------------------------------------
// CExampleEffect destructor
//------------------------------------------------------------------------------
CExampleEffect::~CExampleEffect( )
{
}

//------------------------------------------------------------------------------
// CExampleEffect init
//------------------------------------------------------------------------------
void CExampleEffect::Init( )
{
	// This is just example code, init your effect material here
	//m_Material.Init( "engine/exampleeffect", TEXTURE_GROUP_OTHER );

	m_bEnable = false;
}

//------------------------------------------------------------------------------
// CExampleEffect shutdown
//------------------------------------------------------------------------------
void CExampleEffect::Shutdown( )
{
	m_Material.Shutdown();
}

//------------------------------------------------------------------------------
// CExampleEffect enable
//------------------------------------------------------------------------------
void CExampleEffect::Enable( bool bEnable )
{
	// This is just example code, don't enable it
	// m_bEnable = bEnable;
}

bool CExampleEffect::IsEnabled( )
{
	return m_bEnable;
}

//------------------------------------------------------------------------------
// CExampleEffect SetParameters
//------------------------------------------------------------------------------
void CExampleEffect::SetParameters( KeyValues *params )
{
	if( params->GetDataType( "example_param" ) == KeyValues::TYPE_STRING )
	{
		// ...
	}
}

//------------------------------------------------------------------------------
// CExampleEffect render
//------------------------------------------------------------------------------
void CExampleEffect::Render( int x, int y, int w, int h )
{
	if ( !IsEnabled() )
		return;

	// Render Effect
	Rect_t actualRect;
	UpdateScreenEffectTexture( 0, x, y, w, h, false, &actualRect );
	ITexture *pTexture = GetFullFrameFrameBufferTexture( 0 );

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->DrawScreenSpaceRectangle( m_Material, x, y, w, h,
											actualRect.x, actualRect.y, actualRect.x+actualRect.width-1, actualRect.y+actualRect.height-1, 
											pTexture->GetActualWidth(), pTexture->GetActualHeight() );
}

using namespace ::ks::reflect;

// SharedFunctorUtils.cpp
// Useful functors

// #ifdef CLIENT_DLL
// 	#include "ClientTerrorPlayer.h"
// #else
// 	#include "TerrorPlayer.h"
// #endif
