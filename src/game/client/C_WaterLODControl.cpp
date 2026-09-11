//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Water LOD control entity.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "iviewrender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Water LOD control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_WaterLODControl" } ]]
      C_WaterLODControl : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_WaterLODControl, C_BaseEntity );

	DECLARE_CLIENTCLASS();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

private:
	[[= ks::reflect::Net{} ]] float m_flCheapWaterStartDistance;
	[[= ks::reflect::Net{} ]] float m_flCheapWaterEndDistance;
};

IMPLEMENT_REFLECT_CLIENTCLASS( C_WaterLODControl, DT_WaterLODControl, CWaterLODControl )


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_WaterLODControl::OnDataChanged(DataUpdateType_t updateType)
{
	view->SetCheapWaterStartDistance( m_flCheapWaterStartDistance );
	view->SetCheapWaterEndDistance( m_flCheapWaterEndDistance );
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_WaterLODControl::ShouldDraw()
{
	return false;
}

