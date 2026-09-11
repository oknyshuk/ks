//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------------------------------------------------------- //
// An entity used to test traceline
// -------------------------------------------------------------------------------- //

class [[= ks::reflect::NetTable{ .name = "DT_TestTraceline", .base = false } ]]
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::ENC_VECTOR }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::ENC_FLOAT, .index = 0 }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::ENC_FLOAT, .index = 1 }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::ENC_FLOAT, .index = 2 }>{} ]]
      [[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
      CTestTraceline : public CPointEntity
{
public:
	DECLARE_CLASS( CTestTraceline, CPointEntity );

	void	Spawn( void );
	int  	UpdateTransmitState();

	DECLARE_SERVERCLASS();

private:
	void	Spin( void );
};
							  

// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CTestTraceline, DT_TestTraceline )

LINK_ENTITY_TO_CLASS( test_traceline, CTestTraceline );



void	CTestTraceline::Spawn( void )
{
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	SetNextThink( gpGlobals->curtime );

	SetThink( &CTestTraceline::Spin );
}

void	CTestTraceline::Spin( void )
{
	static ConVar	traceline_spin( "traceline_spin","1" );

	if (traceline_spin.GetInt())
	{
		float s = sin( gpGlobals->curtime );
		QAngle angles = GetLocalAngles();

		angles[0] = 180.0 * 0.5 * (s * s * s + 1.0f) + 90;
		angles[1] = gpGlobals->curtime * 10;
		   
		SetLocalAngles( angles );

	}
	SetNextThink( gpGlobals->curtime );
}

int CTestTraceline::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}
