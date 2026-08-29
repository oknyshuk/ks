//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COUNTERSTRIKEVIEWPORT_H
#define COUNTERSTRIKEVIEWPORT_H

#include "cs_shareddefs.h"
#include "baseviewport.h"




class CCSTeamMenu;
class CCSClassMenu;
class CCSSpectatorGUI;
class CCSClientScoreBoard;
class CBuyMenu;
class CCSClientScoreBoardDialog;

void PrintBuyTimeOverMessage( void );

//==============================================================================
class CounterStrikeViewport : public CBaseViewport
{

private:
	typedef CBaseViewport BaseClass;

public:

	IViewPortPanel* CreatePanelByName( const char *szPanelName );
	void CreateDefaultPanels( void );

	virtual void Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager );
		
	int GetDeathMessageStartHeight( void );

	//=============================================================================
	// HPE_BEGIN:
	// [mhansen] We want to let the team screen know if this is the first time
	// we chose a team so we can decide what the "back" action is
	//=============================================================================
	bool GetChoseTeamAndClass() { return m_bChoseTeamAndClass; }
	void SetChoseTeamAndClass( bool chose ) { m_bChoseTeamAndClass = chose; }
	//=============================================================================
	// HPE_END
	//=============================================================================

	virtual void FireGameEvent( IGameEvent * event );

	virtual void UpdateAllPanels( void );



private:
	//=============================================================================
	// HPE_BEGIN:
	// [mhansen] We want to let the team screen know if this is the first time
	// we chose a team so we can decide what the "back" action is
	//=============================================================================
	bool m_bChoseTeamAndClass;
	//=============================================================================
	// HPE_END
	//=============================================================================
};


#endif // COUNTERSTRIKEVIEWPORT_H
