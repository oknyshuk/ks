#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation


import ks.core.threadtools;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.netadr;
import ks.tier1.ns_address;

import std;

#include "appframework/IAppSystemGroup.h"
#include "matchmaking/imatchframework.h"
#include "engine/inetsupport.h"

#include "tier2/tier2.h"


class CNetSupportImpl : public CTier2AppSystem< INetSupport >
{
	typedef CTier2AppSystem< INetSupport > BaseClass;

	// Methods of IAppSystem
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of INetSupport
public:
	// Get engine build number
	virtual int GetEngineBuildNumber();

	// Get server info
	virtual void GetServerInfo( ServerInfo_t *pServerInfo );

	// Get client info
	virtual void GetClientInfo( ClientInfo_t *pClientInfo );

	// Update a local server reservation
	virtual void UpdateServerReservation( uint64 uiReservation );

	// Update a client reservation
	virtual void UpdateClientReservation( uint64 uiReservation, uint64 uiMachineIdHost );

	// Submit a server reservation packet
	virtual void ReserveServer(
		const ns_address &netAdrPublic, const ns_address &netAdrPrivate,
		uint64 nServerReservationCookie, KeyValues *pKVGameSettings,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) override;

	// Check server reservation cookie matches cookie held by client
	virtual bool CheckServerReservation( 
		const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) override;

	virtual bool ServerPing( const ns_address &netAdrPublic,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation ) override;

	// When client event is fired
	virtual void OnMatchEvent( KeyValues *pEvent );

	// Send a network packet
	virtual int SendPacket (
		INetChannel *chan, int sock,  const netadr_t &to,
		const void *data, int length,
		bf_write *pVoicePayload = nullptr,
		bool bUseCompression = false );

public:
	CNetSupportImpl();
	~CNetSupportImpl();
};
