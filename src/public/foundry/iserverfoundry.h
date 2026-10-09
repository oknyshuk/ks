#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

//-----------------------------------------------------------------------------
// Purpose: exposed from Foundry (Hammer) to game DLL
//-----------------------------------------------------------------------------
class IServerFoundry : public IBaseInterface
{
public:
	// This is used when you load a savegame and use certain Hammer entities on top of the savegame data.
	virtual bool GetRestoredEntityReplacementData( int iHammerID, std::vector< char > &data ) = 0;
	virtual void OnFinishedRestoreSavegame() = 0;

	// Move Hammer's version of the entity.
	virtual void MoveEntityTo( int nHammerID, const Vector &vPos, const QAngle &vAngles ) = 0;

	// Move Hammer's 3D view to the specified position.
	virtual void MoveHammerViewTo( const Vector &vPos, const QAngle &vAngles ) = 0;

	// If the game is controlling the input, release it.
	virtual void EngineGetMouseControl() = 0;
	virtual void EngineReleaseMouseControl() = 0;

	// Select the specified entities in Hammer.
	virtual void SelectEntities( int *pHammerIDs, int nIDs ) = 0;

	// Simulate a mouse click in the center of the 3D view.
	virtual void SelectionClickInCenterOfView( const Vector &vPos, const QAngle &vAngles ) = 0;
};

inline constexpr const char *VSERVERFOUNDRY_INTERFACE_VERSION = "VSERVERFOUNDRY001";
