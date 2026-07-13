//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_MENU_H
#define HUD_MENU_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "hudelement.h"

#define MENU_SELECTION_TIMEOUT	5.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudMenu : public CHudElement
{
public:
	explicit CHudMenu( const char *pElementName );
	void Init( void );
	void VidInit( void );
	void Reset( void );
	virtual bool ShouldDraw( void );
	bool MsgFunc_ShowMenu( const CCSUsrMsg_ShowMenu &msg );
	void HideMenu( void );
	void ShowMenu( const char * menuName, int keySlot );
	void ShowMenu_KeyValueItems( KeyValues *pKV );

	bool IsMenuOpen( void );
	void SelectMenuItem( int menu_item );

	CUserMessageBinder m_UMCMsgShowMenu;

private:
	void OnThink();
	void		ProcessText( void );

	struct ProcessedLine
	{
		int	menuitem; // -1 for just text
		int startchar;
		int length;
		int pixels;
		int height;
	};

	CUtlVector< ProcessedLine >	m_Processed;

	int				m_nMaxPixels;
	int				m_nHeight;

	bool			m_bMenuDisplayed;
	int				m_bitsValidSlots;
	float			m_flShutoffTime;
	int				m_nSelectedItem;
	bool			m_bMenuTakesInput;

	float			m_flSelectionTime;

	float			m_flOpenCloseTime;
};

#endif // HUD_MENU_H
