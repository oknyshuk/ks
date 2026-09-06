//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( C_TE_BASEBEAM_H )
#define C_TE_BASEBEAM_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "c_basetempentity.h"

//-----------------------------------------------------------------------------
// Purpose: Base entity for beam te's
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_BaseBeam", .base = false } ]]
      [[= ks::reflect::From<"r", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"g", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"b", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"a", ks::reflect::Net{}>{} ]]
      C_TEBaseBeam : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBaseBeam, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

private:

public:

					C_TEBaseBeam( void );
	virtual			~C_TEBaseBeam( void );

	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nHaloIndex;
	[[= ks::reflect::Net{} ]] int				m_nStartFrame;
	[[= ks::reflect::Net{} ]] int				m_nFrameRate;
	[[= ks::reflect::Net{} ]] float			m_fLife;
	[[= ks::reflect::Net{} ]] float			m_fWidth;
	[[= ks::reflect::Net{} ]] float			m_fEndWidth;
	[[= ks::reflect::Net{} ]] int				m_nFadeLength;
	[[= ks::reflect::Net{} ]] float			m_fAmplitude;
	int				r, g, b, a;
	[[= ks::reflect::Net{} ]] int				m_nSpeed;
	[[= ks::reflect::Net{} ]] int				m_nFlags;
};

EXTERN_RECV_TABLE(DT_BaseBeam);

#endif // C_TE_BASEBEAM_H
