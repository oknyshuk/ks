//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------------------------------------------------------- //
// An entity used to access overlays (and change their texture)
// -------------------------------------------------------------------------------- //

class [[= ks::reflect::NetTable{ .name = "DT_InfoOverlayAccessor", .base = false } ]]
      [[= ks::reflect::From<"m_iTextureFrameIndex", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      CInfoOverlayAccessor : public CPointEntity
{
public:

	DECLARE_CLASS( CInfoOverlayAccessor, CPointEntity );

	int  	UpdateTransmitState();

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:

	CNetworkVar( int, m_iOverlayID, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "OverlayID" } ]] );
};
							  

// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CInfoOverlayAccessor, DT_InfoOverlayAccessor )

LINK_ENTITY_TO_CLASS( info_overlay_accessor, CInfoOverlayAccessor );

IMPLEMENT_REFLECT_DATAMAP( CInfoOverlayAccessor )


int CInfoOverlayAccessor::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}
