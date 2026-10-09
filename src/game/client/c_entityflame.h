#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "c_baseentity.h"
#include "reflect_annotations.h"

//
// Entity flame, client-side implementation
//

class [[= ks::reflect::NetTable{ .name = "DT_EntityFlame" } ]] C_EntityFlame : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_EntityFlame, C_BaseEntity );

	C_EntityFlame( void );
	virtual ~C_EntityFlame( void );

	virtual bool	Simulate( void );
	virtual void	UpdateOnRemove( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	[[= ks::reflect::Net{} ]] EHANDLE	m_hEntAttached;		// The entity that we are burning (attached to).

private:
	void	CreateEffect( void );
	void	StopEffect( void );

	ks::tracked_ref<CNewParticleEffect> m_hEffect;
	EHANDLE				m_hOldAttached;
	[[= ks::reflect::Net{} ]] bool				m_bCheapEffect;
};
