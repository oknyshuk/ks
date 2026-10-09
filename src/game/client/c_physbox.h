#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Client-side CBasePlayer

#include "reflect_annotations.h"

#include "c_baseentity.h"

class [[= ks::reflect::NetTable{ .name = "DT_PhysBox" } ]]
      C_PhysBox : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PhysBox, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_PhysBox();
	virtual			~C_PhysBox();
	virtual ShadowType_t ShadowCastType();
	
public:
	[[= ks::reflect::Net{} ]] float			m_mass;	// TEST..
};
