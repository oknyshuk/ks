// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "dlight.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "icliententity.h"
#include "model_types.h"
#include "tier2/resourceprecacher.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "client_class.h"
#include "declare_class.h"
#include "irecipientfilter.h"
#include "reflect_annotations.h"
#include "reflect_recvtable.h"
#include "engine/ivmodelinfo.h"
#include "engine/decal_flags.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "usermessages_schema.h"
#include "imaterialproxydict.h"
#include "const.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.core.cmdline;
import ks.core.dbg;
import ks.core.random;
import ks.core.types;
import ks.core.vprof;
import ks.game.tempentity;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vector2d;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.fixed_pool;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.intrusive_ptr;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :props;
import :iinput;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import :util;
import :particlemgr;
import :precache;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :noopanimcontroller;
import ks.core.color;
import ks.core.wchartypes;
import ks.tier1.keyvalues;
import ks.game.cstrike15.playerdecals_signature;
import ks.tier1.fmtstr;
import ks.tier1.interfaces;
import ks.tier1.strtools;
import ks.vstdlib.callqueue;
import ks.vstdlib.jobthread;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;

class CEffectData;
class CBaseEntity;
class CBasePlayer;
class CLocalTempEntity;
class SimpleParticle;

using namespace ::ks::reflect;

extern ConVar muzzleflash_light;

static constexpr auto TENT_WIND_ACCEL = 50;

color24 white24 = {255, 255, 255};

//Precache the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectMuzzleFlash )

	PRECACHE( MATERIAL, "effects/muzzleflash1" )
	PRECACHE( MATERIAL, "effects/muzzleflash2" )
	PRECACHE( MATERIAL, "effects/muzzleflash3" )
	PRECACHE( MATERIAL, "effects/muzzleflash4" )
	PRECACHE( MATERIAL, "effects/muzzleflash1_noz" )
	PRECACHE( MATERIAL, "effects/muzzleflash2_noz" )
	PRECACHE( MATERIAL, "effects/muzzleflash3_noz" )
	PRECACHE( MATERIAL, "effects/muzzleflash4_noz" )

PRECACHE_REGISTER_END()

ConVar func_break_max_pieces( side_registry(), "func_break_max_pieces", "15", FCVAR_ARCHIVE | FCVAR_REPLICATED );

ConVar cl_fasttempentcollision( side_registry(), "cl_fasttempentcollision", "5" );

// Temp entity interface
static CTempEnts g_TempEnts;
// Expose to rest of the client .dll
ITempEnts *tempents = ( ITempEnts * )&g_TempEnts;

CLocalTempEntity::CLocalTempEntity()
{
	m_vecTempEntAcceleration.Init();
	m_pfnDrawHelper = 0;
	m_pszImpactEffect = nullptr;
}

static constexpr auto TE_RIFLE_SHELL = 1024;
static constexpr auto TE_PISTOL_SHELL = 2048;
static constexpr auto TE_SHOTGUN_SHELL = 4096;

//-----------------------------------------------------------------------------
// Purpose: Prepare a temp entity for creation
// Input  : time - 
//			*model - 
//-----------------------------------------------------------------------------
void CLocalTempEntity::Prepare( model_t *pmodel, float time )
{
	Interp_SetupMappings( GetVarMapping() );

	index = -1;
	Clear();

	// Use these to set per-frame and termination conditions / actions
	flags = FTENT_NONE;		
	die = time + 0.75;
	SetModelPointer( pmodel );
	SetRenderMode( kRenderNormal );
	SetRenderFX( kRenderFxNone );
	SetBody( 0 );
	SetSkin( 0 ); 
	fadeSpeed = 0.5;
	hitSound = 0;
	clientIndex = -1;
	bounceFactor = 1;
	m_nFlickerFrame = 0;
	m_bParticleCollision = false;
}

//-----------------------------------------------------------------------------
// Sets the velocity
//-----------------------------------------------------------------------------
void CLocalTempEntity::SetVelocity( const Vector &vecVelocity )
{
	m_vecTempEntVelocity = vecVelocity;
}

//-----------------------------------------------------------------------------
// Sets the velocity
//-----------------------------------------------------------------------------
void CLocalTempEntity::SetAcceleration( const Vector &vecVelocity )
{
	m_vecTempEntAcceleration = vecVelocity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CLocalTempEntity::DrawStudioModel( int flags )
{
	CVProfScope _( "CLocalTempEntity::DrawStudioModel", 0, VPROF_BUDGETGROUP_MODEL_RENDERING, false, BUDGETFLAG_OTHER );
	int drawn = 0;

	if ( !GetModel() || modelinfo->GetModelType( GetModel() ) != mod_studio )
		return drawn;
	
	// Make sure m_pstudiohdr is valid for drawing
	MDLCACHE_CRITICAL_SECTION();
	if ( !GetModelPtr() )
		return drawn;

	if ( m_pfnDrawHelper )
	{
		drawn = ( *m_pfnDrawHelper )( this, flags );
	}
	else
	{
		drawn = modelrender->DrawModel( 
			flags, 
			this,
			MODEL_INSTANCE_INVALID,
			index, 
			GetModel(),
			GetAbsOrigin(),
			GetAbsAngles(),
			GetSkin(),
			GetBody(),
			m_nHitboxSet );
	}
	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
//-----------------------------------------------------------------------------
int	CLocalTempEntity::DrawModel( int flags, const RenderableInstance_t &instance )
{
	int drawn = 0;

	if ( !GetModel() )
	{
		return drawn;
	}

	if ( this->flags & FTENT_BEOCCLUDED )
	{
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		// Check normal
		Vector vecDelta = (GetAbsOrigin() - MainViewOrigin(nSlot));
		VectorNormalize( vecDelta );
		float flDot = DotProduct( m_vecNormal, vecDelta );
		if ( flDot > 0 )
		{
			float flAlpha = RemapVal( std::min<std::common_type_t<float, double>>(flDot,0.3), 0, 0.3, 0, 1 );
			flAlpha = std::max<std::common_type_t<double, float>>( 1.0, tempent_renderamt - (tempent_renderamt * flAlpha) );
			SetRenderAlpha( flAlpha );
		}
	}

	switch ( modelinfo->GetModelType( GetModel() ) )
	{
	case mod_sprite:
		drawn = DrawSprite( 
			this,
			GetModel(), 
			GetAbsOrigin(), 
			GetAbsAngles(), 
			m_flFrame,  // sprite frame to render
			GetBody() > 0 ? cl_entitylist->GetBaseEntity( GetBody() ) : nullptr,  // attach to
			GetSkin(),  // attachment point
			GetRenderMode(), // rendermode
			GetRenderFX(), // renderfx
			GetRenderAlpha(), // alpha
			GetRenderColorR(),
			GetRenderColorG(),
			GetRenderColorB(),
			m_flSpriteScale		  // sprite scale
			);
		break;
	case mod_studio:
		drawn = DrawStudioModel( flags );
		break;
	default:
		break;
	}

	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CLocalTempEntity::IsActive( void )
{
	bool active = true;

	float life = die - gpGlobals->curtime;
	
	if ( life < 0 )
	{
		if ( flags & FTENT_FADEOUT )
		{
			int alpha;
			if (GetRenderMode() == kRenderNormal)
			{
				SetRenderMode( kRenderTransTexture );
			}
			
			alpha = tempent_renderamt * ( 1 + life * fadeSpeed );
			
			if ( alpha <= 0 )
			{
				active = false;
				alpha = 0;
			}

			SetRenderAlpha( alpha );
		}
		else 
		{
			active = false;
		}
	}

	// Never die tempents only die when their die is cleared
	if ( flags & FTENT_NEVERDIE )
	{
		active = (die != 0);
	}

	return active;
}

bool CLocalTempEntity::Frame( float frametime, int framenumber )
{
	float fastFreq = gpGlobals->curtime * 5.5;
	float gravity = -frametime * sv_gravity.GetFloat();
	float gravitySlow = gravity * 0.5;
	float traceFraction = 1;

	contract_assert( !GetMoveParent() );

	m_vecPrevLocalOrigin = GetLocalOrigin();

	m_vecTempEntVelocity = m_vecTempEntVelocity + ( m_vecTempEntAcceleration * frametime );

	if ( flags & FTENT_PLYRATTACHMENT )
	{
		if ( IClientEntity *pClient = cl_entitylist->GetClientEntity( clientIndex ) )
		{
			SetLocalOrigin( pClient->GetAbsOrigin() + tentOffset );
		}
	}
	else if ( flags & FTENT_SINEWAVE )
	{
		x += m_vecTempEntVelocity[0] * frametime;
		y += m_vecTempEntVelocity[1] * frametime;

		SetLocalOrigin( Vector(
			x + std::sin( m_vecTempEntVelocity[2] + gpGlobals->curtime /* * anim.prevframe */ ) * (10*m_flSpriteScale),
			y + sin( m_vecTempEntVelocity[2] + fastFreq + 0.7 ) * (8*m_flSpriteScale),
			GetLocalOriginDim( Z_INDEX ) + m_vecTempEntVelocity[2] * frametime ) );
	}
	else if ( flags & FTENT_SPIRAL )
	{
		float s, c;
		SinCos( m_vecTempEntVelocity[2] + fastFreq, &s, &c );

		SetLocalOrigin( GetLocalOrigin() + Vector(
			m_vecTempEntVelocity[0] * frametime + 8 * std::sin( gpGlobals->curtime * 20 ),
			m_vecTempEntVelocity[1] * frametime + 4 * sin( gpGlobals->curtime * 30 ),
			m_vecTempEntVelocity[2] * frametime ) );
	}
	else
	{
		SetLocalOrigin( GetLocalOrigin() + m_vecTempEntVelocity * frametime );
	}
	
	if ( flags & FTENT_SPRANIMATE )
	{
		m_flFrame += frametime * m_flFrameRate;
		if ( m_flFrame >= m_flFrameMax )
		{
			m_flFrame = m_flFrame - (int)(m_flFrame);

			if ( !(flags & FTENT_SPRANIMATELOOP) )
			{
				// this animating sprite isn't set to loop, so destroy it.
				die = 0.0f;
				return false;
			}
		}
	}
	else if ( flags & FTENT_SPRCYCLE )
	{
		m_flFrame += frametime * 10;
		if ( m_flFrame >= m_flFrameMax )
		{
			m_flFrame = m_flFrame - (int)(m_flFrame);
		}
	}

	if ( flags & FTENT_SMOKEGROWANDFADE )
	{
		m_flSpriteScale += frametime * 0.5f;
		//m_clrRender.a -= frametime * 1500;
	}

	if ( flags & FTENT_ROTATE )
	{
		SetLocalAngles( GetLocalAngles() + m_vecTempEntAngVelocity * frametime );
	}
	else if ( flags & FTENT_ALIGNTOMOTION )
	{
		if ( m_vecTempEntVelocity.Length() > 0.0f )
		{
			QAngle angles;
			VectorAngles( m_vecTempEntVelocity, angles );
			SetAbsAngles( angles );
		}
	}

	if ( flags & (FTENT_COLLIDEALL | FTENT_COLLIDEWORLD) )
	{
		Vector	traceNormal;
		traceNormal.Init();
		bool bShouldCollide = true;

		trace_t trace;

		if ( flags & FTENT_COLLIDEALL )
		{
			Vector vPrevOrigin = m_vecPrevLocalOrigin;

			if ( cl_fasttempentcollision.GetInt() > 0 && flags & FTENT_USEFASTCOLLISIONS )
			{
				if ( m_iLastCollisionFrame + cl_fasttempentcollision.GetInt() > gpGlobals->framecount )
				{
					bShouldCollide = false;
				}
				else
				{
					if ( m_vLastCollisionOrigin != vec3_origin )
					{
						vPrevOrigin = m_vLastCollisionOrigin;
					}

					m_iLastCollisionFrame = gpGlobals->framecount;
					bShouldCollide = true; 
				}
			}

			if ( bShouldCollide == true )
			{
				// If the FTENT_COLLISIONGROUP flag is set, use the entity's collision group
				int collisionGroup = COLLISION_GROUP_NONE;
				if ( flags & FTENT_COLLISIONGROUP )
				{
					collisionGroup = GetCollisionGroup();
				}

				UTIL_TraceLine( vPrevOrigin, GetLocalOrigin(), MASK_SOLID, GetOwnerEntity(), collisionGroup, &trace );

				// Make sure it didn't bump into itself... (?!?)
				if  ( 
					(trace.fraction != 1) && 
						( (TraceDidHitWorld( trace )) || 
						  (trace.Ent<CBaseEntity>() != ClientEntityList().GetEnt(clientIndex)) ) 
					)
				{
					traceFraction = trace.fraction;
					VectorCopy( trace.plane.normal, traceNormal );
				}

				m_vLastCollisionOrigin = trace.endpos;
			}
		}
		else if ( flags & FTENT_COLLIDEWORLD )
		{
			CTraceFilterWorldOnly traceFilter;
			UTIL_TraceLine( m_vecPrevLocalOrigin, GetLocalOrigin(), MASK_SOLID, &traceFilter, &trace );
			if ( trace.fraction != 1 )
			{
				traceFraction = trace.fraction;
				VectorCopy( trace.plane.normal, traceNormal );
			}
		}
		
		if ( traceFraction != 1  )	// Decent collision now, and damping works
		{
			float  proj, damp;
			SetLocalOrigin( trace.endpos );
			
			// Damp velocity
			damp = bounceFactor;
			if ( flags & (FTENT_GRAVITY|FTENT_SLOWGRAVITY) )
			{
				damp *= 0.5;
				if ( traceNormal[2] > 0.9 )		// Hit floor?
				{
					if ( m_vecTempEntVelocity[2] <= 0 && m_vecTempEntVelocity[2] >= gravity*3 )
					{
						damp = 0;		// Stop
						flags &= ~(FTENT_ROTATE|FTENT_GRAVITY|FTENT_SLOWGRAVITY|FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL);
						SetLocalAnglesDim( X_INDEX, 0 );
						SetLocalAnglesDim( Z_INDEX, 0 );
					}
				}
			}

			if ( flags & (FTENT_CHANGERENDERONCOLLIDE) )
			{
				OnTranslucencyTypeChanged();
				flags &= ~FTENT_CHANGERENDERONCOLLIDE;
			}	

			if (hitSound)
			{
				tempents->PlaySound(this, damp);
			}

			if ( m_pszImpactEffect )
			{
				CEffectData data;
				//data.m_vOrigin = newOrigin;
				data.m_vOrigin = trace.endpos;
				data.m_vStart = trace.startpos;
				data.m_nSurfaceProp = trace.surface.surfaceProps;
				data.m_nHitBox = trace.hitbox;

				data.m_nDamageType = TEAM_UNASSIGNED;

				IClientNetworkable *pClient = cl_entitylist->GetClientEntity( clientIndex );

				if ( pClient )
				{
					CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>(pClient);
					if( pPlayer )
					{
						data.m_nDamageType = pPlayer->GetTeamNumber();
					}
				}

				if ( trace.Ent<CBaseEntity>() )
				{
					data.m_hEntity = ClientEntityList().EntIndexToHandle( trace.Ent<CBaseEntity>()->entindex() );
				}
				DispatchEffect( m_pszImpactEffect, data );
			}

			// Check for a collision and stop the particle system.
			if ( flags & FTENT_CLIENTSIDEPARTICLES )
			{
				// Stop the emission of particles on collision - removed from the ClientEntityList on removal from the tempent pool.
				ParticleProp()->StopEmission();
				m_bParticleCollision = true;
			}

			if (flags & FTENT_COLLIDEKILL)
			{
				// die on impact
				flags &= ~FTENT_FADEOUT;	
				die = gpGlobals->curtime;			
			}
			else if ( flags & FTENT_ATTACHTOTARGET)
			{
				// If we've hit the world, just stop moving
				if ( TraceDidHitWorld( trace ) && !( trace.surface.flags & SURF_SKY ) )
				{
					m_vecTempEntVelocity = vec3_origin;
					m_vecTempEntAcceleration = vec3_origin;

					// Remove movement flags so we don't keep tracing
					flags &= ~(FTENT_COLLIDEALL | FTENT_COLLIDEWORLD);
				}
				else
				{
					// Couldn't attach to this entity. Die.
					flags &= ~FTENT_FADEOUT;
					die = gpGlobals->curtime;
				}
			}
			else
			{
				// Reflect velocity
				if ( damp != 0 )
				{
					proj = ((Vector)m_vecTempEntVelocity).Dot(traceNormal);
					VectorMA( m_vecTempEntVelocity, -proj*2, traceNormal, m_vecTempEntVelocity );
					// Reflect rotation (fake)
					SetLocalAnglesDim( Y_INDEX, -GetLocalAnglesDim( Y_INDEX ) );
				}
				
				if ( damp != 1 )
				{
					VectorScale( m_vecTempEntVelocity, damp, m_vecTempEntVelocity );
					SetLocalAngles( GetLocalAngles() * 0.9 );
				}
			}
		}
	}

	if ( (flags & FTENT_FLICKER) && framenumber == m_nFlickerFrame )
	{
		dlight_t *dl = effects->CL_AllocDlight (LIGHT_INDEX_TE_DYNAMIC);
		VectorCopy (GetLocalOrigin(), dl->origin);
		dl->radius = 60;
		dl->color.r = 255;
		dl->color.g = 120;
		dl->color.b = 0;
		dl->die = gpGlobals->curtime + 0.01;
	}

	if ( flags & FTENT_SMOKETRAIL )
	{
		 contract_assert( !"FIXME:  Rework smoketrail to be client side\n" );
	}

	// add gravity if we didn't collide in this frame
	if ( traceFraction == 1 )
	{
		if ( flags & FTENT_GRAVITY )
			m_vecTempEntVelocity[2] += gravity;
		else if ( flags & FTENT_SLOWGRAVITY )
			m_vecTempEntVelocity[2] += gravitySlow;
	}

	if ( flags & FTENT_WINDBLOWN )
	{
		Vector vecWind;
		GetWindspeedAtTime( gpGlobals->curtime, vecWind );

		for ( int i = 0 ; i < 2 ; i++ )
		{
			if ( m_vecTempEntVelocity[i] < vecWind[i] )
			{
				m_vecTempEntVelocity[i] += ( frametime * TENT_WIND_ACCEL );

				// clamp
				if ( m_vecTempEntVelocity[i] > vecWind[i] )
					m_vecTempEntVelocity[i] = vecWind[i];
			}
			else if (m_vecTempEntVelocity[i] > vecWind[i] )
			{
				m_vecTempEntVelocity[i] -= ( frametime * TENT_WIND_ACCEL );

				// clamp.
				if ( m_vecTempEntVelocity[i] < vecWind[i] )
					m_vecTempEntVelocity[i] = vecWind[i];
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Attach a particle effect to a temp entity.
//-----------------------------------------------------------------------------
void CLocalTempEntity::AddParticleEffect( const char *pszParticleEffect )
{
	// Do we have a valid particle effect.
	if ( !pszParticleEffect || ( pszParticleEffect[0] == '\0' ) )
		return;

	// Check to see that we don't already have a particle effect.
	if ( ( flags & FTENT_CLIENTSIDEPARTICLES ) != 0 )
		return;

	// Add the entity to the ClientEntityList and create the particle system.
	ClientEntityList().AddNonNetworkableEntity( this );
	ParticleProp()->Create( pszParticleEffect, PATTACH_ABSORIGIN_FOLLOW );

	// Set the particle flag on the temp entity and save the name of the particle effect.
	flags |= FTENT_CLIENTSIDEPARTICLES;
	SetParticleEffect( pszParticleEffect );
}

//-----------------------------------------------------------------------------
// Purpose: This helper keeps track of batches of "breakmodels" so that they can all share the lighting origin
//  of the first of the group (because the server sends down 15 chunks at a time, and rebuilding 15 light cache
//  entries for a map with a lot of worldlights is really slow).
//-----------------------------------------------------------------------------
class CBreakableHelper
{
public:
	void	Insert( CLocalTempEntity *entity, bool isSlave );
	void	Remove( CLocalTempEntity *entity );

	void	Clear();

	const Vector *GetLightingOrigin( CLocalTempEntity *entity );

private:

	// A context is the first master until the next one, which starts a new context
	struct BreakableList_t
	{
		unsigned int		context;
		CLocalTempEntity	*entity;
	};

	std::vector< BreakableList_t >	m_Breakables;
	unsigned int			m_nCurrentContext;
};

//-----------------------------------------------------------------------------
// Purpose: Adds brekable to list, starts new context if needed
// Input  : *entity - 
//			isSlave - 
//-----------------------------------------------------------------------------
void CBreakableHelper::Insert( CLocalTempEntity *entity, bool isSlave )
{
	// A master signifies the start of a new run of broken objects
	if ( !isSlave )
	{
		++m_nCurrentContext;
	}
	
	BreakableList_t entry;
	entry.context = m_nCurrentContext;
	entry.entity = entity;

	m_Breakables.push_back( entry );
}

//-----------------------------------------------------------------------------
// Purpose: Removes all instances of entity in the list
// Input  : *entity - 
//-----------------------------------------------------------------------------
void CBreakableHelper::Remove( CLocalTempEntity *entity )
{
	std::erase_if( m_Breakables, [entity]( const BreakableList_t &e ) { return e.entity == entity; } );
}

//-----------------------------------------------------------------------------
// Purpose: For a given breakable, find the "first" or head object and use it's current
//  origin as the lighting origin for the entire group of objects
// Input  : *entity - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector *CBreakableHelper::GetLightingOrigin( CLocalTempEntity *entity )
{
	unsigned int nCurContext = 0;
	CLocalTempEntity *head = nullptr;
	for ( BreakableList_t &e : m_Breakables )
	{
		if ( e.context != nCurContext )
		{
			nCurContext = e.context;
			head = e.entity;
		}

		if ( e.entity == entity )
		{
			contract_assert( head );
			return head ? &head->GetAbsOrigin() : nullptr;
		}
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Wipe breakable helper list
// Input  :  - 
//-----------------------------------------------------------------------------
void CBreakableHelper::Clear()
{
	m_Breakables.clear();
	m_nCurrentContext = 0;
}

static CBreakableHelper g_BreakableHelper;

//-----------------------------------------------------------------------------
// Purpose: See if it's in the breakable helper list and, if so, remove
// Input  :  - 
//-----------------------------------------------------------------------------
void CLocalTempEntity::OnRemoveTempEntity()
{
	g_BreakableHelper.Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTempEnts::CTempEnts( void ) :
	m_TempEntsPool( ( MAX_TEMP_ENTITIES / 20 ), ks::fixed_pool::grow::slow, 16 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTempEnts::~CTempEnts( void )
{
	m_TempEntsPool.clear();
	m_TempEnts.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Create a fizz effect
// Input  : *pent - 
//			modelIndex - 
//			density - 
//-----------------------------------------------------------------------------
void CTempEnts::FizzEffect( CBaseEntity *pent, int modelIndex, int density, int current )
{
	CLocalTempEntity		*pTemp;
	const model_t	*model;
	int				i, width, depth, count, frameCount;
	float			maxHeight, speed, xspeed, yspeed;
	Vector			origin;
	Vector			mins, maxs;

	if ( !pent->GetModel() || !modelIndex ) 
		return;

	model = modelinfo->GetModel( modelIndex );
	if ( !model )
		return;

	count = density + 1;
	density = count * 3 + 6;

	modelinfo->GetModelBounds( pent->GetModel(), mins, maxs );

	maxHeight = maxs[2] - mins[2];
	width = maxs[0] - mins[0];
	depth = maxs[1] - mins[1];
	speed = current;

	SinCos( pent->GetLocalAngles()[1]*std::numbers::pi/180, &yspeed, &xspeed );
	xspeed *= speed;
	yspeed *= speed;
	frameCount = modelinfo->GetModelFrameCount( model );

	for (i=0 ; i<count ; i++)
	{
		origin[0] = mins[0] + random_valve->RandomInt(0,width-1);
		origin[1] = mins[1] + random_valve->RandomInt(0,depth-1);
		origin[2] = mins[2];
		pTemp = TempEntAlloc( origin, (model_t *)model );
		if (!pTemp)
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		float zspeed = random_valve->RandomInt(80,140);
		pTemp->SetVelocity( Vector(xspeed, yspeed, zspeed) );
		pTemp->die = gpGlobals->curtime + (maxHeight / zspeed) - 0.1;
		pTemp->m_flFrame = random_valve->RandomInt(0,frameCount-1);
		// Set sprite scale
		pTemp->m_flSpriteScale = 1.0 / random_valve->RandomFloat(2,5);
		pTemp->SetRenderMode( kRenderTransAlpha );
		pTemp->SetRenderAlpha( 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubbles
// Input  : *mins - 
//			*maxs - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void CTempEnts::Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed )
{
	CLocalTempEntity			*pTemp;
	const model_t		*model;
	int					i, frameCount;
	float				sine, cosine;
	Vector				origin;

	if ( !modelIndex ) 
		return;

	model = modelinfo->GetModel( modelIndex );
	if ( !model )
		return;

	frameCount = modelinfo->GetModelFrameCount( model );

	for (i=0 ; i<count ; i++)
	{
		origin[0] = random_valve->RandomInt( mins[0], maxs[0] );
		origin[1] = random_valve->RandomInt( mins[1], maxs[1] );
		origin[2] = random_valve->RandomInt( mins[2], maxs[2] );
		pTemp = TempEntAlloc( origin, ( model_t * )model );
		if (!pTemp)
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		SinCos( random_valve->RandomInt( -std::numbers::pi, std::numbers::pi ), &sine, &cosine );
		
		float zspeed = random_valve->RandomInt(80,140);
		pTemp->SetVelocity( Vector(speed * cosine, speed * sine, zspeed) );
		pTemp->die = gpGlobals->curtime + ((height - (origin[2] - mins[2])) / zspeed) - 0.1;
		pTemp->m_flFrame = random_valve->RandomInt( 0, frameCount-1 );
		
		// Set sprite scale
		pTemp->m_flSpriteScale = 1.0 / random_valve->RandomFloat(4,16);
		pTemp->SetRenderMode( kRenderTransAlpha );
		
		pTemp->SetRenderColor( 255, 255, 255 );
		pTemp->SetRenderAlpha( 192 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubble trail
// Input  : *start - 
//			*end - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void CTempEnts::BubbleTrail( const Vector &start, const Vector &end, float flWaterZ, int modelIndex, int count, float speed )
{
	CLocalTempEntity			*pTemp;
	const model_t		*model;
	int					i, frameCount;
	float				dist, angle;
	Vector				origin;

	if ( !modelIndex ) 
		return;

	model = modelinfo->GetModel( modelIndex );
	if ( !model )
		return;

	frameCount = modelinfo->GetModelFrameCount( model );

	for (i=0 ; i<count ; i++)
	{
		dist = random_valve->RandomFloat( 0, 1.0 );
		VectorLerp( start, end, dist, origin );
		pTemp = TempEntAlloc( origin, ( model_t * )model );
		if (!pTemp)
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = random_valve->RandomInt( -std::numbers::pi, std::numbers::pi );

		float zspeed = random_valve->RandomInt(80,140);
		pTemp->SetVelocity( Vector(speed * std::cos(angle), speed * std::sin(angle), zspeed) );
		pTemp->die = gpGlobals->curtime + ((flWaterZ - origin[2]) / zspeed) - 0.1;
		pTemp->m_flFrame = random_valve->RandomInt(0,frameCount-1);
		// Set sprite scale
		pTemp->m_flSpriteScale = 1.0 / random_valve->RandomFloat(4,8);
		pTemp->SetRenderMode( kRenderTransAlpha );
		
		pTemp->SetRenderColor( 255, 255, 255 );
		pTemp->SetRenderAlpha( 192 );
	}
}

static constexpr auto SHARD_VOLUME = 12.0; // on shard ever n^3 units

//-----------------------------------------------------------------------------
// Purpose: Only used by BreakModel temp ents for now.  Allows them to share a single
//  lighting origin amongst a group of objects.  If the master object goes away, the next object
//  in the group becomes the new lighting origin, etc.
// Input  : *entity - 
//			flags - 
// Output : int
//-----------------------------------------------------------------------------
int BreakModelDrawHelper( CLocalTempEntity *entity, int flags )
{
	ModelRenderInfo_t sInfo;
	sInfo.flags = flags;
	sInfo.pRenderable = entity;
	sInfo.instance = MODEL_INSTANCE_INVALID;
	sInfo.entity_index = entity->index;
	sInfo.pModel = entity->GetModel();
	sInfo.origin = entity->GetRenderOrigin();
	sInfo.angles = entity->GetRenderAngles();
	sInfo.skin = entity->GetSkin();
	sInfo.body = entity->GetBody();
	sInfo.hitboxset = entity->m_nHitboxSet;

	// This is the main change, look up a lighting origin from the helper singleton
	const Vector *pLightingOrigin = g_BreakableHelper.GetLightingOrigin( entity );
	if ( pLightingOrigin )
	{
		sInfo.pLightingOrigin = pLightingOrigin;
	}

	int drawn = modelrender->DrawModelEx( sInfo );
	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: Create model shattering shards
// Input  : *pos - 
//			*size - 
//			*dir - 
//			random - 
//			life - 
//			count - 
//			modelIndex - 
//			flags - 
//-----------------------------------------------------------------------------
void CTempEnts::BreakModel( const Vector &pos, const QAngle &angles, const Vector &size, const Vector &dir, 
						   float randRange, float life, int count, int modelIndex, char flags)
{
	int					i, frameCount;
	CLocalTempEntity			*pTemp;
	const model_t		*pModel;

	if (!modelIndex) 
		return;

	pModel = modelinfo->GetModel( modelIndex );
	if ( !pModel )
		return;

	// See g_BreakableHelper above for notes...
	bool isSlave = ( flags & BREAK_SLAVE ) ? true : false;

	frameCount = modelinfo->GetModelFrameCount( pModel );

	if (count == 0)
	{
		// assume surface (not volume)
		count = (size[0] * size[1] + size[1] * size[2] + size[2] * size[0])/(3 * SHARD_VOLUME * SHARD_VOLUME);
	}

	if ( count > func_break_max_pieces.GetInt() )
	{
		count = func_break_max_pieces.GetInt();
	}

	matrix3x4_t transform;
	AngleMatrix( angles, pos, transform );
	for ( i = 0; i < count; i++ ) 
	{
		Vector vecLocalSpot, vecSpot;

		// fill up the box with stuff
		vecLocalSpot[0] = random_valve->RandomFloat(-0.5,0.5) * size[0];
		vecLocalSpot[1] = random_valve->RandomFloat(-0.5,0.5) * size[1];
		vecLocalSpot[2] = random_valve->RandomFloat(-0.5,0.5) * size[2];
		VectorTransform( vecLocalSpot, transform, vecSpot );

		pTemp = TempEntAlloc(vecSpot, ( model_t * )pModel);
		
		if (!pTemp)
			return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = flags;
		
		if ( modelinfo->GetModelType( pModel ) == mod_sprite )
		{
			pTemp->m_flFrame = random_valve->RandomInt(0,frameCount-1);
		}
		else if ( modelinfo->GetModelType( pModel ) == mod_studio )
		{
			pTemp->SetBody( random_valve->RandomInt(0,frameCount-1) );
		}

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if ( random_valve->RandomInt(0,255) < 200 ) 
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->m_vecTempEntAngVelocity[0] = random_valve->RandomFloat(-256,255);
			pTemp->m_vecTempEntAngVelocity[1] = random_valve->RandomFloat(-256,255);
			pTemp->m_vecTempEntAngVelocity[2] = random_valve->RandomFloat(-256,255);
		}

		if ( (random_valve->RandomInt(0,255) < 100 ) && (flags & BREAK_SMOKE) )
		{
			pTemp->flags |= FTENT_SMOKETRAIL;
		}

		if ((flags & BREAK_GLASS) || (flags & BREAK_TRANS))
		{
			pTemp->SetRenderMode( kRenderTransTexture );
			pTemp->SetRenderAlpha( 128 );
			pTemp->tempent_renderamt = 128;
			pTemp->bounceFactor = 0.3f;
		}
		else
		{
			pTemp->SetRenderMode( kRenderNormal );
			pTemp->tempent_renderamt = 255;		// Set this for fadeout
		}

		pTemp->SetVelocity( Vector( dir[0] + random_valve->RandomFloat(-randRange,randRange),
							dir[1] + random_valve->RandomFloat(-randRange,randRange),
							dir[2] + random_valve->RandomFloat(   0,randRange) ) );

		pTemp->die = gpGlobals->curtime + life + random_valve->RandomFloat(0,1);	// Add an extra 0-1 secs of life

		// We use a special rendering function because these objects will want to share their lighting
		//  origin with the first/master object.  Prevents a huge spike in Light Cache building in maps
		//  with many worldlights.
		pTemp->SetDrawHelper( BreakModelDrawHelper );
		g_BreakableHelper.Insert( pTemp, isSlave );
	}
}

void CTempEnts::PhysicsProp( int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor)
{
	CPhysPropClientside *pEntity = CPhysPropClientside::CreateNew();
	
	if ( !pEntity )
		return;

	const model_t *model = modelinfo->GetModel( modelindex );

	if ( !model )
	{
		DevMsg("CTempEnts::PhysicsProp: model index %i not found\n", modelindex );
		return;
	}

	pEntity->SetModelName( modelinfo->GetModelName(model) );
	pEntity->SetSkin( skin );
	pEntity->SetAbsOrigin( pos );
	pEntity->SetAbsAngles( angles );
	pEntity->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );
	pEntity->SetEffects( effects );
	pEntity->SetRenderColor( renderColor.r, renderColor.g, renderColor.b );

	if ( flags & 1 )
	{
		// We are not calling initialize on this entity here, so we need to manually set some of the required values otherwise set in initialize.
		pEntity->SetModelIndex( modelindex );
		pEntity->SetCollisionGroup( COLLISION_GROUP_PUSHAWAY );
		pEntity->SetAbsVelocity( vel );
		const model_t *mod = pEntity->GetModel();
		if ( mod )
		{
			Vector mins, maxs;
			modelinfo->GetModelBounds( mod, mins, maxs );
			pEntity->SetCollisionBounds( mins, maxs );
		}

		pEntity->Spawn();
		pEntity->SetHealth( 0 );
		pEntity->Break();
		return;
	}

	if ( !pEntity->Initialize() )
	{
		pEntity->Release();
		return;
	}

	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

	if( pPhysicsObject )
	{
		pPhysicsObject->AddVelocity( &vel, nullptr );
	}
	else
	{
		// failed to create a physics object
		pEntity->Release();
		return;
	}

	if ( flags & 2 )
	{
		int numBodygroups = pEntity->GetBodygroupCount( 0 );
		pEntity->SetBodygroup( 0, RandomInt( 0, numBodygroups - 1 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a clientside projectile
// Input  : vecOrigin - 
//			vecVelocity - 
//			modelindex - 
//			lifetime - 
//			*pOwner - 
//-----------------------------------------------------------------------------
CLocalTempEntity *CTempEnts::ClientProjectile( const Vector& vecOrigin, const Vector& vecVelocity, const Vector& vecAcceleration, int modelIndex, int lifetime, CBaseEntity *pOwner, const char *pszImpactEffect, const char *pszParticleEffect )
{
	CLocalTempEntity	*pTemp;
	const model_t		*model;

	if ( !modelIndex ) 
		return nullptr;

	model = modelinfo->GetModel( modelIndex );
	if ( !model )
	{
		Warning("ClientProjectile: No model %d!\n", modelIndex);
		return nullptr;
	}

	pTemp = TempEntAlloc( vecOrigin, ( model_t * )model );
	if (!pTemp)
		return nullptr;

	pTemp->SetVelocity( vecVelocity );
	pTemp->SetAcceleration( vecAcceleration );
	QAngle angles;
	VectorAngles( vecVelocity, angles );
	pTemp->SetAbsAngles( angles );
	pTemp->SetAbsOrigin( vecOrigin );
	pTemp->die = gpGlobals->curtime + lifetime;
	pTemp->flags = FTENT_COLLIDEALL | FTENT_ATTACHTOTARGET | FTENT_ALIGNTOMOTION;
	pTemp->clientIndex = ( pOwner != nullptr ) ? pOwner->entindex() : 0; 
	pTemp->SetOwnerEntity( pOwner );
	pTemp->SetImpactEffect( pszImpactEffect );
	if ( pszParticleEffect )
	{
		// Add the entity to the ClientEntityList and create the particle system.
		ClientEntityList().AddNonNetworkableEntity( pTemp );
		pTemp->ParticleProp()->Create( pszParticleEffect, PATTACH_ABSORIGIN_FOLLOW );

		// Set the particle flag on the temp entity and save the name of the particle effect.
		pTemp->flags |= FTENT_CLIENTSIDEPARTICLES;
	 	pTemp->SetParticleEffect( pszParticleEffect );
	}
	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Create sprite TE
// Input  : *pos - 
//			*dir - 
//			scale - 
//			modelIndex - 
//			rendermode - 
//			renderfx - 
//			a - 
//			life - 
//			flags - 
// Output : CLocalTempEntity
//-----------------------------------------------------------------------------
CLocalTempEntity *CTempEnts::TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags, const Vector &normal )
{
	CLocalTempEntity			*pTemp;
	const model_t		*model;
	int					frameCount;

	if ( !modelIndex ) 
		return nullptr;

	model = modelinfo->GetModel( modelIndex );
	if ( !model )
	{
		Warning("No model %d!\n", modelIndex);
		return nullptr;
	}

	frameCount = modelinfo->GetModelFrameCount( model );

	pTemp = TempEntAlloc( pos, ( model_t * )model );
	if (!pTemp)
		return nullptr;

	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->m_flFrameRate = 10;
	pTemp->SetRenderMode( (RenderMode_t)rendermode );
	pTemp->SetRenderFX( (RenderFx_t)renderfx );
	pTemp->m_flSpriteScale = scale;
	pTemp->tempent_renderamt = a * 255;
	pTemp->m_vecNormal = normal;
	pTemp->SetRenderColor( 255, 255, 255 );
	pTemp->SetRenderAlpha( a * 255 );

	pTemp->flags |= flags;

	pTemp->SetVelocity( dir );
	pTemp->SetLocalOrigin( pos );
	if ( life )
		pTemp->die = gpGlobals->curtime + life;
	else
		pTemp->die = gpGlobals->curtime + (frameCount * 0.1) + 1;

	pTemp->m_flFrame = 0;
	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Spray sprite
// Input  : *pos - 
//			*dir - 
//			modelIndex - 
//			count - 
//			speed - 
//			iRand - 
//-----------------------------------------------------------------------------
void CTempEnts::Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand )
{
	CLocalTempEntity			*pTemp;
	const model_t		*pModel;
	float				noise;
	float				znoise;
	int					frameCount;
	int					i;

	noise = (float)iRand / 100;

	// more vertical displacement
	znoise = noise * 1.5;
	
	if ( znoise > 1 )
	{
		znoise = 1;
	}

	pModel = modelinfo->GetModel( modelIndex );
	
	if ( !pModel )
	{
		Warning("No model %d!\n", modelIndex);
		return;
	}

	frameCount = modelinfo->GetModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		pTemp = TempEntAlloc( pos, ( model_t * )pModel );
		if (!pTemp)
			return;

		pTemp->SetRenderMode( kRenderTransAlpha );
		pTemp->SetRenderColor( 255, 255, 255 );
		pTemp->SetRenderAlpha( 255 );
		pTemp->tempent_renderamt = 255;
		pTemp->SetRenderFX( kRenderFxNoDissipation );
		//pTemp->scale = random->RandomFloat( 0.1, 0.25 );
		pTemp->m_flSpriteScale = 0.5;
		pTemp->flags |= FTENT_FADEOUT | FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0;

		// make the spittle fly the direction indicated, but mix in some noise.
		Vector velocity;
		velocity.x = dir[ 0 ] + random_valve->RandomFloat ( -noise, noise );
		velocity.y = dir[ 1 ] + random_valve->RandomFloat ( -noise, noise );
		velocity.z = dir[ 2 ] + random_valve->RandomFloat ( 0, znoise );
		velocity *= random_valve->RandomFloat( (speed * 0.8), (speed * 1.2) );
		pTemp->SetVelocity( velocity );

		pTemp->SetLocalOrigin( pos );

		pTemp->die = gpGlobals->curtime + 0.35;

		pTemp->m_flFrame = random_valve->RandomInt( 0, frameCount );
	}
}

void CTempEnts::Sprite_Trail( const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed )
{
	CLocalTempEntity	*pTemp;
	const model_t		*pModel;
	int					flFrameCount;

	pModel = modelinfo->GetModel( modelIndex );
	
	if ( !pModel )
	{
		Warning("No model %d!\n", modelIndex);
		return;
	}

	flFrameCount = modelinfo->GetModelFrameCount( pModel );

	Vector vecDelta;
	VectorSubtract( vecEnd, vecStart, vecDelta );

	Vector vecDir;
	VectorCopy( vecDelta, vecDir );
	VectorNormalize( vecDir );

	flAmplitude /= 256.0;

	for ( int i = 0 ; i < nCount; i++ )
	{
		Vector vecPos;

		// Be careful of divide by 0 when using 'count' here...
		if ( i == 0 )
		{
			VectorMA( vecStart, 0, vecDelta, vecPos );
		}
		else
		{
			VectorMA( vecStart, i / (nCount - 1.0), vecDelta, vecPos );
		}

		pTemp = TempEntAlloc( vecPos, ( model_t * )pModel );
		if (!pTemp)
			return;

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		Vector vecVel = vecDir * flSpeed;
		vecVel.x += random_valve->RandomInt( -127,128 ) * flAmplitude;
		vecVel.y += random_valve->RandomInt( -127,128 ) * flAmplitude;
		vecVel.z += random_valve->RandomInt( -127,128 ) * flAmplitude;
		pTemp->SetVelocity( vecVel );
		pTemp->SetLocalOrigin( vecPos );

		pTemp->m_flSpriteScale		= flSize;
		pTemp->SetRenderMode( kRenderGlow );
		pTemp->SetRenderFX( kRenderFxNoDissipation );
		pTemp->tempent_renderamt	= nRenderamt;
		pTemp->SetRenderColor( 255, 255, 255 );

		pTemp->m_flFrame	= random_valve->RandomInt( 0, flFrameCount - 1 );
		pTemp->m_flFrameMax	= flFrameCount - 1;
		pTemp->die			= gpGlobals->curtime + flLife + random_valve->RandomFloat( 0, 4 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attaches entity to player
// Input  : client - 
//			modelIndex - 
//			zoffset - 
//			life - 
//-----------------------------------------------------------------------------
void CTempEnts::AttachTentToPlayer( int client, int modelIndex, float zoffset, float life )
{
	CLocalTempEntity			*pTemp;
	const model_t		*pModel;
	Vector				position;
	int					frameCount;

	if ( client <= 0 || client > gpGlobals->maxClients )
	{
		Warning("Bad client in AttachTentToPlayer()!\n");
		return;
	}

	IClientEntity *clientClass = cl_entitylist->GetClientEntity( client );
	if ( !clientClass )
	{
		Warning("Couldn't get IClientEntity for %i\n", client );
		return;
	}

	pModel = modelinfo->GetModel( modelIndex );
	
	if ( !pModel )
	{
		Warning("No model %d!\n", modelIndex);
		return;
	}

	VectorCopy( clientClass->GetAbsOrigin(), position );
	position[ 2 ] += zoffset;

	pTemp = TempEntAllocHigh( position, ( model_t * )pModel );
	if (!pTemp)
	{
		Warning("No temp ent.\n");
		return;
	}

	pTemp->SetRenderMode( kRenderNormal );
	pTemp->SetRenderAlpha( 255 );
	pTemp->tempent_renderamt = 255;
	pTemp->SetRenderFX( kRenderFxNoDissipation );
	
	pTemp->clientIndex = client;
	pTemp->tentOffset[ 0 ] = 0;
	pTemp->tentOffset[ 1 ] = 0;
	pTemp->tentOffset[ 2 ] = zoffset;
	pTemp->die = gpGlobals->curtime + life;
	pTemp->flags |= FTENT_PLYRATTACHMENT | FTENT_PERSIST;

	// is the model a sprite?
	if ( modelinfo->GetModelType( pTemp->GetModel() ) == mod_sprite )
	{
		frameCount = modelinfo->GetModelFrameCount( pModel );
		pTemp->m_flFrameMax = frameCount - 1;
		pTemp->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
		pTemp->m_flFrameRate = 10;
	}
	else
	{
		// no animation support for attached clientside studio models.
		pTemp->m_flFrameMax = 0;
	}

	pTemp->m_flFrame = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Detach entity from player
//-----------------------------------------------------------------------------
void CTempEnts::KillAttachedTents( int client )
{
	if ( client <= 0 || client > gpGlobals->maxClients )
	{
		Warning("Bad client in KillAttachedTents()!\n");
		return;
	}

	for ( CLocalTempEntity *pTemp : m_TempEnts )
	{
		if ( pTemp->flags & FTENT_PLYRATTACHMENT )
		{
			// this TENT is player attached.
			// if it is attached to this client, set it to die instantly.
			if ( pTemp->clientIndex == client )
			{
				pTemp->die = gpGlobals->curtime;// good enough, it will die on next tent update. 
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create ricochet sprite
// Input  : *pos - 
//			*pmodel - 
//			duration - 
//			scale - 
//-----------------------------------------------------------------------------
void CTempEnts::RicochetSprite( const Vector &pos, model_t *pmodel, float duration, float scale )
{
	CLocalTempEntity	*pTemp;

	pTemp = TempEntAlloc( pos, ( model_t * )pmodel );
	if (!pTemp)
		return;

	pTemp->SetRenderMode( kRenderGlow );
	pTemp->SetRenderFX( kRenderFxNoDissipation );
	pTemp->SetRenderAlpha( 200 );
	pTemp->tempent_renderamt = 200;
	pTemp->m_flSpriteScale = scale;
	pTemp->flags = FTENT_FADEOUT;

	pTemp->SetVelocity( vec3_origin );

	pTemp->SetLocalOrigin( pos );
	
	pTemp->fadeSpeed = 8;
	pTemp->die = gpGlobals->curtime;

	pTemp->m_flFrame = 0;
	pTemp->SetLocalAnglesDim( Z_INDEX, 45 * random_valve->RandomInt( 0, 7 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Create blood sprite
// Input  : *org - 
//			colorindex - 
//			modelIndex - 
//			modelIndex2 - 
//			size - 
//-----------------------------------------------------------------------------
void CTempEnts::BloodSprite( const Vector &org, int r, int g, int b, int a, int modelIndex, int modelIndex2, float size )
{
	const model_t			*model;

	//Validate the model first
	if ( modelIndex && (model = modelinfo->GetModel( modelIndex ) ) != nullptr )
	{
		CLocalTempEntity		*pTemp;
		int						frameCount = modelinfo->GetModelFrameCount( model );

		//Large, single blood sprite is a high-priority tent
		if ( ( pTemp = TempEntAllocHigh( org, ( model_t * )model ) ) != nullptr )
		{
			pTemp->SetRenderMode( kRenderTransTexture );
			pTemp->SetRenderFX( kRenderFxNone );
			pTemp->m_flSpriteScale	= random_valve->RandomFloat( size / 25, size / 35);
			pTemp->flags			= FTENT_SPRANIMATE;
 
			pTemp->SetRenderColor( r, g, b );
			pTemp->SetRenderAlpha( a );
			pTemp->tempent_renderamt= pTemp->GetRenderAlpha();

			pTemp->SetVelocity( vec3_origin );

			pTemp->m_flFrameRate	= frameCount * 4; // Finish in 0.250 seconds
			pTemp->die				= gpGlobals->curtime + (frameCount / pTemp->m_flFrameRate); // Play the whole thing Once

			pTemp->m_flFrame		= 0;
			pTemp->m_flFrameMax		= frameCount - 1;
			pTemp->bounceFactor		= 0;
			pTemp->SetLocalAnglesDim( Z_INDEX, random_valve->RandomInt( 0, 360 ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create default sprite TE
// Input  : *pos - 
//			spriteIndex - 
//			framerate - 
// Output : CLocalTempEntity
//-----------------------------------------------------------------------------
CLocalTempEntity *CTempEnts::DefaultSprite( const Vector &pos, int spriteIndex, float framerate )
{
	CLocalTempEntity		*pTemp;
	int				frameCount;
	const model_t	*pSprite;

	// don't spawn while paused
	if ( gpGlobals->frametime == 0.0 )
		return nullptr;

	pSprite = modelinfo->GetModel( spriteIndex );
	if ( !spriteIndex || !pSprite || modelinfo->GetModelType( pSprite ) != mod_sprite )
	{
		DevWarning( 1,"No Sprite %d!\n", spriteIndex);
		return nullptr;
	}

	frameCount = modelinfo->GetModelFrameCount( pSprite );

	pTemp = TempEntAlloc( pos, ( model_t * )pSprite );
	if (!pTemp)
		return nullptr;

	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->m_flSpriteScale = 1.0;
	pTemp->flags |= FTENT_SPRANIMATE;
	if ( framerate == 0 )
		framerate = 10;

	pTemp->m_flFrameRate = framerate;
	pTemp->die = gpGlobals->curtime + (float)frameCount / framerate;
	pTemp->m_flFrame = 0;
	pTemp->SetLocalOrigin( pos );

	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Create sprite smoke
// Input  : *pTemp - 
//			scale - 
//-----------------------------------------------------------------------------
void CTempEnts::Sprite_Smoke( CLocalTempEntity *pTemp, float scale )
{
	if ( !pTemp )
		return;

	pTemp->SetRenderMode( kRenderTransAlpha );
	pTemp->SetRenderFX( kRenderFxNone );
	pTemp->SetVelocity( Vector( 0, 0, 30 ) );
	int iColor = random_valve->RandomInt(20,35);
	pTemp->SetRenderColor( iColor, iColor, iColor );
	pTemp->SetRenderAlpha( 255 );
	pTemp->SetLocalOriginDim( Z_INDEX, pTemp->GetLocalOriginDim( Z_INDEX ) + 20 );
	pTemp->m_flSpriteScale = scale;
	pTemp->flags = FTENT_WINDBLOWN;

}

//-----------------------------------------------------------------------------
// Purpose: Create some simple physically simulated models
//-----------------------------------------------------------------------------
CLocalTempEntity * CTempEnts::SpawnTempModel( model_t *pModel, const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, float flLifeTime, int iFlags )
{
	contract_assert( pModel );

	// Alloc a new tempent
	CLocalTempEntity *pTemp = TempEntAlloc( vecOrigin, pModel );
	if ( !pTemp )
		return nullptr;

	pTemp->SetAbsAngles( vecAngles );
	pTemp->SetBody( 0 );
	pTemp->flags |= iFlags;
	pTemp->m_vecTempEntAngVelocity[0] = random_valve->RandomFloat(-255,255);
	pTemp->m_vecTempEntAngVelocity[1] = random_valve->RandomFloat(-255,255);
	pTemp->m_vecTempEntAngVelocity[2] = random_valve->RandomFloat(-255,255);
	pTemp->SetRenderMode( kRenderNormal );
	pTemp->tempent_renderamt = 255;
	pTemp->SetVelocity( vecVelocity );
	pTemp->die = gpGlobals->curtime + flLifeTime;

	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//			entityIndex - 
//			attachmentIndex - 
//			firstPerson - 
//-----------------------------------------------------------------------------
void CTempEnts::MuzzleFlash( int type, ClientEntityHandle_t hEntity, int attachmentIndex, bool firstPerson )
{
	switch( type )
	{
	case MUZZLEFLASH_COMBINE:
		if ( firstPerson )
		{
			MuzzleFlash_Combine_Player( hEntity, attachmentIndex );
		}
		else
		{
			MuzzleFlash_Combine_NPC( hEntity, attachmentIndex );
		}
		break;

	case MUZZLEFLASH_SMG1:
		if ( firstPerson )
		{
			MuzzleFlash_SMG1_Player( hEntity, attachmentIndex );
		}
		else
		{
			MuzzleFlash_SMG1_NPC( hEntity, attachmentIndex );
		}
		break;

	case MUZZLEFLASH_PISTOL:
		if ( firstPerson )
		{
			MuzzleFlash_Pistol_Player( hEntity, attachmentIndex );
		}
		else
		{
			MuzzleFlash_Pistol_NPC( hEntity, attachmentIndex );
		}
		break;
	case MUZZLEFLASH_SHOTGUN:
		if ( firstPerson )
		{
			MuzzleFlash_Shotgun_Player( hEntity, attachmentIndex );
		}
		else
		{
			MuzzleFlash_Shotgun_NPC( hEntity, attachmentIndex );
		}
		break;
	case MUZZLEFLASH_357:
		if ( firstPerson )
		{
			MuzzleFlash_357_Player( hEntity, attachmentIndex );
		}
		break;
	case MUZZLEFLASH_RPG:
		if ( firstPerson )
		{
			// MuzzleFlash_RPG_Player( hEntity, attachmentIndex );
		}
		else
		{
			MuzzleFlash_RPG_NPC( hEntity, attachmentIndex );
		}
		break;
		break;
	default:
		{
			//NOTENOTE: This means you specified an invalid muzzleflash type, check your spelling?
			contract_assert( 0 );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play muzzle flash
// Input  : *pos1 - 
//			type - 
//-----------------------------------------------------------------------------
void CTempEnts::MuzzleFlash( const Vector& pos1, const QAngle& angles, int type, ClientEntityHandle_t hEntity, bool firstPerson )
{

	return;

}

//-----------------------------------------------------------------------------
// Purpose: Create explosion sprite
// Input  : *pTemp - 
//			scale - 
//			flags - 
//-----------------------------------------------------------------------------
void CTempEnts::Sprite_Explode( CLocalTempEntity *pTemp, float scale, int flags )
{
	if ( !pTemp )
		return;

	if ( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->SetRenderMode( kRenderNormal );
		pTemp->SetRenderAlpha( 255 ); 
	}
	else if( flags & TE_EXPLFLAG_DRAWALPHA )
	{
		// alpha sprite
		pTemp->SetRenderMode( kRenderTransAlpha ); 
		pTemp->SetRenderAlpha( 180 );
	}
	else
	{
		// additive sprite
		pTemp->SetRenderMode( kRenderTransAdd );
		pTemp->SetRenderAlpha( 180 );
	}

	if ( flags & TE_EXPLFLAG_ROTATE )
	{
		pTemp->SetLocalAnglesDim( Z_INDEX, random_valve->RandomInt( 0, 360 ) );
	}

	pTemp->SetRenderFX( kRenderFxNone );
	pTemp->SetVelocity( Vector( 0, 0, 8 ) );
	pTemp->SetRenderColor( 255, 255, 255 );
	pTemp->SetLocalOriginDim( Z_INDEX, pTemp->GetLocalOriginDim( Z_INDEX ) + 10 );
	pTemp->m_flSpriteScale = scale;
}

enum
{
	SHELL_NONE = 0,
	SHELL_SMALL,
	SHELL_BIG,
	SHELL_SHOTGUN,
};

//-----------------------------------------------------------------------------
// Purpose: Clear existing temp entities
//-----------------------------------------------------------------------------
void CTempEnts::Clear( void )
{
	for ( CLocalTempEntity *p : m_TempEnts )
	{
		m_TempEntsPool.free( p );
	}

	m_TempEnts.clear();
	g_BreakableHelper.Clear();
}

CLocalTempEntity *CTempEnts::FindTempEntByID( int nID, int nSubID )
{
	// HACK HACK: We're using skin and hitsounds as a hacky way to store an ID and sub-ID for later identification
	for ( CLocalTempEntity *p : m_TempEnts )
	{
		if ( p && p->m_nSkin == nID && p->hitSound == nSubID )
		{
			return p;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate temp entity ( normal/low priority )
// Input  : *org - 
//			*model - 
// Output : CLocalTempEntity
//-----------------------------------------------------------------------------
CLocalTempEntity *CTempEnts::TempEntAlloc( const Vector& org, model_t *model )
{
	CLocalTempEntity		*pTemp;

	if ( !model )
	{
		DevWarning( 1, "Can't create temporary entity with NULL model!\n" );
		return nullptr;
	}

	pTemp = TempEntAlloc();
	if ( !pTemp )
	{
		DevWarning( 1, "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return nullptr;
	}

	m_TempEnts.push_back( pTemp );

	pTemp->Prepare( model, gpGlobals->curtime );

	pTemp->priority = TENTPRIORITY_LOW;
	pTemp->SetAbsOrigin( org );

	pTemp->AddToLeafSystem( false );

	if ( CommandLine()->CheckParm( "-tools" ) != nullptr )
	{
//		ClientEntityList().AddNonNetworkableEntity(	pTemp );
	}

	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CLocalTempEntity
//-----------------------------------------------------------------------------
CLocalTempEntity *CTempEnts::TempEntAlloc()
{
	if ( (int)m_TempEnts.size() >= MAX_TEMP_ENTITIES )
		return nullptr;

	CLocalTempEntity *pTemp = m_TempEntsPool.alloc_zero();
	return pTemp;
}

void CTempEnts::TempEntFree( std::list< CLocalTempEntity * >::iterator it )
{
	CLocalTempEntity *pTemp = *it;
	if ( pTemp )
	{
		// Remove from the active list.
		m_TempEnts.erase( it );

		// Cleanup its data.
		pTemp->RemoveFromLeafSystem();

		// Remove the tempent from the ClientEntityList before removing it from the pool.
		if ( ( pTemp->flags & FTENT_CLIENTSIDEPARTICLES ) )
		{			
			// Stop the particle emission if this hasn't happened already - collision or system timing out on its own.
			if ( !pTemp->m_bParticleCollision )
			{
				pTemp->ParticleProp()->StopEmission();
			}
			ClientEntityList().RemoveEntity( pTemp->GetRefEHandle() );
		}

		pTemp->OnRemoveTempEntity();
	
		m_TempEntsPool.free( pTemp );
	}
}

// Free the first low priority tempent it finds.
bool CTempEnts::FreeLowPriorityTempEnt()
{
	for ( auto it = m_TempEnts.begin(); it != m_TempEnts.end(); ++it )
	{
		CLocalTempEntity *pActive = *it;

		if ( pActive->priority == TENTPRIORITY_LOW )
		{
			TempEntFree( it );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate a temp entity, if there are no slots, kick out a low priority
//  one if possible
// Input  : *org - 
//			*model - 
// Output : CLocalTempEntity
//-----------------------------------------------------------------------------
CLocalTempEntity *CTempEnts::TempEntAllocHigh( const Vector& org, model_t *model )
{
	CLocalTempEntity		*pTemp;

	if ( !model )
	{
		DevWarning( 1, "temporary ent model invalid\n" );
		return nullptr;
	}

	pTemp = TempEntAlloc();
	if ( !pTemp )
	{
		// no temporary ents free, so find the first active low-priority temp ent 
		// and overwrite it.
		FreeLowPriorityTempEnt();

		pTemp = TempEntAlloc();
	}

	if ( !pTemp )
	{
		// didn't find anything? The tent list is either full of high-priority tents
		// or all tents in the list are still due to live for > 10 seconds. 
		DevWarning( 1,"Couldn't alloc a high priority TENT (max %i)!\n", MAX_TEMP_ENTITIES );
		return nullptr;
	}

	m_TempEnts.push_back( pTemp );

	pTemp->Prepare( model, gpGlobals->curtime );

	pTemp->priority = TENTPRIORITY_HIGH;
	pTemp->SetLocalOrigin( org );

	pTemp->AddToLeafSystem( false );

	if ( CommandLine()->CheckParm( "-tools" ) != nullptr )
	{
		ClientEntityList().AddNonNetworkableEntity(	pTemp );
	}

	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Play sound when temp ent collides with something
// Input  : *pTemp - 
//			damp - 
//-----------------------------------------------------------------------------
void CTempEnts::PlaySound ( CLocalTempEntity *pTemp, float damp )
{
	const char	*soundname = nullptr;
	float fvol;
	bool isshellcasing = false;
	int zvel;

	switch ( pTemp->hitSound )
	{
	default:
		return;	// null sound

	case BOUNCE_GLASS:
		{
			soundname = "Bounce.Glass";
		}
		break;

	case BOUNCE_METAL:
		{
			soundname = "Bounce.Metal";
		}
		break;

	case BOUNCE_FLESH:
		{
			soundname = "Bounce.Flesh";
		}
		break;

	case BOUNCE_WOOD:
		{
			soundname = "Bounce.Wood";
		}
		break;

	case BOUNCE_SHRAP:
		{
			soundname = "Bounce.Shrapnel";
		}
		break;

	case BOUNCE_SHOTSHELL:
		{
			soundname = "Bounce.ShotgunShell";
			isshellcasing = true; // shell casings have different playback parameters
		}
		break;

	case BOUNCE_SHELL:
		{
			soundname = "Bounce.Shell";
			isshellcasing = true; // shell casings have different playback parameters
		}
		break;

	case BOUNCE_CONCRETE:
		{
			soundname = "Bounce.Concrete";
		}
		break;

		case TE_PISTOL_SHELL:
		{
			soundname = "Bounce.PistolShell";
		}
		break;

		case TE_RIFLE_SHELL:
		{
			soundname = "Bounce.RifleShell";
		}
		break;

		case TE_SHOTGUN_SHELL:
		{
			soundname = "Bounce.ShotgunShell";
		}
		break;
	}

	zvel = abs( pTemp->GetVelocity()[2] );
		
	// only play one out of every n

	if ( isshellcasing )
	{	
		// play first bounce, then 1 out of 3		
		if ( zvel < 200 && random_valve->RandomInt(0,3) )
			return;
	}
	else
	{
		if ( random_valve->RandomInt(0,5) ) 
			return;
	}

	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( soundname, params, nullptr ) )
		return;

	fvol = params.volume;

	if ( damp > 0.0 )
	{
		int pitch;
		
		if ( isshellcasing )
		{
			fvol *= std::min(1.0, ((float)zvel) / 350.0); 
		}
		else
		{
			fvol *= std::min(1.0, ((float)zvel) / 450.0); 
		}
		
		if ( !random_valve->RandomInt(0,3) && !isshellcasing )
		{
			pitch = random_valve->RandomInt( params.pitchlow, params.pitchhigh );
		}
		else
		{
			pitch = params.pitch;
		}

		CLocalPlayerFilter filter;

		EmitSound_t ep;
		ep.m_nChannel = params.channel;
		ep.m_pSoundName =  params.soundname;
		ep.m_flVolume = fvol;
		ep.m_SoundLevel = params.soundlevel;
		ep.m_nPitch = pitch;
		ep.m_pOrigin = &pTemp->GetAbsOrigin();

		CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
	}
}
					
//-----------------------------------------------------------------------------
// Purpose: Add temp entity to visible entities list of it's in PVS
// Input  : *pEntity - 
// Output : int
//-----------------------------------------------------------------------------
int CTempEnts::AddVisibleTempEntity( CLocalTempEntity *pEntity )
{
	int i;
	Vector mins, maxs;
	Vector model_mins, model_maxs;

	if ( !pEntity->GetModel() )
		return 0;

	modelinfo->GetModelBounds( pEntity->GetModel(), model_mins, model_maxs );

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = pEntity->GetAbsOrigin()[i] + model_mins[i];
		maxs[i] = pEntity->GetAbsOrigin()[i] + model_maxs[i];
	}

	// FIXME: Vis isn't setup by the time we get here, so this call fails if 
	//		  you try to add a tempent before the first frame is drawn, and it's
	//		  one frame behind the rest of the time. Fix this.
	// does the box intersect a visible leaf?
	//if ( engine->IsBoxInViewCluster( mins, maxs ) )
	{
		// Temporary entities have no corresponding element in cl_entitylist
		pEntity->index = -1;
		
		// Add to list
		pEntity->AddToLeafSystem( false );
		return 1;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Runs Temp Ent simulation routines
//-----------------------------------------------------------------------------
void CTempEnts::Update(void)
{
	CVProfScope _( "CTempEnts::Update", 1, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT );
	static int gTempEntFrame = 0;
	float		frametime;

	// Don't simulate while loading
	if ( m_TempEnts.empty() || !engine->IsInGame() )		
	{
		return;
	}

	// !!!BUGBUG	-- This needs to be time based
	gTempEntFrame = (gTempEntFrame+1) & 31;

	frametime = gpGlobals->frametime;

	// in order to have tents collide with players, we have to run the player prediction code so
	// that the client has the player list. We run this code once when we detect any COLLIDEALL 
	// tent, then set this BOOL to true so the code doesn't get run again if there's more than
	// one COLLIDEALL ent for this update. (often are).

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if ( frametime == 0 )
	{
		for ( CLocalTempEntity *current : m_TempEnts )
		{
			AddVisibleTempEntity( current );
		}
	}
	else
	{
		for ( auto it = m_TempEnts.begin(); it != m_TempEnts.end(); )
		{
			auto cur = it++;

			CLocalTempEntity *current = *cur;

			// Kill it
			if ( !current->IsActive() || !current->Frame( frametime, gTempEntFrame ) )
			{
				TempEntFree( cur );
			}
			else
			{
				// Cull to PVS (not frustum cull, just PVS)
				if ( !AddVisibleTempEntity( current ) )
				{
					if ( !( current->flags & FTENT_PERSIST ) ) 
					{
						// If we can't draw it this frame, just dump it.
						current->die = gpGlobals->curtime;
						// Don't fade out, just die
						current->flags &= ~FTENT_FADEOUT;

						TempEntFree( cur );
					}
				}
			}
		}
	}
}

// Recache tempents which might have been flushed
void CTempEnts::LevelInit()
{

// We don't use any of these models in CSTRIKE15

}

//-----------------------------------------------------------------------------
// Purpose: Initialize TE system
//-----------------------------------------------------------------------------
void CTempEnts::Init (void)
{
	m_pSpriteMuzzleFlash[0] = nullptr;
	m_pSpriteMuzzleFlash[1] = nullptr;
	m_pSpriteMuzzleFlash[2] = nullptr;

	m_pSpriteAR2Flash[0] = nullptr;
	m_pSpriteAR2Flash[1] = nullptr;
	m_pSpriteAR2Flash[2] = nullptr;
	m_pSpriteAR2Flash[3] = nullptr;

	m_pSpriteCombineFlash[0] = nullptr;
	m_pSpriteCombineFlash[1] = nullptr;

	m_pShells[0] = nullptr;
	m_pShells[1] = nullptr;
	m_pShells[2] = nullptr;

	m_pCS_9MMShell		= nullptr;
	m_pCS_57Shell		= nullptr;
	m_pCS_12GaugeShell	= nullptr;
	m_pCS_556Shell		= nullptr;
	m_pCS_762NATOShell	= nullptr;
	m_pCS_338MAGShell	= nullptr;

	// Clear out lists to start
	Clear();
}

void CTempEnts::LevelShutdown()
{
	// Free all active tempents.
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTempEnts::Shutdown()
{
	LevelShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Cache off all material references
// Input  : *pEmitter - Emitter used for material lookup
//-----------------------------------------------------------------------------
void CTempEnts::CacheMuzzleFlashes( void )
{
	int i;
	for ( i = 0; i < 4; i++ )
	{
		if ( m_Material_MuzzleFlash_Player[i] == nullptr )
		{
			m_Material_MuzzleFlash_Player[i] = ParticleMgr()->GetPMaterial( VarArgs( "effects/muzzleflash%d_noz", i+1 ) );
		}
	}

	for ( i = 0; i < 4; i++ )
	{
		if ( m_Material_MuzzleFlash_NPC[i] == nullptr )
		{
			m_Material_MuzzleFlash_NPC[i] = ParticleMgr()->GetPMaterial( VarArgs( "effects/muzzleflash%d", i+1 ) );
		}
	}

	for ( i = 0; i < 2; i++ )
	{
		if ( m_Material_Combine_MuzzleFlash_Player[i] == nullptr )
		{
			m_Material_Combine_MuzzleFlash_Player[i] = ParticleMgr()->GetPMaterial( VarArgs( "effects/combinemuzzle%d_noz", i+1 ) );
		}
	}

	for ( i = 0; i < 2; i++ )
	{
		if ( m_Material_Combine_MuzzleFlash_NPC[i] == nullptr )
		{
			m_Material_Combine_MuzzleFlash_NPC[i] = ParticleMgr()->GetPMaterial( VarArgs( "effects/combinemuzzle%d", i+1 ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : entityIndex - 
//			attachmentIndex - 
//-----------------------------------------------------------------------------
void CTempEnts::MuzzleFlash_Combine_Player( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	CVProfScope _( "MuzzleFlash_Combine_Player", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash", hEntity, attachmentIndex, FLE_VIEWMODEL );

	CacheMuzzleFlashes();

	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset; //NOTENOTE: All coords are in local space

	float flScale = random_valve->RandomFloat( 2.0f, 2.25f );

	pSimple->SetDrawBeforeViewModel( true );

	// Flash
	for ( int i = 1; i < 6; i++ )
	{
		offset = (forward * (i*8.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_Combine_MuzzleFlash_Player[random_valve->RandomInt(0,1)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.025f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 200+random_valve->RandomInt(0,55);

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 255;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 6.0f, 8.0f ) * (12-(i))/12) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	// Tack on the smoke
	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_Combine_MuzzleFlash_Player[random_valve->RandomInt(0,1)], vec3_origin );
		
	if ( pParticle == nullptr )
		return;

	pParticle->m_flLifetime		= 0.0f;
	pParticle->m_flDieTime		= 0.025f;

	pParticle->m_vecVelocity.Init();

	pParticle->m_uchColor[0]	= 255;
	pParticle->m_uchColor[1]	= 255;
	pParticle->m_uchColor[2]	= 255;

	pParticle->m_uchStartAlpha	= random_valve->RandomInt( 64, 128 );
	pParticle->m_uchEndAlpha	= 32;

	pParticle->m_uchStartSize	= random_valve->RandomFloat( 10.0f, 16.0f );
	pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
	
	pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			entityIndex - 
//-----------------------------------------------------------------------------
void CTempEnts::MuzzleFlash_Combine_NPC( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	CVProfScope _( "MuzzleFlash_Combine_NPC", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash_Combine_NPC", hEntity, attachmentIndex );

	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset; //NOTENOTE: All coords are in local space

	float flScale = random_valve->RandomFloat( 1.0f, 1.5f );

	float burstSpeed = random_valve->RandomFloat( 50.0f, 150.0f );

static constexpr auto FRONT_LENGTH = 6;

	// Front flash
	for ( int i = 1; i < FRONT_LENGTH; i++ )
	{
		offset = (forward * (i*2.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_Combine_Muzzleflash[random_valve->RandomInt(0,1)], offset );
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.1f;

		pParticle->m_vecVelocity = forward * burstSpeed;

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 255;

		pParticle->m_uchStartAlpha	= 255.0f;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 6.0f, 8.0f ) * (FRONT_LENGTH*1.25f-(i))/(FRONT_LENGTH)) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}
	
	Vector right(0,1,0), up(0,0,1);
	Vector dir = right - up;

static constexpr auto SIDE_LENGTH = 6;

	burstSpeed = random_valve->RandomFloat( 50.0f, 150.0f );

	// Diagonal flash
	for ( int i = 1; i < SIDE_LENGTH; i++ )
	{
		offset = (dir * (i*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_Combine_Muzzleflash[random_valve->RandomInt(0,1)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.2f;

		pParticle->m_vecVelocity = dir * burstSpeed * 0.25f;

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 255;

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 2.0f, 4.0f ) * (SIDE_LENGTH-(i))/(SIDE_LENGTH*0.5f)) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	dir = right + up;
	burstSpeed = random_valve->RandomFloat( 50.0f, 150.0f );

	// Diagonal flash
	for ( int i = 1; i < SIDE_LENGTH; i++ )
	{
		offset = (-dir * (i*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_Combine_Muzzleflash[random_valve->RandomInt(0,1)], offset );
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.2f;

		pParticle->m_vecVelocity = dir * -burstSpeed * 0.25f;

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 255;

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 2.0f, 4.0f ) * (SIDE_LENGTH-(i))/(SIDE_LENGTH*0.5f)) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	dir = up;
	burstSpeed = random_valve->RandomFloat( 50.0f, 150.0f );

	// Top flash
	for ( int i = 1; i < SIDE_LENGTH; i++ )
	{
		offset = (dir * (i*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_Combine_Muzzleflash[random_valve->RandomInt(0,1)], offset );
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.2f;

		pParticle->m_vecVelocity = dir * burstSpeed * 0.25f;

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 255;

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 2.0f, 4.0f ) * (SIDE_LENGTH-(i))/(SIDE_LENGTH*0.5f)) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}

	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_Combine_Muzzleflash[2], vec3_origin );
	if ( pParticle == nullptr )
		return;

	pParticle->m_flLifetime		= 0.0f;
	pParticle->m_flDieTime		= random_valve->RandomFloat( 0.3f, 0.4f );

	pParticle->m_vecVelocity.Init();

	pParticle->m_uchColor[0]	= 255;
	pParticle->m_uchColor[1]	= 255;
	pParticle->m_uchColor[2]	= 255;

	pParticle->m_uchStartAlpha	= 255;
	pParticle->m_uchEndAlpha	= 0;

	pParticle->m_uchStartSize	= flScale * random_valve->RandomFloat( 12.0f, 16.0f );
	pParticle->m_uchEndSize		= 0.0f;
	pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= 0.0f;

	matrix3x4_t	matAttachment;
	Vector		origin;
	
	// Grab the origin out of the transform for the attachment
	if ( FX_GetAttachmentTransform( hEntity, attachmentIndex, matAttachment ) )
	{
		origin.x = matAttachment[0][3];
		origin.y = matAttachment[1][3];
		origin.z = matAttachment[2][3];
	}
	else
	{
		//NOTENOTE: If you're here, you've specified an entity or an attachment that is invalid
		contract_assert( 0 );
		return;
	}

	if ( muzzleflash_light.GetBool() )
	{
		CBaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( hEntity );
		if ( pEnt )
		{
			dlight_t *el = effects->CL_AllocElight( LIGHT_INDEX_MUZZLEFLASH + pEnt->entindex() );

			el->origin	= origin;

			el->color.r = 64;
			el->color.g = 128;
			el->color.b = 255;
			el->color.exponent = 5;

			el->radius	= random_valve->RandomInt( 32, 128 );
			el->decay	= el->radius / 0.05f;
			el->die		= gpGlobals->curtime + 0.05f;
		}
	}
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CTempEnts::MuzzleFlash_AR2_NPC( const Vector &origin, const QAngle &angles, ClientEntityHandle_t hEntity )
{
	//Draw the cloud of fire
	FX_MuzzleEffect( origin, angles, 1.0f, hEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTempEnts::MuzzleFlash_SMG1_NPC( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	//Draw the cloud of fire
	FX_MuzzleEffectAttached( 1.0f, hEntity, attachmentIndex, nullptr, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTempEnts::MuzzleFlash_SMG1_Player( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	CVProfScope _( "MuzzleFlash_SMG1_Player", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash_SMG1_Player", hEntity, attachmentIndex, FLE_VIEWMODEL );

	CacheMuzzleFlashes();

	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset; //NOTENOTE: All coords are in local space

	float flScale = random_valve->RandomFloat( 1.25f, 1.5f );

	pSimple->SetDrawBeforeViewModel( true );

	// Flash
	for ( int i = 1; i < 6; i++ )
	{
		offset = (forward * (i*8.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_MuzzleFlash_Player[random_valve->RandomInt(0,3)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.025f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 200+random_valve->RandomInt(0,55);

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 255;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 6.0f, 8.0f ) * (8-(i))/6) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CTempEnts::MuzzleFlash_Shotgun_Player( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	CVProfScope _( "MuzzleFlash_Shotgun_Player", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "MuzzleFlash_Shotgun_Player" );

	pSimple->SetDrawBeforeViewModel( true );

	CacheMuzzleFlashes();

	Vector origin;
	QAngle angles;

	// Get our attachment's transformation matrix
	FX_GetAttachmentTransform( hEntity, attachmentIndex, &origin, &angles );

	pSimple->GetBinding().SetBBox( origin - Vector( 4, 4, 4 ), origin + Vector( 4, 4, 4 ) );

	Vector forward;
	AngleVectors( angles, &forward, nullptr, nullptr );

	SimpleParticle *pParticle;
	Vector offset;

	float flScale = random_valve->RandomFloat( 1.25f, 1.5f );

	// Flash
	for ( int i = 1; i < 6; i++ )
	{
		offset = origin + (forward * (i*8.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_MuzzleFlash_Player[random_valve->RandomInt(0,3)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.0001f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 200+random_valve->RandomInt(0,55);

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 255;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 6.0f, 8.0f ) * (8-(i))/6) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CTempEnts::MuzzleFlash_Shotgun_NPC( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	//Draw the cloud of fire
	FX_MuzzleEffectAttached( 0.75f, hEntity, attachmentIndex );

	QAngle	angles;

	Vector	forward;
	int		i;

	// Setup the origin.
	Vector	origin;
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( hEntity );
	if ( !pRenderable )
		return;

	pRenderable->GetAttachment( attachmentIndex, origin, angles );
	AngleVectors( angles, &forward );

	//Embers less often
	if ( random_valve->RandomInt( 0, 2 ) == 0 )
	{
		//Embers
		ks::intrusive_ptr<CEmberEffect> pEmbers = CEmberEffect::Create( "muzzle_embers" );
		pEmbers->SetSortOrigin( origin );

		SimpleParticle	*pParticle;

		int	numEmbers = random_valve->RandomInt( 0, 4 );

		for ( int i = 0; i < numEmbers; i++ )
		{
			pParticle = (SimpleParticle *) pEmbers->AddParticle( sizeof( SimpleParticle ), g_Mat_SMG_Muzzleflash[0], origin );
				
			if ( pParticle == nullptr )
				return;

			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= random_valve->RandomFloat( 0.2f, 0.4f );

			pParticle->m_vecVelocity.Random( -0.05f, 0.05f );
			pParticle->m_vecVelocity += forward;
			VectorNormalize( pParticle->m_vecVelocity );

			pParticle->m_vecVelocity	*= random_valve->RandomFloat( 64.0f, 256.0f );

			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 128;
			pParticle->m_uchColor[2]	= 64;

			pParticle->m_uchStartAlpha	= 255;
			pParticle->m_uchEndAlpha	= 0;

			pParticle->m_uchStartSize	= 1;
			pParticle->m_uchEndSize		= 0;

			pParticle->m_flRoll			= 0;
			pParticle->m_flRollDelta	= 0;
		}
	}

	//
	// Trails
	//
	
	ks::intrusive_ptr<CTrailParticles> pTrails = CTrailParticles::Create( "MuzzleFlash_Shotgun_NPC" );
	pTrails->SetSortOrigin( origin );

	TrailParticle	*pTrailParticle;

	pTrails->SetFlag( bitsPARTICLE_TRAIL_FADE );
	pTrails->m_ParticleCollision.SetGravity( 0.0f );

	int	numEmbers = random_valve->RandomInt( 4, 8 );

	for ( i = 0; i < numEmbers; i++ )
	{
		pTrailParticle = (TrailParticle *) pTrails->AddParticle( sizeof( TrailParticle ), g_Mat_SMG_Muzzleflash[0], origin );
			
		if ( pTrailParticle == nullptr )
			return;

		pTrailParticle->m_flLifetime		= 0.0f;
		pTrailParticle->m_flDieTime		= random_valve->RandomFloat( 0.1f, 0.2f );

		float spread = 0.05f;

		pTrailParticle->m_vecVelocity.Random( -spread, spread );
		pTrailParticle->m_vecVelocity += forward;
		
		VectorNormalize( pTrailParticle->m_vecVelocity );
		VectorNormalize( forward );

		float dot = forward.Dot( pTrailParticle->m_vecVelocity );

		dot = (1.0f-fabs(dot)) / spread;
		pTrailParticle->m_vecVelocity *= (random_valve->RandomFloat( 256.0f, 1024.0f ) * (1.0f-dot));

		Color32Init( pTrailParticle->m_color, 255, 242, 191, 255 );

		pTrailParticle->m_flLength	= 0.05f;
		pTrailParticle->m_flWidth	= random_valve->RandomFloat( 0.25f, 0.5f );
	}
}

//==================================================
// Purpose: 
//==================================================
void CTempEnts::MuzzleFlash_357_Player( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	CVProfScope _( "MuzzleFlash_357_Player", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "MuzzleFlash_357_Player" );

	pSimple->SetDrawBeforeViewModel( true );

	CacheMuzzleFlashes();

	Vector origin;
	QAngle angles;

	// Get our attachment's transformation matrix
	FX_GetAttachmentTransform( hEntity, attachmentIndex, &origin, &angles );

	pSimple->GetBinding().SetBBox( origin - Vector( 4, 4, 4 ), origin + Vector( 4, 4, 4 ) );

	Vector forward;
	AngleVectors( angles, &forward, nullptr, nullptr );

	SimpleParticle *pParticle;
	Vector			offset;

	// Smoke
	offset = origin + forward * 8.0f;

	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_DustPuff[0], offset );
		
	if ( pParticle == nullptr )
		return;

	pParticle->m_flLifetime		= 0.0f;
	pParticle->m_flDieTime		= random_valve->RandomFloat( 0.5f, 1.0f );

	pParticle->m_vecVelocity.Init();
	pParticle->m_vecVelocity = forward * random_valve->RandomFloat( 8.0f, 64.0f );
	pParticle->m_vecVelocity[2] += random_valve->RandomFloat( 4.0f, 16.0f );

	int color = random_valve->RandomInt( 200, 255 );
	pParticle->m_uchColor[0]	= color;
	pParticle->m_uchColor[1]	= color;
	pParticle->m_uchColor[2]	= color;

	pParticle->m_uchStartAlpha	= random_valve->RandomInt( 64, 128 );
	pParticle->m_uchEndAlpha	= 0;

	pParticle->m_uchStartSize	= random_valve->RandomInt( 2, 4 );
	pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 8.0f;
	pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
	pParticle->m_flRollDelta	= random_valve->RandomFloat( -0.5f, 0.5f );

	float flScale = random_valve->RandomFloat( 1.25f, 1.5f );

	// Flash
	for ( int i = 1; i < 6; i++ )
	{
		offset = origin + (forward * (i*8.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_MuzzleFlash_Player[random_valve->RandomInt(0,3)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.01f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 200+random_valve->RandomInt(0,55);

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 255;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 6.0f, 8.0f ) * (8-(i))/6) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CTempEnts::MuzzleFlash_Pistol_Player( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	CVProfScope _( "MuzzleFlash_Pistol_Player", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "MuzzleFlash_Pistol_Player" );
	pSimple->SetDrawBeforeViewModel( true );

	CacheMuzzleFlashes();

	Vector origin;
	QAngle angles;

	// Get our attachment's transformation matrix
	FX_GetAttachmentTransform( hEntity, attachmentIndex, &origin, &angles );

	pSimple->GetBinding().SetBBox( origin - Vector( 4, 4, 4 ), origin + Vector( 4, 4, 4 ) );

	Vector forward;
	AngleVectors( angles, &forward, nullptr, nullptr );

	SimpleParticle *pParticle;
	Vector			offset;

	// Smoke
	offset = origin + forward * 8.0f;

	if ( random_valve->RandomInt( 0, 3 ) != 0 )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_DustPuff[0], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random_valve->RandomFloat( 0.25f, 0.5f );

		pParticle->m_vecVelocity.Init();
		pParticle->m_vecVelocity = forward * random_valve->RandomFloat( 48.0f, 64.0f );
		pParticle->m_vecVelocity[2] += random_valve->RandomFloat( 4.0f, 16.0f );

		int color = random_valve->RandomInt( 200, 255 );
		pParticle->m_uchColor[0]	= color;
		pParticle->m_uchColor[1]	= color;
		pParticle->m_uchColor[2]	= color;

		pParticle->m_uchStartAlpha	= random_valve->RandomInt( 64, 128 );
		pParticle->m_uchEndAlpha	= 0;

		pParticle->m_uchStartSize	= random_valve->RandomInt( 2, 4 );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4.0f;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -0.1f, 0.1f );
	}

	float flScale = random_valve->RandomFloat( 1.0f, 1.25f );

	// Flash
	for ( int i = 1; i < 6; i++ )
	{
		offset = origin + (forward * (i*4.0f*flScale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), m_Material_MuzzleFlash_Player[random_valve->RandomInt(0,3)], offset );
			
		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= 0.01f;

		pParticle->m_vecVelocity.Init();

		pParticle->m_uchColor[0]	= 255;
		pParticle->m_uchColor[1]	= 255;
		pParticle->m_uchColor[2]	= 200+random_valve->RandomInt(0,55);

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 255;

		pParticle->m_uchStartSize	= ( (random_valve->RandomFloat( 6.0f, 8.0f ) * (8-(i))/6) * flScale );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		pParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CTempEnts::MuzzleFlash_Pistol_NPC( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	FX_MuzzleEffectAttached( 0.5f, hEntity, attachmentIndex, nullptr, true );
}

//==================================================
// Purpose: 
// Input: 
//==================================================

void CTempEnts::MuzzleFlash_RPG_NPC( ClientEntityHandle_t hEntity, int attachmentIndex )
{
	//Draw the cloud of fire
	FX_MuzzleEffectAttached( 1.5f, hEntity, attachmentIndex );

}

void CTempEnts::RocketFlare( const Vector& pos )
{
	CLocalTempEntity	*pTemp;
	const model_t		*model;
	int					nframeCount;

	model = (model_t *)engine->LoadModel( "sprites/animglow01.vmt" );
	if ( !model )
	{
		return;
	}

	nframeCount = modelinfo->GetModelFrameCount( model );

	pTemp = TempEntAlloc( pos, (model_t *)model );
	if ( !pTemp )
		return;

	pTemp->m_flFrameMax = nframeCount - 1;
	pTemp->SetRenderMode( kRenderGlow );
	pTemp->SetRenderFX( kRenderFxNoDissipation );
	pTemp->tempent_renderamt = 255;
	pTemp->m_flFrameRate = 1.0;
	pTemp->m_flFrame = random_valve->RandomInt( 0, nframeCount - 1);
	pTemp->m_flSpriteScale = 1.0;
	pTemp->SetAbsOrigin( pos );
	pTemp->die = gpGlobals->curtime + 0.01;
}

using namespace ::ks::reflect;

// Purpose: Muzzle flash temp ent

//-----------------------------------------------------------------------------
// Purpose: User Tracer TE
//-----------------------------------------------------------------------------
class CTEMuzzleFlash : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEMuzzleFlash" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEMuzzleFlash, CBaseTempEntity );
	
	DECLARE_CLIENTCLASS();

					CTEMuzzleFlash( void );
	virtual			~CTEMuzzleFlash( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector		m_vecOrigin;
	[[= ks::reflect::Net{} ]] QAngle		m_vecAngles;
	[[= ks::reflect::Net{} ]] float		m_flScale;
	[[= ks::reflect::Net{} ]] int			m_nType;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMuzzleFlash::CTEMuzzleFlash( void )
{
	m_vecOrigin.Init();
	m_vecAngles.Init();
	m_flScale	= 1.0f;
	m_nType		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEMuzzleFlash::~CTEMuzzleFlash( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEMuzzleFlash::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEMuzzleFlash::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	//FIXME: Index is incorrect
	g_pEffects->MuzzleFlash( m_vecOrigin, m_vecAngles, m_flScale, m_nType );	
}

void TE_MuzzleFlash( IRecipientFilter& filter, float delay,
	const Vector &start, const QAngle &angles, float scale, int type )
{
	g_pEffects->MuzzleFlash( start, angles, scale, 0 );	
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEMuzzleFlash, DT_TEMuzzleFlash, CTEMuzzleFlash )

class CParticleRenderIterator;
class CParticleSimulateIterator;

using namespace ::ks::reflect;

int		ramp1[][3] =
{
	{ 255, 243, 227 },
	{ 223, 171, 39 },
	{ 191, 119, 47 },
	{ 127, 59, 43 },
	{ 99, 47, 31 },
	{ 75, 35, 19 },
	{ 47, 23, 11 },
	{ 175, 103, 35 },
};

int		ramp2[][3] =
{
	{ 255, 243, 227 },
	{ 239, 203, 31 },
	{ 223, 171, 39 },
	{ 207, 143, 43 },
	{ 191, 119, 47 },
	{ 175, 99, 47 },
	{ 143, 67, 51 },
	{ 115, 55, 35 },
};

int		ramp3[][3] =
{
	{ 223, 171, 39 },
	{ 191, 119, 47 },
	{ 91, 91, 91 },
	{ 75, 75, 75 },
	{ 63, 63, 63 },
	{ 47, 47, 47 },
};

static constexpr auto SPARK_COLORCOUNT = 9;

int		gSparkRamp[ SPARK_COLORCOUNT ][3] =
{
	{ 255, 255, 255 },
	{ 255, 247, 199 },
	{ 255, 243, 147 },
	{ 255, 243, 27 },
	{ 239, 203, 31 },
	{ 223, 171, 39 },
	{ 207, 143, 43 },
	{ 127, 59, 43 },
	{ 35, 19, 7 }
};

// ------------------------------------------------------------------------ //
// CTEParticleSystem.
// ------------------------------------------------------------------------ //

IMPLEMENT_REFLECT_CLIENTCLASS( CTEParticleSystem, DT_TEParticleSystem, CTEParticleSystem )

CTEParticleSystem::CTEParticleSystem()
{
	m_vecOrigin.Init();
}

// ------------------------------------------------------------------------ //
// CTEParticleRenderer implementation.
// ------------------------------------------------------------------------ //

CTEParticleRenderer::CTEParticleRenderer( const char *pDebugName ) :
	CParticleEffect( pDebugName )
{
	m_ParticleSize = 1.5f;
	m_MaterialHandle = INVALID_MATERIAL_HANDLE;
}

CTEParticleRenderer::~CTEParticleRenderer()
{
}

ks::intrusive_ptr<CTEParticleRenderer> CTEParticleRenderer::Create( const char *pDebugName, const Vector &vOrigin )
{
	CTEParticleRenderer *pRet = new CTEParticleRenderer( pDebugName );
	if( pRet )
	{
		pRet->SetDynamicallyAllocated( true );
		pRet->SetSortOrigin( vOrigin );
	}

	return pRet;
}

StandardParticle_t* CTEParticleRenderer::AddParticle()
{
	if(m_MaterialHandle == INVALID_MATERIAL_HANDLE)
	{
		m_MaterialHandle = m_ParticleEffect.FindOrAddMaterial("particle/particledefault");
	}

	StandardParticle_t *pParticle = 
		(StandardParticle_t*)BaseClass::AddParticle( sizeof(StandardParticle_t), m_MaterialHandle, m_vSortOrigin );

	if(pParticle)
		pParticle->m_EffectDataWord = 0; // (ramp)

	return pParticle;
}

void CTEParticleRenderer::RenderParticles( CParticleRenderIterator *pIterator )
{
	const StandardParticle_t *pParticle = (const StandardParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Render.
		Vector tPos;
		TransformParticle(ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos);
		float sortKey = tPos.z;

		Vector vColor(pParticle->m_Color[0]/255.9f, pParticle->m_Color[1]/255.9f, pParticle->m_Color[2]/255.9f);
		RenderParticle_ColorSize(
			pIterator->GetParticleDraw(),
			tPos,
			vColor,
			pParticle->m_Color[3]/255.9f,
			m_ParticleSize);

		pParticle = (const StandardParticle_t*)pIterator->GetNext( sortKey );
	}
}

void CTEParticleRenderer::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	StandardParticle_t *pParticle = (StandardParticle_t*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Remove the particle?
		SetParticleLifetime(pParticle, GetParticleLifetime(pParticle) - pIterator->GetTimeDelta());
		if(GetParticleLifetime(pParticle) < 0)
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			float	ft = pIterator->GetTimeDelta();
			float	time3 = 15.0 * ft;
			float	time2 = 10.0 * ft;
			float	time1 = 5.0 * ft;
			float	dvel = 4* ft ;

			float grav = ft * sv_gravity.GetFloat() * 0.05f;

			int		(*colorIndex)[3];
			int		iRamp;

			switch(GetParticleType(pParticle))
			{
			case pt_static:
				break;

			case pt_fire:
				pParticle->m_EffectDataWord += (unsigned short)(time1 * (1 << SIMSHIFT));
				iRamp = pParticle->m_EffectDataWord >> SIMSHIFT;
				if(iRamp >= 6)
				{
					pParticle->m_Lifetime = -1;
				}
				else
				{
					colorIndex = &ramp3[ iRamp ];
					pParticle->SetColor((float)(*colorIndex)[0] / 255.0f, (float)(*colorIndex)[1] / 255.0f, (float)(*colorIndex)[2] / 255.0f);
				}
				pParticle->m_Velocity[2] += grav;
				break;

			case pt_explode:
				pParticle->m_EffectDataWord += (unsigned short)(time2 * (1 << SIMSHIFT));
				iRamp = pParticle->m_EffectDataWord >> SIMSHIFT;
				if(iRamp >= 8)
				{
					pParticle->m_Lifetime = -1;
				}
				else
				{
					colorIndex = &ramp1[ iRamp ];
					pParticle->SetColor((float)(*colorIndex)[0] / 255.0f, (float)(*colorIndex)[1] / 255.0f, (float)(*colorIndex)[2] / 255.0f);
				}
				pParticle->m_Velocity = pParticle->m_Velocity + pParticle->m_Velocity * dvel;
				pParticle->m_Velocity[2] -= grav;
				break;

			case pt_explode2:
				pParticle->m_EffectDataWord += (unsigned short)(time3 * (1 << SIMSHIFT));
				iRamp = pParticle->m_EffectDataWord >> SIMSHIFT;
				if(iRamp >= 8)
				{
					pParticle->m_Lifetime = -1;
				}
				else
				{
					colorIndex = &ramp2[ iRamp ];
					pParticle->SetColor((float)(*colorIndex)[0] / 255.0f, (float)(*colorIndex)[1] / 255.0f, (float)(*colorIndex)[2] / 255.0f);
				}
				pParticle->m_Velocity = pParticle->m_Velocity - pParticle->m_Velocity * ft;
				pParticle->m_Velocity[2] -= grav;
				break;

			case pt_grav:
				pParticle->m_Velocity[2] -= grav * 20;
				break;
			case pt_slowgrav:
				pParticle->m_Velocity[2] = grav;
				break;

			case pt_vox_grav:
				pParticle->m_Velocity[2] -= grav * 8;
				break;
				
			case pt_vox_slowgrav:
				pParticle->m_Velocity[2] -= grav * 4;
				break;

			case pt_blob:
			case pt_blob2:
				pParticle->m_EffectDataWord += (unsigned short)(time2 * (1 << SIMSHIFT));
				iRamp = pParticle->m_EffectDataWord >> SIMSHIFT;
				if(iRamp >= SPARK_COLORCOUNT)
				{
					pParticle->m_EffectDataWord = 0;
					iRamp = 0;
				}
				
				colorIndex = &gSparkRamp[ iRamp ];
				pParticle->SetColor((float)(*colorIndex)[0] / 255.0f, (float)(*colorIndex)[1] / 255.0f, (float)(*colorIndex)[2] / 255.0f);
				
				pParticle->m_Velocity[0] -= pParticle->m_Velocity[0]*0.5*ft;
				pParticle->m_Velocity[1] -= pParticle->m_Velocity[1]*0.5*ft;
				pParticle->m_Velocity[2] -= grav * 5;

				if ( random_valve->RandomInt(0,3) )
				{
					SetParticleType(pParticle, pt_blob);
					pParticle->SetAlpha(0);
				}
				else
				{
					SetParticleType(pParticle, pt_blob2);
					pParticle->SetAlpha(255.9f);
				}
				break;
			}
			// Update position.
			pParticle->m_Pos = pParticle->m_Pos + pParticle->m_Velocity * ft;
		}

		pParticle = (StandardParticle_t*)pIterator->GetNext();
	}
}

// ------------------------------------------------------------------------ //
// Inlines.
// ------------------------------------------------------------------------ //
void CTEParticleRenderer::SetParticleType(StandardParticle_t *pParticle, ptype_t type)
{
	pParticle->m_EffectData = (unsigned char)type;
}

ptype_t CTEParticleRenderer::GetParticleType(StandardParticle_t *pParticle)
{
	return (ptype_t)pParticle->m_EffectData;
}

// Get/set lifetime. Note that 
void CTEParticleRenderer::SetParticleLifetime(StandardParticle_t *pParticle, float lifetime)
{
	pParticle->m_Lifetime = lifetime;
}

float CTEParticleRenderer::GetParticleLifetime(StandardParticle_t *pParticle)
{
	return pParticle->m_Lifetime;
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Breakable Model TE
//-----------------------------------------------------------------------------
class CTEPhysicsProp : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEPhysicsProp" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEPhysicsProp, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEPhysicsProp( void );
	virtual			~CTEPhysicsProp( void );

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
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEPhysicsProp, DT_TEPhysicsProp, CTEPhysicsProp )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPhysicsProp::CTEPhysicsProp( void )
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
CTEPhysicsProp::~CTEPhysicsProp( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TE_PhysicsProp( IRecipientFilter& filter, float delay,
	int modelindex, int skin, const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor )
{
	tempents->PhysicsProp( modelindex, skin, pos, angles, vel, flags, effects, renderColor );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEPhysicsProp::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEPhysicsProp::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	color24 clrRenderConverted;
	clrRenderConverted.r = m_clrRender.r;
	clrRenderConverted.g = m_clrRender.g;
	clrRenderConverted.b = m_clrRender.b;

	tempents->PhysicsProp( m_nModelIndex, m_nSkin, m_vecOrigin, m_angRotation, m_vecVelocity, m_nFlags, m_nEffects, clrRenderConverted );
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

class CEconGraffitiTintDefinition;
class CStickerKit;
class CBaseEntity;
class CCSPlayer;
class CEconItemView;

using namespace ::ks::reflect;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePlayerDecal )
PRECACHE( MATERIAL, "decals/playerlogo01" )
PRECACHE_REGISTER_END()

void QcCreatePreviewDecal( uint32 nStickerKitDefinition, uint32 nTintID, const trace_t& trace, const Vector* pRight );

//-----------------------------------------------------------------------------
// Purpose: Player Decal TE
//-----------------------------------------------------------------------------
class CTEPlayerDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEPlayerDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEPlayerDecal, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEPlayerDecal( void );
	virtual			~CTEPlayerDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] int				m_nPlayer;
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecStart;
	[[= ks::reflect::Net{} ]] Vector			m_vecRight;
	[[= ks::reflect::Net{} ]] int				m_nEntity;
	[[= ks::reflect::Net{} ]] int				m_nHitbox;
};

int g_nPlayerLogoProxyForPreviewKey = ( 8 << 24 ) | 0x1FFFF; // used for preview (model panel and in-game)
// there's a rendering batching approach that uses only low 16-bits of the proxy data for splitting decals into batches, this is sufficient for now, but
// eventually we might want to find all places that use only 16-bits and upgrade them to full proxy data value comparison, e.g. patterns like this:
// ( unPlayerDecalStickerKitID && ( pDecal->flags & FDECAL_PLAYERSPRAY ) && ( uint16( reinterpret_cast< uintp >( pDecal->userdata ) ) != unPlayerDecalStickerKitID ) )
inline int MakeKey( int nUniqueId )
{
	return ( 1 << 24 ) | nUniqueId;
}

// This whole thing exists so we can shunt data from the main thread to the material thread. 
// We will queue calls to create decal data and DeleteDecalData so that the material thread doesn't
// have to grab a mutex to examine s_mapUniqueId2DecalData.
struct DecalData_t
{
	ITexture* m_pTex;
	int m_nRarity;
	int m_nTintID;
	float m_flCreationTime;
};
static std::map< int, DecalData_t > s_mapUniqueId2DecalData;

void CreateDecalData( int nKey, ITexture* pTex, int nRarity, int nTintID, float flCreateTime )
{
	contract_assert( pTex );
	if ( !pTex )
		return;

	auto itExisting = s_mapUniqueId2DecalData.find( nKey );
	if ( itExisting != s_mapUniqueId2DecalData.end() )
	{
		DecalData_t &ddata = itExisting->second;

		// Release the old texture
		contract_assert( ddata.m_pTex );
		ddata.m_pTex->DecrementReferenceCount();
		ddata.m_pTex = nullptr;

		// Overwrite the data for the new item.
		ddata.m_pTex = pTex;
		ddata.m_nRarity = nRarity;
		ddata.m_nTintID = nTintID;
		ddata.m_flCreationTime = flCreateTime;

		return;
	}

	// It wasn't already in the map, so add it.
	DecalData_t dd = { pTex, nRarity, nTintID, flCreateTime };
	s_mapUniqueId2DecalData.emplace( nKey, dd );
}

void DeleteDecalData( int nKey )
{
	auto it = s_mapUniqueId2DecalData.find( nKey );
	contract_assert( ( nKey == g_nPlayerLogoProxyForPreviewKey )
		|| ( it != s_mapUniqueId2DecalData.end() ) );
	if ( it == s_mapUniqueId2DecalData.end() )
		return;

	contract_assert( it->second.m_pTex );
	it->second.m_pTex->DecrementReferenceCount();

	s_mapUniqueId2DecalData.erase( it );
}

bool ReadDecalData( int nKey, DecalData_t* pOutDD )
{
	if ( !pOutDD )
		return false;

	auto it = s_mapUniqueId2DecalData.find( nKey );
	if ( it == s_mapUniqueId2DecalData.end() )
		return false;

	( *pOutDD ) = it->second;
	return true;
}

inline bool BShouldHaveDrips( const Vector& normal )
{
	return fabs( normal.z ) < 0.8;
}

class CFEPlayerDecal;
typedef std::map< int, CFEPlayerDecal * > PlayerDecalsByUniqueId_t;
static PlayerDecalsByUniqueId_t s_mapPlayerDecalsUniqueIDsAll;		// All client-side player decals entities
static PlayerDecalsByUniqueId_t s_mapPlayerDecalsUniqueIDsToApply;	// Player decals entities that should still be applied to the world
static std::map< int, bool > s_mapPlayerDecalsUniqueIDsRecreating;	// Player decals entities that were temporarily destroyed and getting recreated ID->bReapplyPending
void OnPlayerDecalsLevelShutdown()
{	// we purge and re-apply decals here
	// so add all client-side decals that we know about into the IDs to apply map
	for ( const auto &[nUniqueID, pDecal] : s_mapPlayerDecalsUniqueIDsAll )
	{
		s_mapPlayerDecalsUniqueIDsToApply.insert_or_assign( nUniqueID, pDecal );
	}
	s_mapPlayerDecalsUniqueIDsRecreating.clear();
}

class CFEPlayerDecal : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FEPlayerDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFEPlayerDecal, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CFEPlayerDecal( void ) {
		m_nUniqueID = 0;
		m_unAccountID = 0;
		m_unTraceID = 0;
		m_rtGcTime = 0;
		m_vecEndPos.Init();
		m_vecStart.Init();
		m_vecRight.Init();
		m_vecNormal.Init();
		m_nPlayer = 0;
		m_nEntity = 0;
		m_nHitbox = 0;
		m_nTintID = 0;
		m_flCreationTime = 0;
		m_nVersion = 0;
		V_memset( m_ubSignature, 0, sizeof( m_ubSignature ) );

		m_bDecalReadyToApplyToWorld = false;
	}
	virtual			~CFEPlayerDecal( void )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ICallQueue* pCQ = pRenderContext->GetCallQueue();
		if ( pCQ )
			pCQ->QueueCall( DeleteDecalData, MakeKey( m_nUniqueID ) );
		else
			DeleteDecalData( MakeKey( m_nUniqueID ) );

		if ( m_nUniqueID )
		{

			contract_assert( !s_mapPlayerDecalsUniqueIDsToApply.contains( m_nUniqueID ) ||
				( s_mapPlayerDecalsUniqueIDsToApply.at( m_nUniqueID ) == this ) );
			s_mapPlayerDecalsUniqueIDsToApply.erase( m_nUniqueID );

			contract_assert( s_mapPlayerDecalsUniqueIDsAll.contains( m_nUniqueID ) &&
				( s_mapPlayerDecalsUniqueIDsAll.at( m_nUniqueID ) == this ) );
			s_mapPlayerDecalsUniqueIDsAll.erase( m_nUniqueID );
		}
	}

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void SetDestroyedOnRecreateEntities( void ) override;
	
	bool BMakeDecalReadyToApplyToWorld();
	void ApplyDecalDataToWorld();
	void MakeDecalReady( int nKey );

public:
	[[= ks::reflect::Net{} ]] int m_nUniqueID;
	[[= ks::reflect::Net{} ]] uint32 m_unAccountID;
	[[= ks::reflect::Net{} ]] uint32 m_unTraceID;
	[[= ks::reflect::Net{} ]] uint32 m_rtGcTime;
	[[= ks::reflect::Net{} ]] Vector m_vecEndPos;
	[[= ks::reflect::Net{} ]] Vector m_vecStart;
	[[= ks::reflect::Net{} ]] Vector m_vecRight;
	[[= ks::reflect::Net{} ]] Vector m_vecNormal;
	[[= ks::reflect::Net{} ]] int m_nPlayer;
	[[= ks::reflect::Net{} ]] int m_nEntity;
	[[= ks::reflect::Net{} ]] int m_nHitbox;
	[[= ks::reflect::Net{} ]] int m_nTintID;
	[[= ks::reflect::Net{} ]] float m_flCreationTime;
	[[= ks::reflect::Net{} ]] uint8 m_nVersion;
	[[= ks::reflect::Net{} ]] uint8 m_ubSignature[ PLAYERDECALS_SIGNATURE_BYTELEN ];

private:
	bool m_bDecalReadyToApplyToWorld;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPlayerDecal::CTEPlayerDecal( void )
{
	m_nPlayer = 0;
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_vecRight.Init();
	m_nEntity = 0;
	m_nHitbox = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPlayerDecal::~CTEPlayerDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEPlayerDecal::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, const Vector* right, int nPlayerAndStickerKitID, int entity, int hitbox, int nAdditionalDecalFlags )
{
	// Special case for world entity with hitbox:
	trace_t tr;
	if ( ( entity == 0 ) && ( hitbox != 0 ) )
	{
		Ray_t ray;
		ray.Init( *start, *pos );
		staticpropmgr->AddDecalToStaticProp( *start, *pos, hitbox - 1, nPlayerAndStickerKitID, false, tr, ( void * ) nPlayerAndStickerKitID, right, EDF_PLAYERSPRAY | nAdditionalDecalFlags );
	}
	else
	{
		// Only decal the world + brush models
		// Here we deal with decals on entities.
		CBaseEntity* ent;
		if ( ( ent = cl_entitylist->GetEnt( entity ) ) == nullptr )
			return;

		ent->AddDecal( *start, *pos, *pos, hitbox,
			nPlayerAndStickerKitID, false, tr, ADDDECAL_TO_ALL_LODS, right, EDF_PLAYERSPRAY | nAdditionalDecalFlags );
	}
}

void CFEPlayerDecal::SetDestroyedOnRecreateEntities()
{
	BaseClass::SetDestroyedOnRecreateEntities();

	if ( m_nUniqueID )
	{	// Remeber that we are going to be recreating this decal entity
		s_mapPlayerDecalsUniqueIDsRecreating.insert_or_assign( m_nUniqueID,
			s_mapPlayerDecalsUniqueIDsToApply.contains( m_nUniqueID ) );
	}
}

void CFEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	if ( m_nUniqueID )
	{
		if ( !s_mapPlayerDecalsUniqueIDsAll.contains( m_nUniqueID ) )
		{
			auto idxRecreate = s_mapPlayerDecalsUniqueIDsRecreating.find( m_nUniqueID );
			bool bApplyImmediate = true;
			bool bRecreating = ( idxRecreate != s_mapPlayerDecalsUniqueIDsRecreating.end() );
			if ( bRecreating )
			{
				bApplyImmediate = idxRecreate->second;
				s_mapPlayerDecalsUniqueIDsRecreating.erase( idxRecreate );
			}

			s_mapPlayerDecalsUniqueIDsAll.insert_or_assign( m_nUniqueID, this );
			if ( bApplyImmediate )
				s_mapPlayerDecalsUniqueIDsToApply.insert_or_assign( m_nUniqueID, this );
		}
	}

	// Make sure that we push render data to QMS thread every time we get data update about the decal
	m_bDecalReadyToApplyToWorld = BMakeDecalReadyToApplyToWorld();
}

struct CompileTimeConstant_cl_playerspray_debug_pulse_force { inline int GetInt() { return 0; } inline float GetFloat() { return 0; } } cl_playerspray_debug_pulse_force;

// Checks if the local player has an equipped spray and is aiming in a sprayable area with the rosetta menu up and if cooldown is ready
// Note: rosetta menu code is using this check to determine if we're passing all the validity checks to spray. 
bool Helper_CanShowPreviewDecal( CEconItemView **ppOutEconItemView = nullptr, trace_t* pOutSprayTrace = nullptr, Vector *pOutVecPlayerRight = nullptr, uint32* pOutUnStickerKitID = nullptr )
{
    return false; // TODO: implement with RocketUI

	CCSPlayer *pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	if ( !cl_playerspray_debug_pulse_force.GetInt() )
	{
		return false;

		// Check player spray cooldown
		if ( pLocalPlayer->GetNextDecalTime() > gpGlobals->curtime )
			return false;
	}

	Vector playerRight;
	trace_t sprayTrace;
	if ( pLocalPlayer->IsAbleToApplySpray( &sprayTrace, nullptr, &playerRight ) )
		return false;

	if ( pOutSprayTrace )
		*pOutSprayTrace = sprayTrace;

	if ( pOutVecPlayerRight )
		*pOutVecPlayerRight = playerRight;

	CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetLocalCSInventory();
	if ( !pPlayerInv )
		return false;

	CEconItemView* pEconItem = pPlayerInv->GetItemInLoadout( 0, LOADOUT_POSITION_SPRAY0 );
	if ( !pEconItem || !pEconItem->IsValid() )
		return false;

	if ( ppOutEconItemView )
		*ppOutEconItemView = pEconItem;

	uint32 unStickerKitID = pEconItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );
	if ( !unStickerKitID )
		return false;

	if ( pOutUnStickerKitID )
		*pOutUnStickerKitID = unStickerKitID;

	return true;
}

void UpdatePreviewDecal()
{
	uint32 unStickerKitID;
	trace_t sprayTrace;
	Vector playerRight;
	CEconItemView *pEconItem;
	if ( !Helper_CanShowPreviewDecal( &pEconItem, &sprayTrace, &playerRight, &unStickerKitID ) )
		return;

	static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );
	uint32 unTintID = 0;
	if ( !hAttrSprayTintID || !pEconItem->FindAttribute( hAttrSprayTintID, &unTintID ) )
		unTintID = 0;

	QcCreatePreviewDecal( unStickerKitID, unTintID, sprayTrace, &playerRight );
}

void OnPlayerDecalsUpdate()
{
	for ( const auto &[nUniqueID, pDecal] : s_mapPlayerDecalsUniqueIDsToApply )
	{
		pDecal->ApplyDecalDataToWorld();
	}
	s_mapPlayerDecalsUniqueIDsToApply.clear();

	s_mapPlayerDecalsUniqueIDsRecreating.clear();
}

bool CFEPlayerDecal::BMakeDecalReadyToApplyToWorld()
{
	CVProfScope _( "CFEPlayerDecal::BMakeDecalReadyToApplyToWorld", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Validate the signature before applying on the client
	ks::net::PlayerDecalDigitalSignature data;
	data.accountid = m_unAccountID;
	data.trace_id = m_unTraceID;
	data.rtime = m_rtGcTime;
	for ( int k = 0; k < 3; ++ k ) data.endpos.emplace_back( m_vecEndPos[k] );
	for ( int k = 0; k < 3; ++ k ) data.startpos.emplace_back( m_vecStart[k] );
	for ( int k = 0; k < 3; ++ k ) data.right.emplace_back( m_vecRight[k] );
	for ( int k = 0; k < 3; ++ k ) data.normal.emplace_back( m_vecNormal[k] );
	data.tx_defidx = m_nPlayer;
	data.entindex = m_nEntity;
	data.hitbox = m_nHitbox;
	data.tint_id = m_nTintID;
	data.creationtime = m_flCreationTime;
	if ( m_nVersion == PLAYERDECALS_SIGNATURE_VERSION )
		data.signature = std::string( ( const char * ) &m_ubSignature[0], PLAYERDECALS_SIGNATURE_BYTELEN );
	if ( !BValidateClientPlayerDecalSignature( data ) )
		return false;

	// Make the decal ready.
	const int nKey = MakeKey( m_nUniqueID );
	MakeDecalReady( nKey );

	return true;
}

void CFEPlayerDecal::ApplyDecalDataToWorld()
{
	if ( !m_bDecalReadyToApplyToWorld )
		return;

	CLocalPlayerFilter filter;
	const int nKey = MakeKey( m_nUniqueID );
	TE_PlayerDecal( filter, 0.0f, &m_vecEndPos, &m_vecStart, &m_vecRight, nKey, m_nEntity, m_nHitbox, 0 );
}

void QcCreateDecalData( int nKey, int nStickerKitDefinition, int nTintID, bool bDrips, float flCreationTime )
{
	const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinition( nStickerKitDefinition );
	if ( pStickerKit && !pStickerKit->sMaterialPath.empty() )
	{
		// TODO: We should convert this to be an async texture load once texture streaming is in.
		CFmtStr fmtTextureName( "decals/sprays/%s", bDrips ? pStickerKit->sMaterialPath.c_str() : pStickerKit->sMaterialPathNoDrips.c_str() );
		ITexture* texture = materials->FindTexture( fmtTextureName, TEXTURE_GROUP_DECAL, false );

		if ( !texture || texture->IsError() )
		{
			contract_assert( !"Failed to find a texture for this decal. We're never going to see it." );
			Warning( "Failed to find spray '%s', this is never going to render.\n", fmtTextureName.Access() );

			return;
		}

		texture->IncrementReferenceCount();

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ICallQueue* pCQ = pRenderContext->GetCallQueue();
		if ( pCQ )
			pCQ->QueueCall( CreateDecalData, nKey, texture, pStickerKit->nRarity, nTintID, flCreationTime );
		else
			CreateDecalData( nKey, texture, pStickerKit->nRarity, nTintID, flCreationTime );
	}
}

void QcCreatePreviewDecal( uint32 nStickerKitDefinition, uint32 nTintID, const trace_t& trace, const Vector* pRight )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue* pCQ = pRenderContext->GetCallQueue();
	if ( pCQ )
		pCQ->QueueCall( DeleteDecalData, g_nPlayerLogoProxyForPreviewKey );
	else
		DeleteDecalData( g_nPlayerLogoProxyForPreviewKey );

	bool bHasDrips = BShouldHaveDrips( trace.plane.normal );

	// Matches code in CCSPlayer::SprayPaint. Update that if you touch this.
	Vector startPos = trace.endpos + trace.plane.normal;

	CLocalPlayerFilter filter;
	QcCreateDecalData( g_nPlayerLogoProxyForPreviewKey, nStickerKitDefinition, nTintID, bHasDrips, gpGlobals->curtime - PLAYERDECALS_DURATION_APPLY );
	TE_PlayerDecal( filter, 0.0f, &trace.endpos, &startPos, pRight, g_nPlayerLogoProxyForPreviewKey, TraceGetEntityIndex( trace ), trace.hitbox, EDF_IMMEDIATECLEANUP );
}

IMaterial * QcCreateDecalDataForModelPreviewPanel( int nStickerKitDefinition, int nTintID )
{
	IMaterial *pMatStickerOverride = nullptr;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue* pCQ = pRenderContext->GetCallQueue();
	if ( pCQ )
		pCQ->QueueCall( DeleteDecalData, g_nPlayerLogoProxyForPreviewKey );
	else
		DeleteDecalData( g_nPlayerLogoProxyForPreviewKey );

	if ( nStickerKitDefinition > 0 && GetItemSchema() && GetItemSchema()->GetStickerKitDefinition( nStickerKitDefinition ) )
	{
		char const *szDesiredPreviewMaterialPath = "decals/playerlogo01_modelpreview.vmt";
		pMatStickerOverride = materials->FindMaterial( szDesiredPreviewMaterialPath, TEXTURE_GROUP_OTHER );
		if ( pMatStickerOverride->IsErrorMaterial() )
		{
			KeyValues *pSpecificStickerMaterialKeyValues = new KeyValues( "vmt" );
			KeyValues::AutoDelete autodelete_pSpecificStickerMaterialKeyValues( pSpecificStickerMaterialKeyValues );

			if ( pSpecificStickerMaterialKeyValues->LoadFromFile( g_pFullFileSystem, szDesiredPreviewMaterialPath, "GAME" ) )
			{
				pMatStickerOverride = materials->CreateMaterial( szDesiredPreviewMaterialPath, pSpecificStickerMaterialKeyValues );
			}

			autodelete_pSpecificStickerMaterialKeyValues.Detach();
		}
	}

	if ( !pMatStickerOverride || pMatStickerOverride->IsErrorMaterial() )
		return nullptr;

	QcCreateDecalData( g_nPlayerLogoProxyForPreviewKey, nStickerKitDefinition, nTintID, true, gpGlobals->curtime - PLAYERDECALS_DURATION_APPLY );

	return pMatStickerOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CFEPlayerDecal::MakeDecalReady( int nKey )
{
	QcCreateDecalData( nKey, m_nPlayer, m_nTintID, BShouldHaveDrips( m_vecNormal ), m_flCreationTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEPlayerDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Decals disabled?
	if ( !r_decals.GetBool() )
		return;

	CLocalPlayerFilter filter;
	TE_PlayerDecal(  filter, 0.0f, &m_vecOrigin, &m_vecStart, &m_vecRight, m_nPlayer, m_nEntity, m_nHitbox, 0 );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEPlayerDecal, DT_TEPlayerDecal, CTEPlayerDecal )

IMPLEMENT_REFLECT_CLIENTCLASS( CFEPlayerDecal, DT_FEPlayerDecal, CFEPlayerDecal )

//-----------------------------------------------------------------------------
// Purpose: material proxy
//-----------------------------------------------------------------------------
class CPlayerLogoProxy : public IMaterialProxy
{
public:
	CPlayerLogoProxy();

	virtual bool Init( IMaterial* pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release()
	{
		if ( m_pDefaultTexture )
		{
			m_pDefaultTexture->DecrementReferenceCount();
		}

		delete this;
	}

	virtual IMaterial *GetMaterial();

	virtual bool CanBeCalledAsync() const { return true; }

protected:
	virtual void OnLogoBindInternal( const DecalData_t& decalData, bool bPreviewMaterial );
	bool m_bInspectInModelPreviewWindow;
	bool m_bVertexLitMaterial;

private:
	IMaterialVar *m_pAlphaVar;
	IMaterialVar *m_pColorVar;
	IMaterialVar *m_pDetailBlendFactorVar;
	IMaterialVar *m_pBaseTextureVar;
	ITexture *m_pDefaultTexture;
};

CPlayerLogoProxy::CPlayerLogoProxy()
{
	m_bInspectInModelPreviewWindow = false;
	m_bVertexLitMaterial = false;
	m_pAlphaVar = nullptr;
	m_pColorVar = nullptr;
	m_pDetailBlendFactorVar = nullptr;
	m_pBaseTextureVar = nullptr;
	m_pDefaultTexture = nullptr;
}

bool CPlayerLogoProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool found = false;

	m_pAlphaVar = pMaterial->FindVar( "$alpha", &found );
	if ( !found )
		return false;
	if ( !m_pAlphaVar )
		return false;

	m_bVertexLitMaterial = pMaterial->IsVertexLit();
	// m_pColorVar = pMaterial->FindVar( pMaterial->IsVertexLit() ? "$color2" : "$color", &found );
	m_pColorVar = pMaterial->FindVar( "$color", &found );
	if ( !found )
		return false;
	if ( !m_pColorVar )
		return false;

	m_pDetailBlendFactorVar = pMaterial->FindVar( "$detailblendfactor", &found );
	if ( !found )
		return false;
	if ( !m_pDetailBlendFactorVar )
		return false;

	m_pBaseTextureVar = pMaterial->FindVar( "$basetexture", &found );
	if ( !found )
		return false;
	if ( !m_pBaseTextureVar )
		return false;

	m_pDefaultTexture = m_pBaseTextureVar->GetTextureValue();
	if ( !m_pDefaultTexture )
		return false;
	m_pDefaultTexture->IncrementReferenceCount();

	return true;
}

void CPlayerLogoProxy::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return; // dummy bind from mesh init

	if ( !m_pBaseTextureVar )
		return;

	DecalData_t dd;
	if ( ReadDecalData( ( int )( intp ) pC_BaseEntity, &dd ) )
	{
		bool bPreviewMaterial = ( g_nPlayerLogoProxyForPreviewKey == ( int )( intp ) pC_BaseEntity );
		OnLogoBindInternal( dd, bPreviewMaterial );
		return;
	}

	m_pBaseTextureVar->SetTextureValue( m_pDefaultTexture );
	m_pAlphaVar->SetFloatValue( 0.0f );
	m_pColorVar->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	m_pDetailBlendFactorVar->SetFloatValue( 0.0f );
}

struct CompileTimeConstant_cl_playerspray_debug_pulse_timescale { inline int GetInt() { return 0.6; } inline float GetFloat() { return 0.6; } } cl_playerspray_debug_pulse_timescale;
struct CompileTimeConstant_cl_playerspray_debug_pulse_alpha_low { inline int GetInt() { return 0.125; } inline float GetFloat() { return 0.125; } } cl_playerspray_debug_pulse_alpha_low;
struct CompileTimeConstant_cl_playerspray_debug_pulse_alpha_fraction { inline int GetInt() { return 1.0; } inline float GetFloat() { return 1.0; } } cl_playerspray_debug_pulse_alpha_fraction;

void CPlayerLogoProxy::OnLogoBindInternal( const DecalData_t& decalData, bool bPreviewMaterial )
{
	contract_assert( decalData.m_pTex );

	m_pBaseTextureVar->SetTextureValue( decalData.m_pTex );

	// Drive alpha to expiration (but not outside of creation time)
	float flRenderAlpha = 0;
	float flDetailBlendFactor = 0;
	if ( gpGlobals->curtime >= decalData.m_flCreationTime )
	{
		float flDecalTime = gpGlobals->curtime;
		if ( !m_bInspectInModelPreviewWindow && bPreviewMaterial )
		{
			flRenderAlpha = flDecalTime * cl_playerspray_debug_pulse_timescale.GetFloat(); // make the full cycle longer than 1 second
			flRenderAlpha -= std::floorf( flRenderAlpha ); // [0..1)[0..1)
			flRenderAlpha = std::fabsf( flRenderAlpha - 0.5f ); // 0.5..0..0.5..0..0.5
			flRenderAlpha = cl_playerspray_debug_pulse_alpha_low.GetFloat() + flRenderAlpha/cl_playerspray_debug_pulse_alpha_fraction.GetFloat(); // 12.5% ... 63% opacity cycle over 0.85 sec
		}
		else if ( gpGlobals->curtime >= decalData.m_flCreationTime + PLAYERDECALS_DURATION_APPLY )
		{
			float flFadeStart = decalData.m_flCreationTime + PLAYERDECALS_DURATION_SOLID;
			float flFadeEnd = flFadeStart + PLAYERDECALS_DURATION_FADE2;
			float flFadeAlphaStart = flFadeEnd - PLAYERDECALS_DURATION_FADE1;
			flDetailBlendFactor = RemapValClamped( flDecalTime, flFadeStart, flFadeEnd, 0.0f, 1.0f );
			flRenderAlpha = RemapValClamped( flDecalTime, flFadeAlphaStart, flFadeEnd, 1.0f, 0.0f );
		}
		else
		{
			float flFadeStart = decalData.m_flCreationTime;
			float flFadeEnd = flFadeStart + PLAYERDECALS_DURATION_APPLY;
			float flFadeAlphaStart = flFadeStart + PLAYERDECALS_DURATION_APPLY/2;
			flDetailBlendFactor = RemapValClamped( flDecalTime, flFadeStart, flFadeEnd, 1.0f, 0.0f );
			flRenderAlpha = RemapValClamped( flDecalTime, flFadeStart, flFadeAlphaStart, 0.0f, 1.0f );
		}
	}

	//
	// Convert tint HSV to RGB
	//
	float flRenderRGB[ 3 ] = { 1.0f, 1.0f, 1.0f };
	uint8 unRenderHSVID = CombinedTintIDGetHSVID( decalData.m_nTintID );
	if ( unRenderHSVID )
	{
		if ( const CEconGraffitiTintDefinition *pDef = GEconItemSchema().GetGraffitiTintDefinitionByID( unRenderHSVID ) )
		{
			uint32 uiRGB = pDef->GetHexColorRGB();
			flRenderRGB[ 0 ] = float( ( uiRGB >> 16 ) & 0xFF ) / float( 0xFF );
			flRenderRGB[ 1 ] = float( ( uiRGB >> 8 ) & 0xFF ) / float( 0xFF );
			flRenderRGB[ 2 ] = float( ( uiRGB ) & 0xFF ) / float( 0xFF );
		}
	}

	// Do srgb color transform when applied to brush models via lightmapped generic
	if ( !m_bVertexLitMaterial && !m_bInspectInModelPreviewWindow )
	{
		for ( int j = 0; j < 3; ++j )
		{
			flRenderRGB[ j ] = SrgbGammaToLinear( flRenderRGB[ j ] );
		}
	}
	m_pColorVar->SetVecValue( flRenderRGB[ 0 ], flRenderRGB[ 1 ], flRenderRGB[ 2 ], 1.0f );

	m_pAlphaVar->SetFloatValue( flRenderAlpha );
	m_pDetailBlendFactorVar->SetFloatValue( flDetailBlendFactor );
}

IMaterial *CPlayerLogoProxy::GetMaterial()
{
	return m_pBaseTextureVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CPlayerLogoProxy, PlayerLogo );

class CPlayerLogoProxyForModelWeaponPreviewPanel : public CPlayerLogoProxy
{
public:
	CPlayerLogoProxyForModelWeaponPreviewPanel()
	{
		m_bInspectInModelPreviewWindow = true;
	}
	virtual void OnBind( void *pC_BaseEntity ) override
	{
		CPlayerLogoProxy::OnBind( ( void * ) ( intp ) g_nPlayerLogoProxyForPreviewKey );
	}
};

EXPOSE_MATERIAL_PROXY( CPlayerLogoProxyForModelWeaponPreviewPanel, PlayerLogoModelPreview );

class CBaseEntity;

using namespace ::ks::reflect;

// UNDONE:  Get rid of this?
#define FDECAL_PERMANENT			0x01

//-----------------------------------------------------------------------------
// Purpose: Projected Decal TE
//-----------------------------------------------------------------------------
class CTEProjectedDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEProjectedDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEProjectedDecal, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEProjectedDecal( void );
	virtual			~CTEProjectedDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::QAngles } ]] QAngle			m_angRotation;
	[[= ks::reflect::Net{} ]] float			m_flDistance;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEProjectedDecal, DT_TEProjectedDecal, CTEProjectedDecal )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEProjectedDecal::CTEProjectedDecal( void )
{
	m_vecOrigin.Init();
	m_angRotation.Init();
	m_flDistance = 0.0f;
	m_nIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEProjectedDecal::~CTEProjectedDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEProjectedDecal::Precache( void )
{
}

void TE_ProjectDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const QAngle *angles, float distance, int index )
{

	trace_t	tr;

	Vector fwd;
	AngleVectors( *angles, &fwd );

	Vector endpos;
	VectorMA( *pos, distance, fwd, endpos );

	CTraceFilterHitAll traceFilter;
	UTIL_TraceLine( *pos, endpos, MASK_ALL, &traceFilter, &tr );

	if ( tr.fraction == 1.0f )
	{
		return;
	}

	CBaseEntity* ent = tr.Ent<CBaseEntity>();
	contract_assert( ent );

	int hitbox = tr.hitbox;

	if ( tr.hitbox != 0 )
	{
		staticpropmgr->AddDecalToStaticProp( *pos, endpos, hitbox - 1, index, false, tr );
	}
	else
	{
		// Only decal the world + brush models
		ent->AddDecal( *pos, endpos, endpos, hitbox, 
			index, false, tr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEProjectedDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEProjectedDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBroadcastRecipientFilter filter;
	TE_ProjectDecal( filter, 0.0f, &m_vecOrigin, &m_angRotation, m_flDistance, m_nIndex );
}

//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------
void TE_ProjectDecal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	QAngle angles;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	angles.x = pKeyValues->GetFloat( "anglesx" );
	angles.y = pKeyValues->GetFloat( "anglesy" );
	angles.z = pKeyValues->GetFloat( "anglesz" );
	float flDistance = pKeyValues->GetFloat( "distance" );
	const char *pDecalName = pKeyValues->GetString( "decalname" );

	TE_ProjectDecal( filter, 0.0f, &vecOrigin, &angles, flDistance, effects->Draw_DecalIndexFromName( (char*)pDecalName ) );
}

#undef FDECAL_PERMANENT

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Show Line TE
//-----------------------------------------------------------------------------
class CTEShowLine : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEShowLine" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEShowLine, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

					CTEShowLine( void );
	virtual			~CTEShowLine( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecEnd;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEShowLine::CTEShowLine( void )
{
	m_vecEnd.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEShowLine::~CTEShowLine( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void CTEShowLine::PostDataUpdate( DataUpdateType_t updateType )
{
	Vector		vec;
	float		len;
	StandardParticle_t	*p;
	int			dec;
	static int	tracercount;

	VectorSubtract (m_vecEnd, m_vecOrigin, vec);
	len = VectorNormalize (vec);

	dec = 3;

	VectorScale(vec, dec, vec);

	ks::intrusive_ptr<CTEParticleRenderer> pRen = CTEParticleRenderer::Create( "TEShowLine", m_vecOrigin );
	if( !pRen )
		return;
	
	while (len > 0)
	{
		len -= dec;

		p = pRen->AddParticle();
		if ( p )
		{
			p->m_Velocity.Init();

			pRen->SetParticleLifetime(p, 30);
			
			p->SetColor(0, 1, 1);
			p->SetAlpha(1);
			pRen->SetParticleType(p, pt_static);

			p->m_Pos = m_vecOrigin;
			
			m_vecOrigin += vec;
		}
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEShowLine, DT_TEShowLine, CTEShowLine )

void TE_ShowLine( IRecipientFilter& filter, float delay,
	const Vector* start, const Vector* end )
{
	// Major hack to simulate receiving network message
	__g_CTEShowLine.m_vecOrigin = *start;
	__g_CTEShowLine.m_vecEnd = *end;

	__g_CTEShowLine.PostDataUpdate( DATA_UPDATE_CREATED );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Smoke TE
//-----------------------------------------------------------------------------
class CTESmoke : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TESmoke" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESmoke, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTESmoke( void );
	virtual			~CTESmoke( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] float			m_fScale;
	[[= ks::reflect::Net{} ]] int				m_nFrameRate;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESmoke::CTESmoke( void )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nFrameRate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESmoke::~CTESmoke( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTESmoke::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTESmoke::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// The number passed down is 10 times smaller...
	g_pEffects->Smoke( m_vecOrigin, m_nModelIndex, m_fScale * 10.0f, m_nFrameRate );
}

void TE_Smoke( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float scale, int framerate )
{
	// The number passed down is 10 times smaller...
	g_pEffects->Smoke( *pos, modelindex, scale * 10.0f, framerate );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTESmoke, DT_TESmoke, CTESmoke )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Sparks TE
//-----------------------------------------------------------------------------
class CTESparks : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TESparks" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESparks, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

					CTESparks( void );
	virtual			~CTESparks( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	Precache( void );

	[[= ks::reflect::Net{} ]] int m_nMagnitude;
	[[= ks::reflect::Net{} ]] int m_nTrailLength;
	[[= ks::reflect::Net{} ]] Vector m_vecDir;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESparks::CTESparks( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESparks::~CTESparks( void )
{
}

void CTESparks::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTESparks::PostDataUpdate( DataUpdateType_t updateType )
{
	g_pEffects->Sparks( m_vecOrigin, m_nMagnitude, m_nTrailLength, &m_vecDir );
}

void TE_Sparks( IRecipientFilter& filter, float delay,
	const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir )
{
	g_pEffects->Sparks( *pos, nMagnitude, nTrailLength, pDir );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTESparks, DT_TESparks, CTESparks )

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Sprite TE
//-----------------------------------------------------------------------------
class CTESprite : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TESprite" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESprite, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTESprite( void );
	virtual			~CTESprite( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] float			m_fScale;
	[[= ks::reflect::Net{} ]] int				m_nBrightness;
};

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTESprite, DT_TESprite, CTESprite )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESprite::CTESprite( void )
{
	m_vecOrigin.Init();
	m_nModelIndex = 0;
	m_fScale = 0;
	m_nBrightness = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESprite::~CTESprite( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTESprite::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTESprite::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	float a = ( 1.0 / 255.0 ) * m_nBrightness;
	tempents->TempSprite( m_vecOrigin, vec3_origin, m_fScale, m_nModelIndex, kRenderTransAdd, 0, a, 0, FTENT_SPRANIMATE );
}

void TE_Sprite( IRecipientFilter& filter, float delay,
	const Vector* pos, int modelindex, float size, int brightness )
{
	float a = ( 1.0 / 255.0 ) * brightness;
	tempents->TempSprite( *pos, vec3_origin, size, modelindex, kRenderTransAdd, 0, a, 0, FTENT_SPRANIMATE );
}

void TE_Sprite( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecDirection;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	float flScale = pKeyValues->GetFloat( "scale" );
	int nBrightness = pKeyValues->GetInt( "brightness" );

	TE_Sprite( filter, delay, &vecOrigin, nModelIndex, flScale, nBrightness );
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Sprite Spray TE
//-----------------------------------------------------------------------------
class CTESpriteSpray : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TESpriteSpray" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTESpriteSpray, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTESpriteSpray( void );
	virtual			~CTESpriteSpray( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] Vector			m_vecDirection;
	[[= ks::reflect::Net{} ]] int				m_nModelIndex;
	[[= ks::reflect::Net{} ]] int				m_nSpeed;
	[[= ks::reflect::Net{} ]] float			m_fNoise;
	[[= ks::reflect::Net{} ]] int				m_nCount;
};

//-----------------------------------------------------------------------------
// Networking 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTESpriteSpray, DT_TESpriteSpray, CTESpriteSpray )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESpriteSpray::CTESpriteSpray( void )
{
	m_vecOrigin.Init();
	m_vecDirection.Init();
	m_nModelIndex = 0;
	m_fNoise = 0;
	m_nSpeed = 0;
	m_nCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTESpriteSpray::~CTESpriteSpray( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTESpriteSpray::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTESpriteSpray::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	tempents->Sprite_Spray( m_vecOrigin, m_vecDirection, m_nModelIndex, m_nCount, m_nSpeed * 0.2, m_fNoise * 100.0 );
}

void TE_SpriteSpray( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count )
{
	tempents->Sprite_Spray( *pos, *dir, modelindex, count, speed * 0.2, noise * 100.0 );
}

void TE_SpriteSpray( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin, vecDirection;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	vecDirection.x = pKeyValues->GetFloat( "directionx" );
	vecDirection.y = pKeyValues->GetFloat( "directiony" );
	vecDirection.z = pKeyValues->GetFloat( "directionz" );
	const char *pModelName = pKeyValues->GetString( "model" );
	int nModelIndex = pModelName[0] ? modelinfo->GetModelIndex( pModelName ) : 0;
	int nSpeed = pKeyValues->GetInt( "speed" );
	float flNoise = pKeyValues->GetFloat( "noise" );
	int nCount = pKeyValues->GetInt( "count" );

	TE_SpriteSpray( filter, delay, &vecOrigin, &vecDirection, nModelIndex, nSpeed, flNoise, nCount );
}

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: World Decal TE
//-----------------------------------------------------------------------------
class CTEWorldDecal : public CBaseTempEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TEWorldDecal" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEWorldDecal, CBaseTempEntity );
	DECLARE_CLIENTCLASS();

					CTEWorldDecal( void );
	virtual			~CTEWorldDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	[[= ks::reflect::Net{} ]] Vector			m_vecOrigin;
	[[= ks::reflect::Net{} ]] int				m_nIndex;
};

//-----------------------------------------------------------------------------
// Networking 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEWorldDecal, DT_TEWorldDecal, CTEWorldDecal )

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CTEWorldDecal::CTEWorldDecal( void )
{
	m_vecOrigin.Init();
	m_nIndex = 0;
}

CTEWorldDecal::~CTEWorldDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEWorldDecal::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTEWorldDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "CTEWorldDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( r_decals.GetInt() )
	{
		CBaseEntity *ent = cl_entitylist->GetEnt( 0 );
		if ( ent )
		{
			effects->DecalShoot( m_nIndex, 0, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), m_vecOrigin, 0, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Client-side effects
//-----------------------------------------------------------------------------
void TE_WorldDecal( IRecipientFilter& filter, float delay, const Vector* pos, int index )
{
	if ( r_decals.GetInt() )
	{
		CBaseEntity *ent = cl_entitylist->GetEnt( 0 );
		if ( ent )
		{
			effects->DecalShoot( index, 0, ent->GetModel(), ent->GetAbsOrigin(), ent->GetAbsAngles(), *pos, 0, 0 );
		}
	}
}

void TE_WorldDecal( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	Vector vecOrigin;
	vecOrigin.x = pKeyValues->GetFloat( "originx" );
	vecOrigin.y = pKeyValues->GetFloat( "originy" );
	vecOrigin.z = pKeyValues->GetFloat( "originz" );
	const char *pDecalName = pKeyValues->GetString( "decalname" );

	TE_WorldDecal( filter, 0.0f, &vecOrigin, effects->Draw_DecalIndexFromName( (char*)pDecalName ) );
}
