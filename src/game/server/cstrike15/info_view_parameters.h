#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

#include "reflect_annotations.h"
#include "baseentity.h"

class CInfoViewParameters : public CBaseEntity
{
public:
	DECLARE_CLASS( CInfoViewParameters, CBaseEntity );
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "ViewMode" } ]] int m_nViewMode;
};
