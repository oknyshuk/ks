// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Simple entity to switch the map's 2D skybox texture when triggered.

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
#include "reflect_datamap.h"
#include "reflect_annotations.h"

//-----------------------------------------------------------------------------
// CSkyboxSwapper
//-----------------------------------------------------------------------------
class CSkyboxSwapper : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CSkyboxSwapper, CServerOnlyPointEntity );
public:
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void Precache_Impl( void );

	[[= ks::reflect::Input{ .name = "Trigger", .type = FIELD_VOID } ]] void InputTrigger( inputdata_t &inputdata );

protected:
	[[= ks::reflect::Key{ .name = "SkyboxName" } ]] string_t m_iszSkyboxName;
};

LINK_ENTITY_TO_CLASS(skybox_swapper, CSkyboxSwapper);

IMPLEMENT_REFLECT_DATAMAP( CSkyboxSwapper )


//-----------------------------------------------------------------------------
// Purpose: not much
//-----------------------------------------------------------------------------
void CSkyboxSwapper::Spawn_Impl( void )
{
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Precache the skybox materials.
//-----------------------------------------------------------------------------
void CSkyboxSwapper::Precache_Impl( void )
{
	if ( V_strlen( m_iszSkyboxName.ToCStr() ) == 0 )
	{
		Warning( "skybox_swapper (%s) has no skybox specified!\n", GetEntityName().ToCStr() );
		return;
	}

	char  name[ MAX_PATH ];
	const char *skyboxsuffix[ 6 ] = { "rt", "bk", "lf", "ft", "up", "dn" };
	for ( int i = 0; i < 6; i++ )
	{
		V_snprintf( name, sizeof( name ), "skybox/%s%s", m_iszSkyboxName.ToCStr(), skyboxsuffix[i] );
		PrecacheMaterial( name );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the skybox swap.
//-----------------------------------------------------------------------------
void CSkyboxSwapper::InputTrigger( inputdata_t &inputdata )
{
	static ConVarRef skyname( "sv_skyname", false );
	if ( !skyname.IsValid() )
	{
		Warning( "skybox_swapper (%s) trigger input failed - cannot find 'sv_skyname' convar!\n", GetEntityName().ToCStr() );
		return;
	}
	skyname.SetValue( m_iszSkyboxName.ToCStr() );
}
