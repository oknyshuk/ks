#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import std;
import ks.tier1;

#include "localize/ilocalize.h"
#include "engine/inetsupport.h"
#include "cdll_int.h"
#include "eiface.h"
#include "igameevents.h"
#include "eiface.h"

#include "mm_framework.h"

#include "engine/ienginevoice.h"

class CMatchExtensions : public IMatchExtensions
{
	// Methods of IMatchExtensions
public:
	// Registers an extension interface
	void RegisterExtensionInterface( char const *szInterfaceString, void *pvInterface );

	// Unregisters an extension interface
	void UnregisterExtensionInterface( char const *szInterfaceString, void *pvInterface );

	// Gets a pointer to a registered extension interface
	void * GetRegisteredExtensionInterface( char const *szInterfaceString );

public:
	CMatchExtensions();
	~CMatchExtensions();

protected:
	struct RegisteredInterface_t
	{
		void *m_pvInterface;
		int m_nRefCount;

		RegisteredInterface_t() : m_nRefCount( 0 ), m_pvInterface( 0 ) {}
	};
	typedef ks::idict< RegisteredInterface_t > InterfaceMap_t;
	InterfaceMap_t m_mapRegisteredInterfaces;

protected:
	void OnExtensionInterfaceUpdated( char const *szInterfaceString, void *pvInterface );

public:
	ILocalize * GetILocalize() { return m_exts.m_pILocalize; }
	INetSupport * GetINetSupport() { return m_exts.m_pINetSupport; }
	IEngineVoice * GetIEngineVoice() { return m_exts.m_pIEngineVoice; }
	IVEngineClient * GetIVEngineClient() { return m_exts.m_pIVEngineClient; }
	IVEngineServer * GetIVEngineServer() { return m_exts.m_pIVEngineServer; }
	IServerGameDLL * GetIServerGameDLL() { return m_exts.m_pIServerGameDLL; }
	IGameEventManager2 * GetIGameEventManager2() { return m_exts.m_pIGameEventManager2; }
	IBaseClientDLL * GetIBaseClientDLL() { return m_exts.m_pIBaseClientDLL; }

protected:
	// Known extension interfaces
	struct Exts_t
	{
		inline Exts_t() { std::memset( this, 0, sizeof( *this ) ); }

		ILocalize *m_pILocalize;
		INetSupport *m_pINetSupport;
		IEngineVoice *m_pIEngineVoice;
		IVEngineClient *m_pIVEngineClient;
		IVEngineServer *m_pIVEngineServer;
		IServerGameDLL *m_pIServerGameDLL;
		IGameEventManager2 *m_pIGameEventManager2;
		IBaseClientDLL *m_pIBaseClientDLL;
	}
	m_exts;
};

// Match title singleton
extern CMatchExtensions *g_pMatchExtensions;
