// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
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
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
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
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseanimating.h"
#include "SkyCamera.h"
#include "studio.h"

// HACK HACK:  Must match cl_dll/cl_animevent.h!!!!
#define CL_EVENT_SPRITEGROUP_CREATE		6002

//-----------------------------------------------------------------------------
// An entity which emits other entities at points 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvParticleScript" } ]]
      CEnvParticleScript : public CBaseAnimating
{
public:
	DECLARE_CLASS( CEnvParticleScript, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CEnvParticleScript();

	virtual void Precache();
	virtual void Spawn();
	virtual void Activate();
	virtual int  UpdateTransmitState();

	[[= ks::reflect::Input{ .name = "SetSequence", .type = FIELD_STRING } ]] void InputSetSequence( inputdata_t &inputdata );

private:

	void	PrecacheAnimationEventMaterials();

	CNetworkVar( float, m_flSequenceScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};


//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CEnvParticleScript )

LINK_ENTITY_TO_CLASS( env_particlescript, CEnvParticleScript );


//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( CEnvParticleScript, DT_EnvParticleScript )


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEnvParticleScript::CEnvParticleScript()
{
	UseClientSideAnimation();
}


void CEnvParticleScript::PrecacheAnimationEventMaterials()
{
	CStudioHdr *hdr = GetModelPtr();
	if ( hdr )
	{
		int numseq = hdr->GetNumSeq();
		for ( int i = 0; i < numseq; ++i )
		{
			mstudioseqdesc_t& seqdesc = hdr->pSeqdesc( i );
			int ecount = seqdesc.numevents;
			for ( int j = 0 ; j < ecount; ++j )
			{
				const mstudioevent_t* event = (const mstudioevent_for_client_server_t*)seqdesc.pEvent( j );
				if ( event->Event() == CL_EVENT_SPRITEGROUP_CREATE )
				{
					char pAttachmentName[256];
					char pSpriteName[256];
					int nArgs = std::sscanf( event->pszOptions(), "%255s %255s", pAttachmentName, pSpriteName );
					if ( nArgs == 2 )
					{
						PrecacheMaterial( pSpriteName );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CEnvParticleScript::Precache()
{
	BaseClass::Precache();
	PrecacheModel( STRING( GetModelName() ) );
	
	// We need a model for its animation sequences even though we don't render it
	SetModel( STRING( GetModelName() ) );

	PrecacheAnimationEventMaterials();
}


//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEnvParticleScript::Spawn()
{
	Precache();
	BaseClass::Spawn();
	AddEffects( EF_NOSHADOW );
	// We need a model for its animation sequences even though we don't render it
	SetModel( STRING( GetModelName() ) );
}


//-----------------------------------------------------------------------------
// Activate
//-----------------------------------------------------------------------------
void CEnvParticleScript::Activate()
{
	BaseClass::Activate();

	DetectInSkybox();
	CSkyCamera *pCamera = GetEntitySkybox();
	if ( pCamera )
	{
		float flSkyboxScale = pCamera->m_skyboxData.scale;
		if ( flSkyboxScale == 0.0f )
		{
			flSkyboxScale = 1.0f;
		}

		m_flSequenceScale = flSkyboxScale;
	}
	else
	{
		m_flSequenceScale = 1.0f;
	}

	m_flPlaybackRate = 1.0f;
}

//-----------------------------------------------------------------------------
// Should we transmit it to the client?
//-----------------------------------------------------------------------------
int CEnvParticleScript::UpdateTransmitState()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	if ( IsEFlagSet( EFL_IN_SKYBOX ) )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_PVSCHECK );
}


//-----------------------------------------------------------------------------
// Purpose: Input that sets the sequence of the entity
//-----------------------------------------------------------------------------
void CEnvParticleScript::InputSetSequence( inputdata_t &inputdata )
{
	if ( inputdata.value.StringID() != NULL_STRING )
	{
		int nSequence = LookupSequence( STRING( inputdata.value.StringID() ) );
		if ( nSequence != ACT_INVALID )
		{
			SetSequence( nSequence );
		}
	}
}
