// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "IGameUIFuncs.h"
#include "game/client/iviewport.h"
#include "igameevents.h"
module ks.client:game_controls.baseviewport;

import std;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import :particlemgr;
import :noopanimcontroller;

class CNoopAnimController;

// viewport interface for the rest of the dll

class IGameEventManager;

//==============================================================================
class CBaseViewport : public IViewPort, public CGameEventListener
{
public: 
	CBaseViewport();
	virtual ~CBaseViewport();

	virtual IViewPortPanel* CreatePanelByName(const char *szPanelName);
	virtual IViewPortPanel* FindPanelByName(const char *szPanelName);
	virtual IViewPortPanel* GetActivePanel( void );
	virtual void LevelInit( void );
	virtual void RemoveAllPanels( void);
	virtual void RecreatePanel( const char *szPanelName );

	virtual void ShowPanel( const char *pName, bool state, KeyValues *data, bool autoDeleteData );
	virtual void ShowPanel( const char *pName, bool state );
	virtual void ShowPanel( IViewPortPanel* pPanel, bool state );
	virtual bool AddNewPanel( IViewPortPanel* pPanel, char const *pchDebugName );
	virtual void CreateDefaultPanels( void );
	virtual void UpdateAllPanels( void );
	virtual void PostMessageToPanel( const char *pName, KeyValues *pKeyValues );

	virtual void Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 *pGameEventManager );

	virtual void ReloadScheme(const char *fromFile);
	virtual void ActivateClientUI();
	virtual void HideClientUI();
	virtual bool AllowedToPrintText( void );

	void LoadHudLayout( void );

	virtual CNoopAnimController *GetAnimationController() { return &m_AnimController; }

	virtual void ShowBackGround(bool bShow) {}

	virtual int GetDeathMessageStartHeight( void );	

public: // Game event handler
	void FireGameEvent( IGameEvent * event);

protected:

	void SetAsFullscreenViewportInterface( void );
	bool IsFullscreenViewport() const;

protected:
	IGameUIFuncs*		m_GameuiFuncs; // for key binding details
	IGameEventManager2*	m_GameEventManager;

	ks::idict<IViewPortPanel*>	m_Panels;
	std::vector< IViewPortPanel* >	m_UnorderedPanels;

	bool				m_bHasParent;
	bool				m_bInitialized;
	bool				m_bFullscreenViewport;
	IViewPortPanel		*m_pActivePanel;

	CNoopAnimController	m_AnimController;
	int					m_OldSize[2];

private:
	virtual void InitViewportSingletons( void );
};

IViewPort *GetViewPortInterface();

IViewPort *GetFullscreenViewPortInterface();
