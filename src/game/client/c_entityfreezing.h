#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "reflect_annotations.h"

struct EntityFreezingHitboxBlobData_t
{
	std::vector< Vector > m_vPoints;
};


//-----------------------------------------------------------------------------
// Entity Dissolve, client-side implementation
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EntityFreezing" } ]] C_EntityFreezing : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_EntityFreezing, C_BaseEntity );

	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType( );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool	ShouldDraw() { return true; }
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void			ClientThink( void );

private:
	[[= ks::reflect::Net{} ]] Vector	m_vFreezingOrigin;
	[[= ks::reflect::Net{} ]] float	m_flFrozenPerHitbox[ 50 ];
	[[= ks::reflect::Net{} ]] float	m_flFrozen;
	[[= ks::reflect::Net{} ]] bool	m_bFinishFreezing;

	std::vector< EntityFreezingHitboxBlobData_t > m_HitboxBlobData;
};
