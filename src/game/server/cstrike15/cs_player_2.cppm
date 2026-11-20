// SPDX-License-Identifier: None

module;
#include "declare_class.h"
#include "igameevents.h"
module ks.server:cstrike15.cs_player_2;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import std;
import :cstrike15.cs_playeranimstate;
import ks.tier1.createinterface;
import :vscript;
import ks.game.shareddefs;

class CBaseEntity;
class CCSPlayer;


// 
// Configuration for using high priority entities by CS players
//
class CConfigurationForHighPriorityUseEntity_t
{
public:
	enum EPriority_t
	{	// Priority of use entities, higher number is higher priority for use
		k_EPriority_Default,
		k_EPriority_Hostage,
		k_EPriority_Bomb
	};
	enum EPlayerUseType_t
	{
		k_EPlayerUseType_Start,		// Player wants to initiate the use
		k_EPlayerUseType_Progress	// Player wants to make progress using the entity
	};
	enum EDistanceCheckType_t
	{
		k_EDistanceCheckType_3D,
		k_EDistanceCheckType_2D
	};

	CBaseEntity *m_pEntity;
	EPriority_t m_ePriority;
	EDistanceCheckType_t m_eDistanceCheckType;
	Vector m_pos;
	float m_flMaxUseDistance;
	float m_flLosCheckDistance;
	float m_flDotCheckAngle;
	float m_flDotCheckAngleMax;

public:
	// Check if this high priority use entity is better for use than the other one
	bool IsBetterForUseThan( CConfigurationForHighPriorityUseEntity_t const &other ) const;

	// Check if this entity can be used by the given player according to its use rules
	bool UseByPlayerNow( CCSPlayer *pPlayer, EPlayerUseType_t ePlayerUseType );
};

struct HalloweenMaskModelStruct
{
	const char* model;
};

inline const HalloweenMaskModelStruct s_HalloweenMaskModels[] =
{
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_hoxton.mdl" },
	{ "models/player/holiday/facemasks/porcelain_doll.mdl" },
	{ "models/player/holiday/facemasks/facemask_skull.mdl" },	
	{ "models/player/holiday/facemasks/facemask_samurai.mdl" },	
	{ "models/player/holiday/facemasks/evil_clown.mdl" },
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_wolf.mdl" },
	{ "models/player/holiday/facemasks/facemask_sheep_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_bunny_gold.mdl" },
	{ "models/player/holiday/facemasks/facemask_anaglyph.mdl" },
	{ "models/player/holiday/facemasks/facemask_porcelain_doll_kabuki.mdl" },
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_dallas.mdl" },
	{ "models/player/holiday/facemasks/facemask_pumpkin.mdl" },
	{ "models/player/holiday/facemasks/facemask_sheep_bloody.mdl" },
	{ "models/player/holiday/facemasks/facemask_devil_plastic.mdl" },	
	{ "models/player/holiday/facemasks/facemask_boar.mdl" },	
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_chains.mdl" },	
	{ "models/player/holiday/facemasks/facemask_tiki.mdl" },
	{ "models/player/holiday/facemasks/facemask_bunny.mdl" },
	{ "models/player/holiday/facemasks/facemask_sheep_gold.mdl" },
	{ "models/player/holiday/facemasks/facemask_zombie_fortune_plastic.mdl" },
	{ "models/player/holiday/facemasks/facemask_chicken.mdl" },
	{ "models/player/holiday/facemasks/facemask_skull_gold.mdl" },	
};

inline const HalloweenMaskModelStruct s_HalloweenMaskModelsCompetitive[] =
{
	{ "models/player/holiday/facemasks/facemask_samurai.mdl" },
	{ "models/player/holiday/facemasks/facemask_boar.mdl" },
	{ "models/player/holiday/facemasks/facemask_zombie_fortune_plastic.mdl" },
	{ "models/player/holiday/facemasks/facemask_porcelain_doll_kabuki.mdl" },
	{ "models/player/holiday/facemasks/facemask_pumpkin.mdl" },

	{ "models/player/holiday/facemasks/facemask_hoxton.mdl" },
	{ "models/player/holiday/facemasks/facemask_chains.mdl" },
	{ "models/player/holiday/facemasks/evil_clown.mdl" },
	{ "models/player/holiday/facemasks/facemask_dallas.mdl" },
	{ "models/player/holiday/facemasks/facemask_wolf.mdl" },
};

inline const HalloweenMaskModelStruct s_HalloweenMaskModelsTF2[] =
{
	{ "models/player/holiday/facemasks/facemask_tf2_demo_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_engi_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_heavy_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_medic_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_pyro_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_scout_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_sniper_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_soldier_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_spy_model.mdl" },
};

struct FunFact
{
	FunFact() :
		id(-1),
		szLocalizationToken(nullptr),
		iPlayer(0),
		iData1(0),
		iData2(0),
		iData3(0),
		fMagnitude(0.0f)
		{}
	int id;
	const char* szLocalizationToken;
	int iPlayer;
	int iData1;
	int iData2;
	int iData3;
	float fMagnitude;
};

typedef std::vector< FunFact > FunFactVector;

class FunFactEvaluator
{
	DECLARE_CLASS_NOBASE( FunFactEvaluator );
public:
	FunFactEvaluator( int id, const char* szLocalizationToken, float fCoolness ) :
		m_id(id),
		m_pLocalizationToken(szLocalizationToken),
		m_fCoolness(fCoolness)
	{}

	virtual ~FunFactEvaluator() {}

	int GetId() const { return m_id; }
	const char* GetLocalizationToken() const { return m_pLocalizationToken; }
	float GetCoolness() const { return m_fCoolness; }

	virtual bool Evaluate( e_RoundEndReason iRoundResult, FunFactVector& results ) const = 0;

private:
	int  m_id;
	const char* m_pLocalizationToken;
	float m_fCoolness;
};

typedef FunFactEvaluator* (*funfactCreateFunc) (void);
class CFunFactHelper
{
public:
	CFunFactHelper ( funfactCreateFunc createFunc );
	funfactCreateFunc m_pfnCreate;
	CFunFactHelper *m_pNext;
	static CFunFactHelper *s_pFirst;
};

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
public:
	void FireGameEvent( IGameEvent *event );
protected:

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
