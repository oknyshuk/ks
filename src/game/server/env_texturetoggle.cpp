//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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
	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_target );
		
	while( pEntity ) 
	{
		int iCurrentIndex =  pEntity->GetTextureFrameIndex() + 1;
		pEntity->SetTextureFrameIndex( iCurrentIndex );

		pEntity = gEntList.FindEntityByName( pEntity, m_target ); 
	}
}

void CTextureToggle::InputSetBrushTexIndex( inputdata_t& inputdata )
{
	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_target );
		
	while( pEntity ) 
	{
		int iData = inputdata.value.Int();

		pEntity->SetTextureFrameIndex( iData );
		pEntity = gEntList.FindEntityByName( pEntity, m_target ); 
	}
}

