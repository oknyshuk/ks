#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_framework.h"
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

class CMatchNetworkMsgControllerBase : public IMatchNetworkMsgController
{
	// Methods of IMatchNetworkMsgController
public:
	// To determine host Quality-of-Service
	MM_QOS_t GetQOS();

	KeyValues * GetActiveServerGameDetails( KeyValues *pRequest );

	KeyValues * UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply );

	void PackageGameDetailsForQOS( KeyValues *pSettings, ks::byte_writer &buf );

	KeyValues * PackageGameDetailsForReservation( KeyValues *pSettings );

public:
	CMatchNetworkMsgControllerBase();
	~CMatchNetworkMsgControllerBase();
};

// Match title singleton
extern CMatchNetworkMsgControllerBase *g_pMatchNetMsgControllerBase;
