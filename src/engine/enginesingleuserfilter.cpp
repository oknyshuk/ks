// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "quakedef.h"
#include "server.h"
#include "enginesingleuserfilter.h"
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.vector_ops;

void SV_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEngineRecipientFilter::CEngineRecipientFilter()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineRecipientFilter::Reset( void )
{
	m_bReliable			= false;
	m_bInit				= false;
	m_Recipients.clear();
}

void CEngineRecipientFilter::MakeReliable( void )
{
	m_bReliable = true;
}


void CEngineRecipientFilter::MakeInitMessage( void )
{
	m_bInit = true;
}

int CEngineRecipientFilter::GetRecipientCount( void ) const
{
	return std::ssize( m_Recipients );
}

int	CEngineRecipientFilter::GetRecipientIndex( int slot ) const
{
	if ( slot < 0 || slot >= GetRecipientCount() )
		return -1;

	return m_Recipients[ slot ];
}

void CEngineRecipientFilter::AddAllPlayers( void )
{
	m_Recipients.clear();

	for ( int i = 0; i < sv.GetClientCount(); i++ )
	{
		IClient *cl = sv.GetClient( i );

		if ( !cl->IsActive() )
			continue;

		m_Recipients.push_back( i+1 );
	}
}


void CEngineRecipientFilter::AddRecipient( int index )
{
	// Already in list
	if ( ks::find_index( m_Recipients, index ) != -1 )
		return;

	m_Recipients.push_back( index );
}

void CEngineRecipientFilter::RemoveRecipient( int index )
{
	// Remove it if it's in the list
	ks::erase_first( m_Recipients, index );
}

void CEngineRecipientFilter::AddPlayersFromBitMask( CPlayerBitVec& playerbits )
{
	for( int i = 0; i < sv.GetClientCount(); i++ )
	{
		if ( !playerbits[i] )
			continue;

		IClient *cl = sv.GetClient( i );
		if ( !cl->IsActive() )
			continue;

		AddRecipient( i + 1 );
	}
}

bool CEngineRecipientFilter::IncludesPlayer(int playerindex)
{
	for( int i = 0; i < GetRecipientCount(); i++ )
	{
		if ( playerindex ==	GetRecipientIndex(i) )
			return true;
	}

	return false;
}

void CEngineRecipientFilter::AddPlayersFromFilter(const IRecipientFilter *filter )
{
	for( int i = 0; i < filter->GetRecipientCount(); i++ )
	{
		AddRecipient( filter->GetRecipientIndex(i) );
	}
}

void CEngineRecipientFilter::AddRecipientsByPVS( const Vector& origin )
{
	if ( sv.GetMaxClients() == 1 )
	{
		AddAllPlayers();
	}
	else
	{
		CPlayerBitVec playerbits;
		SV_DetermineMulticastRecipients( false, origin, playerbits );
		AddPlayersFromBitMask( playerbits );
	}
}

void CEngineRecipientFilter::AddRecipientsByPAS( const Vector& origin )
{
	if ( sv.GetMaxClients() == 1 )
	{
		AddAllPlayers();
	}
	else
	{
		CPlayerBitVec playerbits;
		SV_DetermineMulticastRecipients( true, origin, playerbits );
		AddPlayersFromBitMask( playerbits );
	}
}
