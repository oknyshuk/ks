#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "iclientmode.h"
#include "GameEventListener.h"
#include <baseviewport.h>
#include "usermessages.h"
#include "noopanimcontroller.h"
#include "cliententitylist.h"
#include "cdll_client_int.h"
#include "networkvar.h"
#include "baseplayer_shared.h"

class CBaseHudChat;
class CBaseHudWeaponSelection;
class C_BaseEntity;
class C_BasePlayer;


inline constexpr int ACHIEVEMENT_ANNOUNCEMENT_MIN_TIME = 10;
template <typename T1> [[gnu::always_inline]] inline auto USERID2PLAYER( T1 i ) { return ToBasePlayer( ClientEntityList().GetEnt( engine->GetPlayerForUserID( i ) ) ); }

extern IClientMode *GetClientModeNormal(); // must be implemented
extern IClientMode *GetFullscreenClientMode();

// This class implements client mode functionality common to HL2 and TF2.
class ClientModeShared : public IClientMode, public CGameEventListener
{
// IClientMode overrides.
public:
	DECLARE_CLASS_NOBASE( ClientModeShared );

					ClientModeShared();
	virtual			~ClientModeShared();
	
	virtual void	Init();
	virtual void	InitViewport();
	virtual void	UI_Shutdown();
	virtual void	Shutdown();

	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );

	virtual void	Enable();
	virtual void	Disable();
	virtual void	Layout( bool bForce = false );

	virtual void	OverrideView( CViewSetup *pSetup );
	virtual bool	OverrideRenderBounds( int &x, int &y, int &w, int &h, int &insetX, int &insetY ) { return false; }
	virtual void	OverrideAudioState( AudioState_t *pAudioState ) { return; }
	virtual bool	ShouldDrawDetailObjects( );
	virtual bool	ShouldDrawEntity(C_BaseEntity *pEnt);
	virtual bool	ShouldDrawLocalPlayer( C_BasePlayer *pPlayer );
	virtual bool	ShouldDrawViewModel();
	virtual bool	ShouldDrawParticles( );
	virtual bool	ShouldDrawCrosshair( void );
	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual void	PreRender(CViewSetup *pSetup);
	virtual void	PostRender();
	virtual void	PostRenderVGui();
	virtual void	ProcessInput(bool bActive);
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *cmd );
	virtual void	Update();
	virtual void	SetBlurFade( float scale ) {}
	virtual float	GetBlurFade( void ) { return 0.0f; }

	// Input
	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual void	OverrideMouseInput( float *x, float *y );
	virtual void	StartMessageMode( int iMessageModeType );

	// The mode can choose to not draw fog
	virtual bool	ShouldDrawFog( void );
	
	virtual float	GetViewModelFOV( void );

	// Gets at the viewports vgui panel animation controller, if there is one...
	virtual CNoopAnimController *GetViewportAnimationController()
		{ return m_pViewport->GetAnimationController(); }
	
	void FireGameEvent( IGameEvent *event );

	virtual bool CanRecordDemo( char *errorMsg, int length ) const { return true; }

	virtual int HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual void InitChatHudElement( void );
	virtual void InitWeaponSelectionHudElement( void );

	virtual wchar_t*	GetServerName( void ) { return nullptr; }
	virtual void		SetServerName( wchar_t *name ) {}
	virtual wchar_t*	GetMapName( void ) { return nullptr; }
	virtual void		SetMapName( wchar_t *name ) {}

	virtual void	UpdateCameraManUIState( int iType, int nOptionalParam, uint64 xuid );
	virtual void	ScoreboardOff( void );
	virtual void	GraphPageChanged( void );

	CUserMessageBinder m_UMCMsgVGUIMenu;
	CUserMessageBinder m_UMCMsgRumble;

protected:
	CBaseViewport			*m_pViewport;

	int			GetSplitScreenPlayerSlot() const;

private:
	// Message mode handling
	// All modes share a common chat interface
	CBaseHudChat			*m_pChatElement;
	CBaseHudWeaponSelection *m_pWeaponSelection;
	int						m_nRootSize[2];
};


bool ContainsBinding( const char *pszBindingString, const char *pszBinding, bool bSearchAliases = true );

void SendCameraManUIStateChange( HltvUiType_t eventType, int nOptionalParam = 0 );
