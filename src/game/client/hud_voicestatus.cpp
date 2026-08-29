//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "localize/ilocalize.h"
#include "c_baseplayer.h"
#include "voice_status.h"
#include "clientmode_shared.h"
#include "c_playerresource.h"
#include "voice_common.h"
#include "bitvec.h"
#include "engineinterface.h"
#include "steam/steam_api.h"

#include "tier0/memdbgon.h"


extern ConVar sv_talk_enemy_dead;
extern ConVar sv_talk_enemy_living;
//=============================================================================
// Icon for the local player using voice
//=============================================================================
class CHudVoiceSelfStatus : public CHudElement
{
public:

	explicit CHudVoiceSelfStatus( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void VidInit();

private:
	CHudTexture *m_pVoiceIcon;


	Color	m_clrIcon;
};


//DECLARE_HUDELEMENT( CHudVoiceSelfStatus );


CHudVoiceSelfStatus::CHudVoiceSelfStatus( const char *pName ) :
	CHudElement( pName )
{
	m_pVoiceIcon = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_clrIcon = Color(255,255,255,255);
}

	
void CHudVoiceSelfStatus::VidInit( void )
{
}

bool CHudVoiceSelfStatus::ShouldDraw()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return false;

	if ( GetClientVoiceMgr()->IsLocalPlayerSpeaking( player->GetSplitScreenPlayerSlot() ) == false )
		return false;

	return CHudElement::ShouldDraw();	
}

void CHudVoiceSelfStatus::Paint()
{
}


//=============================================================================
// Icons for other players using voice
//=============================================================================
class CHudVoiceStatus : public CHudElement
{
public:

	explicit CHudVoiceStatus( const char *name );
	~CHudVoiceStatus( void );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void VidInit();
	virtual void Init();
	virtual void OnThink();

protected:
	void ClearActiveList();
	int FindActiveSpeaker( int playerId );

private:
	CHudTexture *m_pVoiceIcon;

	Color	m_clrIcon;

	struct ActiveSpeaker
	{
		int					playerId;
		bool				bSpeaking;
		float				fAlpha;
	};

	CUtlLinkedList< ActiveSpeaker > m_SpeakingList;
};


//DECLARE_HUDELEMENT( CHudVoiceStatus );


CHudVoiceStatus::CHudVoiceStatus( const char *pName ) :
	CHudElement( pName )
{
	m_pVoiceIcon = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_clrIcon = Color(255,255,255,255);
}

CHudVoiceStatus::~CHudVoiceStatus()
{
	ClearActiveList();
}

void CHudVoiceStatus::Init( void )
{
	ClearActiveList();
}

void CHudVoiceStatus::VidInit( void )
{
}

void CHudVoiceStatus::OnThink( void )
{
}

bool CHudVoiceStatus::ShouldDraw()
{
	if ( IsInFreezeCam() == true )
		return false;

	return true;
}

void CHudVoiceStatus::Paint()
{
}

int CHudVoiceStatus::FindActiveSpeaker( int playerId )
{
	FOR_EACH_LL(m_SpeakingList, i)
	{
		if (m_SpeakingList[i].playerId == playerId)
			return i;
	}
	return m_SpeakingList.InvalidIndex();
}

void CHudVoiceStatus::ClearActiveList()
{
	m_SpeakingList.RemoveAll();
}
