// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.tier1;
import std;
#include "const.h"
#include "string_t.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "npcevent.h"
#include "debugoverlay_shared.h"
#include "fx_envelope.h"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_EnvelopeFX::C_EnvelopeFX( void )
{
	m_active = false;
}

C_EnvelopeFX::~C_EnvelopeFX()
{
	RemoveRenderable();
}

const matrix3x4_t & C_EnvelopeFX::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	SetIdentityMatrix( mat );
	PositionMatrix( GetRenderOrigin(), mat );
	return mat;
}

void C_EnvelopeFX::RemoveRenderable()
{
	ClientLeafSystem()->RemoveRenderable( m_hRenderHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the envelope being in the client's known entity list
//-----------------------------------------------------------------------------
void C_EnvelopeFX::Update( void )
{
	if ( m_active )
	{
		if ( m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE )
		{
			ClientLeafSystem()->AddRenderable( this, false, RENDERABLE_IS_TRANSLUCENT, RENDERABLE_MODEL_ENTITY );
		}
		else
		{
			ClientLeafSystem()->RenderableChanged( m_hRenderHandle );
		}
	}
	else
	{
		RemoveRenderable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts up the effect
// Input  : entityIndex - entity to be attached to
//			attachment - attachment point (if any) to be attached to
//-----------------------------------------------------------------------------
void C_EnvelopeFX::EffectInit( int entityIndex, int attachment )
{
	m_entityIndex = entityIndex;
	m_attachment = attachment;

	m_active = 1;
	m_t = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the effect
//-----------------------------------------------------------------------------
void C_EnvelopeFX::EffectShutdown( void ) 
{
	m_active = 0;
	m_t = 0;
}
