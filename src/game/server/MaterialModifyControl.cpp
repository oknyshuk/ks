//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Material modify control entity.
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "reflect_sendtable.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight.
//------------------------------------------------------------------------------

#define MATERIAL_MODIFY_STRING_SIZE			255
#define MATERIAL_MODIFY_ANIMATION_UNSET		-1

// Must match C_MaterialModifyControl.cpp
enum MaterialModifyMode_t
{
	MATERIAL_MODIFY_MODE_NONE = 0,
	MATERIAL_MODIFY_MODE_SETVAR = 1,
	MATERIAL_MODIFY_MODE_ANIM_SEQUENCE = 2,
	MATERIAL_MODIFY_MODE_FLOAT_LERP = 3,
};

ConVar debug_materialmodifycontrol( "debug_materialmodifycontrol", "0" );

class [[= ks::reflect::NetTable{ .name = "DT_MaterialModifyControl" } ]] CMaterialModifyControl : public CBaseEntity
{
public:

	DECLARE_CLASS( CMaterialModifyControl, CBaseEntity );

	CMaterialModifyControl();

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	int UpdateTransmitState();
	int ShouldTransmit( const CCheckTransmitInfo *pInfo );

	[[= ks::reflect::Input{ .name = "SetMaterialVar", .type = FIELD_STRING } ]] void SetMaterialVar( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaterialVarToCurrentTime", .type = FIELD_VOID } ]] void SetMaterialVarToCurrentTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartAnimSequence", .type = FIELD_STRING } ]] void InputStartAnimSequence( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartFloatLerp", .type = FIELD_STRING } ]] void InputStartFloatLerp( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkString( m_szMaterialName, MATERIAL_MODIFY_STRING_SIZE, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szMaterialVar, MATERIAL_MODIFY_STRING_SIZE, [[= ks::reflect::Net{} ]] );
	CNetworkString( m_szMaterialVarValue, MATERIAL_MODIFY_STRING_SIZE, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_iFrameStart, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVar( int, m_iFrameEnd, [[= ks::reflect::Net{ .bits = 8 } ]] );
	CNetworkVar( bool, m_bWrap, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFramerate, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( bool, m_bNewAnimCommandsSemaphore, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFloatLerpStartValue, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFloatLerpEndValue, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFloatLerpTransitionTime, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nModifyMode, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
};

LINK_ENTITY_TO_CLASS(material_modify_control, CMaterialModifyControl);

IMPLEMENT_REFLECT_DATAMAP( CMaterialModifyControl )

IMPLEMENT_REFLECT_SERVERCLASS( CMaterialModifyControl, DT_MaterialModifyControl )

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMaterialModifyControl::CMaterialModifyControl()
{
	m_iFrameStart = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_iFrameEnd = MATERIAL_MODIFY_ANIMATION_UNSET;
	m_nModifyMode = MATERIAL_MODIFY_MODE_NONE;
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CMaterialModifyControl::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
bool CMaterialModifyControl::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "materialName" ) )
	{
		Q_strncpy( m_szMaterialName.GetForModify(), szValue, MATERIAL_MODIFY_STRING_SIZE );
		return true;
	}

	if ( FStrEq( szKeyName, "materialVar" ) )
	{
		Q_strncpy( m_szMaterialVar.GetForModify(), szValue, MATERIAL_MODIFY_STRING_SIZE );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model.
//------------------------------------------------------------------------------
int CMaterialModifyControl::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// Send if the parent is being sent:
//-----------------------------------------------------------------------------
int CMaterialModifyControl::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	CBaseEntity *pEnt = GetMoveParent();
	if ( pEnt )
	{
		return pEnt->ShouldTransmit( pInfo );
	}
	
	return FL_EDICT_DONTSEND;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterialModifyControl::SetMaterialVar( inputdata_t &inputdata )
{
	//if( debug_materialmodifycontrol.GetBool() && Q_stristr( GetDebugName(), "alyx" ) )
	//{
		//DevMsg( 1, "CMaterialModifyControl::SetMaterialVar %s %s %s=\"%s\"\n", 
			//GetDebugName(), m_szMaterialName.Get(), m_szMaterialVar.Get(), inputdata.value.String() );
	//}
	Q_strncpy( m_szMaterialVarValue.GetForModify(), inputdata.value.String(), MATERIAL_MODIFY_STRING_SIZE );
	m_nModifyMode = MATERIAL_MODIFY_MODE_SETVAR;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::SetMaterialVarToCurrentTime( inputdata_t &inputdata )
{
	char temp[32];
	Q_snprintf( temp, 32, "%f", gpGlobals->curtime );
	Q_strncpy( m_szMaterialVarValue.GetForModify(), temp, MATERIAL_MODIFY_STRING_SIZE );
	m_nModifyMode = MATERIAL_MODIFY_MODE_SETVAR;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::InputStartAnimSequence( inputdata_t &inputdata )
{
	char parseString[255];
	Q_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// Get the start & end frames
	char *pszParam = strtok(parseString," ");
	if ( pszParam && pszParam[0] )
	{
		int iFrameStart = atoi(pszParam);

		pszParam = strtok(nullptr," ");
		if ( pszParam && pszParam[0] )
		{
			int iFrameEnd = atoi(pszParam);

			pszParam = strtok(nullptr," ");
			if ( pszParam && pszParam[0] )
			{
				float flFramerate = atof(pszParam);

				pszParam = strtok(nullptr," ");
				if ( pszParam && pszParam[0] )
				{
					bool bWrap = atoi(pszParam) != 0;

					// Got all the parameters. Save 'em and return;
					m_iFrameStart = iFrameStart;
					m_iFrameEnd = iFrameEnd;
					m_flFramerate = flFramerate;
					m_bWrap = bWrap;
					m_nModifyMode = MATERIAL_MODIFY_MODE_ANIM_SEQUENCE;
					m_bNewAnimCommandsSemaphore = !m_bNewAnimCommandsSemaphore;
					return;
				}
			}
		}
	}

	Warning("%s (%s) received StartAnimSequence input without correct parameters. Syntax: <Frame Start> <Frame End> <Frame Rate> <Loop>\nSetting <Frame End> to -1 uses the last frame of the texture. <Loop> should be 1 or 0.\n", GetClassname(), GetDebugName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaterialModifyControl::InputStartFloatLerp( inputdata_t &inputdata )
{
	char parseString[255];
	Q_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

//	if( debug_materialmodifycontrol.GetBool() )//&& Q_stristr( GetDebugName(), "alyx" ) )
//	{
//		DevMsg( 1, "CMaterialModifyControl::InputStartFloatLerp %s %s %s \"%s\"\n", 
//			GetDebugName(), m_szMaterialName.Get(), m_szMaterialVar.Get(), inputdata.value.String() );
//	}

	// Get the start & end values
	char *pszParam = strtok(parseString," ");
	if ( pszParam && pszParam[0] )
	{
		float flStartValue = atof(pszParam);

		pszParam = strtok(nullptr," ");
		if ( pszParam && pszParam[0] )
		{
			float flEndValue = atof(pszParam);

			pszParam = strtok(nullptr," ");
			if ( pszParam && pszParam[0] )
			{
				float flTransitionTime = atof(pszParam);

				pszParam = strtok(nullptr," ");
				if ( pszParam && pszParam[0] )
				{
					bool bWrap = atoi(pszParam) != 0;
					// We don't implement wrap currently.
					bWrap = bWrap;

					// Got all the parameters. Save 'em and return;
					m_flFloatLerpStartValue = flStartValue;
					m_flFloatLerpEndValue = flEndValue;
					m_flFloatLerpTransitionTime = flTransitionTime;
					m_nModifyMode = MATERIAL_MODIFY_MODE_FLOAT_LERP;
					m_bNewAnimCommandsSemaphore = !m_bNewAnimCommandsSemaphore;
					return;
				}
			}
		}
	}

	Warning("%s (%s) received StartFloatLerp input without correct parameters. Syntax: <Start Value> <End Value> <Transition Time> <Loop>\n<Loop> should be 1 or 0.\n", GetClassname(), GetDebugName() );
}
