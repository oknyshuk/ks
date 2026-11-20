#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

class CNetworkStringTableItem
{
public:
	enum
	{
		MAX_USERDATA_BITS = 14,
		MAX_USERDATA_SIZE = (1 << MAX_USERDATA_BITS)
	};

	struct itemchange_s {
		int				tick;
		int				length;
		unsigned char	*data;
	};

	CNetworkStringTableItem( void );
	~CNetworkStringTableItem( void );

	void			EnableChangeHistory( void );
	void 			UpdateChangeList( int tick, int length, const void *userData );
	int				RestoreTick( int tick );
	inline int		GetTickCreated( void ) const { return m_nTickCreated; }
	
	bool			SetUserData( int tick, int length, const void *userdata );
	const void		*GetUserData( int *length=0 );
	inline int		GetUserDataLength() const { return m_nUserDataLength; }
	
	// Used by server only
	// void			SetTickCount( int count ) ;
	inline int		GetTickChanged( void ) const { return m_nTickChanged; }

public:
	unsigned char	*m_pUserData;
	int				m_nUserDataLength;
	int				m_nTickChanged;

	int				m_nTickCreated;
	std::vector< itemchange_s > *m_pChangeList;	
};
