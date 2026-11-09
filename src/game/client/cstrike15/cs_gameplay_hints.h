#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Global object for storing gameplay hint strings for the UI to display.


#include "igamesystem.h"

#if !defined CS_GAMEPLAY_HINTS_H_ 
#define CS_GAMEPLAY_HINTS_H_ 


struct CSGameplayHint_t;
class CCSGameplayHints : public CAutoGameSystem
{
public:
	CCSGameplayHints();
	virtual ~CCSGameplayHints();

	virtual void PostInit();
	virtual void Shutdown();
	
	const char* GetRandomLeastPlayedHint( void );
	enum HintRequiredContextFlags
	{
		HINT_CONTEXT_ALWAYS_SHOW		= 0,
		HINT_CONTEXT_BOMB_MAP			= (1<<0), 
		HINT_CONTEXT_HOSTAGE_MAP		= (1<<1), 
		HINT_CONTEXT_GUNGAME			= (1<<2), 
		// HINT_CONTEXT_ELO_BRACKET_SHOWN	= (1<<3),  -- obsolete
	};

private:
	std::vector< std::unique_ptr< CSGameplayHint_t > > m_HintList;
	KeyValues *m_pHintKV;

	void Cleanup( void );
	uint32 GetCurrentContextFlags( void );
};

extern CCSGameplayHints g_CSGameplayHints;

#endif // CS_GAMEPLAY_HINTS_H_ 
