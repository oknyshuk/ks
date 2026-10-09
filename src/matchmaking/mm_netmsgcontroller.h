#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_framework.h"

class CMatchNetworkMsgControllerBase : public IMatchNetworkMsgController
{
	// Methods of IMatchNetworkMsgController
public:
	// To determine host Quality-of-Service
	virtual MM_QOS_t GetQOS();

	virtual KeyValues * GetActiveServerGameDetails( KeyValues *pRequest );

	virtual KeyValues * UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply );

	virtual void PackageGameDetailsForQOS( KeyValues *pSettings, ks::byte_writer &buf );

	virtual KeyValues * PackageGameDetailsForReservation( KeyValues *pSettings );

public:
	CMatchNetworkMsgControllerBase();
	~CMatchNetworkMsgControllerBase();
};

// Match title singleton
extern CMatchNetworkMsgControllerBase *g_pMatchNetMsgControllerBase;
