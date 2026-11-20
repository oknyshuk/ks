// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

module;
#include "client_class.h"
#include "declare_class.h"
#include "edict.h"
#include "reflect_annotations.h"
module ks.client:NextBot.NextBot;

import std;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :baseanimatedtextureproxy;
import :util;

class CBaseCombatCharacter;

using namespace ::ks::reflect;

// NextBot.h
// Next generation bot system
// Author: Michael Booth, April 2005

//----------------------------------------------------------------------------------------------------------------
/**
* The interface holding IBody information
*/
class IBodyClient
{
public:
	enum ActivityType 
	{ 
		MOTION_CONTROLLED_XY	= 0x0001,	// XY position and orientation of the bot is driven by the animation.
		MOTION_CONTROLLED_Z		= 0x0002,	// Z position of the bot is driven by the animation.
		ACTIVITY_UNINTERRUPTIBLE= 0x0004,	// activity can't be changed until animation finishes
		ACTIVITY_TRANSITORY		= 0x0008,	// a short animation that takes over from the underlying animation momentarily, resuming it upon completion
		ENTINDEX_PLAYBACK_RATE	= 0x0010,	// played back at different rates based on entindex
	};
};

//--------------------------------------------------------------------------------------------------------
/**
 * The client-side implementation of the NextBot
 */
class CNextBotCombatCharacter : public CBaseCombatCharacter
{
	[[= ks::reflect::NetTable{ .name = "DT_NextBot" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CNextBotCombatCharacter, CBaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	CNextBotCombatCharacter();
	virtual ~CNextBotCombatCharacter();

public:	
	virtual void Spawn( void );
	virtual void UpdateClientSideAnimation( void );
	virtual ShadowType_t ShadowCastType( void );
	virtual bool IsNextBot() { return true; }
	void ForceShadowCastType( bool bForce, ShadowType_t forcedShadowType = SHADOWS_NONE ) { m_bForceShadowType = bForce; m_forcedShadowType = forcedShadowType; }
	bool GetForcedShadowCastType( ShadowType_t* pForcedShadowType ) const;

	// Local In View Data.
	void InitFrustumData( void )						{ m_bInFrustum = false; m_flFrustumDistanceSqr = std::numeric_limits<float>::max(); m_nInFrustumFrame = gpGlobals->framecount; }
	bool IsInFrustumValid( void )						{ return ( m_nInFrustumFrame == gpGlobals->framecount ); }
	void SetInFrustum( bool bInFrustum )				{ m_bInFrustum = bInFrustum; }
	bool IsInFrustum( void )							{ return m_bInFrustum; }
	void SetInFrustumDistanceSqr( float flDistance )	{ m_flFrustumDistanceSqr = flDistance; }
	float GetInFrustumDistanceSqr( void )				{ return m_flFrustumDistanceSqr; }

private:
	ShadowType_t	m_shadowType;			// Are we LOD'd to simple shadows?
	CountdownTimer m_shadowTimer;	// Timer to throttle checks for shadow LOD
	ShadowType_t	m_forcedShadowType;
	bool			m_bForceShadowType;
	void UpdateShadowLOD( void );

	// Local In View Data.
	int			m_nInFrustumFrame;
	bool		m_bInFrustum;
	float		m_flFrustumDistanceSqr;

private:
	CNextBotCombatCharacter( const CNextBotCombatCharacter & );				// not defined, not accessible
};

//--------------------------------------------------------------------------------------------------------
/**
 * The CNextBotManager manager 
 */
class CNextBotManager
{
public:
	CNextBotManager( void );
	~CNextBotManager();

	/**
	 * Execute functor for each NextBot in the system.
	 * If a functor returns false, stop iteration early
	 * and return false.
	 */	
	template < typename Functor >
	bool ForEachCombatCharacter( Functor &func )
	{
		for( int i=0; i < std::ssize( m_botList ); ++i )
		{
			CNextBotCombatCharacter *character = m_botList[i];
			if ( character->IsPlayer() )
			{
				continue;
			}

			if ( character->IsDormant() )
			{
				continue;
			}

			if ( !func( character ) )
			{
				return false;
			}
		}

		return true;
	}

	int	 GetActiveCount()						    { return std::ssize( m_botList ); }

	bool SetupInFrustumData( void );
	bool IsInFrustumDataValid( void )				{ return ( m_nInFrustumFrame == gpGlobals->framecount ); }

private:
	friend class CNextBotCombatCharacter;

	void Register( CNextBotCombatCharacter *bot );
	void UnRegister( CNextBotCombatCharacter *bot );

	std::vector< CNextBotCombatCharacter * > m_botList;				///< list of all active NextBots

	int	m_nInFrustumFrame;
};

// singleton accessor
extern CNextBotManager &TheClientNextBots( void );
