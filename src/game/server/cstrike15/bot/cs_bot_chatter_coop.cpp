// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: / Custom chatter rules for bots when playing in cooperative modes

#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "cs_player.h"

#include "bot_util.h"
#include "cs_bot.h"
#include "cs_bot_chatter.h"
#include "cs_team.h"

BotChatterCoop::BotChatterCoop( CCSBot *me ) :
BotChatterInterface( me, Kind::Coop )
{
}
//---------------------------------------------------------------------------------------------------------------
void BotChatterCoop::KilledMyEnemy( int victimID )
{
	if ( GetGlobalCSTeam(TEAM_CT)->GetAliveMembers() == 0  )
	{
		CelebrateWin();
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMY_ACTION, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
		say->SetSubject( victimID );

		AddStatement( say );
	}
}

void BotChatterCoop::EnemiesRemaining( void )
{
	if ( GetGlobalCSTeam(TEAM_CT)->GetAliveMembers() == 0  )
	{
		CelebrateWin();
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_ENEMIES_REMAINING, 5.0f );
		say->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
		say->SetStartTime( gpGlobals->curtime );

		AddStatement( say );
	}
}

void BotChatterCoop::EnemySpotted( void )
{
	float flChance = RandomFloat();
	if( flChance < 0.3 )
	{
		EnemySpottedDefault();
	}
	else if ( flChance < 0.7 )
	{
		BotStatement *say = new BotStatement( this, REPORT_EMOTE, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "GoGoGo" ) );
		AddStatement( say );
	}
	else
	{
		BotStatement *say = new BotStatement( this, REPORT_EMOTE, 3.0f );
		say->AppendPhrase( TheBotPhrases->GetPhrase( "Cheer" ) );
		AddStatement( say );
	}
}

void BotChatterCoop::CelebrateWin( void )
{
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 15.0f );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRound" ) );
	AddStatement( say );
}
