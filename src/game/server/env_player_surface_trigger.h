//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENV_PLAYER_SURFACE_TRIGGER_H
#define ENV_PLAYER_SURFACE_TRIGGER_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "entityoutput.h"

//-----------------------------------------------------------------------------
// Purpose: Entity that fires outputs whenever the player stands on a different surface
//-----------------------------------------------------------------------------
class CEnvPlayerSurfaceTrigger : public CPointEntity
{
	DECLARE_CLASS( CEnvPlayerSurfaceTrigger, CPointEntity );
public:
	DECLARE_DATADESC();

	~CEnvPlayerSurfaceTrigger( void );
	void	Spawn( void );
	void	OnRestore( void );

	// Main interface to all surface triggers
	static void	SetPlayerSurface( CBasePlayer *pPlayer, char gameMaterial );

	void	UpdateMaterialThink( void );

private:
	void	PlayerSurfaceChanged( CBasePlayer *pPlayer, char gameMaterial );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "gamematerial" } ]] int		m_iTargetGameMaterial;
	int		m_iCurrentGameMaterial;
	bool	m_bDisabled;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnSurfaceChangedToTarget" } ]] COutputEvent m_OnSurfaceChangedToTarget;
	[[= ks::reflect::Key{ .name = "OnSurfaceChangedFromTarget" } ]] COutputEvent m_OnSurfaceChangedFromTarget;
};

#endif // ENV_PLAYER_SURFACE_TRIGGER_H
