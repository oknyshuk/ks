#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import ks.tier1;

namespace ks { class byte_writer; }

struct MM_QOS_t
{
	int nPingMsMin;		// Minimum round-trip time in ms
	int nPingMsMed;		// Median round-trip time in ms
	float flBwUpKbs;	// Bandwidth upstream in kilobytes/s
	float flBwDnKbs;	// Bandwidth downstream in kilobytes/s
	float flLoss;		// Average packet loss in percents
};

struct MM_GameDetails_QOS_t
{
	void *m_pvData;	// Encoded game details
	int m_numDataBytes; // Length of game details

	int m_nPing;	// Average ping in ms
};

class IMatchNetworkMsgController
{
public:
	// To determine host Quality-of-Service
	MM_QOS_t GetQOS();

	KeyValues * GetActiveServerGameDetails( KeyValues *pRequest );

	KeyValues * UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply );

	void PackageGameDetailsForQOS( KeyValues *pSettings, ks::byte_writer &buf );

	KeyValues * PackageGameDetailsForReservation( KeyValues *pSettings );
};
