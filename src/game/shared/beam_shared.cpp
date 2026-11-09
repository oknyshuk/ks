// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
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
#include "debugoverlay_shared.h"
#elif defined(GAME_DLL)
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
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#endif
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#else
#include "reflect_sendtable.h"
#endif
#include "beam_shared.h"
#include "decals.h"
#include "model_types.h"
#include "IEffects.h"
#include "util_shared.h"

#if !defined( CLIENT_DLL )
#include "ndebugoverlay.h"
#include "sendproxy.h"
#else
#include "iviewrender_beams.h"
#include "c_pixel_visibility.h"
#include "iclientmode.h"
#include "viewrender.h"
#include "view.h"
#include "toolframework_client.h"
#endif

constexpr auto BEAM_DEFAULT_HALO_SCALE = 10;

#if !defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// info targets are like point entities except you can force them to spawn on the client
//-----------------------------------------------------------------------------
class CInfoTarget : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoTarget, CPointEntity );

	void	Spawn_Impl( void );
	KS_VIRTUAL int UpdateTransmitState_Impl();
};


//-----------------------------------------------------------------------------
// Force transmission
//-----------------------------------------------------------------------------
void CInfoTarget::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	if ( HasSpawnFlags(0x01) )
	{
		SetEFlags( EFL_FORCE_CHECK_TRANSMIT );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Always transmitted to clients
//-----------------------------------------------------------------------------
int CInfoTarget::UpdateTransmitState_Impl()
{
	// Spawn flags 2 means we always transmit
	if ( HasSpawnFlags(0x02) )
		return SetTransmitState( FL_EDICT_ALWAYS );
	return BaseClass::UpdateTransmitState_Impl();
}


LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );
#endif


//-----------------------------------------------------------------------------
// Purpose: Returns true if the given entity is a fixed target for lightning.
//-----------------------------------------------------------------------------
bool IsStaticPointEntity( CBaseEntity *pEnt )
{
	if ( pEnt->GetMoveParent() )
		return false;

	if ( !pEnt->GetModelIndex() )
		return 1;

	if ( FClassnameIs( pEnt, "info_target" ) || FClassnameIs( pEnt, "info_landmark" ) || 
		FClassnameIs( pEnt, "path_corner" ) )
		return true;

	return false;
}

#if defined( CLIENT_DLL )
extern bool ComputeBeamEntPosition( CBaseEntity *pEnt, int nAttachment, bool bInterpretAttachmentIndexAsHitboxIndex, Vector& pt );
extern void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Beam_ScrollSpeed( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_Beam *beam;
	float	val;

	// Unpack the data.
	val	= pData->m_Value.m_Float;
	val *= 0.1;

	beam = ( C_Beam * )pStruct;
	contract_assert( pOut == &beam->m_fSpeed );

	beam->m_fSpeed = val;
}
#else
#endif

// This table encodes the CBeam data.
IMPLEMENT_NETWORKCLASS_ALIASED( Beam, DT_Beam )
LINK_ENTITY_TO_CLASS_ALIASED( beam, Beam );


IMPLEMENT_REFLECT_TABLE( CBeam, DT_Beam );

#if !defined( CLIENT_DLL )
IMPLEMENT_REFLECT_DATAMAP( CBeam )

#else

IMPLEMENT_REFLECT_PREDMAP( CBeam );

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBeam::CBeam( void )
{
#ifdef _DEBUG
	// necessary since in debug, we initialize vectors to NAN for debugging
	m_vecEndPos.Init();
#endif

	m_flHDRColorScale = 1.0f; // default value.

#if !defined( CLIENT_DLL )
	m_nDissolveType = -1;
#else
	m_queryHandleHalo = 0;
	AddToEntityList(ENTITY_LIST_SIMULATE);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szModelName - 
//-----------------------------------------------------------------------------
void CBeam::SetModel_Impl( const char *szModelName )
{
	int modelIndex = modelinfo->GetModelIndex( szModelName );
	const model_t *model = modelinfo->GetModel( modelIndex );
	if ( model && modelinfo->GetModelType( model ) != mod_sprite )
	{
		Msg( "Setting CBeam to non-sprite model %s\n", szModelName );
	}
#if !defined( CLIENT_DLL )
	UTIL_SetModel( this, szModelName );
#else
	BaseClass::SetModel_Impl( szModelName );
#endif
}


void CBeam::Spawn_Impl( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetRenderMode( kRenderTransTexture );

	// Opt out of all shadow routines
	// Beams are highly visible; always render these into reflection buffer even when using fast reflection
	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW | EF_MARKED_FOR_FAST_REFLECTION );

	Precache( );
}


void CBeam::Precache_Impl( void )
{
	if ( GetOwnerEntity() )
	{
		SetStartEntity( GetOwnerEntity() );
	}
	
	if ( m_hEndEntity.Get() )
	{
		SetEndEntity( m_hEndEntity );
	}
}


void CBeam::SetType( int type )		
{ 
	contract_assert( type < NUM_BEAM_TYPES );
	m_nBeamType = type;
}

void CBeam::SetBeamFlags( int flags )	
{ 
	contract_assert( flags < (1 << NUM_BEAM_FLAGS) );
	m_nBeamFlags = flags;
}

void CBeam::SetBeamFlag( int flag )		
{ 
	m_nBeamFlags |= flag;
}

int CBeam::GetType( void ) const 
{ 
	return m_nBeamType;
}

int CBeam::GetBeamFlags( void ) const
{ 
	return m_nBeamFlags;
}

void CBeam::SetStartEntity( CBaseEntity *pEntity )
{ 
	contract_assert( m_nNumBeamEnts >= 2 );
	m_hAttachEntity.Set( 0, pEntity );
	SetOwnerEntity( pEntity );
	RelinkBeam();
	if ( pEntity )
	{
		pEntity->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	}
}

void CBeam::SetEndEntity( CBaseEntity *pEntity ) 
{ 
	contract_assert( m_nNumBeamEnts >= 2 );
	m_hAttachEntity.Set( m_nNumBeamEnts-1, pEntity );
	m_hEndEntity = pEntity;
	RelinkBeam();
	if ( pEntity )
	{
		pEntity->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	}
}


//-----------------------------------------------------------------------------
// This will change things so the abs position matches the requested spot
//-----------------------------------------------------------------------------
void CBeam::SetAbsStartPos( const Vector &pos )
{
	if (!GetMoveParent())
	{
		SetStartPos( pos );
		return;
	}

	Vector vecLocalPos;
	matrix3x4_t worldToBeam;
	MatrixInvert( EntityToWorldTransform(), worldToBeam );
	VectorTransform( pos, worldToBeam, vecLocalPos );
	SetStartPos( vecLocalPos );
}

void CBeam::SetAbsEndPos( const Vector &pos )
{
	if (!GetMoveParent())
	{
		SetEndPos( pos );
		return;
	}

	Vector vecLocalPos;
	matrix3x4_t worldToBeam;
	MatrixInvert( EntityToWorldTransform(), worldToBeam );
	VectorTransform( pos, worldToBeam, vecLocalPos );
	SetEndPos( vecLocalPos );
}

#if !defined( CLIENT_DLL )

// These don't take attachments into account
const Vector &CBeam::GetAbsStartPos( void ) const
{
	if ( GetType() == BEAM_ENTS && GetStartEntity() )
	{
		edict_t *pent =  INDEXENT( GetStartEntity() );
		CBaseEntity *ent = CBaseEntity::Instance( pent );
		if ( !ent )
		{
			return GetAbsOrigin();
		}
		return ent->GetAbsOrigin();
	}
	return GetAbsOrigin();
}


const Vector &CBeam::GetAbsEndPos( void ) const
{
	if ( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE && GetEndEntity() ) 
	{
		edict_t *pent = INDEXENT( GetEndEntity() );
		CBaseEntity *ent = CBaseEntity::Instance( pent );
		if ( ent )
			return ent->GetAbsOrigin();
	}

	if (!const_cast<CBeam*>(this)->GetMoveParent())
		return m_vecEndPos.Get();

	// FIXME: Cache this off?
	static Vector vecAbsPos;
	VectorTransform( m_vecEndPos, EntityToWorldTransform(), vecAbsPos );
	return vecAbsPos;
}

#else

//-----------------------------------------------------------------------------
// Unlike the server, these take attachments into account
//-----------------------------------------------------------------------------
const Vector &C_Beam::GetAbsStartPos( void ) const
{
	static Vector vecStartAbsPosition;
	if ( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE ) 
	{
		if (ComputeBeamEntPosition( m_hAttachEntity[0], m_nAttachIndex[0], false, vecStartAbsPosition ))
			return vecStartAbsPosition;
	}

	return GetAbsOrigin();
}


const Vector &C_Beam::GetAbsEndPos( void ) const
{
	static Vector vecEndAbsPosition;
	if ( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE ) 
	{
		if (ComputeBeamEntPosition( m_hAttachEntity[m_nNumBeamEnts-1], m_nAttachIndex[m_nNumBeamEnts-1], false, vecEndAbsPosition ))
			return vecEndAbsPosition;
	}

	if (!const_cast<C_Beam*>(this)->GetMoveParent())
		return m_vecEndPos.Get();

	// FIXME: Cache this off?
	VectorTransform( m_vecEndPos, EntityToWorldTransform(), vecEndAbsPosition );
	return vecEndAbsPosition;
}
#endif

CBeam *CBeam::BeamCreate( const char *pSpriteName, float width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam  = CREATE_ENTITY( CBeam, "beam" );
	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSprite
//-----------------------------------------------------------------------------
CBeam *CBeam::BeamCreatePredictable( const char *module, int line, bool persist, const char *pSpriteName, float width, CBasePlayer *pOwner )
{
	CBeam *pBeam = ( CBeam * )CBaseEntity::CreatePredictedEntityByName( "beam", module, line, persist );
	if ( pBeam )
	{
		pBeam->BeamInit( pSpriteName, width );
		pBeam->SetOwnerEntity( pOwner );
		pBeam->SetPlayerSimulated( pOwner );
	}

	return pBeam;
}

void CBeam::BeamInit( const char *pSpriteName, float width )
{
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	SetModelName( MAKE_STRING( pSpriteName ) );
	SetRenderMode( kRenderTransTexture );
	SetTexture( PrecacheModel( pSpriteName ) );
	SetWidth( width );
	SetEndWidth( width );
	SetFadeLength( 0 );			// No fade

	Spawn();

	for (int i=0;i<MAX_BEAM_ENTS;i++)
	{
		m_hAttachEntity.Set( i, INVALID_EHANDLE );
		m_nAttachIndex.Set( i, 0 );
	}
	m_nHaloIndex	= 0;
	m_fHaloScale	= BEAM_DEFAULT_HALO_SCALE;
	m_nBeamType		= 0;
	m_nBeamFlags    = 0;
}


void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	m_nNumBeamEnts = 2;
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	m_nNumBeamEnts = 2;
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::PointEntInit( const Vector &start, CBaseEntity *pEndEntity )
{
	SetType( BEAM_ENTPOINT );
	m_nNumBeamEnts = 2;
	SetStartPos( start );
	SetEndEntity( pEndEntity );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( CBaseEntity *pStartEntity, CBaseEntity *pEndEntity )
{
	SetType( BEAM_ENTS );
	m_nNumBeamEnts = 2;
	SetStartEntity( pStartEntity );
	SetEndEntity( pEndEntity );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::LaserInit( CBaseEntity *pStartEntity, CBaseEntity *pEndEntity )
{
	SetType( BEAM_LASER );
	m_nNumBeamEnts = 2;
	SetStartEntity( pStartEntity );
	SetEndEntity( pEndEntity );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::SplineInit( int nNumEnts, CBaseEntity** pEntList, int *attachment )
{
	if (nNumEnts < 2)
	{
		Msg("ERROR: Min of 2 ents required for spline beam.\n");
	}
	else if (nNumEnts > MAX_BEAM_ENTS)
	{
		Msg("ERROR: Max of %i ents allowed for spline beam.\n",MAX_BEAM_ENTS);
	}
	SetType( BEAM_SPLINE );

	for (int i=0;i<nNumEnts;i++)
	{
		m_hAttachEntity.Set( i, pEntList[i] ); 
		m_nAttachIndex.Set( i, attachment[i] );
	}
	m_nNumBeamEnts = nNumEnts;
	RelinkBeam();
}


void CBeam::RelinkBeam( void )
{
	// FIXME: Why doesn't this just define the absbox too?
	// It seems that we don't need to recompute the absbox
	// in CBaseEntity::SetObjectCollisionBox, in fact the absbox
	// computed there seems way too big
	Vector startPos = GetAbsStartPos(), endPos = GetAbsEndPos();

	// UNDONE: Should we do this to make the boxes smaller?
	//SetAbsOrigin( startPos );

	Vector vecBeamMin, vecBeamMax;
	VectorMin( startPos, endPos, vecBeamMin );
	VectorMax( startPos, endPos, vecBeamMax );

	SetCollisionBounds( vecBeamMin - GetAbsOrigin(), vecBeamMax - GetAbsOrigin() );
}


CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
#if !defined( CLIENT_DLL )
	int total = 0;

	CBaseEntity *pEntity = nullptr;
	CBaseEntity *pNewEntity = nullptr;
	while ((pNewEntity = gEntList.FindEntityByName( pNewEntity, szName )) != nullptr)
	{
		total++;
		if (random->RandomInt(0,total-1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
#else
	return nullptr;
#endif
}


void CBeam::DoSparks( const Vector &start, const Vector &end )
{
#if !defined( CLIENT_DLL )
	if ( HasSpawnFlags(SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND) )
	{
		if ( HasSpawnFlags( SF_BEAM_SPARKSTART ) )
		{
			g_pEffects->Sparks( start );
		}
		if ( HasSpawnFlags( SF_BEAM_SPARKEND ) )
		{
			g_pEffects->Sparks( end );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Damages anything in the beam.
// Input  : ptr - 
//-----------------------------------------------------------------------------
void CBeam::BeamDamage( trace_t *ptr )
{
	RelinkBeam();
#if !defined( CLIENT_DLL )
	if ( ptr->fraction != 1.0 && ptr->Ent<CBaseEntity>() != nullptr )
	{
		CBaseEntity *pHit = ptr->Ent<CBaseEntity>();
		if ( pHit )
		{
			ClearMultiDamage();
			Vector dir = ptr->endpos - GetAbsOrigin();
			VectorNormalize( dir );
			int nDamageType = DMG_ENERGYBEAM;

			if (m_nDissolveType == 0)
			{
				nDamageType = DMG_DISSOLVE;
			}
			else if ( m_nDissolveType > 0 )
			{
				nDamageType = DMG_DISSOLVE | DMG_SHOCK; 
			}

			CTakeDamageInfo info( this, this, m_flDamage * (gpGlobals->curtime - m_flFireTime), nDamageType );
			CalculateMeleeDamageForce( &info, dir, ptr->endpos );
			pHit->DispatchTraceAttack( info, dir, ptr );
			ApplyMultiDamage();
			if ( HasSpawnFlags( SF_BEAM_DECALS ) )
			{
				if ( pHit->IsBSPModel() )
				{
					UTIL_DecalTrace( ptr, GetDecalName() );
				}
			}
		}
	}
#endif
	m_flFireTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeam::TurnOn( void )
{
	AddEffects( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBeam::TurnOff( void )
{
	RemoveEffects( EF_NODRAW );
}

#if !defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: Input handler for the beam width. Sets the end width based on the
//			beam width.
// Input  : Beam width in tenths of world units.
//-----------------------------------------------------------------------------
void CBeam::InputWidth( inputdata_t &inputdata )
{
	SetWidth( inputdata.value.Float() );
	SetEndWidth( inputdata.value.Float() );
}

void CBeam::InputColorRedValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( nNewColor, m_clrRender->g, m_clrRender->b );
}

void CBeam::InputColorGreenValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( m_clrRender->r, nNewColor, m_clrRender->b );
}

void CBeam::InputColorBlueValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( m_clrRender->r, m_clrRender->g, nNewColor );
}

void CBeam::InputNoise( inputdata_t &inputdata )
{
	SetNoise( inputdata.value.Float() );
}

int CBeam::UpdateTransmitState_Impl( void )
{
	// we must call ShouldTransmit() if we have a move parent
	if ( GetMoveParent() )
		return SetTransmitState( FL_EDICT_FULLCHECK );

	return BaseClass::UpdateTransmitState_Impl( );
}

void CBeam::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );
	
	// Force our attached entities to go too...
	for ( int i=0; i < MAX_BEAM_ENTS; ++i )
	{
		if ( m_hAttachEntity[i].Get() )
		{
			m_hAttachEntity[i]->SetTransmit( pInfo, bAlways );
		}
	}
}

int CBeam::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return FL_EDICT_DONTSEND;

	// Transmit us with the same rules as our move parent
	if ( GetMoveParent() )
	{
		return GetMoveParent()->ShouldTransmit( pInfo );
	}

	return BaseClass::ShouldTransmit_Impl( pInfo );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays.
// Output : Returns the new text offset from the top.
//-----------------------------------------------------------------------------
int CBeam::DrawDebugTextOverlays_Impl(void)
{
#if !defined( CLIENT_DLL )
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();
	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		// Print state
		char tempstr[512];
		V_snprintf(tempstr, sizeof(tempstr), "start: (%.2f,%.2f,%.2f)", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr, sizeof(tempstr), "end  : (%.2f,%.2f,%.2f)", m_vecEndPos.GetX(), m_vecEndPos.GetY(), m_vecEndPos.GetZ());
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	
	return text_offset;
#else
	return 0;
#endif
}

#if defined( CLIENT_DLL )

// Purpose: 
// Input  : isbeingremoved - 
//			*predicted - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBeam::OnPredictedEntityRemove( bool isbeingremoved, C_BaseEntity *predicted )
{
	BaseClass::OnPredictedEntityRemove( isbeingremoved, predicted );

	CBeam *beam = dynamic_cast< CBeam * >( predicted );
	if ( !beam )
	{
		// Hrm, we didn't link up to correct type!!!
		contract_assert( 0 );
		// Delete right away since it's fucked up
		return true;
	}

	if ( beam->IsEFlagSet( EFL_KILLME ) )
	{
		// Don't delete right away
		AddEFlags( EFL_KILLME );
		return false;
	}

	// Go ahead and delete if it's not short-lived
	return true;
}

extern bool g_bRenderingScreenshot;
extern ConVar r_drawviewmodel;

int CBeam::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( IsMarkedForDeletion() )
		return 0;

	if ( CurrentViewID() == VIEW_SHADOW_DEPTH_TEXTURE )
		return 0;

	// Tracker 16432:  If rendering a savegame screenshot don't draw beams 
	//   who have viewmodels as their attached entity
	if ( g_bRenderingScreenshot || !r_drawviewmodel.GetBool() )
	{
		// If the beam is attached
		for (int i=0;i<MAX_BEAM_ENTS;i++)
		{
			C_BaseViewModel *vm = ToBaseViewModel(m_hAttachEntity[i].Get());
			if ( vm )
			{
				return 0;
			}
		}
	}

	beams->DrawBeam( this, instance );
	return 0;
}

void CBeam::OnDataChanged( DataUpdateType_t updateType )
{
	MarkMessageReceived();

	// Make sure that the correct model is referenced for this entity
	SetModelPointer( modelinfo->GetModel( GetModelIndex() ) );

	// Convert weapon world models to viewmodels if they're weapons being carried by the local player
	for (int i=0;i<MAX_BEAM_ENTS;i++)
	{
		C_BaseEntity *pEnt = m_hAttachEntity[i].Get();
		if ( pEnt )
		{
			C_BaseCombatWeapon *pWpn = pEnt->MyCombatWeaponPointer();
			if ( pWpn && pWpn->IsCarriedByLocalPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( pWpn->GetOwner() );

				C_BaseViewModel *pViewModel = player ? player->GetViewModel( 0 ) : nullptr;
				if ( pViewModel )
				{
					// Get the viewmodel and use it instead
					m_hAttachEntity.Set( i, pViewModel );
				}
			}
		}
	}

	// Compute the bounds here...
	Vector mins, maxs;
	ComputeBounds( mins, maxs );
	SetCollisionBounds( mins, maxs );
	AddToEntityList( ENTITY_LIST_SIMULATE );
}

RenderableTranslucencyType_t CBeam::ComputeTranslucencyType() 
{ 
	return RENDERABLE_IS_TRANSLUCENT; 
}

//-----------------------------------------------------------------------------
// Purpose: Adds to beam entity list
//-----------------------------------------------------------------------------
bool CBeam::Simulate( void )
{
	bool bRet = false;
	// If set to invisible, skip. Do this before resetting the entity pointer so it has 
	// valid data to decide whether it's visible.
	if ( ShouldDraw() )
	{

		//FIXME: If we're hooked up to an attachment point, then recompute our bounds every frame
		if ( m_hAttachEntity[0].Get() || m_hAttachEntity[1].Get() )
		{
			// Compute the bounds here...
			Vector mins, maxs;
			ComputeBounds( mins, maxs );
			SetCollisionBounds( mins, maxs );
			bRet = true;
		}

		MoveToLastReceivedPosition();
	}
	BaseClass::Simulate();
	return bRet;
}

//-----------------------------------------------------------------------------
// Computes the bounding box of a beam local to the origin of the beam
//-----------------------------------------------------------------------------
void CBeam::ComputeBounds( Vector& mins, Vector& maxs )
{
	Vector vecAbsStart = GetAbsStartPos();
	Vector vecAbsEnd = GetAbsEndPos();

	switch( GetType() )
	{
	case BEAM_LASER:
	case BEAM_ENTS:
	case BEAM_SPLINE:
		{
			// Compute the bounds here...
			Vector attachmentPoint( 0, 0, 0 );
			mins.Init( 99999, 99999, 99999 );
			maxs.Init( -99999, -99999, -99999 );
			for (int i = 0; i < m_nNumBeamEnts; ++i )
			{
				C_BaseEntity *pTestEnt = m_hAttachEntity[i].Get();
				if ( pTestEnt )
				{
					if ( pTestEnt == this )
					{
						mins = maxs = GetAbsOrigin();
					}
					else
					{
						// We do this so we don't have to calculate attachments (and do expensive bone-setup calculations) on our attachments.
						Vector attMins, attMaxs;
						m_hAttachEntity[i]->GetRenderBoundsWorldspace( attMins, attMaxs );

						mins = mins.Min( attMins );
						mins = mins.Min( attMaxs );
						
						maxs = maxs.Max( attMins );
						maxs = maxs.Max( attMaxs );
					}
					
					//ASSERT_COORD( mins );
					//ASSERT_COORD( maxs );
				}
				else
				{
					if (i == 0)
					{
						VectorCopy( vecAbsStart, attachmentPoint );
					}
					else if (i == 1)
					{
						VectorCopy( vecAbsEnd, attachmentPoint );
					}
					else
					{
						contract_assert( 0 );
					}

					mins = mins.Min( attachmentPoint );
					maxs = maxs.Max( attachmentPoint );
				}
			}
		}
		break;
	case BEAM_POINTS:
	default:
		{
			for (int i = 0; i < 3; ++i)
			{
				if (vecAbsStart[i] < vecAbsEnd[i])
				{
					mins[i] = vecAbsStart[i];
					maxs[i] = vecAbsEnd[i];
				}
				else
				{
					mins[i] = vecAbsEnd[i];
					maxs[i] = vecAbsStart[i];
				}
			}
		}
		break;
	}

	// bloat the bounding box by the width of the beam
	float rad = 0.5f * std::max( m_fWidth.Get(), m_fEndWidth.Get() );
	Vector vecRad( rad, rad, rad );
	mins -= vecRad;
	maxs += vecRad;

	// Make sure the bounds are measured in *relative coords*
	Vector vecAbsOrigin = GetAbsOrigin();
	mins -= vecAbsOrigin;
	maxs -= vecAbsOrigin;
}

#endif
