//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( C_BASEDOOR_H )
#define C_BASEDOOR_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"
#include "c_basetoggle.h"

#if defined( CLIENT_DLL )
#define CBaseDoor C_BaseDoor
#endif

class [[= ks::reflect::NetTable{ .name = "DT_BaseDoor" } ]]
      C_BaseDoor : public C_BaseToggle
{
public:
	DECLARE_CLASS( C_BaseDoor, C_BaseToggle );
	DECLARE_CLIENTCLASS();

	C_BaseDoor( void );
	~C_BaseDoor( void );

public:
	[[= ks::reflect::Net{} ]] float		m_flWaveHeight;
};

#endif // C_BASEDOOR_H
