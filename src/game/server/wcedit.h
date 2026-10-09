#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Namespace for functions having to do with WC Edit mode

import ks.math;
#include "reflect_annotations.h"
import std;
class CBasePlayer;

class CBaseEntity;

//=============================================================================
//	>> NWCEdit
//=============================================================================
namespace NWCEdit
{
	Vector	AirNodePlacementPosition( void );
	bool	IsWCVersionValid(void);
	void	CreateAINode(   CBasePlayer *pPlayer );
	void	DestroyAINode(  CBasePlayer *pPlayer );
	void	CreateAILink(	CBasePlayer *pPlayer );
	void	DestroyAILink(  CBasePlayer *pPlayer );
	void	UndoDestroyAINode(void);
	void	RememberEntityPosition( CBaseEntity *pEntity );
	void	UpdateEntityPosition( CBaseEntity *pEntity );
};
