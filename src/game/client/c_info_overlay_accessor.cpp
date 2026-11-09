// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "materialsystem/imesh.h"
#include "toolframework_client.h"

// -------------------------------------------------------------------------------- //
// An entity used to access overlays (and change their texture)
// -------------------------------------------------------------------------------- //
class [[= ks::reflect::NetTable{ .name = "DT_InfoOverlayAccessor", .base = false } ]]
      [[= ks::reflect::From<"m_iTextureFrameIndex", ks::reflect::Net{}>{} ]]
      C_InfoOverlayAccessor : public C_BaseEntity
{
public:

	DECLARE_CLASS( C_InfoOverlayAccessor, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_InfoOverlayAccessor();

	virtual void OnDataChanged( DataUpdateType_t updateType );

private:

	[[= ks::reflect::Net{} ]] int		m_iOverlayID;
};

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(C_InfoOverlayAccessor, DT_InfoOverlayAccessor, CInfoOverlayAccessor);

IMPLEMENT_REFLECT_TABLE( C_InfoOverlayAccessor, DT_InfoOverlayAccessor );


// -------------------------------------------------------------------------------- //
// Functions.
// -------------------------------------------------------------------------------- //

C_InfoOverlayAccessor::C_InfoOverlayAccessor()
{
}

void C_InfoOverlayAccessor::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Update overlay's bind proxy
		engine->SetOverlayBindProxy( m_iOverlayID, GetClientRenderable() );
	}
}
