//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENVLASER_H
#define ENVLASER_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "beam_shared.h"
#include "entityoutput.h"


class CSprite;


class CEnvLaser : public CBeam
{
	DECLARE_CLASS( CEnvLaser, CBeam );
public:
	void	Spawn( void );
	void	Precache( void );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	void	TurnOn( void );
	void	TurnOff( void );
	int		IsOn( void );

	void	FireAtPoint( trace_t &point );
	void	StrikeThink( void );

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "LaserTarget" } ]] string_t m_iszLaserTarget;	// Name of entity or entities to strike at, randomly picked if more than one match.
	CSprite	*m_pSprite;
	[[= ks::reflect::Key{ .name = "EndSprite" } ]] string_t m_iszSpriteName;
	Vector  m_firePosition;

	[[= ks::reflect::Key{ .name = "framestart" } ]] float	m_flStartFrame;
};

#endif // ENVLASER_H
