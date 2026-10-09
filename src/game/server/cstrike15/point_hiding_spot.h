#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "baseentity.h"
#include "nav_area.h"

class CPointHidingSpot : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPointHidingSpot, CServerOnlyPointEntity )
public:
	CPointHidingSpot();
	DECLARE_DATADESC();

	virtual void Activate() override;
	virtual void UpdateOnRemove() override;
	void PostActivateSetupThink();
	void DetachFromHidingSpot();
protected:
	CNavArea *m_pNavArea;		// nav our hiding spot is associated with
	HidingSpot *m_pSpot;
};
