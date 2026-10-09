// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_framework.h"


PlayerManager::PlayerManager()
{
}

PlayerManager::~PlayerManager()
{
}

static PlayerManager g_PlayerManager;
PlayerManager *g_pPlayerManager = &g_PlayerManager;

IPlayerLocal * PlayerManager::GetLocalPlayer(int playerIndex)
{
	return nullptr;
}

IPlayerFriend * PlayerManager::GetFriendByXUID( XUID xuid )
{
	return nullptr;
}

IPlayer * PlayerManager::FindPlayer( XUID xuid )
{
	return nullptr;
}

enum SyncKeyValueDirection_t
{
	KVSTAT_WRITE_STAT,
	KVSTAT_READ_STAT
};
static void SyncKeyValueWithStatField( KeyValues *kvValue, IPlayerLocal *pPlayerLocal, TitleDataFieldsDescription_t const *pField, SyncKeyValueDirection_t eOp )
{
	switch( pField->m_eDataType )
	{
	case TitleDataFieldsDescription_t::DT_BITFIELD:
		if ( eOp == KVSTAT_WRITE_STAT )
			TitleDataFieldsDescriptionSetBit( pField, pPlayerLocal, !!kvValue->GetInt( "" ) );
		else
			kvValue->SetInt( "", TitleDataFieldsDescriptionGetBit( pField, pPlayerLocal ) ? 1 : 0 );
		break;
	case TitleDataFieldsDescription_t::DT_uint8:
		if ( eOp == KVSTAT_WRITE_STAT )
			TitleDataFieldsDescriptionSetValue<uint8>( pField, pPlayerLocal, (uint8)kvValue->GetInt( "" ) );
		else
			kvValue->SetInt( "", TitleDataFieldsDescriptionGetValue<uint8>( pField, pPlayerLocal ) );
		break;
	case TitleDataFieldsDescription_t::DT_uint16:
		if ( eOp == KVSTAT_WRITE_STAT )
			TitleDataFieldsDescriptionSetValue<uint16>( pField, pPlayerLocal, (uint16)kvValue->GetInt( "" ) );
		else
			kvValue->SetInt( "", TitleDataFieldsDescriptionGetValue<uint16>( pField, pPlayerLocal ) );
		break;
	case TitleDataFieldsDescription_t::DT_uint32:
		if ( eOp == KVSTAT_WRITE_STAT )
			TitleDataFieldsDescriptionSetValue<uint32>( pField, pPlayerLocal, (uint32)kvValue->GetInt( "" ) );
		else
			kvValue->SetInt( "", TitleDataFieldsDescriptionGetValue<uint32>( pField, pPlayerLocal ) );
		break;
	case TitleDataFieldsDescription_t::DT_float:
		if ( eOp == KVSTAT_WRITE_STAT )
			TitleDataFieldsDescriptionSetValue<float>( pField, pPlayerLocal, (float)kvValue->GetFloat( "" ) );
		else
			kvValue->SetInt( "", TitleDataFieldsDescriptionGetValue<float>( pField, pPlayerLocal ) );
		break;
	case TitleDataFieldsDescription_t::DT_uint64:
		if ( eOp == KVSTAT_WRITE_STAT )
			TitleDataFieldsDescriptionSetValue<uint64>( pField, pPlayerLocal, (uint64)kvValue->GetUint64( "" ) );
		else
			kvValue->SetUint64( "", TitleDataFieldsDescriptionGetValue<uint64>( pField, pPlayerLocal ) );
		break;
	}
}

void PlayerManager::OnEvent( KeyValues *pEvent )
{
	char const *szName = pEvent->GetName();

	if ( !V_stricmp( szName, "Client::CmdKeyValues" ) )
	{
		KeyValues *pCmd = pEvent->GetFirstTrueSubKey();
		if ( !pCmd )
			return;

		int nSlot = pEvent->GetInt( "slot" );
		int iCtrlr = XBX_GetUserId( nSlot );
		IPlayerLocal *pPlayerLocal = GetLocalPlayer( iCtrlr );

		char const *szCmd = pCmd->GetName();
		if ( !V_stricmp( "write_stats", szCmd ) )
		{
			if ( pPlayerLocal )
			{
				TitleDataFieldsDescription_t const *pFields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
				for ( KeyValues *kvValue = pCmd->GetFirstValue(); kvValue; kvValue = kvValue->GetNextValue() )
				{
					char const *szStatName = kvValue->GetName();
					// Try to find the stat to write
					if ( TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( pFields, szStatName ) )
					{
						// Found the stat to write
						DevMsg( "pPlayerLocal(%s)->write_stat(%s)\n", pPlayerLocal->GetName(), pField->m_szFieldName );
						SyncKeyValueWithStatField( kvValue, pPlayerLocal, pField, KVSTAT_WRITE_STAT );
						szStatName = nullptr;
					}
					if ( szStatName )
					{
						DevWarning( "pPlayerLocal(%s)->write_stat(%s) UNKNOWN STAT!\n", pPlayerLocal->GetName(), szStatName );
					}
				}
			}
			else
			{
				DevWarning( "pPlayerLocal(#%d)->write_stat UNKNOWN SLOT!\n", nSlot );
			}
		}
		else if ( !V_stricmp( "read_stats", szCmd ) )
		{
			KeyValues *kvReply = pCmd->MakeCopy();
			if ( pPlayerLocal )
			{
				TitleDataFieldsDescription_t const *pFields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
				for ( KeyValues *kvValue = kvReply->GetFirstValue(); kvValue; kvValue = kvValue->GetNextValue() )
				{
					char const *szStatName = kvValue->GetName();
					// Try to find the stat to read
					if ( TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( pFields, szStatName ) )
					{
						// Found the stat to read
						SyncKeyValueWithStatField( kvValue, pPlayerLocal, pField, KVSTAT_READ_STAT );
						szStatName = nullptr;
					}
					if ( szStatName )
					{
						DevWarning( "pPlayerLocal(%s)->read_stat(%s) UNKNOWN STAT!\n", pPlayerLocal->GetName(), szStatName );
					}
				}
			}
			else
			{
				DevWarning( "pPlayerLocal(#%d)->read_stats UNKNOWN SLOT!\n", nSlot );
			}

			// Send the reply to server
			int nActiveSlot = g_pMatchExtensions->GetIVEngineClient()->GetActiveSplitScreenPlayerSlot();
			g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( nSlot );
			g_pMatchExtensions->GetIVEngineClient()->ServerCmdKeyValues( kvReply );
			g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( nActiveSlot );
		}
	}
}
