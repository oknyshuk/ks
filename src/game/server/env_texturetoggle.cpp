// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "recipientfilter.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

class CTextureToggle : public CPointEntity
{
public:
	DECLARE_CLASS( CTextureToggle, CPointEntity );

	[[= ks::reflect::Input{ .name = "IncrementTextureIndex", .type = FIELD_VOID } ]] void	InputIncrementBrushTexIndex( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTextureIndex", .type = FIELD_INTEGER } ]] void	InputSetBrushTexIndex( inputdata_t &inputdata );

private:
	
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_texturetoggle, CTextureToggle );

IMPLEMENT_REFLECT_DATAMAP( CTextureToggle )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CTextureToggle::InputIncrementBrushTexIndex( inputdata_t& inputdata )
{
	CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_target );
		
	while( pEntity ) 
	{
		int iCurrentIndex =  pEntity->GetTextureFrameIndex() + 1;
		pEntity->SetTextureFrameIndex( iCurrentIndex );

		pEntity = gEntList.FindEntityByName( pEntity, m_target ); 
	}
}

void CTextureToggle::InputSetBrushTexIndex( inputdata_t& inputdata )
{
	CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_target );
		
	while( pEntity ) 
	{
		int iData = inputdata.value.Int();

		pEntity->SetTextureFrameIndex( iData );
		pEntity = gEntList.FindEntityByName( pEntity, m_target ); 
	}
}
