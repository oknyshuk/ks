// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
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
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "scratchpad_gamedll_helpers.h"
#include "iscratchpad3d.h"
#include "player.h"
#include "collisionproperty.h"

void ScratchPad_DrawWorldToScratchPad(
	IScratchPad3D *pPad,
	unsigned long flags )
{
	pPad->SetRenderState( IScratchPad3D::RS_FillMode, IScratchPad3D::FillMode_Wireframe );

	if ( flags & SPDRAWWORLD_DRAW_WORLD )
	{
		engine->DrawMapToScratchPad( pPad, 0 );
	}

	if ( flags & (SPDRAWWORLD_DRAW_PLAYERS | SPDRAWWORLD_DRAW_ENTITIES) )
	{
		CBaseEntity *pCur = gEntList.FirstEnt();
		while ( pCur )
		{
			bool bPlayer = ( dynamic_cast< CBasePlayer* >( pCur ) != 0 );
			if ( (bPlayer && !( flags & SPDRAWWORLD_DRAW_PLAYERS )) ||
				(!bPlayer && !( flags & SPDRAWWORLD_DRAW_ENTITIES )) )
			{
				pCur = gEntList.NextEnt( pCur );
				continue;
			}

			ScratchPad_DrawEntityToScratchPad( 
				pPad, 
				flags,
				pCur, 
				bPlayer ? Vector( 1.0, 0.5, 0 ) : Vector( 0.3, 0.3, 1.0 )
				);

			pCur = gEntList.NextEnt( pCur );
		}
	}
}


void ScratchPad_DrawEntityToScratchPad(
	IScratchPad3D *pPad,
	unsigned long flags,
	CBaseEntity *pEnt,
	const Vector &vColor )
{
	// Draw the entity's bbox [todo: draw OBBs here too].
	Vector mins, maxs;
	pEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );

	pPad->DrawWireframeBox( mins, maxs, vColor );

	// Draw the edict's index or class?
	char str[512];
	str[0] = 0;
	if ( flags & SPDRAWWORLD_DRAW_EDICT_INDICES )
	{
		char tempStr[512];
		V_snprintf( tempStr, sizeof( tempStr ), "edict: %d", pEnt->entindex() );
		V_strncat( str, tempStr, sizeof( str ), COPY_ALL_CHARACTERS );
	}

	if ( flags & SPDRAWWORLD_DRAW_ENTITY_CLASSNAMES )
	{
		if ( str[0] != 0 )
			V_strncat( str, ", ", sizeof( str ), COPY_ALL_CHARACTERS );

		char tempStr[512];
		V_snprintf( tempStr, sizeof( tempStr ), "class: %s", pEnt->GetClassname() );
		V_strncat( str, tempStr, sizeof( str ), COPY_ALL_CHARACTERS );
	}

	if ( str[0] != 0 )
	{
		CTextParams params;
		params.m_vPos = (mins + maxs) * 0.5f;
		params.m_bCentered = true;
		params.m_flLetterWidth = 2;
		pPad->DrawText( str, params );
	}
}
