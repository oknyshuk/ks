// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncOccluder" } ]]
      C_FuncOccluder : public C_BaseEntity
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_FuncOccluder, C_BaseEntity );

// Overrides.
public:
	virtual bool	ShouldDraw();
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

private:
	[[= ks::reflect::Net{} ]] int m_nOccluderIndex;
	[[= ks::reflect::Net{} ]] bool m_bActive;
};

IMPLEMENT_REFLECT_CLIENTCLASS( C_FuncOccluder, DT_FuncOccluder, CFuncOccluder )


void C_FuncOccluder::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	engine->ActivateOccluder( m_nOccluderIndex, m_bActive );
}

bool C_FuncOccluder::ShouldDraw()
{
	return false;
}

int C_FuncOccluder::DrawModel( int flags, const RenderableInstance_t &instance )
{
	contract_assert( 0 );
	return 0;
}
