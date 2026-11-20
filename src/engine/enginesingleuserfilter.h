#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
#include "irecipientfilter.h"
import std;
#include "const.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEngineRecipientFilter : public IRecipientFilter
{
public:	// IRecipientFilter interface:
	
					CEngineRecipientFilter();
	virtual int		GetRecipientCount( void ) const;
	virtual int		GetRecipientIndex( int slot ) const;
	virtual bool	IsReliable( void ) const { return m_bReliable; };
	virtual bool	IsInitMessage( void )  const { return m_bInit; };

public:

	void			Reset( void );

	void			MakeInitMessage( void );
	void			MakeReliable( void );

	void			AddAllPlayers( void );
	void			AddRecipientsByPVS( const Vector& origin );
	void			AddRecipientsByPAS( const Vector& origin );
	void			AddPlayersFromBitMask( CPlayerBitVec& playerbits );
	void			AddPlayersFromFilter( const IRecipientFilter *filter );
	void			AddRecipient( int playerindex );
	void			RemoveRecipient( int playerindex );
	bool			IncludesPlayer(int playerindex);
	
private:

	bool				m_bInit;
	bool				m_bReliable;
	std::vector< int >	m_Recipients;
};

//-----------------------------------------------------------------------------
// Purpose: Simple filter for doing MSG_ONE type stuff directly in engine
//-----------------------------------------------------------------------------
class CEngineSingleUserFilter : public IRecipientFilter
{
public:
	CEngineSingleUserFilter( int clientindex, bool bReliable = false )
	{
		m_nClientIndex = clientindex;
		m_bReliable = bReliable;
	}

	virtual bool	IsReliable( void ) const
	{
		return m_bReliable;
	}

	virtual int		GetRecipientCount( void ) const
	{
		return 1;
	}

	virtual int		GetRecipientIndex( int slot ) const
	{
		return m_nClientIndex;
	}

	virtual bool	IsBroadcastMessage( void ) const
	{
		return false;
	}

	virtual bool	IsInitMessage( void ) const
	{
		return false;
	}

private:
	int				m_nClientIndex;
	bool			m_bReliable;
};
