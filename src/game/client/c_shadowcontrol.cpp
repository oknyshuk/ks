//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Shadow control entity.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ShadowControl" } ]]
      C_ShadowControl : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ShadowControl, C_BaseEntity );

	DECLARE_CLIENTCLASS();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

private:
	[[= ks::reflect::Net{} ]] Vector m_shadowDirection;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32 m_shadowColor;
	[[= ks::reflect::Net{} ]] float m_flShadowMaxDist;
	[[= ks::reflect::Net{} ]] bool m_bDisableShadows;
	[[= ks::reflect::Net{} ]] bool m_bEnableLocalLightShadows;
};

IMPLEMENT_REFLECT_CLIENTCLASS( C_ShadowControl, DT_ShadowControl, CShadowControl )


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void C_ShadowControl::OnDataChanged(DataUpdateType_t updateType)
{
	// Set the color, direction, distance...
	g_pClientShadowMgr->SetShadowDirection( m_shadowDirection );
	g_pClientShadowMgr->SetShadowColor( m_shadowColor.r, m_shadowColor.g, m_shadowColor.b );
	g_pClientShadowMgr->SetShadowDistance( m_flShadowMaxDist );
	g_pClientShadowMgr->SetShadowsDisabled( m_bDisableShadows );
	g_pClientShadowMgr->SetShadowFromWorldLightsEnabled( m_bEnableLocalLightShadows );
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_ShadowControl::ShouldDraw()
{
	return false;
}

