//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MODELENTITIES_H
#define MODELENTITIES_H

#include "reflect_annotations.h"

#include "positionwatcher.h"

//!! replace this with generic start enabled/disabled
#define SF_WALL_START_OFF		0x0001
#define SF_IGNORE_PLAYERUSE		0x0002

//-----------------------------------------------------------------------------
// Purpose: basic solid geometry
// enabled state:	brush is visible
// disabled staute:	brush not visible
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_FuncBrush" } ]]
      CFuncBrush : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncBrush, CBaseEntity );

	virtual void Spawn( void );
	virtual void Activate( void );
	bool CreateVPhysics( void );

	virtual int	ObjectCaps( void ) { return HasSpawnFlags(SF_IGNORE_PLAYERUSE) ? BaseClass::ObjectCaps() : BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }

	virtual int DrawDebugTextOverlays( void );

	void TurnOff( void );
	void TurnOn( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetExcluded", .type = FIELD_STRING } ]] void InputSetExcluded( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetInvert", .type = FIELD_BOOLEAN } ]] void InputSetInvert( inputdata_t &inputdata );

	enum BrushSolidities_e {
		BRUSHSOLID_TOGGLE = 0,
		BRUSHSOLID_NEVER  = 1,
		BRUSHSOLID_ALWAYS = 2,
	};

	[[= ks::reflect::Key{ .name = "Solidity" } ]] BrushSolidities_e m_iSolidity;
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] int m_iDisabled;
	[[= ks::reflect::Key{ .name = "excludednpc" } ]] string_t m_iszExcludedClass;
	[[= ks::reflect::Key{ .name = "solidbsp" } ]] bool m_bSolidBsp;
	[[= ks::reflect::Key{ .name = "invert_exclusion" } ]] bool m_bInvertExclusion;

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual bool IsOn( void );
};


#endif // MODELENTITIES_H
