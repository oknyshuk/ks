#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IGameUI;

//-----------------------------------------------------------------------------
// Purpose: Accessor function to get game ui interface
//-----------------------------------------------------------------------------
inline IGameUI *gameui()
{
	extern IGameUI *g_pGameUI;
	return g_pGameUI;
}
