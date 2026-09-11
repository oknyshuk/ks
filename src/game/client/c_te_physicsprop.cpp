//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "c_te_legacytempents.h"
#include "tier1/keyvalues.h"
#include "toolframework_client.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Breakable Model TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEPhysicsProp" } ]]
      C_TEPhysicsProp : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPhysicsProp, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEPhysicsProp( void );
	virtual			~C_TEPhysicsProp( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .index = 2 } ]] [[= ks::reflect::Net{ .index = 1 } ]] [[= ks::reflect::Net{ .index = 0 } ]] QAngle			m_angRotation;
	[[= ks::reflect::Net{} ]] Vector			m_vecVelocity;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nSkin;
	[[= ks::reflect::Net{} ]] int				m_nFlags;
	[[= ks::reflect::Net{} ]] int				m_nEffects;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32>{} ]] color32			m_clrRender;
};


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEPhysicsProp, DT_TEPhysicsProp, CTEPhysicsProp )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEPhysicsProp::C_TEPhysicsProp( void )
{
	color32 white = {255, 255, 255, 255};

	m_vecOrigin.Init();
	m_angRotation.Init();
	m_vecVelocity.Init();
	m_nModelIndex		= 0;
	m_nSkin				= 0;
	m_nFlags			= 0;
	m_nEffects			= 0;
	m_clrRender			= white;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEPhysicsProp::~C_TEPhysicsProp( void )
{
}


//-----------------------------------------------------------------------------
// Recording 
//-----------------------------------------------------------------------------
static inline void RecordPhysicsProp( const Vector& start, const QAngle &angles, 
	const Vector& vel, int nModelIndex, int flags, int nSkin, int nEffects, color24 renderColor )
{
	if ( !ToolsEnabled() )
		return;

	if ( clienttools->IsInRecordingMode() )
	{
		const model_t* pModel = (nModelIndex != 0) ? modelinfo->GetModel( nModelIndex ) : nullptr;
		const char *pModelName = pModel ? modelinfo->GetModelName( pModel ) : "";
		Color convertedRenderColor((int)renderColor.r, (int)renderColor.g, (int)renderColor.b);

		KeyValues *msg = new KeyValues( "TempEntity" );

 		msg->SetInt( "te", TE_PHYSICS_PROP );
 		msg->SetString( "name", "TE_PhysicsProp" );
		msg->SetFloat( "time", gpGlobals->curtime );
		msg->SetFloat( "originx", start.x );
		msg->SetFloat( "originy", start.y );
		msg->SetFloat( "originz", start.z );
		msg->SetFloat( "anglesx", angles.x );
		msg->SetFloat( "anglesy", angles.y );
		msg->SetFloat( "anglesz", angles.z );
		msg->SetFloat( "velx", vel.x );
		msg->SetFloat( "vely", vel.y );
		msg->SetFloat( "velz", vel.z );
  		msg->SetString( "model", pModelName );
 		msg->SetInt( "breakmodel", flags );
		msg->SetInt( "skin", nSkin );
		msg->SetInt( "effects", nEffects );
		msg->SetColor( "rendercolor", convertedRenderColor );

		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		msg->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor )
{
	tempents->PhysicsProp( modelindex, skin, pos, angles, vel, flags, effects, renderColor );
	RecordPhysicsProp( pos, angles, vel, modelindex, flags, skin, effects, renderColor );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEPhysicsProp::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEPhysicsProp::PostDataUpdate" );

	color24 clrRenderConverted;
	clrRenderConverted.r = m_clrRender.r;
	clrRenderConverted.g = m_clrRender.g;
	clrRenderConverted.b = m_clrRender.b;

	tempents->PhysicsProp( m_nModelIndex, m_nSkin, m_vecOrigin, m_angRotation, m_vecVelocity, m_nFlags, m_nEffects, clrRenderConverted );
	RecordPhysicsProp( m_vecOrigin, m_angRotation, m_vecVelocity, m_nModelIndex, m_nFlags, m_nSkin, m_nEffects, clrRenderConverted );
}

void TE_PhysicsProp( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecVel;
	QAngle angles;
	int nSkin;
	nSkin = pKeyValues->GetInt( "skin", 0 );
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	angles.x = pKeyValues->GetFloat( "anglesx" );
	angles.y = pKeyValues->GetFloat( "anglesy" );
	angles.z = pKeyValues->GetFloat( "anglesz" );
	vecVel.x = pKeyValues->GetFloat( "velx" );
	vecVel.y = pKeyValues->GetFloat( "vely" );
	vecVel.z = pKeyValues->GetFloat( "velz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	int flags = pKeyValues->GetInt( "breakmodel" );
	int nEffects = pKeyValues->GetInt( "effects" );
	Color renderColor = pKeyValues->GetColor( "rendercolor" );
	color24 convertedRenderColor;
	convertedRenderColor.r = (byte)renderColor.r();
	convertedRenderColor.g = (byte)renderColor.g();
	convertedRenderColor.b = (byte)renderColor.b();

	TE_PhysicsProp( filter, delay, nModelIndex, nSkin, vecOrigin, angles, vecVel, flags, nEffects, convertedRenderColor );
}

