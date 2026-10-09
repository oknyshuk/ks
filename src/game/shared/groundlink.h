#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "ehandle.h"

#if defined( CLIENT_DLL )
class C_BaseEntity;
typedef CHandle<C_BaseEntity> EHANDLE;
#else
class CBaseEntity;
typedef CHandle<CBaseEntity> EHANDLE;
#endif

//-----------------------------------------------------------------------------
// Purpose: Used for tracking many to one ground entity chains ( many ents can share a single ground entity )
//-----------------------------------------------------------------------------
struct groundlink_t
{
	EHANDLE					entity;
	groundlink_t			*nextLink;
	groundlink_t			*prevLink;
};
