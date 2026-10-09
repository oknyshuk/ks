#pragma once

#include "GameEventListener.h"
#include "funfact_cs.h"
import std;
#include "cs_shareddefs.h"
#include "igamesystem.h"

class FunFactEvaluator;

class CCSFunFactMgr : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CCSFunFactMgr();
	~CCSFunFactMgr();

	virtual bool Init();
	virtual void Shutdown();
	virtual void Update( float frametime );

	bool GetRoundEndFunFact( int iWinningTeam, e_RoundEndReason iRoundResult, FunFact& funfact );

protected:
	float ScoreFunFact( const FunFact& funfact );
	void FireGameEvent( IGameEvent *event );

private:

	// Weights for all players. Updated every round
	// index 0 is for "all players" funfacts, and has the same cooldown behavior as for individual players
	float m_playerCooldown[MAX_PLAYERS + 1];	

	struct FunFactDatabaseEntry
	{
		std::unique_ptr< const FunFactEvaluator > pEvaluator;
		int iOccurrences;
		float fCooldown;
	};
	std::map<int, FunFactDatabaseEntry> m_funFactDatabase;
	int m_numRounds;
};


