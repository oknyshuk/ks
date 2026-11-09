#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: work in progress

#include "igamesystem.h"

class CGlobalEvent;


class CGlobalEventLog : public CAutoGameSystemPerFrame
{
public:
	typedef enum
	{
		GLOBAL_EVENT_NPCS = 0,

		GLOBAL_EVENT_MAX
	} EGlobalEvent;

public:
	CGlobalEventLog( );

	CGlobalEvent	*GetGlobalEvent( EGlobalEvent GlobalEvent );

	CGlobalEvent	*CreateEvent( const char *pszName, bool bIsHighLevel, CGlobalEvent *pParent = nullptr );
	CGlobalEvent	*CreateTempEvent( const char *pszName, CGlobalEvent *pParent = nullptr );
	void			RemoveEvent( CGlobalEvent *pEvent );

	void			AddKeyValue( CGlobalEvent *pEvent, bool bVarying, const char *pszKey, const char *pszValueFormat, ... );

	void			SendUpdate( );

protected:
	virtual void PostInit( );
	virtual void FrameUpdatePostEntityThink( );

private:
	unsigned int					m_nNextID;
	std::vector< CGlobalEvent * >	m_Events;
	std::vector< CGlobalEvent * >	m_TempEvents;
	std::vector< CGlobalEvent * >	m_DirtyEvents;
	CGlobalEvent					*m_pGlobalEvents[ GLOBAL_EVENT_MAX ];
};


extern CGlobalEventLog	GlobalEventLog;
