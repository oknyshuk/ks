#include "string_t.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "usermessages.h"
#include "funfactmgr_cs.h"
#include "cs_shareddefs.h"

const float kCooldownRatePlayer		= 0.5f;
const float kCooldownRateFunFact	= 0.2f;

const float kWeightPlayerCooldown	= 0.7f;
const float kWeightFunFactCooldown	= 1.0f;
const float kWeightCoolness			= 1.2f;
const float kWeightRarity			= 1.0f;


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CCSFunFactMgr::CCSFunFactMgr() : 
	CAutoGameSystemPerFrame( "CCSFunFactMgr" )
{
	for ( int i = 0; i < std::size( m_playerCooldown ); ++i )
	{
		m_playerCooldown[i] = 0.0f;
	}
}

CCSFunFactMgr::~CCSFunFactMgr()
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the fun fact manager
//-----------------------------------------------------------------------------
bool CCSFunFactMgr::Init()
{
	ListenForGameEvent( "player_connect" );

	CFunFactHelper *pFunFactHelper = CFunFactHelper::s_pFirst;

	// create database of all fun fact evaluators (and initial usage metrics)
	while ( pFunFactHelper )
	{
		FunFactDatabaseEntry entry;
		entry.fCooldown = 0.0f;
		entry.iOccurrences = 0;
		entry.pEvaluator.reset( pFunFactHelper->m_pfnCreate() );
		int id = entry.pEvaluator->GetId();
		m_funFactDatabase.emplace( id, std::move( entry ) );

		pFunFactHelper = pFunFactHelper->m_pNext;
	}

	for (int i = 0; i < std::size( m_playerCooldown ); ++i)
	{
		m_playerCooldown[i] = 0.0f;
	}

	m_numRounds = 0;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the fun fact manager
//-----------------------------------------------------------------------------
void CCSFunFactMgr::Shutdown()
{
	m_funFactDatabase.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Per frame processing
//-----------------------------------------------------------------------------
void CCSFunFactMgr::Update( float frametime )
{

}

//-----------------------------------------------------------------------------
// Purpose: Listens for game events.  Clears out map based stats and player based stats when necessary
//-----------------------------------------------------------------------------
void CCSFunFactMgr::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( V_strcmp( "player_connect", eventname ) == 0 )
	{
		int index = event->GetInt("index");// player slot (entity index-1)
		ASSERT( index >= 0 && index <= MAX_PLAYERS );
		if( index >= 0 && index <= MAX_PLAYERS )
		{
			m_playerCooldown[index] = 0.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the best fun fact to display and returns all necessary information through the parameters
//-----------------------------------------------------------------------------
bool CCSFunFactMgr::GetRoundEndFunFact( int iWinningTeam, e_RoundEndReason iRoundResult, FunFact& funfact )
{

	//No fun fact for surrender
	if ( iRoundResult == CTs_Surrender || iRoundResult == Terrorists_Surrender )
	{
		return false;
	}

	FunFactVector validFunFacts;

	// Generate a vector of all valid fun facts for this round
	for ( auto &[id, entry] : m_funFactDatabase )
	{
		FunFact funFact;
		if ( entry.pEvaluator->Evaluate(iRoundResult, validFunFacts) )
		{
			entry.iOccurrences++;
		}
	}

	m_numRounds++;

	if (validFunFacts.empty())
		return false;

	// pick the fun fact with the highest score
	float fBestScore = -std::numeric_limits<float>::max();
	int iFunFactIndex = -1;

	for ( int i = 0; i < std::ssize( validFunFacts ); i++ )
	{
		float fScore = ScoreFunFact(validFunFacts[i]);

		if (fScore > fBestScore)
		{
			fBestScore = fScore;
			iFunFactIndex = i;
		}
	}

	if (iFunFactIndex < 0)
		return false;
	
	funfact = validFunFacts[iFunFactIndex];

	// decay player cooldowns
	for (int i = 0; i < std::size( m_playerCooldown ); ++i )
	{
		m_playerCooldown[i] *= (1.0f - kCooldownRatePlayer);
	}

	// decay funfact cooldowns
	for ( auto &[id, entry] : m_funFactDatabase )
	{
		entry.fCooldown *= (1.0f - kCooldownRateFunFact);
	}

	// set player cooldown for player in funfact
	m_playerCooldown[funfact.iPlayer] = 1.0f;

	// set funfact cooldown for current funfact
	m_funFactDatabase.find(funfact.id)->second.fCooldown = 1.0f;

	return true;
}

float CCSFunFactMgr::ScoreFunFact( const FunFact& funfact )
{
	float fScore = 0.0f;
	const FunFactDatabaseEntry& dbEntry = m_funFactDatabase.find(funfact.id)->second;

	// add the coolness score for the funfact
	fScore += kWeightCoolness * dbEntry.pEvaluator->GetCoolness() * (1.0f + funfact.fMagnitude);

	// subtract the cooldown for the funfact
	fScore -= kWeightFunFactCooldown * dbEntry.fCooldown;
	
	// subtract the cooldown for the player
	fScore -= kWeightPlayerCooldown * m_playerCooldown[funfact.iPlayer];

	// add the rarity bonus
	fScore += kWeightRarity * std::powf((1.0f - (float)dbEntry.iOccurrences / m_numRounds), 4.0f);

	return fScore;
}
