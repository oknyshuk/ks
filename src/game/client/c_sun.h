//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_SUN_H
#define C_SUN_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"
#include "utllinkedlist.h"
#include "glow_overlay.h"
#include "sun_shared.h"

//
// Special glow overlay
//

class C_SunGlowOverlay : public CGlowOverlay
{
	virtual void CalcSpriteColorAndSize( float flDot, CGlowSprite *pSprite, float *flHorzSize, float *flVertSize, Vector *vColor )
	{
		if ( m_bModulateByDot )
		{
			float alpha = RemapVal( flDot, 1.0f, 0.9f, 0.75f, 0.0f );
			alpha = clamp( alpha, 0.0f, 0.75f );

			*flHorzSize = pSprite->m_flHorzSize * 6.0f;
			*flVertSize = pSprite->m_flVertSize * 6.0f;
			*vColor = pSprite->m_vColor * alpha * m_flGlowObstructionScale;
		}
		else
		{
			*flHorzSize = pSprite->m_flHorzSize;
			*flVertSize = pSprite->m_flVertSize;
			*vColor = pSprite->m_vColor * m_flGlowObstructionScale;
		}
	}

public:

	void SetModulateByDot( bool state = true )
	{
		m_bModulateByDot = state;
	}

protected:

	bool m_bModulateByDot;
};

//
// Sun entity
//

// named by the Bare<> annotation below, so it cannot be a .cpp static
void RecvProxy_SunHDRColorScale( const CRecvProxyData *pData, void *pStruct, void *pOut );

class [[= ks::reflect::NetTable{ .name = "DT_Sun", .base = false } ]]
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
      [[= ks::reflect::Bare<"HDRColorScale",
            ks::reflect::Net{ .enc = ks::reflect::ENC_FLOAT }, RecvProxy_SunHDRColorScale>{} ]]
      C_Sun : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_Sun, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_Sun();
					~C_Sun();

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:
	C_SunGlowOverlay	m_Overlay;
	C_SunGlowOverlay	m_GlowOverlay;
	
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WIRE_RECV>{} ]] color32				m_clrOverlay;
	[[= ks::reflect::Net{} ]] int					m_nSize;
	[[= ks::reflect::Net{} ]] int					m_nOverlaySize;
	[[= ks::reflect::Net{} ]] Vector				m_vDirection;
	[[= ks::reflect::Net{ .enc = ks::reflect::ENC_INT } ]] bool				m_bOn;

	[[= ks::reflect::Net{} ]] int					m_nMaterial;
	[[= ks::reflect::Net{} ]] int					m_nOverlayMaterial;
};


#endif // C_SUN_H
