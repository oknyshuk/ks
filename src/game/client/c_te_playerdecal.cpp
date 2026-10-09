// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.vstdlib;
import std;
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
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basetempentity.h"
#include "fx.h"
#include "materialsystem/imaterialsystem.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialvar.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"
#include "precache_register.h"
#include "playerdecals_signature.h"
#include "engine/decal_flags.h"
#include "c_cs_player.h"
#include "cs_item_inventory.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePlayerDecal )
PRECACHE( MATERIAL, "decals/playerlogo01" )
PRECACHE_REGISTER_END()


void QcCreatePreviewDecal( uint32 nStickerKitDefinition, uint32 nTintID, const trace_t& trace, const Vector* pRight );

//-----------------------------------------------------------------------------
// Purpose: Player Decal TE
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_TEPlayerDecal" } ]]
      C_TEPlayerDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEPlayerDecal( void );
	virtual			~C_TEPlayerDecal( void );

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

class C_FEPlayerDecal;
typedef std::map< int, C_FEPlayerDecal * > PlayerDecalsByUniqueId_t;
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

class [[= ks::reflect::NetTable{ .name = "DT_FEPlayerDecal" } ]]
      C_FEPlayerDecal : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FEPlayerDecal, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_FEPlayerDecal( void ) {
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
	virtual			~C_FEPlayerDecal( void )
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
C_TEPlayerDecal::C_TEPlayerDecal( void )
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
C_TEPlayerDecal::~C_TEPlayerDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEPlayerDecal::Precache( void )
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
		C_BaseEntity* ent;
		if ( ( ent = cl_entitylist->GetEnt( entity ) ) == nullptr )
			return;

		ent->AddDecal( *start, *pos, *pos, hitbox,
			nPlayerAndStickerKitID, false, tr, ADDDECAL_TO_ALL_LODS, right, EDF_PLAYERSPRAY | nAdditionalDecalFlags );
	}
}

void C_FEPlayerDecal::SetDestroyedOnRecreateEntities()
{
	BaseClass::SetDestroyedOnRecreateEntities();

	if ( m_nUniqueID )
	{	// Remeber that we are going to be recreating this decal entity
		s_mapPlayerDecalsUniqueIDsRecreating.insert_or_assign( m_nUniqueID,
			s_mapPlayerDecalsUniqueIDsToApply.contains( m_nUniqueID ) );
	}
}

void C_FEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
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

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
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

bool C_FEPlayerDecal::BMakeDecalReadyToApplyToWorld()
{
	CVProfScope _( "C_FEPlayerDecal::BMakeDecalReadyToApplyToWorld", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

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
#ifdef _DEBUG
	{
		float flendpos[ 3 ] = { data.endpos[ 0 ], data.endpos[ 1 ], data.endpos[ 2 ] };
		float flstartpos[ 3 ] = { data.startpos[ 0 ], data.startpos[ 1 ], data.startpos[ 2 ] };
		float flright[ 3 ] = { data.right[ 0 ], data.right[ 1 ], data.right[ 2 ] };
		float flnormal[ 3 ] = { data.normal[ 0 ], data.normal[ 1 ], data.normal[ 2 ] };
		DevMsg( "Client signature #%u e(%08X,%08X,%08X) s(%08X,%08X,%08X) r(%08X,%08X,%08X) n(%08X,%08X,%08X)\n", data.trace_id,
			*reinterpret_cast< uint32 * >( &flendpos[ 0 ] ), *reinterpret_cast< uint32 * >( &flendpos[ 1 ] ), *reinterpret_cast< uint32 * >( &flendpos[ 2 ] ),
			*reinterpret_cast< uint32 * >( &flstartpos[ 0 ] ), *reinterpret_cast< uint32 * >( &flstartpos[ 1 ] ), *reinterpret_cast< uint32 * >( &flstartpos[ 2 ] ),
			*reinterpret_cast< uint32 * >( &flright[ 0 ] ), *reinterpret_cast< uint32 * >( &flright[ 1 ] ), *reinterpret_cast< uint32 * >( &flright[ 2 ] ),
			*reinterpret_cast< uint32 * >( &flnormal[ 0 ] ), *reinterpret_cast< uint32 * >( &flnormal[ 1 ] ), *reinterpret_cast< uint32 * >( &flnormal[ 2 ] )
			);
	}
#endif
	if ( !BValidateClientPlayerDecalSignature( data ) )
		return false;

	// Make the decal ready.
	const int nKey = MakeKey( m_nUniqueID );
	MakeDecalReady( nKey );

	return true;
}

void C_FEPlayerDecal::ApplyDecalDataToWorld()
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
	TE_PlayerDecal( filter, 0.0f, &trace.endpos, &startPos, pRight, g_nPlayerLogoProxyForPreviewKey, trace.GetEntityIndex(), trace.hitbox, EDF_IMMEDIATECLEANUP );
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
void C_FEPlayerDecal::MakeDecalReady( int nKey )
{
	QcCreateDecalData( nKey, m_nPlayer, m_nTintID, BShouldHaveDrips( m_vecNormal ), m_flCreationTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	CVProfScope _( "C_TEPlayerDecal::PostDataUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Decals disabled?
	if ( !r_decals.GetBool() )
		return;

	CLocalPlayerFilter filter;
	TE_PlayerDecal(  filter, 0.0f, &m_vecOrigin, &m_vecStart, &m_vecRight, m_nPlayer, m_nEntity, m_nHitbox, 0 );
}

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( C_TEPlayerDecal, DT_TEPlayerDecal, CTEPlayerDecal )

IMPLEMENT_REFLECT_CLIENTCLASS( C_FEPlayerDecal, DT_FEPlayerDecal, CFEPlayerDecal )




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
