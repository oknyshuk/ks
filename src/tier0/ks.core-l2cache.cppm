// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core:l2cache;

import :platform;

export extern "C++" {

class P4Event_BSQ_cache_reference;

class CL2Cache
{
public:

	CL2Cache();
	~CL2Cache();

	void Start( void );
	void End( void );

	//-------------------------------------------------------------------------
	// GetL2CacheMisses
	//-------------------------------------------------------------------------
	int GetL2CacheMisses( void )
	{
		return m_iL2CacheMissCount;
	}

private:

	int								m_nID;

	P4Event_BSQ_cache_reference		*m_pL2CacheEvent;
	int64							m_i64Start;
	int64							m_i64End;
	int								m_iL2CacheMissCount;
};

}
