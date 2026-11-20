// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include "filesystem.h"
#include "GameUI/IGameUI.h"
#include "IGameUIFuncs.h"
#include "cdll_int.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "game/client/IGameClientExports.h"
#include "iachievementmgr.h"
#include "iengineui.h"
#include "igameevents.h"
#include "localize/ilocalize.h"
#include "matchmaking/imatchframework.h"
#include "IRunGameEngine.h"
#include "eiface.h"
#include "./GameUI/IGameUI.h"
#include "../../cdll_client_int.h"
module ks.client;

import std;
import ks.core.dbg;
import ks.core.platform;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import :cstrike15.gameui.bonusmapsdatabase;
import ks.tier1.vector_ops;
import ks.core.cmdline;
import ks.core.random;
import ks.core.types;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :particlemgr;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :cstrike15.RocketUI.rkconsole;
import :precache;
import :baseentity;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import :noopanimcontroller;
import ks.core.basetypes;
import ks.core.vprof;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.core.scratcharena;
import ks.core.threadtools;
import ks.tier1.fmtstr;
import ks.tier1.tokenset;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;

#define MOD_DIR ( "MOD" )

const char g_pszMedalNames[4][8] =
{
	"none",
	"bronze",
	"silver",
	"gold"
};

bool WriteBonusMapSavedData( KeyValues *data )
{
	ks::text_writer buf;

	data->RecursiveSaveToFile( buf, 0 );

	char	szFilename[k_cMaxPath];

V_snprintf( szFilename, sizeof( szFilename ), "save/bonus_maps_data.bmd" );

	bool bWriteSuccess = g_pFullFileSystem->WriteFile( szFilename, MOD_DIR, buf.bytes() );

	return bWriteSuccess;
}

void GetBooleanStatus( KeyValues *pBonusFilesKey, BonusMapDescription_t &map )
{
	KeyValues *pFileKey = nullptr;
	KeyValues *pBonusKey = nullptr;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( V_strcmp( pFileKey->GetName(), map.szFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( V_strcmp( pBonusKey->GetName(), map.szMapName ) == 0 )
				{
					// Found the data
					break;
				}
			}
			break;
		}
	}

	if ( pBonusKey )
	{
		map.bLocked = ( pBonusKey->GetInt( "lock" ) != 0 );
		map.bComplete = ( pBonusKey->GetInt( "complete" ) != 0 );
	}
}

bool SetBooleanStatus( KeyValues *pBonusFilesKey, const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue )
{
	// Don't create entries for files that don't exist
	if ( !g_pFullFileSystem->FileExists( pchFileName, "MOD" ) )
	{
		DevMsg( "Failed to set boolean status for file %s.", pchFileName );
		return false;
	}

	bool bChanged = false;

	KeyValues *pFileKey = nullptr;
	KeyValues *pBonusKey = nullptr;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( V_strcmp( pFileKey->GetName(), pchFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( V_strcmp( pBonusKey->GetName(), pchMapName ) == 0 )
				{
					// Found the data
					break;
				}
			}
			break;
		}
	}

	if ( !pFileKey )
	{
		// Didn't find it, so create a new spot for the data
		pFileKey = new KeyValues( pchFileName );
		pBonusFilesKey->AddSubKey( pFileKey );
	}

	if ( !pBonusKey )
	{
		pBonusKey = new KeyValues( pchMapName, pchName, "0" );
		pFileKey->AddSubKey( pBonusKey );
		bChanged = true;
	}

	if ( ( pBonusKey->GetInt( pchName ) != 0 ) != bValue )
	{
		bChanged = true;
		pBonusKey->SetInt( pchName, bValue );
	}

	return bChanged;
}

float GetChallengeBests( KeyValues *pBonusFilesKey, BonusMapDescription_t &challenge )
{
	// There's no challenges, so bail and assume 0% challenge completion
	if ( challenge.m_pChallenges == nullptr || std::ssize( *challenge.m_pChallenges ) == 0 )
		return 0.0f;

	KeyValues *pFileKey = nullptr;
	KeyValues *pBonusKey = nullptr;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( V_strcmp( pFileKey->GetName(), challenge.szFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( V_strcmp( pBonusKey->GetName(), challenge.szMapName ) == 0 )
				{
					// Found the data
					break;
				}
			}
			break;
		}
	}

	float fChallengePoints = 0.0f;

	for ( int iChallenge = 0; iChallenge < std::ssize( *challenge.m_pChallenges ); ++iChallenge )
	{
		ChallengeDescription_t *pChallengeDescription = &((*challenge.m_pChallenges)[ iChallenge ]);
		pChallengeDescription->iBest = ( ( pBonusKey ) ? ( pBonusKey->GetInt( pChallengeDescription->szName, -1 ) ) : ( -1 ) );

		if ( pChallengeDescription->iBest >= 0 )
		{
			if ( pChallengeDescription->iBest <= pChallengeDescription->iGold )
				fChallengePoints += 3.0f;
			else if ( pChallengeDescription->iBest <= pChallengeDescription->iSilver )
				fChallengePoints += 2.0f;
			else if ( pChallengeDescription->iBest <= pChallengeDescription->iBronze )
				fChallengePoints += 1.0f;
		}
	}

	return fChallengePoints / ( std::ssize( *challenge.m_pChallenges ) * 3.0f );
}

bool UpdateChallengeBest( KeyValues *pBonusFilesKey, const BonusMapChallenge_t &challenge )
{
	// Don't create entries for files that don't exist
	if ( !g_pFullFileSystem->FileExists( challenge.szFileName, "MOD" ) )
	{
		DevMsg( "Failed to set challenge best for file %s.", challenge.szFileName );
		return false;
	}

	bool bChanged = false;

	KeyValues *pFileKey = nullptr;
	KeyValues *pBonusKey = nullptr;

	for ( pFileKey = pBonusFilesKey->GetFirstSubKey(); pFileKey; pFileKey = pFileKey->GetNextTrueSubKey() )
	{
		if ( V_strcmp( pFileKey->GetName(), challenge.szFileName ) == 0 )
		{
			for ( pBonusKey = pFileKey->GetFirstSubKey(); pBonusKey; pBonusKey = pBonusKey->GetNextKey() )
			{
				if ( V_strcmp( pBonusKey->GetName(), challenge.szMapName ) == 0 )
				{
					// Found the challenge
					break;
				}
			}
			break;
		}
	}

	if ( !pFileKey )
	{
		// Didn't find it, so create a new spot for data
		pFileKey = new KeyValues( challenge.szFileName );
		pBonusFilesKey->AddSubKey( pFileKey );
	}

	if ( !pBonusKey )
	{
		pBonusKey = new KeyValues( challenge.szMapName, challenge.szChallengeName, -1 );
		pFileKey->AddSubKey( pBonusKey );
		bChanged = true;
	}

	int iCurrentBest = pBonusKey->GetInt( challenge.szChallengeName, -1 );
	if ( iCurrentBest == -1 || iCurrentBest > challenge.iBest )
	{
		bChanged = true;
		pBonusKey->SetInt( challenge.szChallengeName, challenge.iBest );
	}

	return bChanged;
}

void GetChallengeMedals( ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal )
{
	iBest = pChallengeDescription->iBest;

	if ( iBest == -1 )
		iEarnedMedal = 0;
	else if ( iBest <= pChallengeDescription->iGold )
		iEarnedMedal = 3;
	else if ( iBest <= pChallengeDescription->iSilver )
		iEarnedMedal = 2;
	else if ( iBest <= pChallengeDescription->iBronze )
		iEarnedMedal = 1;
	else
		iEarnedMedal = 0;

	iNext = -1;

	switch ( iEarnedMedal )
	{
	case 0:
		iNext = pChallengeDescription->iBronze;
		iNextMedal = 1;
		break;
	case 1:
		iNext = pChallengeDescription->iSilver;
		iNextMedal = 2;
		break;
	case 2:
		iNext = pChallengeDescription->iGold;
		iNextMedal = 3;
		break;
	case 3:
		iNext = -1;
		iNextMedal = -1;
		break;
	}
}

CBonusMapsDatabase *g_pBonusMapsDatabase = nullptr;

CBonusMapsDatabase *BonusMapsDatabase( void )
{
	if ( !g_pBonusMapsDatabase )
		static CBonusMapsDatabase StaticBonusMapsDatabase;

	return g_pBonusMapsDatabase;
}

//-----------------------------------------------------------------------------
// Purpose:Constructor
//-----------------------------------------------------------------------------
CBonusMapsDatabase::CBonusMapsDatabase( void )
{
	contract_assert( g_pBonusMapsDatabase == nullptr );	// There should only be 1 bonus maps database
	g_pBonusMapsDatabase = this;

	RootPath();

	m_pBonusMapsManifest = new KeyValues( "bonus_maps_manifest" );
	m_pBonusMapsManifest->LoadFromFile( g_pFullFileSystem, "scripts/bonus_maps_manifest.txt", nullptr );

	m_iX360BonusesUnlocked = -1;	// Only used on X360
	m_bHasLoadedSaveData = false;

	ReadBonusMapSaveData();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBonusMapsDatabase::~CBonusMapsDatabase()
{
	WriteSaveData();

	g_pBonusMapsDatabase = nullptr;
}

// Re-homed from deleted BasePanel.cpp (VGUI teardown).
bool g_bIsCreatingNewGameMenuForPreFetching = false;

bool CBonusMapsDatabase::ReadBonusMapSaveData( void )
{
	if ( !m_pBonusMapSavedData )
		m_pBonusMapSavedData = new KeyValues( "bonus_map_saved_data" );

	if ( g_bIsCreatingNewGameMenuForPreFetching )
	{
		// Although we may have a storage device it's not going to be able to find our file at this point! BAIL!
		return false;
	}

	char	szFilename[k_cMaxPath];

V_snprintf( szFilename, sizeof( szFilename ), "save/bonus_maps_data.bmd" );

	m_pBonusMapSavedData->LoadFromFile( g_pFullFileSystem, szFilename, nullptr );

	m_bSavedDataChanged = false;
	m_bHasLoadedSaveData = true;

	return true;
}

bool CBonusMapsDatabase::WriteSaveData( void )
{
	bool bSuccess = false;

	if ( m_bSavedDataChanged )
		bSuccess = WriteBonusMapSavedData( m_pBonusMapSavedData );

	if ( bSuccess )
		m_bSavedDataChanged = false;

	return bSuccess;
}

void CBonusMapsDatabase::RootPath( void )
{
	m_iDirDepth = 0;
	V_strcpy( m_szCurrentPath, "." );
}

void CBonusMapsDatabase::AppendPath( const char *pchAppend )
{
	++m_iDirDepth;
	V_snprintf( m_szCurrentPath, sizeof( m_szCurrentPath ), "%s/%s", m_szCurrentPath, pchAppend );
}

void CBonusMapsDatabase::BackPath( void )
{
	if ( m_iDirDepth == 0 )
		return;

	if ( m_iDirDepth == 1 )
	{
		RootPath();	// back to root
		return;
	}

	--m_iDirDepth;
	V_strrchr( m_szCurrentPath, '/' )[ 0 ] = '\0';	// remove a dir from the end
}

void CBonusMapsDatabase::SetPath( const char *pchPath, int iDirDepth )
{
	V_strcpy( m_szCurrentPath, pchPath );
	m_iDirDepth = iDirDepth;
}

void CBonusMapsDatabase::ClearBonusMapsList( void )
{
	m_BonusMaps.clear();
}

//-----------------------------------------------------------------------------
// Purpose: builds bonus map list from directory
//-----------------------------------------------------------------------------
void CBonusMapsDatabase::ScanBonusMaps( void )
{
	// Don't load in the bonus maps before we've properly read in the save data
	if ( !m_bHasLoadedSaveData )
	{
		if ( ! ReadBonusMapSaveData() )
			return;
	}

	char *pCurrentPath = &(m_szCurrentPath[2]);

	// Reset completion percentage
	m_iCompletableLevels = 0;
	m_fCurrentCompletion = 0.0f;

	// populate list box with all bonus maps in the current path
	char szDirectory[k_cMaxPath];

	if ( V_strcmp( m_szCurrentPath, "." ) == 0 )
	{
		// We're at the root, so look at the directories in the manifest
		KeyValues *pKey = nullptr;
		for ( pKey = m_pBonusMapsManifest->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
		{
			const char *pchType = pKey->GetName();
			if ( V_strcmp( pchType, "search" ) == 0 )
			{
				// Search through the directory
				V_snprintf( szDirectory, sizeof( szDirectory ), "%s/", pKey->GetString() );

				BuildSubdirectoryList( szDirectory, true );
				BuildBonusMapsList( szDirectory, true );
			}
			else if ( V_strcmp( pchType, "dir" ) == 0 )
			{
				AddBonus( "", pKey->GetString(), true );
			}
			else if ( V_strcmp( pchType, "map" ) == 0 )
			{
				AddBonus( "", pKey->GetString(), false );
			}
		}
	}
	else
	{
		// Search through the current directory
		V_snprintf( szDirectory, sizeof( szDirectory ), "%s/", pCurrentPath );

		BuildSubdirectoryList( szDirectory, false );
		BuildBonusMapsList( szDirectory, false );
	}
}

void CBonusMapsDatabase::RefreshMapData( void )
{
	// Reset completion percentage
	m_iCompletableLevels = 0;
	m_fCurrentCompletion = 0.0f;

	for ( int iMap = 0; iMap < std::ssize( m_BonusMaps ); ++iMap )
	{
		BonusMapDescription_t *pMap = &m_BonusMaps[ iMap ];

		float fCompletion = GetChallengeBests( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

		// If all the challenges are completed set it as complete
		if ( fCompletion == 1.0f )
			SetBooleanStatus( "complete", pMap->szFileName, pMap->szMapName, true );

		GetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

		if ( pMap->bComplete )
			fCompletion = 1.0f;

		if ( !pMap->bIsFolder )
		{
			m_fCurrentCompletion += fCompletion;
			++m_iCompletableLevels;
		}
	}
}

int CBonusMapsDatabase::BonusCount( void )
{
	if ( m_BonusMaps.empty() )
		ScanBonusMaps();

	return std::ssize( m_BonusMaps );
}

bool CBonusMapsDatabase::GetBlink( void )
{
	KeyValues *pBlinkKey = m_pBonusMapSavedData->FindKey( "blink" );
	if ( !pBlinkKey )
		return false;
	
	return ( pBlinkKey->GetInt() != 0 );
}

void CBonusMapsDatabase::SetBlink( bool bState )
{
	KeyValues *pBlinkKey = m_pBonusMapSavedData->FindKey( "blink" );
	if ( pBlinkKey )
	{
		bool bCurrentState = ( pBlinkKey->GetInt() != 0 );
		if ( bState && !bCurrentState )
		{
			pBlinkKey->SetStringValue( "1" );
			m_bSavedDataChanged = true;
		}
		else if ( !bState && bCurrentState )
		{
			pBlinkKey->SetStringValue( "0" );
			m_bSavedDataChanged = true;
		}
	}
}

// Only used on X360
bool CBonusMapsDatabase::BonusesUnlocked( void )
{
	if ( m_iX360BonusesUnlocked == -1 )
	{
		// Never checked, so set up the proper X360 scan
		BonusMapsDatabase()->ClearBonusMapsList();	// clear the current list
		BonusMapsDatabase()->RootPath();
		BonusMapsDatabase()->ScanBonusMaps();

		m_iX360BonusesUnlocked = 0;
	}

	if ( m_iX360BonusesUnlocked == 0 )
	{
		// Hasn't been recorded as unlocked yet
		for ( int iBonusMap = 0; iBonusMap < BonusMapsDatabase()->BonusCount(); ++iBonusMap )
		{
			BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( iBonusMap );
			if ( V_strcmp( pMap->szMapName, "#Bonus_Map_AdvancedChambers" ) == 0 && !pMap->bLocked )
			{
				// All bonuses unlocked, remember this and set up the proper X360 scan to get info.
				m_iX360BonusesUnlocked = 1;
				break;
			}
		}
	}

	return ( m_iX360BonusesUnlocked != 0 );
}

void CBonusMapsDatabase::SetCurrentChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName )
{
	V_strcpy( m_CurrentChallengeNames.szFileName, pchFileName );
	V_strcpy( m_CurrentChallengeNames.szMapName, pchMapName );
	V_strcpy( m_CurrentChallengeNames.szChallengeName, pchChallengeName );
}

void CBonusMapsDatabase::GetCurrentChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
	V_strcpy( pchFileName, m_CurrentChallengeNames.szFileName );
	V_strcpy( pchMapName, m_CurrentChallengeNames.szMapName );
	V_strcpy( pchChallengeName, m_CurrentChallengeNames.szChallengeName );
}

void CBonusMapsDatabase::SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold )
{
	m_CurrentChallengeObjectives.iBronze = iBronze;
	m_CurrentChallengeObjectives.iSilver = iSilver;
	m_CurrentChallengeObjectives.iGold = iGold;
}

void CBonusMapsDatabase::GetCurrentChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
	iBronze = m_CurrentChallengeObjectives.iBronze;
	iSilver = m_CurrentChallengeObjectives.iSilver;
	iGold = m_CurrentChallengeObjectives.iGold;
}

bool CBonusMapsDatabase::SetBooleanStatus( const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue )
{
	bool bChanged = ::SetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), pchName, pchFileName, pchMapName, bValue );
	if ( bChanged )
		m_bSavedDataChanged = true;

	return bChanged;
}

bool CBonusMapsDatabase::SetBooleanStatus( const char *pchName, int iIndex, bool bValue )
{
	BonusMapDescription_t *pMap = &(m_BonusMaps[iIndex]);

	bool bChanged = SetBooleanStatus( pchName, pMap->szFileName, pMap->szMapName, bValue );
	GetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

	return bChanged;
}

bool CBonusMapsDatabase::UpdateChallengeBest( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	BonusMapChallenge_t challenge;
	V_strcpy( challenge.szFileName, pchFileName );
	V_strcpy( challenge.szMapName, pchMapName );
	V_strcpy( challenge.szChallengeName, pchChallengeName );
	challenge.iBest = iBest;

	bool bChanged = ::UpdateChallengeBest( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), challenge );
	if ( bChanged )
		m_bSavedDataChanged = true;

	return bChanged;
}

float CBonusMapsDatabase::GetCompletionPercentage( void )
{
	// Avoid divide by zero
	if ( m_iCompletableLevels <= 0 )
		return 0.0f;

	return m_fCurrentCompletion / m_iCompletableLevels;
}

int CBonusMapsDatabase::NumAdvancedComplete( void )
{
	char szCurrentPath[k_cMaxPath];
	V_strcpy( szCurrentPath, m_szCurrentPath );
	int iDirDepth = m_iDirDepth;

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( "./scripts/advanced_chambers", 1 );
	ScanBonusMaps();

	int iNumComplete = 0;

	// Look through all the bonus maps
	for ( int iBonusMap = 0; iBonusMap < BonusMapsDatabase()->BonusCount(); ++iBonusMap )
	{
		BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( iBonusMap );

		if ( pMap && V_strstr( pMap->szMapName, "Advanced" ) != nullptr )
		{
			// It's an advanced map, so check if it's complete
			if ( pMap->bComplete )
				++iNumComplete;
		}
	}

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( szCurrentPath, iDirDepth );
	ScanBonusMaps();

	return iNumComplete;
}

void CBonusMapsDatabase::NumMedals( int piNumMedals[ 3 ] )
{
	char szCurrentPath[k_cMaxPath];
	V_strcpy( szCurrentPath, m_szCurrentPath );
	int iDirDepth = m_iDirDepth;

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( "./scripts/challenges", 1 );
	ScanBonusMaps();

	for ( int i = 0; i < 3; ++i )
		piNumMedals[ i ] = 0;

	// Look through all the bonus maps
	for ( int iBonusMap = 0; iBonusMap < BonusMapsDatabase()->BonusCount(); ++iBonusMap )
	{
		BonusMapDescription_t *pMap = BonusMapsDatabase()->GetBonusData( iBonusMap );

		if ( pMap && pMap->m_pChallenges )
		{
			for ( int iChallenge = 0; iChallenge < std::ssize( *pMap->m_pChallenges ); ++iChallenge )
			{
				ChallengeDescription_t *pChallengeDescription = &((*pMap->m_pChallenges)[ iChallenge ]);

				int iBest, iEarnedMedal, iNext, iNextMedal;
				GetChallengeMedals( pChallengeDescription, iBest, iEarnedMedal, iNext, iNextMedal );

				// Increase the count for this medal and every medal below it
				while ( iEarnedMedal > 0 )
				{
					--iEarnedMedal;	// Medals are 1,2&3 so subtract 1 first
					piNumMedals[ iEarnedMedal ]++;
				}
			}
		}
	}

	BonusMapsDatabase()->ClearBonusMapsList();
	SetPath( szCurrentPath, iDirDepth );
	ScanBonusMaps();
}

void CBonusMapsDatabase::AddBonus( const char *pCurrentPath, const char *pDirFileName, bool bIsFolder )
{
	char szFileName[k_cMaxPath];
	V_snprintf( szFileName, sizeof( szFileName ), "%s%s", pCurrentPath, pDirFileName );

	// Only load bonus maps from the current mod's maps dir
	if( !g_pFullFileSystem->FileExists( szFileName, "MOD" ) )
		return;

	ParseBonusMapData( szFileName, pDirFileName, bIsFolder );
}

void CBonusMapsDatabase::BuildSubdirectoryList( const char *pCurrentPath, bool bOutOfRoot )
{
	char szDirectory[k_cMaxPath];
	V_snprintf( szDirectory, sizeof( szDirectory ), "%s*", pCurrentPath );

	FileFindHandle_t dirHandle;
	const char *pDirFileName = g_pFullFileSystem->FindFirst( szDirectory, &dirHandle );

	while (pDirFileName)
	{
		// Skip it if it's not a directory, is the root, is back, or is an invalid folder
		if ( !g_pFullFileSystem->FindIsDirectory( dirHandle ) || 
			 V_strcmp( pDirFileName, "." ) == 0 || 
			 V_strcmp( pDirFileName, ".." ) == 0 ||
			 V_stricmp( pDirFileName, "soundcache" ) == 0 ||
			 V_stricmp( pDirFileName, "graphs" ) == 0 )
		{
			pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
			continue;
		}

		if ( !bOutOfRoot )
			AddBonus( pCurrentPath, pDirFileName, true );
		else
		{
			char szFileName[k_cMaxPath];
			V_snprintf( szFileName, sizeof( szFileName ), "%s%s", pCurrentPath, pDirFileName );
			AddBonus( "", szFileName, true );
		}

		pDirFileName = g_pFullFileSystem->FindNext( dirHandle );
	}

	g_pFullFileSystem->FindClose( dirHandle );
}

void CBonusMapsDatabase::BuildBonusMapsList( const char *pCurrentPath, bool bOutOfRoot )
{
	char szDirectory[k_cMaxPath];
	V_snprintf( szDirectory, sizeof( szDirectory ), "%s*.bns", pCurrentPath );

	FileFindHandle_t mapHandle;
	const char *pMapFileName = g_pFullFileSystem->FindFirst( szDirectory, &mapHandle );

	while (pMapFileName)
	{
		// Skip it if it's a directory or is the folder info
		if ( g_pFullFileSystem->FindIsDirectory( mapHandle ) || V_strstr( pMapFileName, "folderinfo.bns" ) )
		{
			pMapFileName = g_pFullFileSystem->FindNext( mapHandle );
			continue;
		}

		if ( !bOutOfRoot )
			AddBonus( pCurrentPath, pMapFileName, false );
		else
		{
			char szFileName[k_cMaxPath];
			V_snprintf( szFileName, sizeof( szFileName ), "%s%s", pCurrentPath, pMapFileName );
			AddBonus( "", szFileName, false );
		}

		pMapFileName = g_pFullFileSystem->FindNext( mapHandle );
	}

	g_pFullFileSystem->FindClose( mapHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the save game info out of the .sav file header
//-----------------------------------------------------------------------------
void CBonusMapsDatabase::ParseBonusMapData( char const *pszFileName, char const *pszShortName, bool bIsFolder )
{
	if ( !pszFileName || !pszShortName )
		return;

	char szMapInfo[k_cMaxPath];

	// if it's a directory, there's no optional info
	if ( bIsFolder )
	{
		// get the folder info file name
		V_snprintf( szMapInfo, sizeof(szMapInfo), "%s/folderinfo.bns", pszFileName );
	}
	else
	{
		// get the map info file name
		V_strncpy( szMapInfo, pszFileName, sizeof(szMapInfo) );
	}

	KeyValues *kv = new KeyValues( pszShortName );
	if ( !kv->LoadFromFile( g_pFullFileSystem, szMapInfo, nullptr ) )
		DevMsg( "Unable to load bonus map info file\n" );

	while ( kv )
	{
		int iMap = std::ssize( m_BonusMaps ); m_BonusMaps.emplace_back();

		BonusMapDescription_t *pMap = &m_BonusMaps[ iMap ];

		// set required map data
		V_strncpy( pMap->szFileName, pszFileName, sizeof(pMap->szFileName) );
		V_strncpy( pMap->szShortName, pszShortName, sizeof(pMap->szShortName) );
		pMap->bIsFolder = bIsFolder;

		// set optional map data
		V_strcpy( pMap->szMapName, kv->GetName() );
		V_strcpy( pMap->szMapFileName, kv->GetString( "map" ) );
		V_strcpy( pMap->szChapterName, kv->GetString( "chapter" ) );
		V_strcpy( pMap->szImageName, kv->GetString( "image" ) );
		V_strcpy( pMap->szComment, kv->GetString( "comment" ) );
		pMap->bLocked = ( kv->GetInt( "lock", 0 ) != 0 );
		pMap->bComplete = ( kv->GetInt( "complete", 0 ) != 0 );

		float fCompletion = 0.0f;

		KeyValues *pChallenges = kv->FindKey( "challenges" );

		if ( pChallenges )
		{
			for ( KeyValues *pChallengeKey = pChallenges->GetFirstSubKey(); pChallengeKey; pChallengeKey = pChallengeKey->GetNextKey() )
			{
				if ( !pMap->m_pChallenges )
					pMap->m_pChallenges = new std::vector< ChallengeDescription_t >;

				int iChallenge = std::ssize( *pMap->m_pChallenges ); pMap->m_pChallenges->emplace_back();

				ChallengeDescription_t *pChallenge = &(*pMap->m_pChallenges)[ iChallenge ];
				V_strcpy( pChallenge->szName, pChallengeKey->GetName() );
				V_strcpy( pChallenge->szComment, pChallengeKey->GetString( "comment" ) );
				pChallenge->iType = pChallengeKey->GetInt( "type", -1 );
				pChallenge->iBronze = pChallengeKey->GetInt( "bronze" );
				pChallenge->iSilver = pChallengeKey->GetInt( "silver" );
				pChallenge->iGold = pChallengeKey->GetInt( "gold" );
			}

			fCompletion = GetChallengeBests( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

			// If all the challenges are completed set it as complete
			if ( fCompletion == 1.0f )
				SetBooleanStatus( "complete", pMap->szFileName, pMap->szMapName, true );
		}

		// Get boolean status last because it can be altered if all the challenges were completed
		GetBooleanStatus( m_pBonusMapSavedData->FindKey( "bonusfiles", true ), *pMap );

		if ( pMap->bComplete )
			fCompletion = 1.0f;

		if ( !pMap->bIsFolder )
		{
			m_fCurrentCompletion += fCompletion;
			++m_iCompletableLevels;
			kv = kv->GetNextTrueSubKey();
		}
		else
			kv = nullptr;
	}
}

BonusMapDescription_t::BonusMapDescription_t( void)
{
	bIsFolder = false;

	szShortName[ 0 ] = '\0';
	szFileName[ 0 ] = '\0';

	szMapFileName[ 0 ] = '\0';
	szChapterName[ 0 ] = '\0';
	szImageName[ 0 ] = '\0';

	szMapName[ 0 ] = '\0';
	szComment[ 0 ] = '\0';

	bLocked = false;
	bComplete = false;

	m_pChallenges = nullptr;
}

#undef MOD_DIR

class CGameUI;

using namespace ::ks::reflect;

// Purpose: Implements all the functions exported by the GameUI dll

// dgoodenough - io.h and direct.h don't exist on PS3
// PS3_BUILDFIX
// @wge Fix for OSX too.
// @wge Fix for OSX too.

#ifdef SendMessage
#undef SendMessage
#endif
																
// interface to engine

// vgui2 interface
// note that GameUI project uses ..\vgui2\include, not ..\utils\vgui\include

// VGUI menu shell (CBaseModPanel/CCStrike15BasePanel) removed in VGUI teardown.
// Menu open/close is routed directly to the RmlUi documents below.

// dgoodenough - select correct stub header based on current console
// PS3_BUILDFIX

IEngineUI *engineuifuncs = nullptr;
// dgoodenough - xonline only exists on the 360.  All uses of xonline have had their
// protection changed like this one
// PS3_BUILDFIX
// FIXME we will have to put in something for Playstation Home.
IAchievementMgr *achievementmgr = nullptr;

CGameUI *g_pGameUI = nullptr;

static CGameUI g_GameUI;

//IRocketUI* RocketUI()
//{
//    return g_pRocketUI;
//}

static IGameClientExports *g_pGameClientExports = nullptr;
IGameClientExports *GameClientExports()
{
	return g_pGameClientExports;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameUI &GameUI()
{
	return g_GameUI;
}

static InterfaceReg s_Reg_CGameUI_IGameUI( side_registry(), []() -> void * { return static_cast<IGameUI *>( &g_GameUI ); }, GAMEUI_INTERFACE_VERSION );;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	g_pGameUI = this;
	m_bTryingToLoadFriends = false;
	m_iFriendsLoadPauseFrames = 0;
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_bActivatedUI = false;
	m_szPreviousStatusText[0] = 0;
	m_bIsConsoleUI = false;
	m_bHasSavedThisMenuSession = false;
	m_bOpenProgressOnStart = false;
	m_iPlayGameStartupSound = 0;
	m_nBackgroundMusicGUID = 0;
	m_bBackgroundMusicDesired = false;
	m_nBackgroundMusicVersion = RandomInt( 1, MAX_BACKGROUND_MUSIC );
	m_flBackgroundMusicStopTime = -1.0;
	m_pMusicExtension = nullptr;
	m_pPreviewMusicExtension = nullptr;
	m_flMainMenuMusicVolume = -1;
	m_flMasterMusicVolume = -1;
	m_flQuestAudioTimeEnd = 0;
	m_flMasterMusicVolumeSavedForMissionAudio = -1;
	m_flMenuMusicVolumeSavedForMissionAudio = -1;
	m_nQuestAudioGUID = 0;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CGameUI::Initialize( CreateInterfaceFn factory )
{
	// The tier libraries are shared with the engine, which has connected them.
	ConVar_Register( side_registry().convars, FCVAR_CLIENTDLL );

	enginesound = (IEngineSound *)factory(IENGINESOUND_CLIENT_INTERFACE_VERSION, nullptr);
	engine = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, nullptr );

	CGameUIConVarRef var( "gameui_xbox" );
	m_bIsConsoleUI = var.IsValid() && var.GetBool();

	// load localization file
	g_pLocalize->AddFile( "resource/gameui_%language%.txt", "GAME", true );

	// load mod info
	ModInfo().LoadCurrentGameInfo();

	// load localization file for kb_act.lst
	g_pLocalize->AddFile( "resource/valve_%language%.txt", "GAME", true );

	bool bFailed = false;
	engineuifuncs = (IEngineUI *)factory( VENGINE_UI_VERSION, nullptr );
	gameuifuncs = (IGameUIFuncs *)factory( VENGINE_GAMEUIFUNCS_VERSION, nullptr );
// dgoodenough - xonline only exists on the 360.
// PS3_BUILDFIX
	bFailed = !gameuifuncs || !engineuifuncs ||
// dgoodenough - xonline only exists on the 360.
// PS3_BUILDFIX
		!g_pMatchFramework;

	if ( bFailed )
	{
		Error( "CGameUI::Initialize() failed to get necessary interfaces\n" );
	}

	// VGUI base panel removed; the RmlUi documents host the menu. Nothing to construct here.
}

void CGameUI::PostInit()
{

}

//-----------------------------------------------------------------------------
// Purpose: connects to client interfaces
//-----------------------------------------------------------------------------
void CGameUI::Connect( CreateInterfaceFn gameFactory )
{
	g_pGameClientExports = (IGameClientExports *)gameFactory(GAMECLIENTEXPORTS_INTERFACE_VERSION, nullptr);

	achievementmgr = engine->GetAchievementMgr();

	if (!g_pGameClientExports)
	{
		Error("CGameUI::Initialize() failed to get necessary interfaces\n");
	}

	m_GameFactory = gameFactory;
}

//-----------------------------------------------------------------------------
// Purpose: Searches for GameStartup*.mp3 files in the sound/ui folder and plays one
//-----------------------------------------------------------------------------
void CGameUI::PlayGameStartupSound()
{
	// CS15 not using this path. using Portal 2 style MP3 looping
	// L4D not using this path, L4D UI now handling with background menu movies   	
	return;

	if ( CommandLine()->FindParm( "-nostartupsound" ) )
		return;

	FileFindHandle_t fh;

	std::vector< std::string > fileNames;

	char path[ 512 ];
	V_snprintf( path, sizeof( path ), "sound/ui/gamestartup*.mp3" );
	V_FixSlashes( path );

	char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
	if ( fn )
	{
		do
		{
			char ext[ 10 ];
			V_ExtractFileExtension( fn, ext, sizeof( ext ) );

			if ( !V_stricmp( ext, "mp3" ) )
			{
				char temp[ 512 ];
				V_snprintf( temp, sizeof( temp ), "ui/%s", fn );

				V_FixSlashes( temp );
				fileNames.push_back( temp );
			}
	
			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}

	// did we find any?
	if ( !fileNames.empty() )
	{
// dgoodenough - SystemTime is absent on PS3, just select first file for now
// PS3_BUILDFIX
// FIXME - we need to find some sort of entropy here and select based on that.
// @wge Fix for OSX too.
		int index = 0;
		if ( ks::is_valid_index( fileNames, index ) )
		{
			char found[ 512 ];

			// escape chars "*#" make it stream, and be affected by snd_musicvolume
			V_snprintf( found, sizeof( found ), "play *#%s", fileNames[index].c_str() );

			engine->ClientCmd_Unrestricted( found );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to setup the game UI
//-----------------------------------------------------------------------------
void CGameUI::Start()
{
	// determine platform location for configuration
	if ( !FindPlatformDirectory( m_szPlatformDir, sizeof( m_szPlatformDir ) ) )
		return;

	// setup config file directory
	char szConfigDir[512];
	V_strncpy( szConfigDir, m_szPlatformDir, sizeof( szConfigDir ) );
	V_strncat( szConfigDir, "config", sizeof( szConfigDir ), COPY_ALL_CHARACTERS );

	Msg( "Config directory: %s\n", szConfigDir );

	g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG");
	g_pFullFileSystem->CreateDirHierarchy("", "CONFIG");

	g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );

	// localization
	g_pLocalize->AddFile( "resource/platform_%language%.txt");
	g_pLocalize->AddFile( "resource/vgui_%language%.txt");

	// ********************************************************************
	// The following is commented out to keep intro music from playing
	// before the intro movie:
	//
	//// Delay playing the startup music until two frames
	//// this allows cbuf commands that occur on the first frame that may start a map
	//m_iPlayGameStartupSound = 2;
	//SetBackgroundMusicDesired( true );
	// ********************************************************************

	// now we are set up to check every frame to see if we can friends/server browser
	m_bTryingToLoadFriends = true;
	m_iFriendsLoadPauseFrames = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Validates the user has a cdkey in the registry
//-----------------------------------------------------------------------------
void CGameUI::ValidateCDKey()
{
}

//-----------------------------------------------------------------------------
// Purpose: Finds which directory the platform resides in
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameUI::FindPlatformDirectory(char *platformDir, int bufferSize)
{
	platformDir[0] = '\0';

	if ( platformDir[0] == '\0' )
	{
		// setup using path relative to game
		if ( ks::platform::current_directory( platformDir, (unsigned long)bufferSize ) )
		{
			V_AppendSlash( platformDir, bufferSize );
			V_strncat(platformDir, "platform", bufferSize, COPY_ALL_CHARACTERS );
			V_AppendSlash( platformDir, bufferSize );
			return true;
		}

		Error( "Unable to determine platform directory\n" );
		return false;
	}

	return (platformDir[0] != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{

	// notify all the modules of Shutdown

	ModInfo().FreeModInfo();
	
	ConVar_Unregister( side_registry().convars );
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to activate the gameUI
//-----------------------------------------------------------------------------
void CGameUI::ActivateGameUI()
{
	engine->ExecuteClientCmd("gameui_activate");
	// Lock the UI to a particular player
	SetGameUIActiveSplitScreenPlayerSlot( engine->GetActiveSplitScreenPlayerSlot() );
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to hide the gameUI
//-----------------------------------------------------------------------------
void CGameUI::HideGameUI()
{
	engine->ExecuteClientCmd("gameui_hide");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::PreventEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_preventescape");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::AllowEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_allowescape");
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIActivated()
{
	bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = true;

	// Lock the UI to a particular player
	if ( !bWasActive )
	{
		SetGameUIActiveSplitScreenPlayerSlot( engine->GetActiveSplitScreenPlayerSlot() );
	}

	// pause the server in case it is pausable
	engine->ClientCmd_Unrestricted( "setpause nomsg" );

	SetSavedThisMenuSession( false );

	// Route menu activation directly to the RmlUi documents.
	if ( IsInLevel() )
	{
		RocketPauseMenuDocument::ShowPanel( true, true );
	}
	else
	{
		if ( RocketPauseMenuDocument::IsActive() )
			RocketPauseMenuDocument::ShowPanel( false, true );
		if ( !RocketMainMenuDocument::IsActive() )
			RocketMainMenuDocument::LoadDialog();
		RocketMainMenuDocument::ShowPanel( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game ui, in whatever state it's in
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIHidden()
{
	bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = false;

	// unpause the game when leaving the UI
	engine->ClientCmd_Unrestricted( "unpause nomsg" );

	if ( RocketPauseMenuDocument::IsActive() )
		RocketPauseMenuDocument::ShowPanel( false, true );

	// Hide the main menu too when the game UI is dismissed (e.g. after connecting
	// to a server); otherwise it stays drawn over gameplay.
	if ( RocketMainMenuDocument::IsActive() )
		RocketMainMenuDocument::ShowPanel( false );

	// Restore to default
	if ( bWasActive )
	{
		SetGameUIActiveSplitScreenPlayerSlot( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{

	// Play the start-up music the first time we run frame
	if ( m_iPlayGameStartupSound > 0 )
	{
		m_iPlayGameStartupSound--;		
	}
	else
	{
		UpdateBackgroundMusic();
	}

	if ( m_bTryingToLoadFriends && m_iFriendsLoadPauseFrames-- < 1  )
	{
		// clear the loading flag
		m_bTryingToLoadFriends = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OLD_OnConnectToServer(const char *game, int IP, int port)
{
	// Nobody should use this anymore.
	// Use OnConnectToServer2 instead.
	contract_assert( false );
	OnConnectToServer2( game, IP, port );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OnConnectToServer2(const char *game, int IP, int connectionPort)
{
	m_iGameIP = IP;
	m_iGameConnectionPort = connectionPort;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::OnDisconnectFromServer()
{
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;

	IGameEvent *event = gameeventmanager->CreateEvent( "client_disconnect" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}

}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog )
{
	GameUI().HideGameUI();

	char mapName[MAX_PATH];
	V_strcpy_safe( mapName, levelName ? levelName : "" );
	V_FixSlashes( mapName, '/' );

	if ( bShowProgressDialog )
	{
		StartProgressBar();
	}

	if ( !RocketLoadingScreenDocument::IsVisible() )
		RocketLoadingScreenDocument::ShowPanel( true );

	// Don't play the start game sound if this happens before we get to the first frame
	m_iPlayGameStartupSound = 0;
}

extern ConVar devCheatSkipInputLocking;
//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingFinished(bool bError, const char *failureReason, const char *extendedReason)
{
	StopProgressBar( bError, failureReason, extendedReason );

	// notify all the modules

	HideLoadingBackgroundDialog();

	// ditto cstrike
	HideGameUI();
	
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateProgressBar(float progress, const char *statusText, bool showDialog )
{
	// if either the progress bar or the status text changes, redraw the screen
	bool bRedraw = false;

	if ( ContinueProgressBar( progress, showDialog ) )
	{
		bRedraw = true;
	}

	if ( SetProgressBarStatusText( statusText, showDialog ) )
	{
		bRedraw = true;
	}

	return bRedraw;
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateSecondaryProgressBar(float progress, const wchar_t *desc )
{
	// if either the progress bar or the status text changes, redraw the screen
	SetSecondaryProgressBar( progress );
	SetSecondaryProgressBarText( desc );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetProgressLevelName( const char *levelName )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar()
{
	// open a loading dialog
	m_szPreviousStatusText[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the screen should be updated
//-----------------------------------------------------------------------------
bool CGameUI::ContinueProgressBar( float progressFraction, bool showDialog )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
// CStrike15 handles error messages elsewhere. (ClientModeCSFullscreen::OnEvent)
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
bool CGameUI::SetProgressBarStatusText(const char *statusText, bool showDialog )
{
	if (!statusText)
		return false;

	if (!stricmp(statusText, m_szPreviousStatusText))
		return false;

	V_strncpy(m_szPreviousStatusText, statusText, sizeof(m_szPreviousStatusText));
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar(float progress /* range [0..1] */)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText( const wchar_t *desc )
{
	if (!desc)
		return;
}

//-----------------------------------------------------------------------------
// Purpose: Returns prev settings
//-----------------------------------------------------------------------------
bool CGameUI::SetShowProgressText( bool show )
{
    return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently playing the game
//-----------------------------------------------------------------------------
bool CGameUI::IsInLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && !engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're at the main menu and a background level is loaded
//-----------------------------------------------------------------------------
bool CGameUI::IsInBackgroundLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're in a multiplayer game
//-----------------------------------------------------------------------------
bool CGameUI::IsInMultiplayer()
{
	return (IsInLevel() && engine->GetMaxClients() > 1);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're console ui
//-----------------------------------------------------------------------------
bool CGameUI::IsConsoleUI()
{
	return m_bIsConsoleUI;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we've saved without closing the menu
//-----------------------------------------------------------------------------
bool CGameUI::HasSavedThisMenuSession()
{
	return m_bHasSavedThisMenuSession;
}

void CGameUI::SetSavedThisMenuSession( bool bState )
{
	m_bHasSavedThisMenuSession = bState;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the loading background dialog visible, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::ShowLoadingBackgroundDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Hides the loading background dialog, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::HideLoadingBackgroundDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a loading background dialog has been set
//-----------------------------------------------------------------------------
bool CGameUI::HasLoadingBackgroundDialog()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Xbox 360 calls from engine to GameUI 
//-----------------------------------------------------------------------------
void CGameUI::ShowMessageDialog( const char* messageID, const char* titleID )
{
}

void CGameUI::CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
}

void CGameUI::CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
}

//-----------------------------------------------------------------------------

void CGameUI::NeedConnectionProblemWaitScreen()
{
}

void CGameUI::ShowPasswordUI( char const *pchCurrentPW )
{
}

//-----------------------------------------------------------------------------
void CGameUI::SetProgressOnStart()
{
	m_bOpenProgressOnStart = true;
}

bool CGameUI::IsPlayingFullScreenVideo()
{
	return false;
}

bool CGameUI::IsTransitionEffectEnabled()
{
	return false;
}

void CGameUI::StartLoadingScreenForCommand( const char* command )
{
}

void CGameUI::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{
}

bool CGameUI::IsBackgroundMusicPlaying( void )
{
	if ( m_nBackgroundMusicGUID == 0 )
	{
		return false;
	}

	return enginesound->IsSoundStillPlaying( m_nBackgroundMusicGUID );
}

void CGameUI::ReleaseBackgroundMusic( void )
{
	if ( m_nBackgroundMusicGUID == 0 )
		return;
	
	enginesound->StopSoundByGuid( m_nBackgroundMusicGUID, true );

	m_nBackgroundMusicGUID = 0;
}

//The way to loop an MP3 is just to constantly check if it is playing and restart it otherwise
static constexpr auto MENUMUSIC_FADETIME = 1.34;

void CGameUI::UpdateBackgroundMusic( void )
{
	if ( m_bBackgroundMusicDesired && !enginesound->GetPreventSound() )
	{	
		const char * pNewMusicExtension = "";

		uint32 unMusicID = 0;
		bool bIsPreview = false;

		if ( m_pPreviewMusicExtension && m_pPreviewMusicExtension[0] )
		{
			pNewMusicExtension = m_pPreviewMusicExtension;
			bIsPreview = true;
		}

		if ( !IsBackgroundMusicPlaying() && !enginesound->IsMoviePlaying() )
		{

			m_flBackgroundMusicStopTime = -1.0;

			char sMusicKit[128];

			m_pMusicExtension = pNewMusicExtension;

			if ( !StringIsEmpty( m_pMusicExtension ) && ( unMusicID > 1 || bIsPreview ) )
			{
				V_sprintf_safe( sMusicKit, "music/%s/%s", m_pMusicExtension, BACKGROUND_MUSIC_FILENAME );
			}
			else
			{
				m_nBackgroundMusicVersion++;

				if ( m_nBackgroundMusicVersion == 1 )
				{
					V_sprintf_safe( sMusicKit, "music/valve_csgo_02/%s", BACKGROUND_MUSIC_FILENAME );
				}
				else
				{
					m_nBackgroundMusicVersion = 0;
					V_sprintf_safe( sMusicKit, "music/valve_csgo_01/%s", BACKGROUND_MUSIC_FILENAME );
				}
			}
			m_nBackgroundMusicGUID = enginesound->EmitAmbientSound( sMusicKit, 1.0 );
			
		}
		else if ( !FStrEq( pNewMusicExtension, m_pMusicExtension ) )
		{
			ReleaseBackgroundMusic();
		}
		else if( ( m_flBackgroundMusicStopTime > -1.0 ) )
		{
			float flDelta = gpGlobals->curtime - m_flBackgroundMusicStopTime;
			float flFadeAmount = 1.0 - ( flDelta / MENUMUSIC_FADETIME );
			enginesound->SetVolumeByGuid( m_nBackgroundMusicGUID, flFadeAmount );
			if( flFadeAmount < .05 )
			{
				SetBackgroundMusicDesired( false );
			}
		}

		if ( m_nQuestAudioGUID && ( m_flQuestAudioTimeEnd < gpGlobals->curtime ) )	// resume main menu music after quest audio is complete
		{
			//		SetBackgroundMusicDesired( true );

			static CGameUIConVarRef snd_musicvolume( "snd_musicvolume" );
			static CGameUIConVarRef snd_menumusic_volume( "snd_menumusic_volume" );

			snd_musicvolume.SetValue( m_flMasterMusicVolumeSavedForMissionAudio );
			snd_menumusic_volume.SetValue( m_flMenuMusicVolumeSavedForMissionAudio );

			m_flQuestAudioTimeEnd = 0;
			m_nQuestAudioGUID = 0;
		}
	}
// 	else if ( m_pAudioFile && m_pAudioFile[ 0 ] )
// 	{
// 		if ( !IsBackgroundMusicPlaying() )
// 		{
// 			m_nBackgroundMusicGUID = enginesound->EmitAmbientSound( m_pAudioFile, 1.0 );
// 		}
// 	}
	else
	{
		ReleaseBackgroundMusic();
	}
}
void CGameUI::StartBackgroundMusicFade( void )
{
	m_flBackgroundMusicStopTime = gpGlobals->curtime;
}
void CGameUI::SetBackgroundMusicDesired( bool bPlayMusic )
{
	m_bBackgroundMusicDesired = bPlayMusic;
}

bool CGameUI::LoadingProgressWantsIsolatedRender( bool bContextValid )
{
	return false;
}

void CGameUI::RestoreTopLevelMenu()
{
	RocketMainMenuDocument::RestorePanel();
}

void CGameUI::SetPreviewBackgroundMusic( const char * pchPreviewMusicPrefix )
{
	static CGameUIConVarRef snd_menumusic_volume("snd_menumusic_volume");
	static CGameUIConVarRef snd_musicvolume("snd_musicvolume");

	// if we're previewing music, store the current volumes and set them to default.
	if ( pchPreviewMusicPrefix && pchPreviewMusicPrefix[0] )
	{
		m_flMainMenuMusicVolume = snd_menumusic_volume.GetFloat();
		m_flMasterMusicVolume = snd_musicvolume.GetFloat();

		snd_menumusic_volume.SetValue( snd_menumusic_volume.GetDefault() );
		snd_musicvolume.SetValue( snd_musicvolume.GetDefault() );
		
	}
	else
	{
		if ( m_flMainMenuMusicVolume != -1 )	// stored music is not -1 so we are previewing
		{
			snd_menumusic_volume.SetValue( m_flMainMenuMusicVolume );
			snd_musicvolume.SetValue( m_flMasterMusicVolume );
		}

		m_flMainMenuMusicVolume = -1;
		m_flMasterMusicVolume = -1;
	}

	m_pPreviewMusicExtension = pchPreviewMusicPrefix;

	ReleaseBackgroundMusic();	// Even when we preview the musickit we have, we still want to restart the track.

}

void CGameUI::PlayQuestAudio( const char * pchAudioFile )
{
static constexpr auto MAINMENU_QUEST_AUDIO_MASTER_VOLUME = 1.0;
static constexpr auto MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME = 0.01;

	if ( !pchAudioFile || !pchAudioFile[0] )
		return;

	if ( m_nQuestAudioGUID )
		return;

	m_flQuestAudioTimeEnd = gpGlobals->curtime + enginesound->GetSoundDuration( pchAudioFile );

//	SetBackgroundMusicDesired( false );

	static CGameUIConVarRef snd_musicvolume( "snd_musicvolume" );

	m_flMasterMusicVolumeSavedForMissionAudio = snd_musicvolume.GetFloat();
	if ( ( snd_musicvolume.GetFloat() < MAINMENU_QUEST_AUDIO_MASTER_VOLUME ) )
	{
		snd_musicvolume.SetValue( ( float )MAINMENU_QUEST_AUDIO_MASTER_VOLUME );
	}

	static CGameUIConVarRef snd_menumusic_volume("snd_menumusic_volume");

	m_flMenuMusicVolumeSavedForMissionAudio = snd_menumusic_volume.GetFloat();
	if ( ( snd_menumusic_volume.GetFloat() > MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME ) )
	{	
		snd_menumusic_volume.SetValue( ( float )MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME );
	}

	m_nQuestAudioGUID = enginesound->EmitAmbientSound( pchAudioFile, 1.0 );

	if (m_nQuestAudioGUID == 0)
	{
		// Playing music failed for some reason so immediately set the music volume
		// back to its initial value.
		snd_musicvolume.SetValue( m_flMasterMusicVolumeSavedForMissionAudio );
		snd_menumusic_volume.SetValue( m_flMenuMusicVolumeSavedForMissionAudio );
		m_flQuestAudioTimeEnd = gpGlobals->curtime;
	}

}

void CGameUI::StopQuestAudio( void )
{
	enginesound->StopSoundByGuid( m_nQuestAudioGUID, true );

	if ( m_flQuestAudioTimeEnd )
		m_flQuestAudioTimeEnd = gpGlobals->curtime;

}

bool CGameUI::IsQuestAudioPlaying( void )
{
	return ( ( m_nQuestAudioGUID != 0 ) ? true : false );

}

static int g_nGameUIActiveSplitscreenPlayerSlot = 0;

int GetGameUIActiveSplitScreenPlayerSlot()
{
	return g_nGameUIActiveSplitscreenPlayerSlot;
}

void SetGameUIActiveSplitScreenPlayerSlot( int nSlot )
{
	if ( g_nGameUIActiveSplitscreenPlayerSlot != nSlot )
	{
		g_nGameUIActiveSplitscreenPlayerSlot = nSlot;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Scans player names
//Passes the player name to be checked in a KeyValues pointer
//with the keyname "name"
// - replaces '&' with '&&' so they will draw in the scoreboard
// - replaces '#' at the start of the name with '*'
//-----------------------------------------------------------------------------

void GameUI_MakeSafeName( const char *oldName, char *newName, int newNameBufSize )
{
	if ( !oldName )
	{
		newName[0] = 0;
		return;
	}

	int newpos = 0;

	for( const char *p=oldName; *p != 0 && newpos < newNameBufSize-1; p++ )
	{
		//check for a '#' char at the beginning

		/*
		if( p == oldName && *p == '#' )
				{
					newName[newpos] = '*';
					newpos++;
				}
				else */

		if( *p == '%' )
		{
			// remove % chars
			newName[newpos] = '*';
			newpos++;
		}
		else if( *p == '&' )
		{
			//insert another & after this one
			if ( newpos+2 < newNameBufSize )
			{
				newName[newpos] = '&';
				newName[newpos+1] = '&';
				newpos+=2;
			}
		}
		else
		{
			newName[newpos] = *p;
			newpos++;
		}
	}
	newName[newpos] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Performs a var args printf into a static return buffer
// Input  : *format - 
//			... - 
// Output : char
//-----------------------------------------------------------------------------
// Unused :: char *VarArgs( const char *format, ... );

// Used to make reading convars simpler; writing convars isn't allowed in this mode
static CEmptyConVar s_EmptyConVar;

// Helper for splitscreen ConVars
CGameUIConVarRef::CGameUIConVarRef( const char *pName )
{
	Init( pName, false );
}

CGameUIConVarRef::CGameUIConVarRef( const char *pName, bool bIgnoreMissing )
{
	Init( pName, bIgnoreMissing );
}

void CGameUIConVarRef::Init( const char *pName, bool bIgnoreMissing )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		cv_t &info = m_Info[ i ];
		char pchName[ 256 ];
		if ( i != 0 )
		{
			V_snprintf( pchName, sizeof( pchName ), "%s%d", pName, i + 1 );
		}
		else
		{
			V_strncpy( pchName, pName, sizeof( pchName ) );
		}

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}

	if ( !IsValid() )
	{
		static bool bFirst = true;
		if ( g_pCVar || bFirst )
		{
			if ( !bIgnoreMissing )
			{
				Warning( "CGameUIConVarRef %s doesn't point to an existing ConVar\n", pName );
			}
			bFirst = false;
		}
	}
}

CGameUIConVarRef::CGameUIConVarRef( ConVar *pConVar )
{
	cv_t &info = m_Info[ 0 ];
	info.m_pConVar = pConVar ? pConVar : &s_EmptyConVar;
	info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );

	for ( int i = 1; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		info = m_Info[ i ];
		char pchName[ 256 ];
		V_snprintf( pchName, sizeof( pchName ), "%s%d", pConVar->GetName(), i + 1 );

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}
}

bool CGameUIConVarRef::IsValid() const
{
	return m_Info[ 0 ].m_pConVar != &s_EmptyConVar;
}

//-----------------------------------------------------------------------------

CGameUiSetActiveSplitScreenPlayerGuard::CGameUiSetActiveSplitScreenPlayerGuard( int slot )
{
	m_nSaveSlot = engine->GetActiveSplitScreenPlayerSlot();
	engine->SetActiveSplitScreenPlayerSlot( slot );
}

CGameUiSetActiveSplitScreenPlayerGuard::~CGameUiSetActiveSplitScreenPlayerGuard()
{
	engine->SetActiveSplitScreenPlayerSlot( m_nSaveSlot );
}

float CGameUIConVarRef::GetMax( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetMaxValue();
}

// [jbright] - Convenience function for retrieving the min value of the convar
float CGameUIConVarRef::GetMin( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetMinValue();
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CModInfo &ModInfo()
{
	static CModInfo s_ModInfo;
	return s_ModInfo;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CModInfo::CModInfo()
{
	m_pModData = new KeyValues("ModData");
	m_wcsGameTitle[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CModInfo::~CModInfo()
{
	FreeModInfo();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModInfo::FreeModInfo()
{
	if (m_pModData)
	{
		m_pModData->deleteThis();
		m_pModData = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::IsMultiplayerOnly()
{
	return (stricmp(m_pModData->GetString("type", ""), "multiplayer_only") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::IsSinglePlayerOnly()
{
	return (stricmp(m_pModData->GetString("type", ""), "singleplayer_only") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModInfo::GetFallbackDir()
{
	return m_pModData->GetString("fallback_dir", "");
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const wchar_t *CModInfo::GetGameTitle()
{
	if (!m_wcsGameTitle[0])
	{
		// for some reason, the standard ILocalize::ConvertANSIToUnicode() strips off
		// the '²' character in 'HALF-LIFE²' - so just do a straight upconvert to unicode
		const char *title = m_pModData->GetString("title", "");
		int i = 0;
		for (; title[i] != 0; ++i)
		{
			m_wcsGameTitle[i] = (wchar_t)title[i];
		}
		m_wcsGameTitle[i] = 0;
	}

	return m_wcsGameTitle;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const wchar_t *CModInfo::GetGameTitle2()
{
	if (!m_wcsGameTitle2[0])
	{
		// for some reason, the standard ILocalize::ConvertANSIToUnicode() strips off
		// the '²' character in 'HALF-LIFE²' - so just do a straight upconvert to unicode
		const char *title2 = m_pModData->GetString("title2", "");
		int i = 0;
		for (; title2[i] != 0; ++i)
		{
			m_wcsGameTitle2[i] = (wchar_t)title2[i];
		}
		m_wcsGameTitle2[i] = 0;
	}

	return m_wcsGameTitle2;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModInfo::GetGameName()
{
	return m_pModData->GetString("game", "");
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
KeyValues *CModInfo::GetHiddenMaps()
{
	return m_pModData->FindKey( "hidden_maps" );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::HasPortals()
{
	return (stricmp(m_pModData->GetString("hasportals", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoDifficulty()
{
	return (stricmp(m_pModData->GetString("nodifficulty", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoModels()
{
	return (stricmp(m_pModData->GetString("nomodels", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoHiModel()
{
	return (stricmp(m_pModData->GetString("nohimodel", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoCrosshair()
{
	return (stricmp(m_pModData->GetString("nocrosshair", "1"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::AdvCrosshair()
{
	return (stricmp(m_pModData->GetString("advcrosshair", "0"), "1") == 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModInfo::LoadCurrentGameInfo()
{
	// Load up gameinfo for the current mod
	char const *filename = "gameinfo.txt";
	m_pModData->LoadFromFile( g_pFullFileSystem, filename );
}

//-----------------------------------------------------------------------------
// Purpose: loads file from null-terminated buffer
//-----------------------------------------------------------------------------
void CModInfo::LoadGameInfoFromBuffer( const char *buffer )
{
	// Load up gameinfo.txt for the current mod
	m_pModData->LoadFromBuffer( "", buffer );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::UseGameLogo()
{
	return ( V_stricmp( m_pModData->GetString( "gamelogo", "0" ), "1" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Interface to running the engine from the UI dlls
//-----------------------------------------------------------------------------
class CRunGameEngine : public IRunGameEngine
{
public:
	// Returns true if the engine is running, false otherwise.
	virtual bool IsRunning()
	{
		return true;
	}

	// Adds text to the engine command buffer. Only works if IsRunning()
	// returns true on success, false on failure
	virtual bool AddTextCommand(const char *text)
	{
		// SERVERBROWSER HACK: -- it just stuffs commands into command buffer
		// which is pretty horrible because it doesn't let game code to react
		if ( text && StringHasPrefix( text, "connect " ) && g_pMatchFramework )
		{
			g_pMatchFramework->CloseSession();
		}

		engine->ClientCmd_Unrestricted((char *)text);
		return true;
	}

	// runs the engine with the specified command line parameters.  Only works if !IsRunning()
	// returns true on success, false on failure
	virtual bool RunEngine(const char *gameName, const char *commandLineParams)
	{
		return false;
	}

	virtual bool RunEngine2(const char *gameDir, const char *commandLineParams, bool isSourceGame)
	{
		return false;
	}

	virtual ERunResult RunEngine( int iAppID, const char *gameDir, const char *commandLineParams )
	{
		return k_ERunResultOkay;
	}
	
	// returns true if the player is currently connected to a game server
	virtual bool IsInGame()
	{
		return engine->GetLevelName() && std::strlen(engine->GetLevelName()) > 0;
	}

	// gets information about the server the engine is currently connected to
	// returns true on success, false on failure
	virtual bool GetGameInfo(char *infoBuffer, int bufferSize)
	{
		//!! need to implement
		return false;
	}

	virtual void SetTrackerUserID(int trackerID, const char *trackerName)
	{
		gameuifuncs->SetFriendsID(trackerID, trackerName);

		// update the player's name if necessary
		ConVarRef name( "name" );
		if ( name.IsValid() && trackerName && *trackerName && !V_strcmp( name.GetString(), "unnamed" ) )
		{
			name.SetValue(trackerName);
		}
	}

	// iterates users
	// returns the number of user
	virtual int GetPlayerCount()
	{
		return engine->GetMaxClients();
	}

	// returns a playerID for a player
	// playerIndex is in the range [0, GetPlayerCount)
	virtual unsigned int GetPlayerFriendsID(int playerIndex)
	{
		player_info_t pi;

		if  ( engine->GetPlayerInfo(playerIndex, &pi ) )
			return pi.friendsID;

		return 0;
	}

	// gets the in-game name of another user, returns NULL if that user doesn't exists
	virtual const char *GetPlayerName(int trackerID)
	{
		return nullptr;
	}

	virtual const char *GetPlayerFriendsName(int trackerID)
	{
		return nullptr;
	}

	// return the build number of the engine
	virtual unsigned int GetEngineBuildNumber()
	{
		return engine->GetEngineBuildNumber();
	}

	// return the product version of the mod being played
	virtual const char *GetProductVersionString()
	{
		return engine->GetProductVersionString();
	}
};

//namespace
//{
////-----------------------------------------------------------------------------
//// Purpose: Interface to running the game engine
////-----------------------------------------------------------------------------
//abstract_class IRunGameEngine_Old : public IBaseInterface
//{
//public:
//	// Returns true if the engine is running, false otherwise.
//	virtual bool IsRunning() = 0;
//
//	// Adds text to the engine command buffer. Only works if IsRunning()
//	// returns true on success, false on failure
//	virtual bool AddTextCommand(const char *text) = 0;
//
//	// runs the engine with the specified command line parameters.  Only works if !IsRunning()
//	// returns true on success, false on failure
//	virtual bool RunEngine(const char *gameDir, const char *commandLineParams) = 0;
//
//	// returns true if the player is currently connected to a game server
//	virtual bool IsInGame() = 0;
//
//	// gets information about the server the engine is currently ctrue on success, false on failure
//	virtual bool GetGameInfo(char *infoBuffer, int bufferSize) = 0;
//
//	// tells the engine our userID
//	virtual void SetTrackerUserID(int trackerID, const char *trackerName) = 0;
//
//	// this next section could probably moved to another interface
//	// iterates users
//	// returns the number of user
//	virtual int GetPlayerCount() = 0;
//
//	// returns a playerID for a player
//	// playerIndex is in the range [0, GetPlayerCount)
//	virtual unsigned int GetPlayerFriendsID(int playerIndex) = 0;
//
//	// gets the in-game name of another user, returns NULL if that user doesn't exists
//	virtual const char *GetPlayerName(int friendsID) = 0;
//
//	// gets the friends name of a player
//	virtual const char *GetPlayerFriendsName(int friendsID) = 0;
//};
//
//#define RUNGAMEENGINE_INTERFACE_VERSION_OLD "RunGameEngine004"
//
//#ifndef _XBOX
//#endif
//}

// BaseModUI High-level windows

//#include "VFoundGames.h"
//#include "VFoundGroupGames.h"
//#include "VGameLobby.h"
//#include "VGenericConfirmation.h"
//#include "VGenericWaitScreen.h"
//#include "VInGameMainMenu.h"
//#include "VMainMenu.h"
//#include "VFooterPanel.h"
//#include "VAttractScreen.h"
//#include "VPasswordEntry.h"
// vgui controls

using namespace BaseModUI;

//setup in GameUI_Interface.cpp
// DWenger - Pulled out temporarily - extern const char *COM_GetModDirectory( void );

// DWenger - Pulled out temporarily - ConVar x360_audio_english("x360_audio_english", "0", 0, "Keeps track of whether we're forcing english in a localized language." );

ConVar demo_ui_enable( side_registry(), "demo_ui_enable", "", FCVAR_DEVELOPMENTONLY, "Suffix for the demo UI" );
ConVar demo_connect_string( side_registry(), "demo_connect_string", "", FCVAR_DEVELOPMENTONLY, "Connect string for demo UI" );

///Asyncronous Operations

ConVar mm_ping_max_green( side_registry(), "ping_max_green", "70" );
ConVar mm_ping_max_yellow( side_registry(), "ping_max_yellow", "140" );
ConVar mm_ping_max_red( side_registry(), "ping_max_red", "250" );

//=============================================================================

const tokenset_t< const char * > BaseModUI::s_characterPortraits[] =
{
	{ "",			"select_Random" },
	{ "random",		"select_Random" },

	//{ "BtnNamVet",	"select_Bill" },
	//{ "BtnTeenGirl",	"select_Zoey" },
	//{ "BtnBiker",		"select_Francis" },
	//{ "BtnManager",	"select_Louis" },

	{ "coach",		"s_panel_lobby_coach" },
	{ "producer",	"s_panel_lobby_producer" },
	{ "gambler",	"s_panel_lobby_gambler" },
	{ "mechanic",	"s_panel_lobby_mechanic" },

	{ "infected",	"s_panel_hand" },

	{ nullptr, "" }
};

//=============================================================================
// Xbox 360 Marketplace entry point
//=============================================================================
struct X360MarketPlaceEntryPoint
{
	DWORD dwEntryPoint;
	uint64 uiOfferID;
};
static X360MarketPlaceEntryPoint g_MarketplaceEntryPoint;

static void GoToMarketplaceForOffer()
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
}

static void ShowMarketplaceUiForOffer()
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
}

// Console command that's fired from the destructive action confirmation for joining a new session while already in a previous session.
static void confirm_join_new_session_exit_current( const CCommand &args );
static ConCommand confirm_join_new_session_exit_current_command( side_registry(), "confirm_join_new_session_exit_current", confirm_join_new_session_exit_current, "Confirm that we wish to join a new session, destroying a previous session", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN );
static void confirm_join_new_session_exit_current( const CCommand &args )
{
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( 
		new KeyValues( "OnInvite", "action", "join" ) );
}

//=============================================================================
//
//=============================================================================
CUIGameData* CUIGameData::m_Instance = 0;
bool CUIGameData::m_bModuleShutDown = false;

//=============================================================================
CUIGameData::CUIGameData() :
	m_CGameUIPostInit( false )
{
	m_LookSensitivity = 1.0f;

	m_flShowConnectionProblemTimer = 0.0f;
	m_flTimeLastFrame = Plat_FloatTime();
	m_bShowConnectionProblemActive = false;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this, [this]( KeyValues *pEvent ) { OnEvent( pEvent ); } );

	m_bXUIOpen = false;

	m_bWaitingForStorageDeviceHandle = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = nullptr;

	m_pSelectStorageClient = nullptr;
}

//=============================================================================
CUIGameData::~CUIGameData()
{
	// Unsubscribe from events system
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//=============================================================================
CUIGameData* CUIGameData::Get()
{
	if ( !m_Instance && !m_bModuleShutDown )
	{
		m_Instance = new CUIGameData();
	}

	return m_Instance;
}

void CUIGameData::Shutdown()
{
	if ( !m_bModuleShutDown )
	{
		m_bModuleShutDown = true;
		delete m_Instance;
		m_Instance = nullptr;
	}
}

//=============================================================================
void CUIGameData::RunFrame()
{

	// DWenger - Pulled out temporarily - RunFrame_Storage();

	// DWenger - Pulled out temporarily - RunFrame_Invite();

	// msmith - Put in the RunFrame for PS3.

	if ( m_flShowConnectionProblemTimer > 0.0f )
	{
		float flCurrentTime = Plat_FloatTime();
		float flTimeElapsed = ( flCurrentTime - m_flTimeLastFrame );

		m_flTimeLastFrame = flCurrentTime;

		if ( flTimeElapsed > 0.0f )
		{
			m_flShowConnectionProblemTimer -= flTimeElapsed;
		}

		if ( m_flShowConnectionProblemTimer > 0.0f )
		{
			// DWenger - Pulled out temporarily
			/*
			if ( !m_bShowConnectionProblemActive &&
				 !CBaseModPanel::GetSingleton().IsVisible() )
			{
				GameUI().ActivateGameUI();
				OpenWaitScreen( "#GameUI_RetryingConnectionToServer", 0.0f );
				m_bShowConnectionProblemActive = true;
			}
			*/
		}
		else
		{
			// DWenger - Pulled out temporarily
			/*
			if ( m_bShowConnectionProblemActive )
			{
				// Before closing this particular waitscreen we need to establish
				// a correct navback, otherwise it will not close - Vitaliy (bugbait #51272)
				if ( CBaseModFrame *pWaitScreen = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
				{
					if ( !pWaitScreen->GetNavBack() )
					{
						if ( CBaseModFrame *pIngameMenu = CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) )
							pWaitScreen->SetNavBack( pIngameMenu );
					}

					if ( !pWaitScreen->GetNavBack() )
					{
						// This waitscreen will fail to close, force the close!
						pWaitScreen->Close();
					}
				}

				CloseWaitScreen( NULL, "Connection Problems" );
				GameUI().HideGameUI();
				m_bShowConnectionProblemActive = false;
			}
			*/
		}
	}
}

//=============================================================================
void CUIGameData::OnGameUIPostInit()
{
	m_CGameUIPostInit = true;
}

//=============================================================================
bool CUIGameData::CanPlayer2Join()
{
	if ( demo_ui_enable.GetString()[0] )
		return false;

	return false;
}

//=============================================================================
void CUIGameData::OpenFriendRequestPanel(int index, uint64 playerXuid)
{
// dgoodenough - this looks to be x360 specific, so tag it as such
// PS3_BUILDFIX
}

//=============================================================================
void CUIGameData::OpenInviteUI( char const *szInviteUiType )
{
}

//=============================================================================
bool CUIGameData::SignedInToLive()
{
	
	return true;
}

bool CUIGameData::AnyUserSignedInToLiveWithMultiplayerDisabled()
{

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg )
{

	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame )
{
		return false;

	if ( SignedInToLive() )
		return false;

	char const *szMsg = "";

	if ( AnyUserSignedInToLiveWithMultiplayerDisabled() )
	{
		szMsg = "#SFUI_MsgBx_NeedLiveNonGoldMsg";

	}
	else
	{
		szMsg = "#SFUI_MsgBx_NeedLiveSinglescreenMsg";

	}

	DisplayOkOnlyMsgBox( pCallerFrame, "#SFUI_XboxLive", szMsg );

	return true;
}

void CUIGameData::DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg )
{
	// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = 
		// DWenger - Pulled out temporarily - static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pCallerFrame, false ) );
	// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;
	// DWenger - Pulled out temporarily - data.pWindowTitle = szTitle;
	// DWenger - Pulled out temporarily - data.pMessageText = szMsg;
	// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
	// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
}

const char *CUIGameData::GetLocalPlayerName( int iController )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

	IPlayer *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !player )
	{
		return "";
	}

	return player->GetName();
}

//////////////////////////////////////////////////////////////////////////

//=============================================================================
void CUIGameData::SetLookSensitivity(float sensitivity)
{
	m_LookSensitivity = sensitivity;

	static CGameUIConVarRef joy_yawsensitivity("joy_yawsensitivity");
	if(joy_yawsensitivity.IsValid())
	{
		float defaultValue = std::atof(joy_yawsensitivity.GetDefault());
		joy_yawsensitivity.SetValue(defaultValue * sensitivity);
	}

	static CGameUIConVarRef joy_pitchsensitivity("joy_pitchsensitivity");
	if(joy_pitchsensitivity.IsValid())
	{
		float defaultValue = std::atof(joy_pitchsensitivity.GetDefault());
		joy_pitchsensitivity.SetValue(defaultValue * sensitivity);
	}
}

//=============================================================================
float CUIGameData::GetLookSensitivity()
{
	return m_LookSensitivity;
}

bool CUIGameData::IsXUIOpen()
{
	return m_bXUIOpen;
}

void CUIGameData::OpenWaitScreen( const char * messageText, float minDisplayTime, KeyValues *pSettings )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] OpenWaitScreen( %s )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// DWenger - Pulled out temporarily - CBaseModFrame * backFrame = CBaseModPanel::GetSingleton().GetWindow( wtActive );
	// DWenger - Pulled out temporarily - if ( wtActive == WT_GENERICWAITSCREEN && backFrame )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - backFrame = backFrame->GetNavBack();
		// DWenger - Pulled out temporarily - DevMsg( "CUIGameData::OpenWaitScreen - setting navback to %s instead of waitscreen\n", backFrame ? backFrame->GetName() : "NULL" );
	// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - if ( wtActive == WT_GENERICCONFIRMATION && backFrame )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - DevWarning( "Cannot display waitscreen! Active window of higher priority: %s\n", backFrame->GetName() );
		// DWenger - Pulled out temporarily - return;
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICWAITSCREEN, backFrame, false, pSettings ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->SetNavBack( backFrame );
		// DWenger - Pulled out temporarily - waitScreen->ClearData();
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::UpdateWaitPanel( const char * messageText, float minDisplayTime )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] UpdateWaitPanel( %s )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime )
{
	// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - Msg( "[GAMEUI] UpdateWaitPanel( %S )\n", messageText );
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - GenericWaitScreen* waitScreen = static_cast<GenericWaitScreen*>( 
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) );
	// DWenger - Pulled out temporarily - if( waitScreen )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - waitScreen->AddMessageText( messageText, minDisplayTime );
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::NeedConnectionProblemWaitScreen ( void )
{
	m_flShowConnectionProblemTimer = 1.0f;
}

static void PasswordEntered()
{
	CUIGameData::Get()->FinishPasswordUI( true );
}

static void PasswordNotEntered()
{
	CUIGameData::Get()->FinishPasswordUI( false );
}

void CUIGameData::ShowPasswordUI( char const *pchCurrentPW )
{
	// DWenger - Pulled out temporarily - PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().OpenWindow( WT_PASSWORDENTRY, NULL, false ) );
	// DWenger - Pulled out temporarily - if ( pwEntry )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - PasswordEntry::Data_t data;
		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_PasswordEntry_Title";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_PasswordEntry_Prompt";
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
		// DWenger - Pulled out temporarily - data.bCancelButtonEnabled = true;
		// DWenger - Pulled out temporarily - data.m_szCurrentPW = pchCurrentPW;
		// DWenger - Pulled out temporarily - data.pfnOkCallback = PasswordEntered;
		// DWenger - Pulled out temporarily - data.pfnCancelCallback = PasswordNotEntered;
		// DWenger - Pulled out temporarily - pwEntry->SetUsageData(data);
	// DWenger - Pulled out temporarily - }
}

void CUIGameData::FinishPasswordUI( bool bOk )
{
	// DWenger - Pulled out temporarily - PasswordEntry *pwEntry = static_cast<PasswordEntry*>( CBaseModPanel::GetSingleton().GetWindow( WT_PASSWORDENTRY ) );
	// DWenger - Pulled out temporarily - if ( pwEntry )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - if ( bOk )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - char pw[ 256 ];
			// DWenger - Pulled out temporarily - pwEntry->GetPassword( pw, sizeof( pw ) );
			// DWenger - Pulled out temporarily - engine->SetConnectionPassword( pw );
		// DWenger - Pulled out temporarily - }
		// DWenger - Pulled out temporarily - else
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->SetConnectionPassword( "" );
		// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - }
}

char const * CUIGameData::GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative )
{
	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		return "WWWWWWWWWWWWWWW";

	return szPlayerNameSpeculative;
}

static void ui_reloadscheme( const CCommand &args );
static ConCommand ui_reloadscheme_command( side_registry(), "ui_reloadscheme", ui_reloadscheme, "Reloads the resource files for the active UI window", 0 );
static void ui_reloadscheme( const CCommand &args )
{
	g_pFullFileSystem->SyncDvdDevCache();
	CUIGameData::Get()->ReloadScheme();
}

void CUIGameData::ReloadScheme()
{
	// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().ReloadScheme();
	// DWenger - Pulled out temporarily - CBaseModFrame *window = CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() );
	// DWenger - Pulled out temporarily - if( window )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - window->ReloadSettings();
	// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
	// DWenger - Pulled out temporarily - if( footer )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - footer->ReloadSettings();
	// DWenger - Pulled out temporarily - }
}

CBaseModFrame * CUIGameData::GetParentWindowForSystemMessageBox()
{
	// DWenger - Pulled out temporarily - WINDOW_TYPE wtActive = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// DWenger - Pulled out temporarily - WINDOW_PRIORITY wPriority = CBaseModPanel::GetSingleton().GetActiveWindowPriority();

	// DWenger - Pulled out temporarily - CBaseModFrame *pCandidate = CBaseModPanel::GetSingleton().GetWindow( wtActive );

	// DWenger - Pulled out temporarily - if ( pCandidate )
	// DWenger - Pulled out temporarily - {
		// DWenger - Pulled out temporarily - if ( wPriority >= WPRI_WAITSCREEN && wPriority <= WPRI_MESSAGE )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - DevMsg( "GetParentWindowForSystemMessageBox: using navback of %s\n", pCandidate->GetName() );
			// DWenger - Pulled out temporarily - }

			// Message would not be able to nav back to waitscreen or another message
			// DWenger - Pulled out temporarily - pCandidate = pCandidate->GetNavBack();
		// DWenger - Pulled out temporarily - }
		// DWenger - Pulled out temporarily - else if ( wPriority > WPRI_MESSAGE )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( UI_IsDebug() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - DevMsg( "GetParentWindowForSystemMessageBox: using NULL since a higher priority window is open %s\n", pCandidate->GetName() );
			// DWenger - Pulled out temporarily - }

			// Message would not be able to nav back to a higher level priority window
			// DWenger - Pulled out temporarily - pCandidate = NULL;
		// DWenger - Pulled out temporarily - }
	// DWenger - Pulled out temporarily - }

	// DWenger - Pulled out temporarily - return pCandidate;
	return nullptr;	// DWenger - temporary code
}

bool CUIGameData::IsActiveSplitScreenPlayerSpectating( void )
{
// 	int iLocalPlayerTeam;
// 	if ( GameClientExports()->GetPlayerTeamIdByUserId( -1, iLocalPlayerTeam ) )
// 	{
// 		if ( iLocalPlayerTeam != GameClientExports()->GetTeamId_Survivor() &&
// 			 iLocalPlayerTeam != GameClientExports()->GetTeamId_Infected() )
// 			return true;
// 	}

	return false;
}

struct ServerCookie_t
{
	uint64 m_uiCookie;
	double m_dblTimeCached;
};
std::map< uint64, ServerCookie_t > g_mapServerCookies;
static uint64 Helper_GetServerCookie( uint64 gsid )
{
	auto it = g_mapServerCookies.find( gsid );
	if ( it == g_mapServerCookies.end() )
		return 0;
	if ( Plat_FloatTime() - it->second.m_dblTimeCached > 60.0f )
		return 0;
	return it->second.m_uiCookie;
}

void CUIGameData::OnEvent( KeyValues *pEvent )
{
	ks::ScratchArena<> scratch;
	char const *szEvent = pEvent->GetName();

	if ( !V_stricmp( "OnSysXUIEvent", szEvent ) )
	{
		m_bXUIOpen = !V_stricmp( "opening", pEvent->GetString( "action", "" ) );
	}
	else if ( !V_stricmp( "OnProfileUnavailable", szEvent ) )
	{
		// Activate game ui to see the dialog
		// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
		// DWenger - Pulled out temporarily - }

		// Pop a message dialog if their storage device was changed
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_AchievementNotWrittenTitle";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_AchievementNotWritten"; 
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

		// DWenger - Pulled out temporarily - confirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( "OnInvite", szEvent ) )
	{
		// Check if the user just accepted invite
		if ( !V_stricmp( "accepted", pEvent->GetString( "action" ) ) )
		{
			// Check if we have an outstanding session
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( !pIMatchSession )
			{
				// DWenger - Pulled out temporarily - Invite_Connecting();
				return;
			}

			// User is accepting an invite and has an outstanding
			// session, TCR requires confirmation of destructive actions
			if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
			{
				*pnConfirmed = 0;
			}

			// Show the prompt to confirm they wish to join a new session
			GameUI().CreateCommandMsgBoxInSlot(
				CMB_SLOT_FULL_SCREEN, 
				"#SFUI_Confirm_JoinAnotherGameTitle", 
				"#SFUI_Confirm_JoinAnotherGameText", 
				true, 
				true, 
				"confirm_join_new_session_exit_current\n", 
				nullptr, 
				nullptr, 
				nullptr );
		}
		else if ( !V_stricmp( "storage", pEvent->GetString( "action" ) ) )
		{
			// DWenger - Pulled out temporarily - if ( !Invite_IsStorageDeviceValid() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - if ( int *pnConfirmed = ( int * ) pEvent->GetPtr( "confirmed" ) )
				// DWenger - Pulled out temporarily - {
					// DWenger - Pulled out temporarily - *pnConfirmed = 0;	// make the invite accepting code wait
				// DWenger - Pulled out temporarily - }
			// DWenger - Pulled out temporarily - }
		}
		else if ( !V_stricmp( "error", pEvent->GetString( "action" ) ) )
		{
			char const *szReason = pEvent->GetString( "error", "" );

			if ( XBX_GetNumGameUsers() < 2 )
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline1", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer1", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole1", RemapText_t::MATCH_FULL },
					{ nullptr, nullptr, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}
			else
			{
				RemapText_t arrText[] = {
					{ "", "#InviteError_Unknown", RemapText_t::MATCH_FULL },
					{ "NotOnline", "#InviteError_NotOnline2", RemapText_t::MATCH_FULL },
					{ "NoMultiplayer", "#InviteError_NoMultiplayer2", RemapText_t::MATCH_FULL },
					{ "SameConsole", "#InviteError_SameConsole2", RemapText_t::MATCH_FULL },
					{ nullptr, nullptr, RemapText_t::MATCH_FULL }
				};

				szReason = RemapText_t::RemapRawText( arrText, szReason );
			}

			// Show the message box
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );

			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_XboxLive";
			// DWenger - Pulled out temporarily - data.pMessageText = szReason;
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		}
	}
	else if ( !V_stricmp( "OnSysStorageDevicesChanged", szEvent ) )
	{

		// If a storage device change is in progress, the simply ignore
		// the notification callback, but pop the dialog
		if ( m_pSelectStorageClient )
		{
			DevWarning( "Ignored OnSysStorageDevicesChanged while the storage selection was in progress...\n" );
		}

		// Activate game ui to see the dialog
		// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
		// DWenger - Pulled out temporarily - }

		// Pop a message dialog if their storage device was changed
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
			// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#GameUI_Console_StorageRemovedTitle";
		// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_StorageDeviceRemoved"; 
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;
		
		// DWenger - Pulled out temporarily - extern void OnStorageDevicesChangedSelectNewDevice();
		// DWenger - Pulled out temporarily - data.pfnOkCallback = m_pSelectStorageClient ? NULL : &OnStorageDevicesChangedSelectNewDevice;	// No callback if already in the middle of selecting a storage device

		// DWenger - Pulled out temporarily - confirmation->SetUsageData( data );
	}
	else if ( !V_stricmp( "OnSysInputDevicesChanged", szEvent ) )
	{
		unsigned int nInactivePlayers = 0;  // Number of users on the spectating team (ie. idle), or disconnected in this call
		int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
		int nDisconnectedDevices = pEvent->GetInt( "mask" );
		for ( unsigned int nSlot = 0; nSlot < XBX_GetNumGameUsers(); ++nSlot, nDisconnectedDevices >>= 1 )
		{
			engine->SetActiveSplitScreenPlayerSlot( nSlot );
			
			// See if this player is spectating (ie. idle)
			bool bSpectator = IsActiveSplitScreenPlayerSpectating();
			if ( bSpectator )
			{
				nInactivePlayers++;
			}
		
			if ( nDisconnectedDevices & 0x1 )
			{
				// Only count disconnections if that player wasn't idle
				if ( !bSpectator )
				{
					nInactivePlayers++;
				}

				engine->ClientCmd( "go_away_from_keyboard" );
			}
		}
		engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

		// If all the spectators and all the disconnections account for all possible users, we need to pop a message
		// Also, if the GameUI is up, always show the disconnection message
		// DWenger - Pulled out temporarily - if ( CBaseModPanel::GetSingleton().IsVisible() || nInactivePlayers == XBX_GetNumGameUsers() )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - if ( !CBaseModPanel::GetSingleton().IsVisible() )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - engine->ExecuteClientCmd( "gameui_activate" );
			// DWenger - Pulled out temporarily - }

			// Pop a message if a valid controller was removed!
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION,
				// DWenger - Pulled out temporarily - GetParentWindowForSystemMessageBox(), false ) );
			
			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;
			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_ControllerUnpluggedTitle";
			// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_ControllerUnplugged";
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		// DWenger - Pulled out temporarily - }
	}
	else if ( !V_stricmp( "OnMatchPlayerMgrReset", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );
		bool bShowDisconnectedMsgBox = true;
		if ( !V_stricmp( szReason, "GuestSignedIn" ) )
		{
			char const *szDestroyedSessionState = pEvent->GetString( "settings/game/state", "lobby" );
			if ( !V_stricmp( "lobby", szDestroyedSessionState ) )
				bShowDisconnectedMsgBox = false;
		}

		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the attract screen
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = bShowDisconnectedMsgBox ? static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) ) : NULL;
		// DWenger - Pulled out temporarily - CAttractScreen::SetAttractMode( CAttractScreen::ATTRACT_GAMESTART );
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_ATTRACTSCREEN, NULL );

		// DWenger - Pulled out temporarily - if ( confirmation )
		// DWenger - Pulled out temporarily - {
			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_SignInChangeC";
			// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_SignInChange";
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			// DWenger - Pulled out temporarily - if ( !V_stricmp( szReason, "GuestSignedIn" ) )
			// DWenger - Pulled out temporarily - {
				// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_DisconnectedFromSession";	// "Disconnect"
				// DWenger - Pulled out temporarily - data.pMessageText = "#SFUI_MsgBx_SignInChange";				// "Sign-in change has occured."
			// DWenger - Pulled out temporarily - }

			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);

		// DWenger - Pulled out temporarily - }
	}
	else if ( !V_stricmp( "OnEngineDisconnectReason", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason", "" );

		if ( char const *szDisconnectHdlr = pEvent->GetString( "disconnecthdlr", nullptr ) )
		{
			// If a disconnect handler was set during the event, then we don't interfere with
			// the dialog explaining disconnection, just let the disconnect handler do everything.
			return;
		}

		RemapText_t arrText[] = {
			{ "", "#DisconnectReason_Unknown", RemapText_t::MATCH_FULL },
			{ "Lost connection to LIVE", "#DisconnectReason_LostConnectionToLIVE", RemapText_t::MATCH_FULL },
			{ "Player removed from host session", "#DisconnectReason_PlayerRemovedFromSession", RemapText_t::MATCH_SUBSTR },
			{ "Connection to server timed out", "#SFUI_MsgBx_DisconnectedFromServer", RemapText_t::MATCH_SUBSTR },
			{ "Server shutting down", "#SFUI_MsgBx_DisconnectedServerShuttingDown", RemapText_t::MATCH_SUBSTR },
			{ "Added to banned list", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "Kicked and banned", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "You have been voted off", "#SessionError_Kicked", RemapText_t::MATCH_SUBSTR },
			{ "All players idle", "#L4D_ServerShutdownIdle", RemapText_t::MATCH_SUBSTR },
			{ nullptr, nullptr, RemapText_t::MATCH_FULL }
		};

		szReason = RemapText_t::RemapRawText( arrText, szReason );

		//
		// Go back to main menu and display the disconnection reason
		//
		engine->HideLoadingPlaque(); // This may not go away unless we force it to hide

		// Go to the main menu
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

		// Show the message box
		// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
		// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

		// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

		// DWenger - Pulled out temporarily - data.pWindowTitle = "#SFUI_MsgBx_DisconnectedFromSession";	// "Disconnect"
		// DWenger - Pulled out temporarily - data.pMessageText = szReason;
		// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

		// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
	}
	else if ( !V_stricmp( "OnEngineEndGame", szEvent ) )
	{
		// If we are connected and there was no session object to handle the event
		if ( !g_pMatchFramework->GetMatchSession() )
		{
			// Issue the disconnect command
			engine->ExecuteClientCmd( "disconnect" );
		}
	}
	else if ( !V_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !V_stricmp( "error", pEvent->GetString( "state", "" ) ) )
		{
			g_pMatchFramework->CloseSession();

			char chErrorMsgBuffer[128] = {0};
			char chErrorTitleBuffer[128] = {0};
			char const *szError = pEvent->GetString( "error", "" );
			char const *szErrorTitle = "#SFUI_MsgBx_DisconnectedFromSession";

			RemapText_t arrText[] = {
				{ "", "#SessionError_Unknown", RemapText_t::MATCH_FULL },
				{ "n/a", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "create", "#SessionError_Create", RemapText_t::MATCH_FULL },
				{ "createclient", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
				{ "connect", "#SessionError_Connect", RemapText_t::MATCH_FULL },
				{ "full", "#SessionError_Full", RemapText_t::MATCH_FULL },
				{ "lock", "#SessionError_Lock", RemapText_t::MATCH_FULL },
				{ "kicked", "#SessionError_Kicked", RemapText_t::MATCH_FULL },
				{ "migrate", "#SessionError_Migrate", RemapText_t::MATCH_FULL },
				{ "nomap", "#SessionError_NoMap", RemapText_t::MATCH_FULL },
				{ nullptr, nullptr, RemapText_t::MATCH_FULL }
			};

			szError = RemapText_t::RemapRawText( arrText, szError );

			if ( !V_stricmp( "turequired", szError ) )
			{
				// Special case for TU required message
				// If we have a localization string for the TU message then this means that the other box
				// is running and older version of the TU
				char const *szTuRequiredCode = pEvent->GetString( "turequired" );
				CFmtStr strLocKey( "#SessionError_TU_Required_%s", szTuRequiredCode );
				if ( g_pLocalize->Find( strLocKey ) )
				{
					V_strncpy( chErrorMsgBuffer, strLocKey, sizeof( chErrorMsgBuffer ) );
					szError = chErrorMsgBuffer;
				}
				else
				{
					szError = "#SessionError_TU_RequiredMessage";
				}
				szErrorTitle = "#SessionError_TU_RequiredTitle";
			}

			// Go to the main menu
			// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().CloseAllWindows( CBaseModPanel::CLOSE_POLICY_EVEN_MSGS );

			// Show the message box
			// DWenger - Pulled out temporarily - GenericConfirmation* confirmation = static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, false ) );
			// DWenger - Pulled out temporarily - CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL );

			// DWenger - Pulled out temporarily - GenericConfirmation::Data_t data;

			// DWenger - Pulled out temporarily - data.pWindowTitle = szErrorTitle;
			// DWenger - Pulled out temporarily - data.pMessageText = szError;
			// DWenger - Pulled out temporarily - data.bOkButtonEnabled = true;

			if ( !V_stricmp( "dlcrequired", szError ) )
			{
				// Special case for DLC required message
				uint64 uiDlcRequiredMask = pEvent->GetUint64( "dlcrequired" );
				int iDlcRequired = 0;

				// Find the first DLC in the reported missing mask that is required
				for ( int k = 1; k < sizeof( uiDlcRequiredMask ); ++ k )
				{
					if ( uiDlcRequiredMask & ( 1ull << k ) )
					{
						iDlcRequired = k;
						break;
					}
				}

				CFmtStr strLocKey( "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				if ( !g_pLocalize->Find( strLocKey ) )
					iDlcRequired = 0;

				// Try to figure out if this DLC is paid/free/unknown
				KeyValues *kvDlcDetails = new KeyValues( "" );
				KeyValues::AutoDelete autodelete_kvDlcDetails( kvDlcDetails );
				if ( !kvDlcDetails->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/dlcdetailsinfo.res", "MOD" ) )
					kvDlcDetails = nullptr;

				// Determine the DLC offer ID
				uint64 uiDlcOfferID = 0ull;
				if ( 1 != std::sscanf( kvDlcDetails->GetString( CFmtStr( "dlc%d/offerid", iDlcRequired ) ), "0x%llx", &uiDlcOfferID ) )
					uiDlcOfferID = 0ull;
				
				// Format the strings
				bool bKicked = !V_stricmp( pEvent->GetString( "action" ), "kicked" );
				wchar_t const *wszLine1 = g_pLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", bKicked ? "Kicked" : "Join", iDlcRequired ) );
				wchar_t const *wszLine2 = g_pLocalize->Find( CFmtStr( "#SessionError_DLC_Required%s_%d", uiDlcOfferID ? "Offer" : "Message", iDlcRequired ) );
				
				int numBytesTwoLines = ( V_wcslen( wszLine1 ) + V_wcslen( wszLine2 ) + 4 ) * sizeof( wchar_t );
				wchar_t *pwszTwoLines = ( wchar_t * ) scratch.AllocBytes( numBytesTwoLines );
				V_snwprintf( pwszTwoLines, numBytesTwoLines, L"%s%s", wszLine1, wszLine2 );
				// DWenger - Pulled out temporarily - data.pMessageTextW = pwszTwoLines;
				// DWenger - Pulled out temporarily - data.pMessageText = NULL;

				V_snprintf( chErrorTitleBuffer, sizeof( chErrorTitleBuffer ), "#SessionError_DLC_RequiredTitle_%d", iDlcRequired );
				// DWenger - Pulled out temporarily - data.pWindowTitle = chErrorTitleBuffer;

				if ( uiDlcOfferID )
				{
					// DWenger - Pulled out temporarily - data.bCancelButtonEnabled = true;
					// DWenger - Pulled out temporarily - data.pfnOkCallback = GoToMarketplaceForOffer;
					
					g_MarketplaceEntryPoint.uiOfferID = uiDlcOfferID;
					g_MarketplaceEntryPoint.dwEntryPoint = kvDlcDetails->GetInt( CFmtStr( "dlc%d/type", iDlcRequired ) );
				}
			}
			
			// DWenger - Pulled out temporarily - confirmation->SetUsageData(data);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
//
// A bunch of helper KeyValues hierarchy readers
//
//
//////////////////////////////////////////////////////////////////////////

bool GameModeHasDifficulty( char const *szGameMode )
{
	return !V_stricmp( szGameMode, "coop" ) || !V_stricmp( szGameMode, "realism" );
}

char const * GameModeGetDefaultDifficulty( char const *szGameMode )
{
	if ( !GameModeHasDifficulty( szGameMode ) )
		return "normal";

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetActiveUserId() );
	if ( !pProfile )
		return "normal";

	UserProfileData const &upd = pProfile->GetPlayerProfileData();
	switch ( upd.difficulty )
	{
	case 1: return "easy";
	case 2: return "hard";
	default: return "normal";
	}
}

bool GameModeHasRoundLimit( char const *szGameMode )
{
	return !V_stricmp( szGameMode, "scavenge" ) || !V_stricmp( szGameMode, "teamscavenge" );
}

bool GameModeIsSingleChapter( char const *szGameMode )
{
	return !V_stricmp( szGameMode, "survival" ) || !V_stricmp( szGameMode, "scavenge" ) || !V_stricmp( szGameMode, "teamscavenge" );
}

// DWenger - Pulled out temporarily
/*
const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( V_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		V_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			V_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = V_strlen( modDir );
			V_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}
*/

uint64 GetDlcInstalledMask()
{
	static CGameUIConVarRef mm_dlcs_mask_fake( "mm_dlcs_mask_fake" );
	char const *szFakeDlcsString = mm_dlcs_mask_fake.GetString();
	if ( *szFakeDlcsString )
		return std::atoi( szFakeDlcsString );

	static CGameUIConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	uint64 uiDLCmask = ( unsigned ) mm_dlcs_mask_extras.GetInt();

	bool bSearchPath = false;
	int numDLCs = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPath );

	for ( int j = 0; j < numDLCs; ++ j )
	{
		unsigned int uiDlcHeader = 0;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( j, &uiDlcHeader, nullptr, 0 ) )
			continue;

		int idDLC = DLC_LICENSE_ID( uiDlcHeader );
		if ( idDLC < 1 || idDLC >= 31 )
			continue;	// unsupported DLC id

		uiDLCmask |= ( 1ull << idDLC );
	}

	return uiDLCmask;
}

using namespace BaseModUI;

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996L
#endif

void CUIGameData::RunFrame_Storage()
{

}

#undef ERROR_SUCCESS
#undef ERROR_IO_INCOMPLETE
