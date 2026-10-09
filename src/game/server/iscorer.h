#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Interface that entities can use to redirect scoring to other entities.
//			i.e. A rocket redirects scoring to the player that fired it.

//-----------------------------------------------------------------------------
// Purpose: Interface that entities can use to redirect scoring to other entities.
//			i.e. A rocket redirects scoring to the player that fired it.
//-----------------------------------------------------------------------------
class IScorer
{
public:
	// Return the entity that should receive the score
	virtual CBasePlayer *GetScorer( void ) = 0;
	// Return the entity that should get assistance credit
	virtual CBasePlayer *GetAssistant( void ) = 0;
};
