// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;


import ks.core.color;
import ks.core.dbg;
import ks.core.logging;
import ks.core.memalloc;
import ks.core.vprof;
import ks.platform;
import ks.simd;
import ks.tier1.checksum_crc;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.string_ops;
import ks.tier1.strtools;
import ks.tier1.vector_ops;

#include "sound.h"
#include "soundservice.h"
#include "filesystem.h"

#include "snd_channels.h"
#include "snd_audio_source.h"
#include "snd_dev_xaudio.h"
#include "snd_sfx.h"

#include "audio/private/posix_stubs.h"

#include "snd_dma.h"
#include "sos_system.h"
#include "sos_op.h"
#include "sound.h"
#include "../../debugoverlay.h"


Color CollectionColor( 245, 245, 245, 255 );
Color StackColor( 225, 225, 245, 255 );

ConVar snd_sos_show_operator_init("snd_sos_show_operator_init", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_start("snd_sos_show_operator_start", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_prestart("snd_sos_show_operator_prestart", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_updates("snd_sos_show_operator_updates", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_shutdown("snd_sos_show_operator_shutdown", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_parse("snd_sos_show_operator_parse", "0", FCVAR_CHEAT );

ConVar snd_sos_list_operator_updates("snd_sos_list_operator_updates", "0", FCVAR_CHEAT );

ConVar snd_sos_show_operator_entry_filter( "snd_sos_show_operator_entry_filter", "", FCVAR_CHEAT );


extern CScratchPad g_scratchpad;

//-----------------------------------------------------------------------------
// CSosOperatorStack
//-----------------------------------------------------------------------------
CSosOperatorStack::CSosOperatorStack( SosStackType_t SosType, stack_data_t &stackData )
{
	m_SOSType = SosType;
	m_pMemPool = nullptr;
	m_nMemSize = 0;
	m_stopType = SOS_STOP_NONE;
	m_flStopTime = -1.0;
	m_nChannelGuid = stackData.m_nGuid;
	m_flStartTime = stackData.m_flStartTime;
	m_nScriptHash = stackData.m_nSoundScriptHash;
	m_pOperatorsKV = stackData.m_pOperatorsKV;

}
CSosOperatorStack::~CSosOperatorStack()
{
	g_pSoundOperatorSystem->RemoveChannelFromTracks( m_nChannelGuid );
	
	if ( m_pMemPool )
	{
		g_pMemAlloc->Free( m_pMemPool );
	}
}

void CSosOperatorStack::SetStopType( SOSStopType_t stopType )
{
	if( stopType != SOS_STOP_NONE )
	{
		m_stopType = stopType;
	}
}
void CSosOperatorStack::SetScriptHash( HSOUNDSCRIPTHASH nHash )
 {
	 m_nScriptHash = nHash;
	 KeyValues *pOperatorsKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( nHash );
	 m_pOperatorsKV = pOperatorsKV;
}
void CSosOperatorStack::AddToTail( CSosOperator *pOperator, const char *pName )
{
	int nIndex = std::ssize( m_vStack ); m_vStack.push_back( pOperator );
	// a repeated name keeps resolving to its first operator
	auto it = m_vOperatorMap.emplace( pName, nIndex ).first;
	m_vOperatorNames.push_back( it->first.c_str() );
}

int CSosOperatorStack::FindOperatorViaOffset( std::size_t nOffset )
{
	size_t memOffset = 0;
	int i;
	for( i = 0; i < std::ssize( m_vStack ); i++ )
	{
		CSosOperator *pOp = m_vStack[i];
		if ( pOp )
		{
			memOffset += SosOps::GetSize( pOp );
			if( memOffset > nOffset )
			{
				break;
			}
		}
	}
	return i;

}

CSosOperator *CSosOperatorStack::FindOperator( const char *pName, void **pStructHandle )
{
	auto itOp = m_vOperatorMap.find( pName );
	int nOpIndex = itOp != m_vOperatorMap.end() ? itOp->second : -1;
	std::size_t memOffset = 0;
	*pStructHandle = m_pMemPool;
	for( int i = 0; i < nOpIndex; i++ )
	{
		CSosOperator *pOp = m_vStack[i];
		if ( pOp )
		{
			memOffset += SosOps::GetSize( pOp );
			char *pStructMem = ((char *)m_pMemPool) + memOffset;
			*pStructHandle = (void *)pStructMem;
		}
		else
		{
			// error!
			return nullptr;
		}
	}
	return m_vStack[nOpIndex];

}
int CSosOperatorStack::FindOperator( const char *pName )
{
	auto itOp = m_vOperatorMap.find( pName );
	int nIndex = itOp != m_vOperatorMap.end() ? itOp->second : -1;
	if ( !ks::is_valid_index( m_vStack, nIndex ) )
	{
		// error
		return -1;
	}
	return nIndex;

}
int CSosOperatorStack::GetOperatorOffset( int nIndex )
{
	if ( !ks::is_valid_index( m_vStack, nIndex ) )
	{
		// error
		return -1;
	}

	std::size_t memOffset = 0;
	for ( int i = 0; i < nIndex; i++ )
	{
		CSosOperator *pOp = m_vStack[i];
		if ( pOp )
		{
			memOffset += SosOps::GetSize( pOp );
		}
	}
	return memOffset;
}

int CSosOperatorStack::GetOperatorOutputOffset( const char *pOperatorName, const char *pOutputName )
{
	int nOpIndex = FindOperator( pOperatorName );
	if ( ! ks::is_valid_index( m_vStack, nOpIndex ) )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Unable to find referenced operator: %s", pOperatorName );
		return -1;
	}
	CSosOperator *pOperator = m_vStack[ nOpIndex ];
	std::size_t nOpOffset = GetOperatorOffset( nOpIndex );
	size_t nOutputOffset = pOperator->GetOutputOffset( pOutputName );

	return nOpOffset + nOutputOffset;

}
void CSosOperatorStack::Init( )
{
	std::size_t TotalSize = 0;
	for( int i = 0 ; i < std::ssize( m_vStack ); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;

		m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
		SosOps::StackInit( m_vStack[i], (void *)pCharMem, this, i );
		TotalSize += SosOps::GetSize( m_vStack[i] );
	}




}
void CSosOperatorStack::Shutdown( )
{
	// 	if ( snd_sos_show_operator_updates.GetInt() )
	// 	{
	// 		Log_Msg( LOG_SND_OPERATORS, StackColor, "Operator Stack: %s\n", m_nName );
	// 	}

	std::size_t TotalSize = 0;
	for( int i = 0 ; i < std::ssize( m_vStack ); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;

		m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
		SosOps::StackShutdown( m_vStack[i], (void *)pCharMem, this, i );
		TotalSize += SosOps::GetSize( m_vStack[i] );
	}
}

bool CSosOperatorStack::ShouldPrintOperators( ) const
{
	const char *pFilterString = snd_sos_show_operator_entry_filter.GetString();

	if( !pFilterString || !pFilterString[0] )
	{
		return true;
	}
	if( V_stristr( g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ), pFilterString ) )
	{
		return true;
	}
	return false;
}

bool CSosOperatorStack::ShouldPrintOperatorUpdates( ) const
{
	if ( !snd_sos_show_operator_updates.GetInt() )
	{
		return false;
	}

	return ShouldPrintOperators();

}
void CSosOperatorStack::Execute( channel_t *pChannel, CScratchPad *pScratchPad )
{
	CVProfScope _( "CSosOperatorStack::Execute", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( ShouldPrintOperatorUpdates( ) || snd_sos_list_operator_updates.GetInt() )
	{
		Color SEColor(255, 255, 145, 255);
		Log_Msg( LOG_SND_OPERATORS, SEColor, "\nUpdate Sound Entry: %s\n", g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ) );
		Log_Msg( LOG_SND_OPERATORS, StackColor, "Operator Stack: %s\n", m_nName );
	}
	if( !pChannel )
	{
		pChannel = S_FindChannelByGuid( m_nChannelGuid );
	}

	std::size_t TotalSize = 0;
	for( int i = 0 ; i < std::ssize( m_vStack ); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;
		CSosOperator_t *pStructMem = (CSosOperator_t *)pCharMem;

		m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
		if( pStructMem->m_flExecute[0] > 0.0 && !( pStructMem->m_bExecuteOnce && pStructMem->m_bHasExecuted ) )
		{
			m_pCurrentOperatorName = m_vOperatorNames[ i ];
			SosOps::Execute( m_vStack[i], (void *)pCharMem, pChannel, pScratchPad, this, i );
			pStructMem->m_bHasExecuted = true;

			if( ShouldPrintOperatorUpdates( ) )
			{
				SosOps::Print( m_vStack[i], (void *)pCharMem, this, i, 0 );
			}
			// diagnostics
	
		}
		else if( ShouldPrintOperatorUpdates( ) )
		{
			SosOps::Print( m_vStack[i], (void *)pCharMem, this, i, 1 );
		}

		TotalSize += SosOps::GetSize( m_vStack[i] );
	}

}

void CSosOperatorStack::ExecuteIterator( channel_t *pChannel, CScratchPad *pScratchPad, const void *pStopMemBlock, const char *pOperatorName, int * pnOperatorIndex )
{
	if ( ShouldPrintOperatorUpdates( ) || snd_sos_list_operator_updates.GetInt() )
	{
		Color SEColor(255, 255, 145, 255);
		Log_Msg( LOG_SND_OPERATORS, SEColor, "\nUpdate Sound Entry: %s\n", g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ) );
		Log_Msg( LOG_SND_OPERATORS, StackColor, "Operator Stack: %s\n", m_nName );
	}

	int nIndex = *pnOperatorIndex;
	if ( nIndex < 0 )
	{
		nIndex = FindOperator( pOperatorName );
		if ( nIndex < 0 )
		{
			Log_Warning(LOG_SND_OPERATORS, "Error: Execute iterator unable to find sound operator %s\n", pOperatorName );
			return;
		}
		*pnOperatorIndex = nIndex;	// Update the passed operator index
	}
	else
	{
	}

	std::size_t TotalSize = 0;
	for( int i = 0; i < std::ssize( m_vStack ); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;
		CSosOperator_t *pStructMem = (CSosOperator_t *)pCharMem;

		if ( i >= nIndex )
		{
			m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
			if( pStructMem->m_flExecute[0] > 0.0 && !( pStructMem->m_bExecuteOnce && pStructMem->m_bHasExecuted ) )
			{
				m_pCurrentOperatorName = m_vOperatorNames[ i ];
				SosOps::Execute( m_vStack[i], (void *)pCharMem, pChannel, pScratchPad, this, i );
				pStructMem->m_bHasExecuted = true;

				// diagnostics
				if ( ShouldPrintOperatorUpdates( ) )
				{
					SosOps::Print( m_vStack[i], (void *)pCharMem, this, i, 0 );
				}
			}
			else if( ShouldPrintOperatorUpdates( ) )
			{
				const char *pFilterString = snd_sos_show_operator_entry_filter.GetString();
				if( pFilterString && pFilterString[0] && 
					V_stristr( g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ), pFilterString ) )
				{
					SosOps::Print( m_vStack[i], (void *)pCharMem, this, i, 1 );
				}
			}
			if( pCharMem == pStopMemBlock )
			{
				return;
			}
		}
		TotalSize += SosOps::GetSize( m_vStack[i] );
	}
}

void CSosOperatorStack::Copy( CSosOperatorStack *pSrcStack, std::size_t nMemOffset )
{
	// copy first so we can alter
	V_memcpy((char *)m_pMemPool + nMemOffset, pSrcStack->m_pMemPool, pSrcStack->m_nMemSize );
	
	std::size_t nTotalOffset = 0;

	// UGLY, MAKE POINTERS!
	for ( int i = 0; i < std::ssize( pSrcStack->m_vStack ); i++ )
	{
 		CSosOperator *pOperator = pSrcStack->m_vStack[i];
		const char *pName = pSrcStack->m_vOperatorNames[ i ];

		// same names not allowed?
		if ( m_vOperatorMap.contains( pName ) )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Importing operator with same name as existing operator: %s\n", pName );
		}

		AddToTail( pOperator, pName );
		pOperator->OffsetConnections( (char *)m_pMemPool + nMemOffset + nTotalOffset, nMemOffset );

		nTotalOffset += SosOps::GetSize( pOperator ); 
	}
}

void CSosOperatorStack::Print( int nLevel )
{
	if( !ShouldPrintOperators() )
	{
		return;
	}
	Log_Msg( LOG_SND_OPERATORS, StackColor, "\n%*sOperator Stack: %s\n%*sSize: %i\n", nLevel, "    ", m_nName, nLevel, "    ", static_cast<int>( m_nMemSize ) );
	std::size_t TotalSize = 0;
	for ( int i = 0 ; i < std::ssize( m_vStack ); i++ )
	{
		SosOps::Print( m_vStack[i], (void *)((char *)m_pMemPool + TotalSize), this, i, nLevel );
		TotalSize += SosOps::GetSize( m_vStack[i] );
	}
}

enum sosStackCreationPasses_t
{
	SND_SOS_MEMORY_ALLOCATION_PASS = 0,
	SND_SOS_MEMORY_INITIALIZATION_PASS,
	SND_SOS_TOTAL_MEMORY_PASSES

};

void CSosOperatorStack::ParseKV( KeyValues *pOperatorsKV )
{
	std::size_t TotalSize = 0;
	KeyValues *pOperator = pOperatorsKV->GetFirstSubKey();

	std::set< std::string, ks::iless > testDict;

	// 2 passes:
	//
	// SND_SOS_MEMORY_ALLOCATION_PASS:
	//
	//    Sum up the total memory necessary for this stack by
	//    accumulating imported stack sizes and newly declared
	//    operator sizes. Then allocate the resulting quantity of memory.
	//
	// SND_SOS_MEMORY_INITIALIZATION_PASS:
	//
	//    Copy the existing stack and operator data into the newly
	//    allocated block of stack memory. Properly set all parameters
	//    via defaults, parsing and updating connection offsets.
	//
	for ( int i = 0; i < SND_SOS_TOTAL_MEMORY_PASSES; i++ )
	{
		while ( pOperator )
		{
			const char *pOpName = pOperator->GetName();
			const char *pOpValue = pOperator->GetString();

			// if it has subkeys it's an operator, otw it's a parameter
			KeyValues *pParams = pOperator->GetFirstSubKey();
			if ( pParams )
			{
				// do we already have this one and just need to override params?
				bool bNewStackOp = testDict.contains( pOpName );

				// is an override of an operator existing in the stack
				if ( m_vOperatorMap.contains( pOpName ) )
				{
					// only the second time through
					if ( i == SND_SOS_MEMORY_INITIALIZATION_PASS )
					{
						m_pCurrentOperatorName = pOpName;
						void *pStructMem;
						CSosOperator *pNewOp = FindOperator( pOpName, &pStructMem );
						// only overwriting
						SosOps::ParseKV( pNewOp, this, pStructMem, pOperator );
					}
				}
				else if( !( bNewStackOp && i == SND_SOS_MEMORY_ALLOCATION_PASS ) ) // do nothing if override of new op on first memory accumulating pass
				{
					CSosOperator *pNewOp = nullptr;
					const char *pOpType = nullptr;
					if( pOperator->FindKey( "operator" ) )
					{
						pOpType = pOperator->GetString( "operator", "" );
					}
					if( pOpType && *pOpType )
					{
						auto itOpType = g_pSoundOperatorSystem->m_vOperatorCollection.find( pOpType );
						if ( itOpType != g_pSoundOperatorSystem->m_vOperatorCollection.end() )
						{
							pNewOp = itOpType->second;
						}
						else
						{
							Log_Warning( LOG_SND_OPERATORS, "Error: Operator: %s : Unknown sound operator type %s\n", pOpName, pOpType );

						}
						// is a new operator
						if ( pNewOp )
						{
							if ( i == SND_SOS_MEMORY_ALLOCATION_PASS )
							{	
								// new operator
								testDict.insert( pOperator->GetName() );
							}
							else if ( i == SND_SOS_MEMORY_INITIALIZATION_PASS )
							{

								// new operator
								m_pCurrentOperatorName = pOpName;
								pNewOp->SetBaseDefaults( (void *)(((char *)m_pMemPool) + TotalSize) );
								SosOps::SetDefaults( pNewOp, (void *)(((char *)m_pMemPool) + TotalSize) );
								SosOps::ParseKV( pNewOp, this, (void *)(((char *)m_pMemPool) + TotalSize), pOperator );
								AddToTail( pNewOp, pOperator->GetName() );
							}
							TotalSize += SosOps::GetSize( pNewOp );
						}
					}
				}
			}
			else if ( pOpName && *pOpName )
			{
 				if ( !V_stricmp( pOpName, "import_stack" ) )
				{
					if ( pOpValue && *pOpValue )
					{
						CSosOperatorStack *pImportStack = g_pSoundOperatorSystem->m_MasterStackCollection.GetStack( pOpValue );

						if ( pImportStack )
						{
							if ( i == SND_SOS_MEMORY_INITIALIZATION_PASS )
							{
								Copy( pImportStack, TotalSize );
							}
							TotalSize += pImportStack->GetSize();
						}
					}
				}
			}
			pOperator = pOperator->GetNextKey();
		}
		// first time through
		if ( i == SND_SOS_MEMORY_ALLOCATION_PASS )
		{
			if ( TotalSize > 0 )
			{
				m_pMemPool = MemAlloc_Alloc( TotalSize );
				m_nMemSize = TotalSize;
			}
			TotalSize = 0;
			pOperator = pOperatorsKV->GetFirstSubKey();
		}
	}
}

KeyValues *S_GetStopTracksKV( KeyValues *pOperatorsKV )
{
	if( !pOperatorsKV )
	{
		if( snd_sos_show_operator_parse.GetInt() )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System has invalid operator KV\n" );
		}
		return nullptr;
	}
	KeyValues *pOperatorDataKV = pOperatorsKV->FindKey( "soundentry_operator_data" );
	if( ! pOperatorDataKV )
	{
		//		Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System cannot find \"soundentry_operator_data\"\n" );
		return nullptr;
	}

	KeyValues *pTrackDataKV = pOperatorDataKV->FindKey("track_data" );
	if( ! pTrackDataKV )
	{
		return nullptr;
	}
	KeyValues *pStopTracksKV = pTrackDataKV->FindKey("stop_tracks_on_start" );
	if( ! pStopTracksKV )
	{
		return nullptr;
	}


	KeyValues *pStopTracksList = pStopTracksKV->GetFirstSubKey();
	return pStopTracksList;
// 
// 	while ( pStopTracksList )
// 	{
// 		const char *pStopTracksTypeString = pStopTracksList->GetName();
// 
// 		if ( pStopTracksTypeString && *pStopTracksTypeString )
// 		{
// 			if ( !V_strcmp( pStopTracksTypeString, pListName ) )
// 			{
// 				return pStopTracksList;
// 			}
// 
// 		}
// 		pStopTracksList = pStopTracksList->GetNextKey();
// 	}
// 
// 
// 	return NULL;
}

KeyValues *S_GetStopTracksKV( HSOUNDSCRIPTHASH nSoundEntryHash )
{
	if( nSoundEntryHash != SOUNDEMITTER_INVALID_HASH )
	{
		KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( nSoundEntryHash );
		if( pOperatorKV )
		{
			return S_GetStopTracksKV( pOperatorKV );
		}
	}
	return nullptr;
}


KeyValues *CSosOperatorStack::GetSyncPointsKV( KeyValues *pOperatorsKV, const char *pListName )
{
	if( !pOperatorsKV )
	{
		if( snd_sos_show_operator_parse.GetInt() )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System has invalid operator KV\n" );
		}
		return nullptr;
	}
	KeyValues *pOperatorDataKV = pOperatorsKV->FindKey( "soundentry_operator_data" );
	if( ! pOperatorDataKV )
	{
//		Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System cannot find \"soundentry_operator_data\"\n" );
		return nullptr;
	}

	KeyValues *pTrackDataKV = pOperatorDataKV->FindKey("track_data" );
	if( ! pTrackDataKV )
	{
		return nullptr;
	}
	KeyValues *pSyncPointsKV = pTrackDataKV->FindKey("syncpoints" );
	if( ! pSyncPointsKV )
	{
		return nullptr;
	}


	KeyValues *pSyncPointList = pSyncPointsKV->GetFirstSubKey();
	while ( pSyncPointList )
	{
		const char *pStackListTypeString = pSyncPointList->GetName();

		if ( pStackListTypeString && *pStackListTypeString )
		{
			if ( !V_strcmp( pStackListTypeString, pListName ) )
			{
				return pSyncPointList;
			}

		}
		pSyncPointList = pSyncPointList->GetNextKey();
	}


	return nullptr;
}

KeyValues *CSosOperatorStack::GetSyncPointsKV( const char *pListName )
{
	KeyValues *pOperatorsKV = GetOperatorsKV();
	return GetSyncPointsKV( pOperatorsKV, pListName );
}

bool S_GetTrackData( KeyValues *pOperatorsKV, track_data_t &trackData )
{
	trackData.SetDefaults();

	if( !pOperatorsKV )
	{
		if( snd_sos_show_operator_parse.GetInt() )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System has invalid operator KV\n" );
		}
		return false;
	}
	KeyValues *pOperatorDataKV = pOperatorsKV->FindKey( "soundentry_operator_data" );
	if( ! pOperatorDataKV )
	{
		//		Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System cannot find \"soundentry_operator_data\"\n" );
		return false;
	}

	KeyValues *pTrackDataKV = pOperatorDataKV->FindKey("track_data" );
	if( ! pTrackDataKV )
	{
		return false;
	}
	const char *pTrackNumber = pTrackDataKV->GetString( "track_name", "-1" );
	trackData.m_pTrackName = pTrackNumber;
	if( pTrackNumber )
	{
		trackData.m_nTrackNumber = V_atoi( pTrackNumber );
	}
	const char *pTrackPriority = pTrackDataKV->GetString( "priority", "-1" );
	if( pTrackPriority )
	{
		trackData.m_nTrackPriority = V_atoi( pTrackPriority );
	}
	const char *pOverridePriority = pTrackDataKV->GetString( "priority_override", "-1" );
	if( pOverridePriority )
	{
		if ( !V_strcasecmp( pOverridePriority, "true" ) )
		{
			trackData.m_bPriorityOverride = true;
		}
	}
	const char *pBlockEqualPriority = pTrackDataKV->GetString( "block_equal_priority", "-1" );
	if( pBlockEqualPriority )
	{
		if ( !V_strcasecmp( pBlockEqualPriority, "true" ) )
		{
			trackData.m_bBlockEqualPriority = true;
		}
	}
	const char *pSyncTrackNumber = pTrackDataKV->GetString( "sync_track_name", "-1" );
	trackData.m_pSyncTrackName = pSyncTrackNumber;
	if( pSyncTrackNumber )
	{
		trackData.m_nSyncTrackNumber = V_atoi( pSyncTrackNumber );
	}
	const char *pStartPoint = pTrackDataKV->GetString( "start_point", "0.0" );
	if( pStartPoint )
	{
		trackData.m_flStartPoint = V_atof( pStartPoint );
	}
	const char *pEndPoint = pTrackDataKV->GetString( "end_point", "0.0" );
	if( pEndPoint )
	{
		trackData.m_flEndPoint = V_atof( pEndPoint );
	}
	return true;
}

void CSosOperatorStack::GetTrackData( track_data_t &trackData ) const
{
	KeyValues *pOperatorsKV = GetOperatorsKV();
	S_GetTrackData( pOperatorsKV, trackData );
}

bool S_GetTrackData( HSOUNDSCRIPTHASH nSoundEntryHash, track_data_t &trackData )
{
	if( nSoundEntryHash != SOUNDEMITTER_INVALID_HASH )
	{
		KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( nSoundEntryHash );
		if( pOperatorKV )
		{
			return S_GetTrackData( pOperatorKV, trackData );
		}
	}
	return false;
}


bool S_TrackHasPriority( track_data_t &newTrackData, track_data_t &existingTrackData )
{
	if( !( newTrackData.m_bPriorityOverride && existingTrackData.m_bPriorityOverride ) )
	{
	if( newTrackData.m_bPriorityOverride )
	{
		return true;
	}
	if( existingTrackData.m_bPriorityOverride )
	{
 		return false;
 	}
	}

	if( existingTrackData.m_bBlockEqualPriority && ( newTrackData.m_nTrackPriority == existingTrackData.m_nTrackPriority ) )
	{
		return false;
	}

	if( newTrackData.m_nTrackPriority >= existingTrackData.m_nTrackPriority )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// CSosOperatorStackList
//-----------------------------------------------------------------------------

CSosOperatorStackList::CSosOperatorStackList()
{
	m_vUpdateStack = nullptr;
// 	m_vStartStack = NULL;
	m_vStopStack  = nullptr;
// 	m_vCueStack = NULL;
	m_stopType = SOS_STOP_NONE;
	m_flStopTime = -1.0;
}
CSosOperatorStackList::~CSosOperatorStackList()
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->Shutdown();
// 		delete m_vCueStack;
// 	}
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->Shutdown();
// 		delete m_vStartStack;
// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->Shutdown();
		delete m_vUpdateStack;
	}
	if ( m_vStopStack )
	{
		m_vStopStack->Shutdown();
		delete m_vStopStack;
	}
}



void CSosOperatorStackList::Execute( CSosOperatorStack::SosStackType_t SosType, channel_t *pChannel, CScratchPad *pScratchPad )
{
	switch( SosType )
	{
	case CSosOperatorStack::SOS_UPDATE:
		if ( m_vUpdateStack )
		{
			m_vUpdateStack->SetStopType( m_stopType );
			m_vUpdateStack->Execute( pChannel, pScratchPad );
			SetStopType( m_vUpdateStack->GetStopType( ) );
		}
		break;
// 	case CSosOperatorStack::SOS_START:
// 		if ( m_vStartStack )
// 		{
// 			m_vStartStack->Execute( pChannel, pScratchPad );
// 		}
// 		break;
	case CSosOperatorStack::SOS_STOP:
		if ( m_vStopStack )
		{
			m_vStopStack->Execute( pChannel, pScratchPad );
		}
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		if ( m_vCueStack )
// 		{
// 			m_vCueStack->Execute( pChannel, pScratchPad );
// 		}
// 		break;

	default:
		break;
	}
}

bool CSosOperatorStackList::HasStack( CSosOperatorStack::SosStackType_t SosType )
{
	switch( SosType )
	{
	case CSosOperatorStack::SOS_UPDATE:
		if ( m_vUpdateStack )
		{
			return true;
		}
		break;
// 	case CSosOperatorStack::SOS_START:
// 		if ( m_vStartStack )
// 		{
// 			return true;
// 		}
// 		break;

	case CSosOperatorStack::SOS_STOP:
		if ( m_vStopStack )
		{
			return true;
		}
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		if ( m_vCueStack )
// 		{
// 			return true;
// 		}
// 		break;
	default:
		return false;
	}
	return false;
}


void CSosOperatorStackList::Print()
{
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nStack List:\n");

// 	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nCUE Operators:\n");
// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->Print( 1 );
// 	}
// 	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nSTART Operators:\n");
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->Print( 1 );
// 	}
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nUPDATE Operators:\n");
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->Print( 1 );
	}

	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nSTOP Operators:\n");
	if ( m_vStopStack )
	{
		m_vStopStack->Print( 1 );
	}
}
CSosOperatorStack *CSosOperatorStackList::GetStack( CSosOperatorStack::SosStackType_t SosType )
{
	switch( SosType )
	{
	case CSosOperatorStack::SOS_UPDATE:
		return m_vUpdateStack;
		break;
// 	case CSosOperatorStack::SOS_START:
// 		return m_vStartStack;
// 		break;
	case CSosOperatorStack::SOS_STOP:
		return m_vStopStack;
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		return m_vCueStack;
// 		break;
	default:
		return nullptr;
	}
	return nullptr;
}
void CSosOperatorStackList::SetScriptHash( HSOUNDSCRIPTHASH nHash )
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetScriptHash( nHash );	
// 	}
	// 	if ( m_vStartStack )
	// 	{
	// 		m_vStartStack->SetScriptHash( nHash );	
	// 	}

	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetScriptHash( nHash );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetScriptHash( nHash );
	}
}

void CSosOperatorStackList::SetChannelGuid( int nGuid )
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetChannelGuid( nGuid );
// 	}
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->SetChannelGuid( nGuid );
// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetChannelGuid( nGuid );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetChannelGuid( nGuid );
	}
}
void CSosOperatorStackList::SetStartTime( float flStartTime )
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetStartTime( flStartTime );
// 	}
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->SetStartTime( flStartTime );
// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetStartTime( flStartTime );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetStartTime( flStartTime );
	}
}
void CSosOperatorStackList::SetStopTime( float flStopTime )
{
	m_flStopTime = flStopTime;

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetStopTime( flStopTime );
// 	}
	// 	if ( m_vStopStack )
	// 	{
	// 		m_vStopStack->SetStopTime( flStopTime );
	// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetStopTime( flStopTime );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetStopTime( flStopTime );
	}
}
void CSosOperatorStackList::SetStopType( SOSStopType_t stopType )
{
	m_stopType = stopType;

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetStopType( stopType );
// 	}
	// 	if ( m_vStopStack )
	// 	{
	// 		m_vStopStack->SetStopTime( stopTime );
	// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetStopType( stopType );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetStopType( stopType );
	}
}
void CSosOperatorStackList::SetStack( CSosOperatorStack *pStack )
{
	switch ( pStack->GetType() )
	{
	case CSosOperatorStack::SOS_UPDATE:
		m_vUpdateStack = pStack;
		break;
// 	case CSosOperatorStack::SOS_START:
// 		m_vStartStack = pStack;
// 		break;
	case CSosOperatorStack::SOS_STOP:
		m_vStopStack = pStack;
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		m_vCueStack = pStack;
// 		break;
	default:
		break;

	}
}


void CSosOperatorStackList::StopStacks( SOSStopType_t stopType )
{
	if( stopType == SOS_STOP_FORCE )
	{
		SetStopType( SOS_STOP_FORCE );
	}
	else if( ( m_stopType == SOS_STOP_NONE || m_stopType == SOS_STOP_QUEUE ) && ( stopType != SOS_STOP_NONE &&  stopType != SOS_STOP_QUEUE ) )
	{
		SetStopType( stopType );
		SetStopTime( g_pSoundServices->GetHostTime() );
		
		Execute( CSosOperatorStack::SOS_STOP, nullptr, &g_scratchpad );
	}
}

void CSosOperatorStackList::ParseKV( stack_data_t &stackData )
{
	KeyValues *pOperatorsKV = stackData.m_pOperatorsKV;

	KeyValues *pStackListType = pOperatorsKV->GetFirstSubKey();
	while ( pStackListType )
	{
		CSosOperatorStack::SosStackType_t SosType = CSosOperatorStack::SOS_NONE;

		const char *pDefaultStack = nullptr;
		const char *pStackListTypeString = pStackListType->GetName();
		if ( pStackListTypeString && *pStackListTypeString )
		{
			if ( !V_strcmp( pStackListTypeString, "update_stack" ) )
			{
				SosType = CSosOperatorStack::SOS_UPDATE;
				pDefaultStack = "update_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "start_stack" ) )
			{
				pStackListType = pStackListType->GetNextKey();
				continue;
// 				SosType = CSosOperatorStack::SOS_START;
// 				pDefaultStack = "start_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "stop_stack" ) )
			{
				SosType = CSosOperatorStack::SOS_STOP;
				pDefaultStack = "stop_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "prestart_stack" ) )
			{
				pStackListType = pStackListType->GetNextKey();
				continue;
// 				SosType = CSosOperatorStack::SOS_CUE;
// 				pDefaultStack = "cue_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "soundentry_operator_data" ) )
			{

				pStackListType = pStackListType->GetNextKey();
				continue;
			}
			else
			{
				Log_Warning( LOG_SND_OPERATORS, "Error: Unknown sound operator stack type: %s\n", pStackListTypeString );
				// gets us out to the stack type level
				pStackListType = pStackListType->GetNextKey();
				continue;
			}

			if ( pDefaultStack && SosType != CSosOperatorStack::SOS_NONE )
			{
				// todo: morasky, default stacks not implemented?
 				CSosOperatorStack *pNewStack = new CSosOperatorStack( SosType, stackData );
				pNewStack->SetName( pStackListTypeString );
 				SetStack( pNewStack );
 				pNewStack->ParseKV( pStackListType );
			}
		}
		pStackListType = pStackListType->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// S_ParseOperatorsKV
//  
//-----------------------------------------------------------------------------
CSosOperatorStackList *S_ParseOperatorsKV( stack_data_t &stackData )
{
	CSosOperatorStackList *pNewStackList = new CSosOperatorStackList();
	pNewStackList->ParseKV( stackData );
	return pNewStackList;

}

CSosOperatorStack *S_GetStack( CSosOperatorStack::SosStackType_t stackType, stack_data_t &stackData )
{
	KeyValues *pOperatorsKV = stackData.m_pOperatorsKV;

	KeyValues *pStackListType = pOperatorsKV->GetFirstSubKey();

	// morasky: should "GetString" instead of brute force search?
	while ( pStackListType )
	{
		const char *pStackListTypeString = pStackListType->GetName();
		if ( pStackListTypeString && *pStackListTypeString )
		{
			if ( ( V_strcmp( pStackListTypeString, "start_stack" ) && stackType == CSosOperatorStack::SOS_START )  ||
				( V_strcmp( pStackListTypeString, "prestart_stack" ) && stackType == CSosOperatorStack::SOS_CUE ) )
			{
				pStackListType = pStackListType->GetNextKey();
				continue;
			}
			else
			{
				CSosOperatorStack *pNewStack = new CSosOperatorStack( stackType, stackData );
				if( pNewStack )
				{
					pNewStack->SetName( pStackListTypeString );
					pNewStack->ParseKV( pStackListType );

					if( snd_sos_show_operator_init.GetInt() )
					{
						if( stackType == CSosOperatorStack::SOS_START )
						{
							Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nSTART Operators:\n");
						}
						else if( stackType == CSosOperatorStack::SOS_CUE )
						{
							Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nCUE Operators:\n");
						}
						
						pNewStack->Print( 1 );
					}
				}
				return pNewStack;
			}
		}
		pStackListType = pStackListType->GetNextKey();
	}

	return nullptr;
}

CSosOperatorStackList *S_InitChannelOperators( stack_data_t &stackData )
{

	CSosOperatorStackList *pStackList = nullptr;

	if ( stackData.m_pOperatorsKV != nullptr )
	{
		pStackList = S_ParseOperatorsKV( stackData );
	}

	if ( ! pStackList )
	{
// 		Log_Warning( LOG_SND_OPERATORS, "Error: Unable to create operator stack list on channel\n");
// 		return;
	}
	else
	{
		if ( snd_sos_show_operator_init.GetInt() )
		{
			pStackList->Print();
		}
	}
	return pStackList;
}


//-----------------------------------------------------------------------------
// CSosOperatorStackCollection
//-----------------------------------------------------------------------------
CSosOperatorStackCollection::~CSosOperatorStackCollection()
{
	Clear();
}

void CSosOperatorStackCollection::Clear()
{
	m_vAllStacks.clear();
	m_vUpdateStacks.clear();
	m_vStartStacks.clear();
	m_vStopStacks.clear();
	m_vCueStacks.clear();
}
CSosOperatorStack *CSosOperatorStackCollection::GetStack( const char *pStackName )
{
	CSosOperatorStack **ppStack = ks::find_ptr( m_vAllStacks, pStackName );
	return ppStack ? *ppStack : nullptr;
}
CSosOperatorStack *CSosOperatorStackCollection::GetStack( CSosOperatorStack::SosStackType_t SosType,  const char *pStackName )
{
	CSosOperatorStack *pOpStack = GetStack( pStackName );
	if ( pOpStack )
	{
		if ( pOpStack->IsType( SosType ) )
		{
			return pOpStack;
		}
	}
	return nullptr;
}
void CSosOperatorStackCollection::ParseKV( CSosOperatorStack::SosStackType_t SosType, KeyValues *pStackType )
{
	stack_data_t stackData;
	KeyValues *pStack = pStackType->GetFirstSubKey();

	while ( pStack )
	{
		const char *pStackName = pStack->GetName();
		auto pNewStack = std::make_unique< CSosOperatorStack >( SosType, stackData );
		pNewStack->SetName( pStackName );

		pNewStack->ParseKV( pStack );

		ks::idict< std::unique_ptr< CSosOperatorStack > > *pStacks = nullptr;
		switch ( SosType )
		{
		case CSosOperatorStack::SOS_UPDATE:
			pStacks = &m_vUpdateStacks;
			break;
		case CSosOperatorStack::SOS_START:
			pStacks = &m_vStartStacks;
			break;
		case CSosOperatorStack::SOS_STOP:
			pStacks = &m_vStopStacks;
			break;
		case CSosOperatorStack::SOS_CUE:
			pStacks = &m_vCueStacks;
			break;
		default:
			// BETTER ERROR HERE
			Log_Warning( LOG_SND_OPERATORS, "Error: Unknown sound operator stack type" );
			break;
		}
		if ( pStacks )
		{
			// a repeated stack name keeps the first definition
			auto [it, bInserted] = pStacks->try_emplace( pStackName, std::move( pNewStack ) );
			if ( bInserted )
			{
				m_vAllStacks.try_emplace( pStackName, it->second.get() );
			}
		}
		pStack = pStack->GetNextKey();
	}
}
void CSosOperatorStackCollection::Print()
{
	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n\nSound Operators Collection:\n");

	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n%*sCue Stacks:\n", 1, "    ");
	for ( auto &[name, pStack] : m_vCueStacks )
	{
		pStack->Print( 3 );
	}
	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n%*sStart Stacks:\n", 1, "    " );
	for ( auto &[name, pStack] : m_vStartStacks )
	{
		pStack->Print( 3 );
	}
	Log_Msg ( LOG_SND_OPERATORS, CollectionColor, "\n%*sUpdate Stacks:\n", 1, "    " );
	for ( auto &[name, pStack] : m_vUpdateStacks )
	{
		pStack->Print( 3 );
	}
	Log_Msg ( LOG_SND_OPERATORS, CollectionColor, "\n%*sStop Stacks:\n", 1, "    " );
	for ( auto &[name, pStack] : m_vStopStacks )
	{
		pStack->Print( 3 );
	}
	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n");
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSosOperatorSystem::CSosOperatorSystem()
{
	m_bHasInitialized = false;
}

CSosOperatorSystem::~CSosOperatorSystem()
{
}

inline constexpr const char *SOUND_OPERATORS_FILE = "scripts/sound_operator_stacks.txt";
void CSosOperatorSystem::Flush()
{
	m_MasterStackCollection.Clear();
	m_bHasInitialized = false;
	Init();
}

void CSosOperatorSystem::Init()
{
	// checking for "restarting sound system"

	if( m_bHasInitialized )
	{
		return;
	}
	m_bHasInitialized = true;

	CRC32_t crc;
	CRC32_Init( &crc );

	KeyValues *pSoundOperatorStacksKV = new KeyValues( SOUND_OPERATORS_FILE );
	if ( g_pFullFileSystem->LoadKeyValues( *pSoundOperatorStacksKV, IFileSystem::TYPE_SOUNDOPERATORS, SOUND_OPERATORS_FILE, "GAME" ) )
	{
		// TODO: Morasky
	//	AccumulateFileNameAndTimestampIntoChecksum( &crc, SOUND_OPERATORS_FILE );

		KeyValues *pStackType = pSoundOperatorStacksKV;
		while ( pStackType )
		{
			CSosOperatorStack::SosStackType_t curStackType = CSosOperatorStack::SOS_NONE;
			const char *pStackTypeString = pStackType->GetName(  );
			if ( pStackTypeString && *pStackTypeString )
			{
				if ( !V_strcmp( pStackTypeString, "update_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_UPDATE;
				}
				else if ( !V_strcmp( pStackTypeString, "start_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_START;
				}
				else if ( !V_strcmp( pStackTypeString, "stop_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_STOP;
				}
				else if ( !V_strcmp( pStackTypeString, "prestart_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_CUE;
				}
				else
				{
					// BETTER ERROR HERE
					Log_Warning( LOG_SND_OPERATORS, "Unknown sound operator stack arg %s", pStackTypeString  );
					continue;
				}
			}
			m_MasterStackCollection.ParseKV( curStackType, pStackType );
			pStackType = pStackType->GetNextKey();
		}
	}
	else
	{
		Log_Warning( LOG_SND_OPERATORS, "Warning: Unable to load sound operators file '%s'\n", SOUND_OPERATORS_FILE );
	}
	pSoundOperatorStacksKV->deleteThis();

	CRC32_Final( &crc );

	if( snd_sos_show_operator_init.GetInt() )
	{
		m_MasterStackCollection.Print();
	}

	//	m_uManifestPlusScriptChecksum = ( unsigned int )crc;
}
void CSosOperatorSystem::PrintOperatorList()
{
	for ( auto &[name, pOperator] : m_vOperatorCollection )
	{
		DevMsg("%s\n", name.c_str() );
	}
}
void S_PrintOperatorList( void )
{
	g_pSoundOperatorSystem->PrintOperatorList();
}

ConCommand snd_sos_print_operators( "snd_sos_print_operators", S_PrintOperatorList, "Prints a list of currently available operators", FCVAR_CHEAT );

void CSosOperatorSystem::ClearSubSystems()
{
	m_vTrackDict.clear();
	m_sosStopChannelQueue.clear();
	m_sosStartEntryQueue.clear();
}

void CSosOperatorSystem::QueueStartEntry( StartSoundParams_t &startParams, float flDelay /*= 0.0*/, bool bFromPrestart /* = false */ )
{
	auto pStartQueue = std::make_unique< SosStartQueueData_t >();
	pStartQueue->m_nStartTime = g_pSoundServices->GetHostTime() + flDelay;
	pStartQueue->m_bFromPrestart = bFromPrestart;

	startParams.Copy( pStartQueue->m_startSoundParams );

	m_sosStartEntryQueue.push_back( std::move( pStartQueue ) );
}

ConVar snd_sos_show_startqueue("snd_sos_show_startqueue", "0", FCVAR_CHEAT );
void CSosOperatorSystem::StartQueuedEntries()
{
	for( int i = std::ssize( m_sosStartEntryQueue ) -1; i > -1; i-- )
	{
		SosStartQueueData_t *pStartQueue = m_sosStartEntryQueue[i].get();
		if( snd_sos_show_startqueue.GetInt() )
		{
			Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: %s time to start of %f\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ), pStartQueue->m_nStartTime - g_pSoundServices->GetHostTime() );
		}

		if( pStartQueue->m_nStartTime <= g_pSoundServices->GetHostTime() )
		{
			if( snd_sos_show_startqueue.GetInt() )
			{
				Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: %s satisfies time\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ) );
			}
			// check if new entry has priority over old
			track_data_t newTrackData;
			track_data_t existingTrackData;

			bool bHasData = S_GetTrackData( pStartQueue->m_startSoundParams.m_nSoundScriptHash, newTrackData );
			bool bHasData2 = false;
			bool bHasPriority = false;

			if( bHasData )
			{
				bHasData2 = GetTrackDataOnTrack( newTrackData.m_pTrackName, existingTrackData );
				if( bHasData2 )
				{
					bHasPriority = S_TrackHasPriority( newTrackData, existingTrackData );
				}
			}

			if( !bHasPriority && bHasData && bHasData2 )
			{
				if( snd_sos_show_startqueue.GetInt() )
				{
					Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: Not starting %s due to existing priority.\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ) );
				}
			}
			else
			{
				if( snd_sos_show_startqueue.GetInt() )
				{
					Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: %s StartSoundEntry\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ) );
				}
				S_StartSoundEntry( pStartQueue->m_startSoundParams , -1, pStartQueue->m_bFromPrestart );

// 				if( bHasData )
// 				{
// 					KeyValues *pStopTracksList = S_GetStopTracksKV( pStartQueue->m_startSoundParams.m_nSoundScriptHash );
// 
// 					if( pStopTracksList )
// 					{
// 						for ( KeyValues *pValue = pStopTracksList->GetFirstValue(); pValue; pValue = pValue->GetNextValue() )
// 						{
// 							DevMsg("***STOP TRACK: %s", pValue->GetString() );
// 						}
// 					}
// 				}
			}

			m_sosStartEntryQueue.erase( m_sosStartEntryQueue.begin() + i );
		}
	}
}

void CSosOperatorSystem::QueueStopChannel( int nChannelGuid, float flDelay /* = 0.0*/ )
{
	auto pStopQueue = std::make_unique< SosStopQueueData_t >();
	pStopQueue->m_nChannelGuid = nChannelGuid;
	pStopQueue->m_nStopTime = g_pSoundServices->GetHostTime() + flDelay;

	m_sosStopChannelQueue.push_back( std::move( pStopQueue ) );
}
bool CSosOperatorSystem::IsInStopQueue( int nChannelGuid )
{
	for( int i = 0; i < std::ssize( m_sosStopChannelQueue ); i++ )
	{
		SosStopQueueData_t *pStopQueue = m_sosStopChannelQueue[i].get();
		if( nChannelGuid == pStopQueue->m_nChannelGuid )
		{
			return true;
		}
	}
	return false;
}

void CSosOperatorSystem::StopQueuedChannels()
{
	for( int i = std::ssize( m_sosStopChannelQueue ) -1; i > -1; i-- )
	{
		SosStopQueueData_t *pStopQueue = m_sosStopChannelQueue[i].get();
		if( pStopQueue->m_nStopTime <= g_pSoundServices->GetHostTime() )
		{
			S_StopSoundByGuid( pStopQueue->m_nChannelGuid );
			m_sosStopChannelQueue.erase( m_sosStopChannelQueue.begin() + i );
		}
	}
}

void CSosOperatorSystem::SetChannelOnTrack( const char *pTrackName, channel_t *pChannel )
{
	m_vTrackDict.insert_or_assign( ks::nz( pTrackName ), pChannel );

}

channel_t *CSosOperatorSystem::GetChannelOnTrack( const char *pTrackName )
{
	channel_t **ppChannel = ks::find_ptr( m_vTrackDict, ks::nz( pTrackName ) );
	return ppChannel ? *ppChannel : nullptr;
}
HSOUNDSCRIPTHASH CSosOperatorSystem::GetSoundEntryOnTrack( const char *pTrackName )
{
	channel_t *pChannel = GetChannelOnTrack( pTrackName );
	if( pChannel && pChannel->sfx )
	{
		return pChannel->m_nSoundScriptHash;
	}
	else
	{
		return SOUNDEMITTER_INVALID_HASH;
	}
}
bool CSosOperatorSystem::GetTrackDataOnTrack( const char *pTrackName, track_data_t &trackData )
{
	HSOUNDSCRIPTHASH nSoundScriptHash = GetSoundEntryOnTrack( pTrackName );
	if( nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
	{
		S_GetTrackData( nSoundScriptHash, trackData );
		return true;
	}
	return false;
}

void CSosOperatorSystem::StopChannelOnTrack( const char *pTrackName, bool bStopAll /* false */, float flStopDelay /* = 0.0 */ )
{
	if( channel_t **ppChannel = ks::find_ptr( m_vTrackDict, ks::nz( pTrackName ) ) )
	{
		channel_t *pPreviousChannelOnTrack = *ppChannel;

		if( pPreviousChannelOnTrack && pPreviousChannelOnTrack->sfx )
		{
			//DevMsg("***STOPPING: Track: %s, Entry: %s Delay: %f\n", pTrackName, g_pSoundEmitterSystem->GetSoundNameForHash( pPreviousChannelOnTrack->m_nSoundScriptHash ), flStopDelay );
			if( pPreviousChannelOnTrack->m_pStackList )
			{
				CSosOperatorStack *pStack = pPreviousChannelOnTrack->m_pStackList->GetStack( CSosOperatorStack::SOS_UPDATE);
				if( pStack )
				{
					pStack->SetStopType( SOS_STOP_QUEUE );
				}
			}
			g_pSoundOperatorSystem->QueueStopChannel( pPreviousChannelOnTrack->guid, flStopDelay );
		}
	}

	// remove a queued entry that is on the same track
	if( bStopAll )
	{
		for( int i = std::ssize( m_sosStartEntryQueue ) -1; i > -1; i-- )
		{
			SosStartQueueData_t *pStartQueue = m_sosStartEntryQueue[i].get();
			track_data_t trackData;
			S_GetTrackData( pStartQueue->m_startSoundParams.m_pOperatorsKV, trackData );
			if( trackData.m_pTrackName && !V_stricmp( trackData.m_pTrackName, ks::nz( pTrackName ) ) )
			{
				m_sosStartEntryQueue.erase( m_sosStartEntryQueue.begin() + i );
			}
		}
	}
}

void CSosOperatorSystem::RemoveChannelFromTracks( int nGuid )
{
	// no negative guids
	if( nGuid < 0 )
	{
		return;
	}

	for ( auto &[name, pChannel] : m_vTrackDict )
	{
		if( pChannel && pChannel->guid == nGuid )
		{
			pChannel = nullptr;
		}
	}
}
void CSosOperatorSystem::RemoveChannelFromTrack( const char *pTrackName, int nGuid )
{
	// no negative guids
	if( nGuid < 0 )
	{
		return;
	}

	channel_t **ppChannel = ks::find_ptr( m_vTrackDict, ks::nz( pTrackName ) );
	if( ppChannel && *ppChannel && (*ppChannel)->guid == nGuid )
	{
		*ppChannel = nullptr;
	}
}

ConVar snd_sos_show_track_list("snd_sos_show_track_list", "0", FCVAR_NONE );
void CSosOperatorSystem::DEBUG_ShowTrackList( void )
{
	if( !snd_sos_show_track_list.GetInt() )
	{
		return;
	}

	int i = 0;
	for ( const auto &[name, pChannel] : m_vTrackDict )
	{
		char chanStr[128];
		std::sprintf( chanStr, "Channel %i : %s = %s", i, name.c_str(),  pChannel ? g_pSoundEmitterSystem->GetSoundNameForHash( pChannel->m_nSoundScriptHash ) : "empty" );
		CDebugOverlay::AddScreenTextOverlay( 0.01, 0.2 + (0.012 * (float)  i) , 0.5, 200, 200, 200, 255, chanStr );
		++i;
	}
}

//Returns a valid id into the string table
bool CSosOperatorSystem::SetOpVarFloat( const char *pString, float flVariable )
{
	m_sosOpVarFloatMap.insert_or_assign( pString, flVariable );
	return true;

// 	int stringId = m_sosFloatOpvarMap.Find( pString );
// 	if ( stringId != m_sosFloatOpvarMap.InvalidIndex() )
// 	{
// 		// found in pool
// 		m_sosFloatOpvarMap[stringId] = flVariable;
// 		return true;
// 	}
// 	stringId = m_sosFloatOpvarMap.AddString( pString );
// 	if ( stringId != m_sosFloatOpvarMap.InvalidIndex() )
// 	{
// 		// found in pool
// 		m_sosFloatOpvarMap[stringId] = flVariable;
// 		return true;
// 	}
// 
// 	DevWarning( "SOS System Warning: Unable to set or create float variable %s\n", pString );
// 	return false;

}

// Returns a valid id into the string table
bool CSosOperatorSystem::GetOpVarFloat( const char *pString, float &flVariable )
{

	float *pValue = ks::find_ptr( m_sosOpVarFloatMap, pString );
	if ( !pValue )
	{
		// error
		return false;
	}
	float flTest = *pValue;
	flVariable = flTest;
	return true;


// 	int stringId = m_sosFloatOpvarMap.Find( pString );
// 	if ( stringId != m_sosFloatOpvarMap.InvalidIndex() )
// 	{
// 		// found in pool
// 		flVariable =  m_sosFloatOpvarMap[stringId];
// 		return true;
// 	}
// 	return false;

}

ConVar snd_sos_show_opvar_list("snd_sos_show_opvar_list", "0", FCVAR_NONE );
void CSosOperatorSystem::DEBUG_ShowOpvarList( void )
{
	if( !snd_sos_show_opvar_list.GetInt() )
	{
		return;
	}


	int nCount = (int)m_sosOpVarFloatMap.size();

	char opVarStr[128];
	std::sprintf( opVarStr, "Opvars %i", nCount );

	CDebugOverlay::AddScreenTextOverlay( 0.01, 0.5, 0.5, 200, 200, 200, 255, opVarStr );

	
	int i = 0;
	for ( const auto &[name, flValue] : m_sosOpVarFloatMap )
	{
		char chanStr[128];
		sprintf( chanStr, "- %s = %f", name.c_str(), flValue );
		CDebugOverlay::AddScreenTextOverlay( 0.01, 0.512 + (0.012 * (float)  i) , 0.5, 200, 200, 200, 255, chanStr );
		i++;
	}

}


void CSosOperatorSystem::Update()
{
	m_sosEntryBlockList.Update();
}


void CSosOperatorSystem::Shutdown()
{

}

CSosOperatorSystem *CSosOperatorSystem::GetSoundOperatorSystem()
{
	static CSosOperatorSystem s_SoundOperatorSystem;
	return &s_SoundOperatorSystem;
}

void S_SOSFlush()
{
	g_pSoundOperatorSystem->Flush();
}


ConCommand snd_sos_flush_operators( "snd_sos_flush_operators", S_SOSFlush, "Flush and re-parse the sound operator system", FCVAR_CHEAT );

bool S_SOSSetOpvarFloat( const char *pOpVarName, float flValue )
{
	if( g_pSoundOperatorSystem )
	{
		return g_pSoundOperatorSystem->SetOpVarFloat( pOpVarName,flValue );
	}
	else
	{
		DevMsg("SOS Error: g_pSoundOperatorSystem used via SOSSetOpvarFloat before it's initialized.");
		return false;
	}
}
bool S_SOSGetOpvarFloat( const char *pOpVarName, float &flValue )
{
	if( g_pSoundOperatorSystem )
	{
		return g_pSoundOperatorSystem->GetOpVarFloat( pOpVarName, flValue );
	}
	else
	{
		DevMsg("SOS Error: g_pSoundOperatorSystem used via SOSGetOpvarFloat before it's initialized.");
		return false;
	}
}
