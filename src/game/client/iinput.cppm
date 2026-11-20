// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) 2026 oknyshuk

module;
#include <time.h>
#include <inputsystem/ButtonCode.h>
#include <igameresources.h>
#include "basehandle.h"
#include "client_class.h"
#include "declare_class.h"
#include "engine/ICollideable.h"
#include "iclientalphaproperty.h"
#include "icliententity.h"
#include "reflect_annotations.h"
#include "edict.h"
#include "engine/ivmodelinfo.h"
#include "networkvar.h"
#include "predictable_entity.h"
#include "vphysics_interface.h"
#include "beam_shared.h"
#include "baseentity.h"
#include "iclientrenderable.h"
#include "beamdraw.h"
#include "dt_send.h"
#include "inputsystem/ButtonCode.h"
#include "vphysics/constraints.h"
#include "basecombatweapon_shared.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "toolframework/itoolentity.h"
#include "tools/bonelist.h"
#include "materialsystem/imesh.h"
#include "cmodel.h"
#include "istudiorender.h"
#include "imaterialproxydict.h"
#include "cdll_int.h"
#include "irecipientfilter.h"
#include "shattersurfacetypes.h"
#include "shake.h"
#include "engine/SndInfo.h"
#include "view_shared.h"
#include "ivrenderview.h"
#include "shaderapi/IShaderDevice.h"
#include "datamap.h"
module ks.client:iinput;

import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import :baseentity;
import :particlemgr;
import ks.tier1.interpolatedvar;
import ks.tier1.intrusive_list;
import ks.tier1.intrusive_ptr;
import ks.tier1.keyvalues;
import ks.tier1.map_ops;
import ks.tier1.tracked_ref;
import std;
import ks.core.basetypes;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.playerid;
import ks.tier1.strtools;
import ks.game.ai_activity;
import :baseanimatedtextureproxy;
import :util;
import :cstrike15.cs_shareddefs;
import ks.game.shareddefs;
import ks.game.beam_flags;
import :basetoggle;
import ks.tier1.interfaces;
import ks.core.color;
import ks.game.precipitation_shared;
import ks.math.vector2d;
import ks.game.sun_shared;
import ks.core.wchartypes;
import ks.math.ssequaternion;
import ks.game.igamemovement;
import ks.tier1.checksum_crc;
import ks.tier1.exprevaluator;
import ks.core.platform;
import ks.game.postprocess_shared;
import ks.math.vector4d;
import ks.tier1.vector_ops;

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: Base class for TEs.  All TEs should derive from this and at
//  least implement OnDataChanged to be notified when the TE has been received
//  from the server
//-----------------------------------------------------------------------------
class CBaseTempEntity : public IClientUnknown, public IClientNetworkable
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseTempEntity", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CBaseTempEntity );
	DECLARE_CLIENTCLASS();
	
									CBaseTempEntity( void );
	virtual							~CBaseTempEntity( void );

// IClientUnknown implementation.
public:

	virtual void SetRefEHandle( const CBaseHandle &handle )	{ contract_assert( false ); }
	virtual const CBaseHandle& GetRefEHandle() const		{ contract_assert( false ); return *( ( CBaseHandle* )0 ); }

	virtual IClientUnknown*		GetIClientUnknown()		{ return this; }
	virtual ICollideable*		GetCollideable()		{ return 0; }
	virtual IClientNetworkable*	GetClientNetworkable()	{ return this; }
	virtual IClientRenderable*	GetClientRenderable()	{ return 0; }
	virtual IClientEntity*		GetIClientEntity()		{ return 0; }
	virtual CBaseEntity*		GetBaseEntity()			{ return 0; }
	virtual IClientThinkable*	GetClientThinkable()	{ return 0; }
	virtual IClientModelRenderable*	GetClientModelRenderable()	{ return 0; }
	virtual IClientAlphaProperty*	GetClientAlphaProperty()	{ return 0; }

// IClientNetworkable overrides.
public:

	virtual void					Release();	
	virtual void					NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void					PreDataUpdate( DataUpdateType_t updateType );
	virtual void					PostDataUpdate( DataUpdateType_t updateType );
	virtual void					OnDataUnchangedInPVS( void ) { }
	virtual void					OnPreDataChanged( DataUpdateType_t updateType );
	virtual void					OnDataChanged( DataUpdateType_t updateType );
	virtual void					SetDormant( bool bDormant );
	virtual bool					IsDormant( void ) const;
	virtual int						entindex( void ) const;
	virtual void					ReceiveMessage( int classID, bf_read &msg );
	virtual void*					GetDataTableBasePtr();
	virtual void					SetDestroyedOnRecreateEntities( void );

public:

	// Dummy for CNetworkVars.
	void NetworkStateChanged() {}
	void NetworkStateChanged( void *pVar ) {}

	virtual bool					Init(int entnum, int iSerialNum);

	virtual void					Precache( void );

	// For dynamic entities, return true to allow destruction
	virtual bool					ShouldDestroy( void ) { return false; };

	CBaseTempEntity				*GetNext( void );

	// Get list of tempentities
	static CBaseTempEntity			*GetList( void );

	CBaseTempEntity				*GetNextDynamic( void );

	// Determine the color modulation amount
	void	GetColorModulation( float* color )
	{
		contract_assert( color );
		color[0] = color[1] = color[2] = 1.0f;
	}

	// Should this object be able to have shadows cast onto it?
	virtual bool	ShouldReceiveProjectedTextures( int flags ) { return false; }

// Static members
public:
	// List of dynamically allocated temp entis
	static CBaseTempEntity			*GetDynamicList();

	// Called at startup to allow temp entities to precache any models/sounds that they need
	static void						PrecacheTempEnts( void );

	static void						ClearDynamicTempEnts( void );

	static void						CheckDynamicTempEnts( void );

private:

	// Next in chain
	CBaseTempEntity		*m_pNext;
	CBaseTempEntity		*m_pNextDynamic;

	// TEs add themselves to this list for the executable.
	static CBaseTempEntity	*s_pTempEntities;
	static CBaseTempEntity *s_pDynamicEntities;
};

using namespace ::ks::reflect;

// Purpose: Client side view model implementation. Responsible for drawing
//			the view model.

class CBaseViewModel;

class CNewParticleEffect;
class CBaseAnimating;
class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CEconItemView;

using namespace ::ks::reflect;


void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Viewmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_ViewmodelSequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Weapon( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_Owner( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CVGuiScreen;

#undef CBaseCombatWeapon
inline constexpr auto NUM_UID_CHARS = 20;


//--------------------------------------------------------------------------------------------------------
class CViewmodelAttachmentModel : public CBaseAnimating
{
	DECLARE_CLASS( CViewmodelAttachmentModel, CBaseAnimating );
public:

	bool InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels );

	void SetViewmodel( CBaseViewModel *pVM );
	virtual int InternalDrawModel( int flags, const RenderableInstance_t &instance );

private:
	CHandle< CBaseViewModel > m_hViewmodel;
};

inline constexpr auto VIEWMODEL_INDEX_BITS = 1;

class CBaseViewModel : public CBaseAnimating, public IHasOwner
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseViewModel", .base = false } ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex, .side = ks::reflect::WireSide::Send }>{} ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .side = ks::reflect::WireSide::Recv }, RecvProxy_Viewmodel>{} ]]
	[[= ks::reflect::From<"m_nBody", ks::reflect::Net{ .bits = ANIMATION_BODY_BITS }>{} ]]
	[[= ks::reflect::From<"m_nSkin", ks::reflect::Net{ .bits = 10 }>{} ]]
	[[= ks::reflect::From<"m_nSequence", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED, .side = ks::reflect::WireSide::Send }>{} ]]
	[[= ks::reflect::From<"m_nSequence", ks::reflect::Net{ .side = ks::reflect::WireSide::Recv }, RecvProxy_ViewmodelSequenceNum>{} ]]
	[[= ks::reflect::From<"m_flPlaybackRate", ks::reflect::Net{ .bits = 8, .low = -4.0, .high = 12.0f, .flags = SPROP_ROUNDUP }>{} ]]
	[[= ks::reflect::From<"m_fEffects", ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED, .side = ks::reflect::WireSide::Send }>{} ]]
	[[= ks::reflect::From<"m_fEffects", ks::reflect::Net{ .side = ks::reflect::WireSide::Recv }, RecvProxy_EffectFlags>{} ]]
	[[= ks::reflect::From<"m_nNewSequenceParity", ks::reflect::Net{ .bits = EF_PARITY_BITS, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nResetEventsParity", ks::reflect::Net{ .bits = EF_PARITY_BITS, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nMuzzleFlashParity", ks::reflect::Net{ .bits = EF_MUZZLEFLASH_BITS, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::PredFrom<"m_nModelIndex", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } >{} ]]
	[[= ks::reflect::PredFrom<"m_nSkin", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_nBody", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_nSequence", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_flPlaybackRate", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } >{} ]]
	[[= ks::reflect::PredFrom<"m_fEffects", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE } >{} ]]
	[[= ks::reflect::PredFrom<"m_flAnimTime", ks::reflect::Pred{ .flags = 0 } >{} ]]
	[[= ks::reflect::PredFrom<"m_flCycle", ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK } >{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseViewModel, CBaseAnimating );
public:

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

							CBaseViewModel( void );
							~CBaseViewModel( void );

	bool IsViewable(void) { return false; }

	virtual void					UpdateOnRemove( void );

	// Weapon client handling
	virtual void			SendViewModelMatchingSequence( int sequence );
	virtual void			SetWeaponModel( const char *pszModelname, CBaseCombatWeapon *weapon );

	virtual void			CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles );
	virtual void			CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, 
								const QAngle& eyeAngles );
	virtual void			AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles ) {};

	void					CalcViewModelUnzoom( CBasePlayer *owner, Vector& origin, QAngle& angles );

	// Initializes the viewmodel for use							
	void					SetOwner( CBaseEntity *pEntity );
	void					SetIndex( int nIndex );
	// Returns which viewmodel it is
	int						ViewModelIndex( ) const;

	virtual void			Precache( void );

	virtual void			Spawn( void );

	virtual CBaseEntity *GetOwner( void ) { return m_hOwner; };

	virtual void			AddEffects( int nEffects );
	virtual void			RemoveEffects( int nEffects );

	void					SpawnControlPanels();
	void					DestroyControlPanels();
	void					SetControlPanelsActive( bool bState );
	void					ShowControlPanells( bool show );

	virtual CBaseCombatWeapon *GetOwningWeapon( void );
	
	virtual CBaseEntity	*GetOwnerViaInterface( void ) { return GetOwner(); }

	virtual bool			IsSelfAnimating()
	{
		return true;
	}

	Vector					m_vecLastFacing;

	virtual bool			IsViewModel() const { return true; }
	virtual bool			IsViewModelOrAttachment() const { return true; }

	void					UpdateAllViewmodelAddons( void );

	CViewmodelAttachmentModel *AddViewmodelArmModel( const char *pszModel, int nSkintoneIndex = -1 );
	CViewmodelAttachmentModel* FindArmModelForLoadoutPosition( loadout_positions_t nPosition ) const;
	void					AddViewmodelLabel( CEconItemView *pItem );
	void					AddViewmodelStatTrak( CEconItemView *pItem, int nStatTrakType, int nWeaponID, AccountID_t holderAcctId );
	void					AddViewmodelStickers( CEconItemView *pItem, int nWeaponID );
	bool					ViewmodelStickersAreValid( int nWeaponID );

	void					RemoveViewmodelArmModels( void );
	void					RemoveViewmodelLabel( void );
	void					RemoveViewmodelStatTrak( void );
	void					RemoveViewmodelStickers( void );

	CNetworkVar(bool, m_bShouldIgnoreOffsetAndAccuracy, [[= ks::reflect::Net{} ]] );
	virtual void			SetShouldIgnoreOffsetAndAccuracy( bool bIgnore ) { m_bShouldIgnoreOffsetAndAccuracy = bIgnore; }

	virtual void			FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual void			PostDataUpdate( DataUpdateType_t updateType );

	virtual CBasePlayer	*GetPredictionOwner( void );

	virtual bool			Interpolate( float currentTime );

	virtual bool			ShouldFlipModel( void );
	void					UpdateAnimationParity( void );

	virtual void			PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] );
	Vector m_vecCamDriverLastPos;
	QAngle m_angCamDriverLastAng;
	float m_flCamDriverAppliedTime;
	float m_flCamDriverWeight;

	virtual void			ApplyBoneMatrixTransform( matrix3x4_t& transform );

	virtual bool			ShouldDraw();
	virtual bool			ShouldSuppressForSplitScreenPlayer( int nSlot );
	virtual int				DrawModel( int flags, const RenderableInstance_t &instance );
	virtual int				DrawOverriddenViewmodel( CBaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance );
	virtual uint8			OverrideAlphaModulation( uint8 nAlpha );
	RenderableTranslucencyType_t ComputeTranslucencyType( void );

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType() { return SHADOWS_NONE; }

	// Should this object receive shadows?
	virtual bool			ShouldReceiveProjectedTextures( int flags )
	{
		return false;
	}

	virtual void			GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS]);

	// See C_StudioModel's definition of this.
	virtual void			UncorrectViewModelAttachment( Vector &vOrigin );

	// (inherited from CBaseAnimating)
	virtual void			FormatViewModelAttachment( int nAttachment, matrix3x4_t &attachmentToWorld );

	CBaseCombatWeapon		*GetWeapon() const { return m_hWeapon.Get(); }

	virtual bool			ShouldResetSequenceOnNewModel( void ) { return false; }

	// Attachments
	virtual int				LookupAttachment( const char *pAttachmentName );
	virtual bool			GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool			GetAttachment( int number, Vector &origin );
	virtual	bool			GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool			GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );

	virtual bool 			Simulate( void );

private:
	CBaseViewModel( const CBaseViewModel & ); // not defined, not accessible

	void					UpdateParticles( int nSlot );

	virtual void 			OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void 			OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	ks::tracked_ref<CNewParticleEffect> m_viewmodelParticleEffect;

private:
	typedef CHandle< CBaseCombatWeapon > CBaseCombatWeaponHandle;
// FTYPEDESC_INSENDTABLE STUFF
	CNetworkVar( int, m_nViewModelIndex, [[= ks::reflect::Net{ .bits = VIEWMODEL_INDEX_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );		// Which viewmodel is it?
	// Used to force restart on client, only needs a few bits
	CNetworkVar( int, m_nAnimationParity, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( CBaseCombatWeaponHandle, m_hWeapon, [[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Weapon, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
// FTYPEDESC_INSENDTABLE STUFF (end)

	CNetworkHandle( CBaseEntity, m_hOwner, [[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Owner, ks::reflect::WireSide::Recv>{} ]] );				// Player or AI carrying this weapon

	// soonest time Update will call WeaponIdle
	float					m_flTimeWeaponIdle;							

	Activity				m_Activity;

	// Weapon art
	string_t				m_sVMName;			// View model of this weapon
	string_t				m_sAnimationPrefix;		// Prefix of the animations that should be used by the player carrying this weapon

	int						m_nOldAnimationParity;

public:
	float					m_fCycleOffset;

	void					UpdateStatTrakGlow( void );
	void					SetStatTrakGlowMultiplier( float flNewIdealGlow ) { m_flStatTrakGlowMultiplierIdeal = flNewIdealGlow; }
	const float				GetStatTrakGlowMultiplier( void ){ return m_flStatTrakGlowMultiplier; }

	void					SetScopeStencilMaskMode( bool bEnabled ) { m_bScopeStencilMaskModeEnabled = bEnabled; }
	bool					GetScopeStencilMaskMode( void ) { return m_bScopeStencilMaskModeEnabled; }

private:

	bool					m_bScopeStencilMaskModeEnabled;
	CHandle< CViewmodelAttachmentModel > m_viewmodelScopeStencilMask;

	std::vector< CHandle< CViewmodelAttachmentModel > > m_vecViewmodelArmModels; // gloves, sleeves, etc
	CHandle< CViewmodelAttachmentModel > m_viewmodelStatTrakAddon;
	CHandle< CViewmodelAttachmentModel > m_viewmodelUidAddon;
	int						m_iAddOnPlayerClass;
	int						m_iAddOnWeaponID;
	float					m_flStatTrakGlowMultiplierIdeal;
	float					m_flStatTrakGlowMultiplier;

	//stickers
	typedef CHandle<CViewmodelAttachmentModel>	StickerHandle_t;
	std::vector< StickerHandle_t >	m_hStickerModelAddons;
	CBaseAnimating* m_pMaterialPreviewShape;

	char					m_szLastSound[64];
	float					m_flLastSoundTime;
	bool					IsSoundSameAsPreviousSound( const char* soundName, float flPastTimeThreshold ) { return ( gpGlobals->curtime - m_flLastSoundTime < flPastTimeThreshold ) && !V_strcmp( soundName, m_szLastSound ); }
	void					ResetTimeSincePreviousSound( void ) { m_flLastSoundTime = gpGlobals->curtime; }
	void					SetPreviousSoundStr( const char* soundName ) { V_strcpy( m_szLastSound, soundName ); }
		
};

inline CBaseViewModel *ToBaseViewModel( CBaseAnimating *pAnim )
{
	if ( pAnim && pAnim->IsViewModel() )
		return static_cast<CBaseViewModel *>(pAnim);
	return nullptr;
}

inline CBaseViewModel *ToBaseViewModel( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return nullptr;
	return ToBaseViewModel(pEntity->GetBaseAnimating());
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// dt_recv.h for CRecvProxyData, which the recv proxies below are declared with; worldsize.h for the
// MIN/MAX_COORD_INTEGER range DT_Beam sends m_vecOrigin in.

inline constexpr auto MAX_BEAM_WIDTH = 102.3f;
inline constexpr auto MAX_BEAM_SCROLLSPEED = 100.0f;
inline constexpr auto MAX_BEAM_NOISEAMPLITUDE = 64;

inline constexpr auto SF_BEAM_STARTON = 0x0001;
inline constexpr auto SF_BEAM_TOGGLE = 0x0002;
inline constexpr auto SF_BEAM_RANDOM = 0x0004;
inline constexpr auto SF_BEAM_RING = 0x0008;
inline constexpr auto SF_BEAM_SPARKSTART = 0x0010;
inline constexpr auto SF_BEAM_SPARKEND = 0x0020;
inline constexpr auto SF_BEAM_DECALS = 0x0040;
inline constexpr auto SF_BEAM_SHADEIN = 0x0080;
inline constexpr auto SF_BEAM_SHADEOUT = 0x0100;
inline constexpr auto SF_BEAM_TAPEROUT = 0x0200; // Tapers to zero
inline constexpr auto SF_BEAM_TEMPORARY = 0x8000;

inline constexpr auto ATTACHMENT_INDEX_BITS = 5;
inline constexpr auto ATTACHMENT_INDEX_MASK = ((1 << ATTACHMENT_INDEX_BITS) - 1);

// A member annotation lives inside a CNetworkVar() argument list, where a preprocessor directive is
// not allowed, so m_fSpeed's recv proxy has to be declared on both sides. It is defined in the
// client half of beam_shared.cpp.
void RecvProxy_Beam_ScrollSpeed( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut );

// I've seen CBeams glitter in the dark near Tannhauser gate...
class CBeam : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Beam", .base = false } ]]
	[[= ks::reflect::From<"m_nRenderFX", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nRenderMode", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex }>{} ]]
	// The remaining inherited props differ by side in the member they read and in their proxy, and a
	// class-level annotation may be guarded where a member-level one may not.
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{}, RecvProxy_ClrRender>{} ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_hNetworkMoveParent", ks::reflect::Net{ .wire = "moveparent" }, RecvProxy_IntToMoveParent>{} ]]
	[[= ks::reflect::PredFrom<"m_nRenderFX", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_nRenderMode", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_clrRender", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_nModelIndex", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } >{} ]]
	[[= ks::reflect::PredFrom<"m_vecOrigin", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } >{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBeam, CBaseEntity );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBeam();

	virtual void SetModel( const char *szModelName );

	void	Spawn( void );
	void	Precache( void );

	virtual int DrawDebugTextOverlays(void);

	// These functions are here to show the way beams are encoded as entities.
	// Encoding beams as entities simplifies their management in the client/server architecture
	void SetType( int type );
	void SetBeamFlags( int flags );
	void SetBeamFlag( int flag );
	
	// NOTE: Start + End Pos are specified in *relative* coordinates 
	void SetStartPos( const Vector &pos );
	void SetEndPos( const Vector &pos );

	// This will change things so the abs position matches the requested spot
	void SetAbsStartPos( const Vector &pos );
	void SetAbsEndPos( const Vector &pos );

	const Vector &GetAbsStartPos( void ) const;
	const Vector &GetAbsEndPos( void ) const;

	void SetStartEntity( CBaseEntity *pEntity );
	void SetEndEntity( CBaseEntity *pEntity );

	void SetStartAttachment( int attachment );
	void SetEndAttachment( int attachment );

	void SetTexture( int spriteIndex );
	void SetHaloTexture( int spriteIndex );
	void SetHaloScale( float haloScale );
	void SetWidth( float width );
	void SetEndWidth( float endWidth );
	void SetFadeLength( float fadeLength );
	void SetNoise( float amplitude );
	void SetColor( int r, int g, int b );
	void SetBrightness( int brightness );
	void SetFrame( float frame );
	void SetScrollRate( int speed );
	void SetFireTime( float flFireTime );
	void SetFrameRate( float flFrameRate ) { m_flFrameRate = flFrameRate; }

	void TurnOn( void );
	void TurnOff( void );

	int	GetType( void ) const;
	int	GetBeamFlags( void ) const;
	CBaseEntity* GetStartEntityPtr( void ) const;
	int	GetStartEntity( void ) const;
	CBaseEntity* GetEndEntityPtr( void ) const;
	int	GetEndEntity( void ) const;
	int GetStartAttachment() const;
	int GetEndAttachment() const;

	virtual const Vector &WorldSpaceCenter( void ) const;

	int GetTexture( void );
	int GetHaloTexture( void ) const;
	float GetWidth( void ) const;
	float GetEndWidth( void ) const;
	float GetFadeLength( void ) const;
	float GetNoise( void ) const;
	float GetHaloScale( void ) const;
	int GetBrightness( void ) const;
	float GetFrame( void ) const;
	float GetScrollRate( void ) const;
	float GetHDRColorScale( void ) const;
	void SetHDRColorScale( float flScale ) { m_flHDRColorScale = flScale; }

	// Call after you change start/end positions
	void		RelinkBeam( void );

	void		DoSparks( const Vector &start, const Vector &end );
	CBaseEntity *RandomTargetname( const char *szName );
	void		BeamDamage( trace_t *ptr );
	// Init after BeamCreate()
	void		BeamInit( const char *pSpriteName, float width );
	void		PointsInit( const Vector &start, const Vector &end );
	void		PointEntInit( const Vector &start, CBaseEntity *pEndEntity );
	void		EntsInit( CBaseEntity *pStartEntity, CBaseEntity *pEndEntity );
	void		LaserInit( CBaseEntity *pStartEntity, CBaseEntity *pEndEntity );
	void		HoseInit( const Vector &start, const Vector &direction );
	void		SplineInit( int nNumEnts, CBaseEntity** pEntList, int *attachment  );

	// Input handlers

	static CBeam *BeamCreate( const char *pSpriteName, float width );
	static CBeam *BeamCreatePredictable( const char *module, int line, bool persist, const char *pSpriteName, float width, CBasePlayer *pOwner );

	void LiveForTime( float time );
	void BeamDamageInstant( trace_t *ptr, float damage );

	virtual const char *GetDecalName( void ) { return "BigShot"; }

	// specify whether the beam should always go all the way to 
	// the end point, or clip against geometry, or clip against
	// geometry and NPCs. This is only used by env_beams at present, but
	// need to be in this CBeam because of the way it affects drawing.
	enum BeamClipStyle_t
	{
		kNOCLIP = 0, // don't clip (default)
		kGEOCLIP = 1,
		kMODELCLIP = 2,

		kBEAMCLIPSTYLE_NUMBITS = 2, //< number of bits needed to represent this object
	};

	inline BeamClipStyle_t GetClipStyle() const { return m_nClipStyle; }

// IClientEntity overrides.
public:
	virtual int			DrawModel( int flags, const RenderableInstance_t &instance );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType();
	virtual void		OnDataChanged( DataUpdateType_t updateType );

	virtual bool		OnPredictedEntityRemove( bool isbeingremoved, CBaseEntity *predicted );

	// Add beam to visible entities list?
	virtual bool		Simulate( void );
	virtual bool		ShouldReceiveProjectedTextures( int flags )
	{
		return false;
	}

// Beam Data Elements
private:
	// Computes the bounding box of a beam local to the origin of the beam
	void ComputeBounds( Vector& mins, Vector& maxs );

	friend void RecvProxy_Beam_ScrollSpeed( const CRecvProxyData *pData, void *pStruct, void *pOut );
	friend class CViewRenderBeams;

protected:
	CNetworkVar( float, m_flFrameRate, [[= ks::reflect::Net{ .bits = 10, .low = -25.0f, .high = 25.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );
	float		m_flFireTime;
	[[= ks::reflect::Key{ .name = "damage" } ]] float		m_flDamage;			// Damage per second to touchers.
	CNetworkVar( int, m_nNumBeamEnts, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	pixelvis_handle_t	m_queryHandleHalo;

private:

	// Beam Data Elements
	CNetworkVar( int, m_nHaloIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	// 3 == Q_log2( NUM_BEAM_TYPES ) + 1, which is not a constant expression: Q_log2 is a function.
	CNetworkVar( int, m_nBeamType, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( int, m_nBeamFlags, [[= ks::reflect::Net{ .bits = NUM_BEAM_FLAGS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkArray( EHANDLE, m_hAttachEntity, MAX_BEAM_ENTS, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkArray( int, m_nAttachIndex, MAX_BEAM_ENTS, [[= ks::reflect::Net{ .bits = ATTACHMENT_INDEX_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fWidth, [[= ks::reflect::Net{ .bits = 10, .low = 0.0f, .high = MAX_BEAM_WIDTH, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fEndWidth, [[= ks::reflect::Net{ .bits = 10, .low = 0.0f, .high = MAX_BEAM_WIDTH, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fFadeLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fHaloScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fAmplitude, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = MAX_BEAM_NOISEAMPLITUDE, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fStartFrame, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 256.0f, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_fSpeed, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = MAX_BEAM_SCROLLSPEED, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Proxy<RecvProxy_Beam_ScrollSpeed, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Key{ .name = "ScrollSpeed", .input = true } ]] );
	CNetworkVar( float, m_flFrame, [[= ks::reflect::Net{ .bits = 20, .low = 0.0f, .high = 256.0f, .flags = SPROP_ROUNDDOWN | SPROP_CHANGES_OFTEN } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( BeamClipStyle_t, m_nClipStyle, [[= ks::reflect::Net{ .bits = kBEAMCLIPSTYLE_NUMBITS + 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVector( m_vecEndPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } ]] );

	EHANDLE		m_hEndEntity;

};

	 

bool IsStaticPointEntity( CBaseEntity *pEnt );

// Macro to wrap creation
template <typename T1, typename T2, typename T3>
inline auto BEAM_CREATE_PREDICTABLE( T1 name, T2 width, T3 player, std::source_location loc = std::source_location::current() )
{
	return CBeam::BeamCreatePredictable( loc.file_name(), static_cast<int>( loc.line() ), false, name, width, player );
}

template <typename T1, typename T2, typename T3>
inline auto BEAM_CREATE_PREDICTABLE_PERSIST( T1 name, T2 width, T3 player, std::source_location loc = std::source_location::current() )
{
	return CBeam::BeamCreatePredictable( loc.file_name(), static_cast<int>( loc.line() ), true, name, width, player );
}

// Start/End Entity is encoded as 12 bits of entity index, and 4 bits of attachment (4:12)

// Beam types, encoded as a byte
enum 
{
	BEAM_POINTS = 0,
	BEAM_ENTPOINT,
	BEAM_ENTS,
	BEAM_HOSE,
	BEAM_SPLINE,
	BEAM_LASER,
	NUM_BEAM_TYPES
};

struct BeamTrail_t;
class CEngineSprite;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Beams fill out this data structure
// This is also used for rendering
//-----------------------------------------------------------------------------

class Beam_t : public CDefaultClientRenderable
{
public:
	Beam_t();

	// Methods of IClientRenderable
	virtual const Vector&	GetRenderOrigin( void );
	virtual const QAngle&	GetRenderAngles( void );
	virtual const matrix3x4_t &RenderableToWorldTransform();
	virtual void			GetRenderBounds( Vector& mins, Vector& maxs );
	virtual bool			ShouldDraw( void );
	virtual int				DrawModel( int flags, const RenderableInstance_t &instance );

	// Resets the beam state
	void			Reset();

	// Method to computing the bounding box
	void			ComputeBounds();

	// Bounding box...
	Vector			m_Mins;
	Vector			m_Maxs;
	pixelvis_handle_t *m_queryHandleHalo;
	float			m_haloProxySize;

	// Data is below..

	// Next beam in list
	Beam_t*			next;

	// Type of beam
	int				type;
	int				flags;

	// Control points for the beam
	int				numAttachments;
	Vector			attachment[MAX_BEAM_ENTS];
	Vector			delta;

	// 0 .. 1 over lifetime of beam
	float			t;		
	float			freq;

	// Time when beam should die
	float			die;
	float			width;
	float			endWidth;
	float			fadeLength;
	float			amplitude;
	float			life;

	// Color
	float			r, g, b;
	float			brightness;

	// Speed
	float			speed;

	// Animation
	float			frameRate;
	float			frame;
	int				segments;

	// Attachment entities for the beam
	EHANDLE			entity[MAX_BEAM_ENTS];
	int				attachmentIndex[MAX_BEAM_ENTS];

	// Model info
	int				modelIndex;
	int				haloIndex;

	float			haloScale;
	int				frameCount;

	float			rgNoise[NOISE_DIVISIONS+1];

	// Popcorn trail for beam follows to use
	BeamTrail_t*	trail;

	// for TE_BEAMRINGPOINT
	float			start_radius;
	float			end_radius;

	// for FBEAM_ONLYNOISEONCE
	bool			m_bCalculatedNoise;

	float			m_flHDRColorScale;
};

int ScreenTransform( const Vector& point, Vector& screen );

void DrawSegs( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				float frame, int rendermode, const Vector& source, const Vector& delta, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale = 1.0f );
void DrawTeslaSegs( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				float frame, int rendermode, const Vector& source, const Vector& delta, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale = 1.0f );
void DrawSplineSegs( int noise_divisions, float *prgNoise, 
				const model_t* beammodel, const model_t* halomodel, float flHaloScale,
				float frame, int rendermode, int numAttachments, Vector* attachment, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale = 1.0f );
void DrawHalo(IMaterial* pMaterial, const Vector& source, float scale, float const* color, float flHDRColorScale = 1.0f );
void BeamDrawHalo( const model_t* spritemodel, float frame, int rendermode, const Vector& source, 
				  float scale, float* color, float flHDRColorScale = 1.0f );
void DrawDisk( int noise_divisions, float *prgNoise, const model_t* spritemodel,
			  float frame, int rendermode, const Vector& source, const Vector& delta, 
			  float width, float scale, float freq, float speed, 
			  int segments, float* color, float flHDRColorScale = 1.0f );
void DrawCylinder( int noise_divisions, float *prgNoise, const model_t* spritemodel, 
				  float frame, int rendermode, const Vector& source, 
				  const Vector&  delta, float width, float scale, float freq, 
				  float speed, int segments, float* color, float flHDRColorScale = 1.0f );
void DrawRing( int noise_divisions, float *prgNoise, void (*pfnNoise)( float *noise, int divs, float scale ), 
			  const model_t* spritemodel, float frame, int rendermode, 
			  const Vector& source, const Vector& delta, float width, float amplitude, 
			  float freq, float speed, int segments, float* color, float flHDRColorScale = 1.0f );
void DrawSprite( const Vector &vecOrigin, float flWidth, float flHeight, color32 color );
void DrawBeamFollow( const model_t* spritemodel, BeamTrail_t* pHead, int frame, int rendermode, Vector& delta, 
					Vector& screen, Vector& screenLast, float die, const Vector& source, 
					int flags, float width, float amplitude, float freq, float* color, float flHDRColorScale = 1.0f );

void DrawBeamQuadratic( const Vector &start, const Vector &control, const Vector &end, float width, const Vector &color, float scrollOffset, float flHDRColorScale = 1.0f );
class CEngineSprite *Draw_SetSpriteTexture( const model_t *pSpriteModel, int frame, int rendermode );

//-----------------------------------------------------------------------------
// Assumes the material has already been bound
//-----------------------------------------------------------------------------

using namespace ::ks::reflect;

// Purpose: Client's CBaseCombatWeapon entity

class CBaseCombatWeapon;
class CBaseWeaponWorldModel;

class CHudTexture;
class CUserCmd;
class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CEconItemView;
class FileWeaponInfo_t;
class WeaponsResource;

using namespace ::ks::reflect;


#undef CBaseCombatWeapon
#undef CBaseWeaponWorldModel

namespace DT_LocalWeaponData { extern RecvTable g_RecvTable; }
namespace DT_LocalActiveWeaponData { extern RecvTable g_RecvTable; }

void RecvProxy_EffectFlagsWeaponWorldmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_WeaponWorldmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_WeaponWorldmodelCosmetics( const CRecvProxyData *pData, void *pStruct, void *pOut );

void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_WeaponWorldmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_EffectFlagsWeaponWorldmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );

void SendProxy_IntAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID );
void RecvProxy_IntSubOne( const CRecvProxyData *pData, void *pStruct, void *pOut );

// DT_BaseCombatWeapon receives m_iState through this; it is defined in the client half of
// basecombatweapon_shared.cpp.
void RecvProxy_State( const CRecvProxyData *pData, void *pStruct, void *pOut );


// How many times to display altfire hud hints (per weapon)
inline constexpr auto WEAPON_ALTFIRE_HUD_HINT_COUNT = 1;
inline constexpr auto WEAPON_RELOAD_HUD_HINT_COUNT = 1;

//Start with a constraint in place (don't drop to floor)
inline constexpr auto SF_WEAPON_START_CONSTRAINED = (1<<0);
inline constexpr auto SF_WEAPON_NO_PLAYER_PICKUP = (1<<1);
inline constexpr auto SF_WEAPON_NO_PHYSCANNON_PUNT = (1<<2);

//Percent
inline constexpr auto CLIP_PERC_THRESHOLD = 0.75f;

// Put this in your derived class definition to declare it's activity table
// UNDONE: Cascade these?
#define DECLARE_ACTTABLE()		static acttable_t m_acttable[];\
	acttable_t *ActivityList( void );\
	int ActivityListCount( void );

// You also need to include the activity table itself in your class' implementation:
// e.g.
//	acttable_t	CWeaponStunstick::m_acttable[] = 
//	{
//		{ ACT_MELEE_ATTACK1, ACT_MELEE_ATTACK_SWING, true },
//	};
//
// The stunstick overrides the ACT_MELEE_ATTACK1 activity, replacing it with ACT_MELEE_ATTACK_SWING.
// This animation is required for this weapon's operation.
//

// Put this after your derived class' definition to implement the accessors for the
// activity table.
// UNDONE: Cascade these?
#define IMPLEMENT_ACTTABLE(className) \
	acttable_t *className::ActivityList( void ) { return m_acttable; } \
	int className::ActivityListCount( void ) { return std::size( m_acttable ); } \

typedef struct
{
	int			baseAct;
	int			weaponAct;
	bool		required;
} acttable_t;


inline constexpr auto MWHEEL_UP = 1;
inline constexpr auto MWHEEL_DOWN = -1;

// -----------------------------------------
//	Vector cones
// -----------------------------------------
// VECTOR_CONE_PRECALCULATED - this resolves to vec3_origin, but adds some
// context indicating that the person writing the code is not allowing
// FireBullets() to modify the direction of the shot because the shot direction
// being passed into the function has already been modified by another piece of
// code and should be fired as specified. See GetActualShotTrajectory(). 

// NOTE: The way these are calculated is that each component == sin (degrees/2)
inline const auto VECTOR_CONE_1DEGREES = Vector( 0.00873, 0.00873, 0.00873 );
inline const auto VECTOR_CONE_2DEGREES = Vector( 0.01745, 0.01745, 0.01745 );
inline const auto VECTOR_CONE_3DEGREES = Vector( 0.02618, 0.02618, 0.02618 );
inline const auto VECTOR_CONE_4DEGREES = Vector( 0.03490, 0.03490, 0.03490 );
inline const auto VECTOR_CONE_5DEGREES = Vector( 0.04362, 0.04362, 0.04362 );
inline const auto VECTOR_CONE_6DEGREES = Vector( 0.05234, 0.05234, 0.05234 );
inline const auto VECTOR_CONE_7DEGREES = Vector( 0.06105, 0.06105, 0.06105 );
#define VECTOR_CONE_8DEGREES		Vector( 0.06976, 0.06976, 0.06976 )
inline const auto VECTOR_CONE_9DEGREES = Vector( 0.07846, 0.07846, 0.07846 );
inline const auto VECTOR_CONE_10DEGREES = Vector( 0.08716, 0.08716, 0.08716 );
inline const auto VECTOR_CONE_20DEGREES = Vector( 0.17365, 0.17365, 0.17365 );

//-----------------------------------------------------------------------------
// Purpose: Base weapon class, shared on client and server
//-----------------------------------------------------------------------------

using BASECOMBATWEAPON_DERIVED_FROM = CBaseAnimating;

// temp states for modular weapon body groups
inline constexpr auto MODULAR_BODYGROUPS_DEFAULT_NONE_SET = 0;
inline constexpr auto MODULAR_BODYGROUPS_NONE_AVAILABLE = 1;
inline constexpr auto MODULAR_BODYGROUPS_RANDOMIZED = 2;

enum WeaponHoldsPlayerAnimCapability_t
{
	WEAPON_PLAYER_ANIMS_UNKNOWN = 0,
	WEAPON_PLAYER_ANIMS_AVAILABLE,
	WEAPON_PLAYER_ANIMS_NOT_AVAILABLE
};

enum WeaponModelClassification_t
{
	WEAPON_MODEL_IS_UNCLASSIFIED = 0,
	WEAPON_MODEL_IS_VIEWMODEL,
	WEAPON_MODEL_IS_WORLDMODEL,
	WEAPON_MODEL_IS_DROPPEDMODEL,
	WEAPON_MODEL_IS_UNRECOGNIZED
};

class CBaseWeaponWorldModel : public CBaseAnimatingOverlay
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseWeaponWorldModel", .base = false } ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex, .side = ks::reflect::WireSide::Send }>{} ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .side = ks::reflect::WireSide::Recv }, RecvProxy_WeaponWorldmodel>{} ]]
	[[= ks::reflect::From<"m_nBody", ks::reflect::Net{ .bits = ANIMATION_BODY_BITS }>{} ]]
	[[= ks::reflect::From<"m_fEffects", ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED, .side = ks::reflect::WireSide::Send }>{} ]]
	[[= ks::reflect::From<"m_fEffects", ks::reflect::Net{ .side = ks::reflect::WireSide::Recv }, RecvProxy_EffectFlagsWeaponWorldmodel>{} ]]
	[[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .side = ks::reflect::WireSide::Send, .wire = "moveparent" }>{} ]]
	[[= ks::reflect::From<"m_hNetworkMoveParent", ks::reflect::Net{ .side = ks::reflect::WireSide::Recv, .wire = "moveparent" }, RecvProxy_IntToMoveParent>{} ]]
	[[= ks::reflect::PredFrom<"m_nModelIndex", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } >{} ]]
	[[= ks::reflect::PredFrom<"m_nBody", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
	[[= ks::reflect::PredFrom<"m_fEffects", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE } >{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseWeaponWorldModel, CBaseAnimatingOverlay );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

public:
	CBaseWeaponWorldModel();
	~CBaseWeaponWorldModel();
	void SetOwningWeapon( CBaseCombatWeapon *pWeaponParent );
	void ShowWorldModel( bool bVisible );
	bool HoldsPlayerAnimations( void );

	void ApplyCustomMaterialsAndStickers( void );
	virtual void	FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual bool	ShouldDraw( void ) override;
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	float * GetRenderClipPlane( void );
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	virtual bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	virtual bool IsFollowingEntity() { return true; } // weapon world models are ALWAYS carried by players

	virtual bool	IsWeaponWorldModel( void ) const { return true; };

	void ValidateParent( void );

	int GetLeftHandAttachBoneIndex();
	int GetRightHandAttachBoneIndex();
	int GetMuzzleAttachIndex();
	int GetMuzzleBoneIndex();
	void ResetCachedBoneIndices();

	bool HasDormantOwner( void );

	typedef CHandle<CBaseCombatWeapon> CBaseCombatWeaponHandle;
	CNetworkVar( CBaseCombatWeaponHandle, m_hCombatWeaponParent, [[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_WeaponWorldmodelCosmetics, ks::reflect::WireSide::Recv>{} ]]);

private:
	WeaponHoldsPlayerAnimCapability_t m_nHoldsPlayerAnims;
	
	int m_nLeftHandAttachBoneIndex;
	int m_nRightHandAttachBoneIndex;
	int m_nMuzzleAttachIndex;
	int m_nMuzzleBoneIndex;

	bool m_bStickersApplied;

};

//-----------------------------------------------------------------------------
// Purpose: Client side rep of CBaseTFCombatWeapon 
//-----------------------------------------------------------------------------
// Hacky
class CBaseCombatWeapon : public BASECOMBATWEAPON_DERIVED_FROM
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseCombatWeapon" } ]]
	[[= ks::reflect::NetTable{ .name = "DT_LocalWeaponData", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_LocalActiveWeaponData", .base = false } ]]
	[[= ks::reflect::SubTable<"LocalWeaponData", &DT_LocalWeaponData::g_RecvTable,
	nullptr, true>{} ]]
	[[= ks::reflect::SubTable<"LocalActiveWeaponData", &DT_LocalActiveWeaponData::g_RecvTable,
	nullptr, true>{} ]]
	[[= ks::reflect::From<"m_nNextThinkTick", ks::reflect::Net{ .bits = -1, .table = "DT_LocalActiveWeaponData" }>{} ]]
	[[= ks::reflect::PredFrom<"m_nNextThinkTick", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseCombatWeapon, BASECOMBATWEAPON_DERIVED_FROM );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

							CBaseCombatWeapon();
	virtual 				~CBaseCombatWeapon();

	// Get unique weapon ID
	// FIXMEL4DTOMAINMERGE
	// We might have to disable this code in main until we refactor all weapons to use this system, as it's a pretty good perf boost
	virtual int GetWeaponID( void ) const		{ return 0; }

	virtual const CEconItemView*	GetEconItemView( void ) const { static CEconItemView s_DummyEconItemView; return &s_DummyEconItemView; }
	virtual CEconItemView*			GetEconItemView( void ) { static CEconItemView s_DummyEconItemView; return &s_DummyEconItemView; }

	// Xuid methods stub (econ removed)
	virtual uint64 GetOriginalOwnerXuid() const { return 0; }
	virtual void SetOriginalOwnerXuid( uint32, uint32 ) {}

	virtual bool			IsBaseCombatWeapon( void ) const { return true; }
	virtual CBaseCombatWeapon *MyCombatWeaponPointer( void ) { return this; }

	// A derived weapon class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const { return false; }

	virtual void			Spawn( void );
	virtual void			Precache( void );

	void					MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );

	// Subtypes are used to manage multiple weapons of the same type on the player.
	virtual int				GetSubType( void ) { return m_iSubType; }
	virtual void			SetSubType( int iType ) { m_iSubType = iType; }

	virtual void			Equip( CBaseCombatCharacter *pOwner );
	virtual void			Drop( const Vector &vecVelocity );

	virtual	int				UpdateClientData( CBasePlayer *pPlayer );

	virtual bool			IsAllowedToSwitch( void );
	virtual bool			CanBeSelected( void );
	virtual bool			VisibleInWeaponSelection( void );
	virtual bool			HasAmmo( void );

	// Weapon Pickup For Player
	virtual void			SetPickupTouch( void );
	virtual void 			DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	virtual void			GiveTo( CBaseEntity *pOther );

	// HUD Hints
	virtual bool			ShouldDisplayAltFireHUDHint();
	virtual void			DisplayAltFireHudHint();	
	virtual void			RescindAltFireHudHint(); ///< undisplay the hud hint and pretend it never showed.

	virtual bool			ShouldDisplayReloadHUDHint();
	virtual void			DisplayReloadHudHint();
	virtual void			RescindReloadHudHint();

	// Weapon client handling
	virtual void			SetViewModelIndex( int index = 0 );
	virtual bool			SendWeaponAnim( int iActivity );
	virtual void			SendViewModelAnim( int nSequence );
	float					GetViewModelSequenceDuration();	// Return how long the current view model sequence is.
	bool					IsViewModelSequenceFinished( void ); // Returns if the viewmodel's current animation is finished

	virtual void			SetViewModel();

	virtual bool			HasWeaponIdleTimeElapsed( void );
	virtual void			SetWeaponIdleTime( float time );
	virtual float			GetWeaponIdleTime( void );

	// Weapon selection
	virtual bool			HasAnyAmmo( void );							// Returns true is weapon has ammo
	virtual bool			HasPrimaryAmmo( void );						// Returns true is weapon has ammo
	virtual bool			HasSecondaryAmmo( void );					// Returns true is weapon has ammo
	bool					UsesPrimaryAmmo( void );					// returns true if the weapon actually uses primary ammo
	bool					UsesSecondaryAmmo( void );					// returns true if the weapon actually uses secondary ammo
	void					GiveDefaultAmmo( void );
	
	virtual bool			CanHolster( void ) { return true; };		// returns true if the weapon can be holstered
	virtual bool			DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual bool			CanDeploy( void ) { return true; }			// return true if the weapon's allowed to deploy
	virtual bool			Deploy( void );								// returns true is deploy was successful
	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo = nullptr );
	virtual CBaseCombatWeapon *GetLastWeapon( void ) { return this; }
	virtual void			SetWeaponVisible( bool visible );
	virtual bool			IsWeaponVisible( void );
	virtual bool			ReloadOrSwitchWeapons( void );
	virtual bool			HolsterOnDetach() { return false; }
	virtual bool			IsHolstered(){ return false; }

	// Weapon behaviour
	virtual void			ItemPreFrame( void );					// called each frame by the player PreThink
	virtual void			ItemPostFrame( void );					// called each frame by the player PostThink
	virtual void			ItemBusyFrame( void );					// called each frame by the player PostThink, if the player's not ready to attack yet
	virtual void			ItemHolsterFrame( void ) {};			// called each frame by the player PreThink, if the weapon is holstered
	virtual void			WeaponIdle( void );						// called when no buttons pressed
	virtual void			HandleFireOnEmpty();					// Called when they have the attack button down
																	// but they are out of ammo. The default implementation
																	// either reloads, switches weapons, or plays an empty sound.

	virtual bool			ShouldBlockPrimaryFire() { return true; }

	virtual void			CreateMove( float flInputSampleTime, CUserCmd *pCmd, const QAngle &vecOldViewAngles ) {}

	virtual bool			IsZoomed() const { return false; }		// Is this weapon in its 'zoomed in' mode?

	// Reloading
	virtual	void			CheckReload( void );
	virtual void			FinishReload( void );
	virtual void			AbortReload( void );
	virtual bool			Reload( void );
	bool					DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

	// Weapon firing
	virtual void			PrimaryAttack( void );						// do "+ATTACK"
	virtual void			SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void			BaseForceFire( CBaseCombatCharacter *pOperator, CBaseEntity *pTarget = nullptr );

	// Firing animations
	virtual Activity		GetPrimaryAttackActivity( void );
	virtual Activity		GetSecondaryAttackActivity( void );
	virtual Activity		GetDrawActivity( void );
	virtual float			GetDefaultAnimSpeed( void ) { return 1.0; }

	// Bullet launch information
	virtual int				GetBulletType( void );
	virtual const Vector&	GetBulletSpread( void );
	virtual Vector			GetBulletSpread( WeaponProficiency_t proficiency )		{ return GetBulletSpread(); }
	virtual float			GetSpreadBias( WeaponProficiency_t proficiency )			{ return 1.0; }
	virtual float			GetAccuracyFishtail() const { return 0.0f; }
	virtual float			GetFireRate( void );
	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 1; }
	virtual float			GetMinRestTime() { return 0.3; }
	virtual float			GetMaxRestTime() { return 0.6; }
	virtual int				GetRandomBurst() { return random_valve->RandomInt( GetMinBurst(), GetMaxBurst() ); }
	virtual void			WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f );
	virtual void			StopWeaponSound( WeaponSound_t sound_type );
	virtual const WeaponProficiencyInfo_t *GetProficiencyValues();

	// Autoaim
	virtual float			GetMaxAutoAimDeflection() { return 0.99f; }
	virtual float			WeaponAutoAimScale() { return 1.0f; } // allows a weapon to influence the perceived size of the target's autoaim radius.

	// TF Sprinting functions
	virtual bool			StartSprinting( void ) { return false; };
	virtual bool			StopSprinting( void ) { return false; };

	// TF Injury functions
	virtual float			GetDamage( float flDistance, int iLocation ) { return 0.0; };

	virtual void			SetActivity( Activity act, float duration );
	inline void				SetActivity( Activity eActivity ) { m_Activity = eActivity; }
	inline Activity			GetActivity( void ) { return m_Activity; }

	virtual void			AddViewKick( void );	// Add in the view kick for the weapon

	virtual const char		*GetDeathNoticeName( void );	// Get the string to print death notices with

	CBaseCombatCharacter	*GetOwner() const;
	void					SetOwner( CBaseCombatCharacter *owner );
	virtual void			OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual void			AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles ) {};
	virtual float			CalcViewmodelBob( void ) { return 0.0f; };

	// Returns information about the various control panels
	virtual void 			GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void			GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );

	virtual bool			ShouldShowControlPanels( void ) { return true; }

	void					Lock( float lockTime, CBaseEntity *pLocker );
	bool					IsLocked( CBaseEntity *pAsker );

	//All weapons can be picked up by NPCs by default
	virtual bool			CanBePickedUpByNPCs( void ) { return true;	}

public:

	// Weapon info accessors for data in the weapon's data file
	const FileWeaponInfo_t	&GetWpnData( void ) const;
	virtual const char		*GetViewModel( int viewmodelindex = 0 ) const;
	virtual const char		*GetWorldModel( void ) const;
	virtual const char		*GetWorldDroppedModel( void ) const;
	virtual const char		*GetAnimPrefix( void ) const;
	virtual int				GetMaxClip1( void ) const { return GetWpnData().GetPrimaryClipSize( GetEconItemView() ); }
	virtual int				GetMaxClip2( void ) const { return GetWpnData().GetSecondaryClipSize( GetEconItemView() ); }
	virtual int				GetDefaultClip1( void ) const { return GetWpnData().GetDefaultPrimaryClipSize( GetEconItemView() ); }
	virtual int				GetDefaultClip2( void ) const { return GetWpnData().GetDefaultSecondaryClipSize( GetEconItemView() ); }
	virtual int				GetReserveAmmoMax( AmmoPosition_t nAmmoPos ) const;
	virtual int				GetWeight( void ) const;
	virtual bool			AllowsAutoSwitchTo( void ) const;
	virtual bool			AllowsAutoSwitchFrom( void ) const;
	virtual int				GetWeaponFlags( void ) const;
	virtual int				GetSlot( void ) const;
	virtual int				GetPosition( void ) const;
	virtual char const		*GetName( void ) const;
	virtual char const		*GetPrintName( void ) const;
	virtual char const		*GetShootSound( int iIndex ) const;
	virtual int				GetRumbleEffect() const;
	virtual bool			UsesClipsForAmmo1( void ) const;
	virtual bool			UsesClipsForAmmo2( void ) const;
	bool					IsMeleeWeapon() const;

	virtual	void			OnMouseWheel( int nDirection ) {}

	// derive this function if you mod uses encrypted weapon info files
	virtual const unsigned char *GetEncryptionKey( void );

	virtual int				GetPrimaryAmmoType( void )  const { return m_iPrimaryAmmoType; }
	virtual int				GetSecondaryAmmoType( void )  const { return m_iSecondaryAmmoType; }
	int						Clip1() const { return m_iClip1; }
	int						Clip2() const { return m_iClip2; }

	// Ammo quantity queries for weapons that do not use clips. These are only
	// used to determine how much ammo is in a weapon that does not have an owner.
	// That is, a weapon that's on the ground for the player to get ammo out of.
	int GetPrimaryAmmoCount() { return m_iPrimaryAmmoCount; }
	void SetPrimaryAmmoCount( int count ) { m_iPrimaryAmmoCount = count; }

	int GetSecondaryAmmoCount() { return m_iSecondaryAmmoCount; }
	void SetSecondaryAmmoCount( int count ) { m_iSecondaryAmmoCount = count; }

	int GetReserveAmmoCount( AmmoPosition_t nAmmoPosition, CBaseCombatCharacter * pForcedOwner = nullptr  );
	int SetReserveAmmoCount( AmmoPosition_t nAmmoPosition, int nCount, bool bSuppressSound = false, CBaseCombatCharacter * pOwner = nullptr );
	int GiveReserveAmmo( AmmoPosition_t nAmmoPosition, int nCount, bool bSuppressSound = false, CBaseCombatCharacter * pOwner = nullptr );

	virtual CHudTexture const	*GetSpriteActive( void ) const;
	virtual CHudTexture const	*GetSpriteInactive( void ) const;
	virtual CHudTexture const	*GetSpriteAmmo( void ) const;
	virtual CHudTexture const	*GetSpriteAmmo2( void ) const;
	virtual CHudTexture const	*GetSpriteCrosshair( void ) const;
	virtual CHudTexture const	*GetSpriteAutoaim( void ) const;
	virtual CHudTexture const	*GetSpriteZoomedCrosshair( void ) const;
	virtual CHudTexture const	*GetSpriteZoomedAutoaim( void ) const;

	virtual Activity		ActivityOverride( Activity baseAct, bool *pRequired );
	virtual	acttable_t*		ActivityList( void ) { return nullptr; }
	virtual	int				ActivityListCount( void ) { return 0; }

	virtual void			Activate( void );

public:

// Server Only Methods

	virtual void			UpdateVisibility( void );

	virtual void			BoneMergeFastCullBloat( Vector &localMins, Vector &localMaxs, const Vector &thisEntityMins, const Vector &thisEntityMaxs  ) const;
	virtual bool			OnFireEvent( CBaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
        return false;
    }

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType();
	virtual void			SetDormant( bool bDormant );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual void			OnRestore();
	virtual void			UpdateOnRemove( void );

	virtual void			Redraw(void);
	virtual void			ViewModelDrawn( int nFlags, CBaseViewModel *pViewModel );
	// Get the position that bullets are seen coming out. Note: the returned values are different
	// for first person and third person.
	bool					GetShootPosition( Vector &vOrigin, QAngle &vAngles );
	virtual void			DrawCrosshair( void );
	virtual bool			ShouldDrawCrosshair( void ) { return true; }
	
	// Weapon state checking
	virtual bool			IsCarriedByLocalPlayer( void );
	virtual bool			IsActiveByLocalPlayer( void );

	bool					IsBeingCarried() const;

	// Returns the aiment render origin + angles
	virtual int				DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool			ShouldDraw( void );
	virtual bool			ShouldSuppressForSplitScreenPlayer( int nSlot );
	virtual bool			ShouldDrawPickup( void );
	virtual void			HandleInput( void ) { return; };
	virtual void			OverrideMouseInput( float *x, float *y ) { return; };
	virtual int				KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) { return 1; }
	virtual bool			AddLookShift( void ) { return true; };

	virtual void			GetViewmodelBoneControllers(CBaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS]) { return; }

	virtual void			NotifyShouldTransmit( ShouldTransmitState_t state );

	virtual void			GetToolRecordingState( KeyValues *msg );
	bool					IsFirstPersonSpectated( void ); //true if the weapon is held by someone we're spectating in first person

	virtual void			GetWeaponCrosshairScale( float &flScale ) { flScale = 1.f; }

	virtual void			GetToolViewModelState( KeyValues *msg ) {} // this is just a stub for viewmodels to request recording of weapon-specific effects, etc

	// Viewmodel overriding
	virtual bool			IsOverridingViewmodel( void ) { return false; };
	virtual int				DrawOverriddenViewmodel( CBaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance ) { return 0; };
	bool					WantsToOverrideViewmodelAttachments( void ) { return false; }

	virtual IClientModelRenderable*	GetClientModelRenderable();

	static std::vector< CBaseCombatWeapon * >& GetWeaponList( void );

	void					ApplyThirdPersonStickers( CBaseAnimating *pWeaponModelTargetOverride = nullptr );

// 	virtual float GetAttributeFloat( const char* szAttribClassName ) const;
// 	virtual bool GetAttributeBool( const char* szAttribClassName ) const;
// 	virtual int GetAttributeInt( const char* szAttribClassName ) const;

	WEAPON_FILE_INFO_HANDLE	GetWeaponFileInfoHandle() const { return m_hWeaponFileInfo; }

	// Is the carrier alive?
	bool					IsCarrierAlive() const;
	virtual bool			IsAlwaysActive( void ) { return false; }

	virtual bool			CanLower( void ) { return false; }
	virtual bool			Ready( void ) { return false; }
	virtual bool			Lower( void ) { return false; }

	virtual void			HideThink( void );
	virtual bool			CanReload( void ){ return true; }

	virtual bool			IsSilentPickupThirdperson( CBaseCombatCharacter *pNewOwner ) { return false; }

// FTYPEDESC_INSENDTABLE STUFF
private:
	typedef CHandle< CBaseCombatCharacter > CBaseCombatCharacterHandle;
	CNetworkVar( CBaseCombatCharacterHandle, m_hOwner , [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// Player carrying this weapon
public:
	// Networked fields
	CNetworkVar( int, m_nViewModelIndex , [[= ks::reflect::Net{ .bits = VIEWMODEL_INDEX_BITS, .flags = SPROP_UNSIGNED, .table = "DT_LocalWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	// Weapon firing
	CNetworkVar( float, m_flNextPrimaryAttack, [[= ks::reflect::Net{ .table = "DT_LocalActiveWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );						// soonest time ItemPostFrame will call PrimaryAttack
	CNetworkVar( float, m_flNextSecondaryAttack, [[= ks::reflect::Net{ .table = "DT_LocalActiveWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );						// soonest time ItemPostFrame will call SecondaryAttack

	// Weapon art
	CNetworkVar( int, m_iViewModelIndex , [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } ]] );
	CNetworkVar( int, m_iWorldModelIndex , [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } ]] );

	CNetworkVar( int, m_iWorldDroppedModelIndex , [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } ]] );

	CNetworkVar( int, m_iWeaponModule , [[= ks::reflect::Net{ .bits = 8, .table = "DT_LocalWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	CNetworkVar( int, m_iNumEmptyAttacks , [[= ks::reflect::Net{ .bits = 8 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	typedef CHandle<CBaseWeaponWorldModel> CBaseWeaponWorldModelHandle;
	CNetworkVar( CBaseWeaponWorldModelHandle, m_hWeaponWorldModel, [[= ks::reflect::Net{} ]] );

	CBaseWeaponWorldModel* GetWeaponWorldModel( void ) { return m_hWeaponWorldModel->Get(); }

public:
	// Weapon data
	CNetworkVar( int, m_iState , [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<RecvProxy_State, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// See WEAPON_* definition
	CNetworkVar( int, m_iPrimaryAmmoType , [[= ks::reflect::Net{ .bits = 8, .table = "DT_LocalWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );		// "primary" ammo index into the ammo info array 
	CNetworkVar( int, m_iSecondaryAmmoType , [[= ks::reflect::Net{ .bits = 8, .table = "DT_LocalWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );	// "secondary" ammo index into the ammo info array
	CNetworkVar( int, m_iClip1 , [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_IntAddOne, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Proxy<RecvProxy_IntSubOne, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// number of shots left in the primary weapon clip, -1 it not used
	CNetworkVar( int, m_iClip2 , [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_IntAddOne, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Proxy<RecvProxy_IntSubOne, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// number of shots left in the secondary weapon clip, -1 it not used

	CNetworkVar( int, m_iPrimaryReserveAmmoCount, [[= ks::reflect::Net{ .bits = 10 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);	// amount of reserve ammo. This used to be on the player ( m_iAmmo ) but we're moving it to the weapon.
	CNetworkVar( int, m_iSecondaryReserveAmmoCount, [[= ks::reflect::Net{ .bits = 10 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);	// amount of reserve ammo. This used to be on the player ( m_iAmmo ) but we're moving it to the weapon.

public:

// Non-networked prediction fields
	CNetworkVar( float, m_flTimeWeaponIdle, [[= ks::reflect::Net{ .table = "DT_LocalActiveWeaponData" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );							// soonest time ItemPostFrame will call WeaponIdle
	// Sounds
	float					m_flNextEmptySoundTime;				// delay on empty sound playing
	float					m_fMinRange1;			// What's the closest this weapon can be used?
	float					m_fMinRange2;			// What's the closest this weapon can be used?
	float					m_fMaxRange1;			// What's the furthest this weapon can be used?
	float					m_fMaxRange2;			// What's the furthest this weapon can be used?
	float					m_fFireDuration;		// The amount of time that the weapon has sustained firing
	int						m_iReloadActivityIndex;
	
private:
	Activity				m_Activity;
	int						m_iPrimaryAmmoCount;
	int						m_iSecondaryAmmoCount;

public:
	string_t				m_iszName;				// Classname of this weapon.

private:
	bool					m_bRemoveable;
public:
	// Weapon state
	CNetworkVar( bool, m_bInReload );									// Are we in the middle of a reload;
	bool					m_bFireOnEmpty;												// True when the gun is empty and the player is still holding down the attack key(s)
	bool					m_bFiresUnderwater;		// true if this weapon can fire underwater
	bool					m_bAltFiresUnderwater;		// true if this weapon can fire underwater
	bool					m_bReloadsSingly;		// Tryue if this weapon reloads 1 round at a time

	float					m_flWeaponTauntHideTimeout;

// FTYPEDESC_INSENDTABLE STUFF (end)
public:
	Activity				GetIdealActivity( void ) { return m_IdealActivity; }
	int						GetIdealSequence( void ) { return m_nIdealSequence; }

	bool					SetIdealActivity( Activity ideal );
	void					MaintainIdealActivity( void );

private:
	int						m_nIdealSequence;
	Activity				m_IdealActivity;

public:


	int						WeaponState() const { return m_iState; }

	int						m_iSubType;

	float					m_flUnlockTime;
	EHANDLE					m_hLocker;				// Who locked this weapon.

	CNetworkVar( bool, m_bFlipViewModel, [[= ks::reflect::Net{ .bits = -1, .table = "DT_LocalWeaponData" } ]] );
	
	CNetworkVar( int, m_iWeaponOrigin, [[= ks::reflect::Net{ .bits = -1, .table = "DT_LocalWeaponData" } ]] );			// How the player acquired the weapon

	IPhysicsConstraint		*GetConstraint() { return m_pConstraint; }

	virtual CStudioHdr			*OnNewModel() override;
	void						ClassifyWeaponModel( void );
	WeaponModelClassification_t	GetWeaponModelClassification( void );
	void						VerifyAndSetContextSensitiveWeaponModel( void );

private:
	WeaponModelClassification_t m_WeaponModelClassification;

	WEAPON_FILE_INFO_HANDLE	m_hWeaponFileInfo;
	IPhysicsConstraint		*m_pConstraint;

	int						m_iAltFireHudHintCount;		// How many times has this weapon displayed its alt-fire HUD hint?
	int						m_iReloadHudHintCount;		// How many times has this weapon displayed its reload HUD hint?
	bool					m_bAltFireHudHintDisplayed;	// Have we displayed an alt-fire HUD hint since this weapon was deployed?
	bool					m_bReloadHudHintDisplayed;	// Have we displayed a reload HUD hint since this weapon was deployed?
	float					m_flHudHintPollTime;	// When to poll the weapon again for whether it should display a hud hint.
	float					m_flHudHintMinDisplayTime; // if the hint is squelched before this, reset my counter so we'll display it again.
	
	// Server only
	bool					m_bJustRestored;
	
	// Allow weapons resource to access m_hWeaponFileInfo directly
	friend class			WeaponsResource;

protected:	
	int						m_iOldState;

};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CBaseCombatWeapon *ToBaseCombatWeapon( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return nullptr;
	return pEntity->MyCombatWeaponPointer();
}

#undef DECLARE_ACTTABLE
#undef IMPLEMENT_ACTTABLE
#undef VECTOR_CONE_8DEGREES

using namespace ::ks::reflect;

class CBaseDoor : public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseDoor" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseDoor, CBaseToggle );
	DECLARE_CLIENTCLASS();

	CBaseDoor( void );
	~CBaseDoor( void );

public:
	[[= ks::reflect::Net{} ]] float		m_flWaveHeight;
};

struct FXLineData_t;
struct FXQuadData_t;

//-----------------------------------------------------------------------------
// Purpose: Base class for client side effects
//-----------------------------------------------------------------------------
class CClientSideEffect
{
public:
	// Constructs the named effect
						CClientSideEffect( const char *name );
	virtual				~CClientSideEffect( void );

	// Update/Draw the effect
	// Derived classes must implement this method!
	virtual void		Draw( double frametime ) = 0;
	// Returns name of effect
	virtual const char	*GetName( void );
	// Retuns whether the effect is still active
	virtual bool		IsActive( void );
	// Sets the effect to inactive so it can be destroed
	virtual void		Destroy( void );
	
private:
	// Name of effect ( static data )
	const char			*m_pszName;
	// Is the effect active
	bool				m_bActive;
};

//-----------------------------------------------------------------------------
// Purpose: Base interface to effects list
//-----------------------------------------------------------------------------
class IEffectsList
{
public:
	virtual			~IEffectsList( void ) {}

	// Add an effect to the list of effects
	virtual void	AddEffect( CClientSideEffect *effect ) = 0;
	// Simulate/Update/Draw effects on list
	virtual void	DrawEffects( double frametime ) = 0;
	// Flush out all effects fbrom the list
	virtual void	Flush( void ) = 0;
};

extern IEffectsList *clienteffects;

//Actual function references
void FX_AddStaticLine( const Vector& start, const Vector& end, float scale, float life, const char *materialName, unsigned char flags );
void FX_AddDiscreetLine( const Vector& start, const Vector& direction, float velocity, float length, float clipLength, float scale, float life, const char *shader );

void FX_AddLine( const FXLineData_t &data );

void FX_AddQuad( const FXQuadData_t &data );

void FX_AddQuad( const Vector &origin, 
				 const Vector &normal, 
				 float startSize, 
				 float endSize, 
				 float sizeBias,
				 float startAlpha, 
				 float endAlpha,
				 float alphaBias,
				 float yaw,
				 float deltaYaw,
				 const Vector &color, 
				 float lifeTime, 
				 const char *shader, 
				 unsigned int flags );

// For safe addition of client effects
void SetFXCreationAllowed( bool state );
bool FXCreationAllowed( void );

bool UI_Startup( CreateInterfaceFn appSystemFactory );
void UI_Shutdown( void );
void UI_CreateGlobalPanels( void );
void UI_CreateClientDLLRootPanel( void );
void UI_DestroyClientDLLRootPanel( void );
void UI_PreRender();
void UI_PostRender();
void UI_GetPanelBounds( int slot, int &x, int &y, int &w, int &h );
// If the engine is inset from the UI_GetPanelBounds due to splitscreen aspect ratio fixups...
void UI_GetEngineRenderBounds( int slot, int &x, int &y, int &w, int &h, int &insetX, int &insetY );
void UI_GetHudBounds( int slot, int &x, int &y, int &w, int &h );
void UI_GetTrueScreenSize( int &w, int &h );
void UI_OnScreenSizeChanged();
bool UI_IsSplitScreen();
bool UI_IsSplitScreenPIP();
bool IsWidescreen( void );

void UI_OnSplitScreenStateChanged();

// This class triggers events at a specified rate. Just call NextEvent() and do an event until it 
// returns false. For example, if you want to spawn particles 10 times per second, do this:
// pTimer->SetRate(10);
// float tempDelta = fTimeDelta;
// while(pTimer->NextEvent(tempDelta))
//		spawn a particle

class TimedEvent
{
public:
				TimedEvent()
				{
					m_TimeBetweenEvents = -1;
					m_fNextEvent = 0;
				}

	// Rate is in events per second (ie: rate of 15 will trigger 15 events per second).
	inline void	Init(float rate)			
	{
		m_TimeBetweenEvents = 1.0f / rate;
		m_fNextEvent = 0;
	}
	
	inline void ResetRate(float rate)
	{
		m_TimeBetweenEvents = 1.0f / rate;
	}

	inline bool NextEvent(float &curDelta)
	{
		// If this goes off, you didn't call Init().
		contract_assert( m_TimeBetweenEvents != -1 );

		if(curDelta >= m_fNextEvent)
		{
			curDelta -= m_fNextEvent;
			
			m_fNextEvent = m_TimeBetweenEvents;
			return true;
		}
		else
		{
			m_fNextEvent -= curDelta;
			return false;
		}
	}

private:
	float		m_TimeBetweenEvents;
	float		m_fNextEvent;	// When the next event should be triggered.
};

class CBaseEntity;


class CBaseToggleTextureProxy : public IMaterialProxy
{
public:
	CBaseToggleTextureProxy();
	virtual ~CBaseToggleTextureProxy();

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	void Cleanup();

	CBaseEntity *BindArgToEntity( void *pArg );

	IMaterialVar *m_TextureVar;
	IMaterialVar *m_TextureFrameNumVar;
	bool m_WrapAnimation;
};

//-----------------------------------------------------------------------------
// No tool DLLs ship with the game, so tool recording is never enabled.
//-----------------------------------------------------------------------------
inline constexpr bool ToolsEnabled() { return false; }
inline void ToolFramework_PostToolMessage( HTOOLHANDLE, KeyValues * ) {}
inline constexpr bool ToolFramework_IsThirdPersonCamera() { return false; }

//-----------------------------------------------------------------------------
// Standard messages
//-----------------------------------------------------------------------------
struct EffectRecordingState_t
{
	bool m_bVisible : 1;
	bool m_bThirdPerson : 1;
	Color m_Color;
	float m_Scale;
	const char *m_pMaterialName;
	int m_nAttachment;
	Vector m_vecAttachment; // only used if m_nAttachment is -1
};

struct BaseEntityRecordingState_t
{
	BaseEntityRecordingState_t() :	
		m_flTime( 0.0f ),
		m_pModelName( 0 ),
		m_nOwner( -1 ),
		m_fEffects( 0 ),
		m_bVisible( false ),
		m_bRecordFinalVisibleSample( false ),
		m_numEffects( 0 ),
		m_pEffects( nullptr ),
		m_nFollowEntity( -1 )
	{
		m_vecRenderOrigin.Init();
		m_vecRenderAngles.Init();
	}

	float m_flTime;
	const char *m_pModelName;
	int m_nOwner;
	int m_fEffects;
	bool m_bVisible : 1;
	bool m_bRecordFinalVisibleSample : 1;
	Vector m_vecRenderOrigin;
	QAngle m_vecRenderAngles;
	int m_nFollowEntity;

	int m_numEffects;
	EffectRecordingState_t *m_pEffects;
};

struct SpriteRecordingState_t
{
	float m_flRenderScale;
	float m_flFrame;
	int m_nRenderMode;
	bool m_nRenderFX;
	Color m_Color;
	float m_flProxyRadius;
};

struct BaseAnimatingHighLevelRecordingState_t
{
	BaseAnimatingHighLevelRecordingState_t()
	:	m_bClearIkTargets( false ),
		m_bIsRagdoll( false ),
		m_bShouldCreateIkContext( false ),
		m_nNumPoseParams( 0 ),
		m_flCycle( 0.0f ),
		m_flPlaybackRate( 1.0f ),
		m_flCycleRate( 0.0f ),
		m_nFrameCount( 0 ),
		m_bInterpEffectActive( false )
	{
	}

	bool m_bClearIkTargets;
	bool m_bIsRagdoll;
	bool m_bShouldCreateIkContext;
	int m_nNumPoseParams;

	float m_flCycle;
	float m_flPlaybackRate;
	float m_flCycleRate;
	int m_nFrameCount;

	float m_flPoseParameter[MAXSTUDIOPOSEPARAM];

	bool m_bInterpEffectActive;
};

struct BaseAnimatingRecordingState_t
{
	BaseAnimatingHighLevelRecordingState_t m_highLevelState;

	int m_nSkin;
	int m_nBody;
	int m_nSequence;
	CBoneList *m_pBoneList;
};

struct BaseFlexRecordingState_t
{
	int m_nFlexCount;
	float *m_pDestWeight;
	Vector m_vecViewTarget;
};

struct CameraRecordingState_t
{
	bool m_bThirdPerson;
	float m_flFOV;
	Vector m_vecEyePosition;
	QAngle m_vecEyeAngles;
	bool m_bPlayerEyeIsPortalled; //for portal. Need to reverse some portal recursion draw logic when the player eye and body straddle a portal
};

struct MonitorRecordingState_t
{
	bool	m_bActive;
	float	m_flFOV;
	bool	m_bFogEnabled;
	float	m_flFogStart;
	float	m_flFogEnd;
	Color	m_FogColor;
};

//-----------------------------------------------------------------------------
// Recorded temp entity structures
//-----------------------------------------------------------------------------
enum TERecordingType_t
{
	TE_DYNAMIC_LIGHT = 0,
	TE_WORLD_DECAL,
	TE_DISPATCH_EFFECT,
	TE_MUZZLE_FLASH,
	TE_ARMOR_RICOCHET,
	TE_METAL_SPARKS,
	TE_SMOKE,
	TE_SPARKS,
	TE_BLOOD_SPRITE,
	TE_BREAK_MODEL,
	TE_GLOW_SPRITE,
	TE_PHYSICS_PROP,
	TE_SPRITE_SINGLE,
	TE_SPRITE_SPRAY,
	TE_CONCUSSIVE_EXPLOSION,
	TE_BLOOD_STREAM,
	TE_SHATTER_SURFACE,
	TE_DECAL,
	TE_PROJECT_DECAL,
	TE_EXPLOSION,

	TE_RECORDING_TYPE_COUNT,
};

class CBaseEntity;

// Purpose: Shared data definition file for touch links

typedef CHandle<CBaseEntity> EHANDLE;

//-----------------------------------------------------------------------------
// Purpose: for resolving touch/untouch pairs
//-----------------------------------------------------------------------------
enum touchlink_flags_t
{
	FTOUCHLINK_START_TOUCH = 0x00000001,
};

struct touchlink_t
{
	CBaseEntity		*entityTouched;
	int					touchStamp;
	touchlink_t			*nextLink;
	touchlink_t			*prevLink;
	int					flags;
};

// means this touchlink is managed external to the main physics system
inline constexpr auto TOUCHSTAMP_EVENT_DRIVEN = -1;

class ParticleDraw;


//-----------------------------------------------------------------------------
// Tracer_Draw(): draws a tracer, assuming the modelview matrix is identity
// This function accepts all arguments in CAMERA (pre-projected) space
//
// arguments
//		[in] Vector& : The origin of the tracer (CAMERA space)
//		[in] Vector& : The direction and length of the tracer (CAMERA space)
//		[in] float  : The tracer width (CAMERA space)
//		[in] float* : r, g, b, a (0 - 1)
//-----------------------------------------------------------------------------
void Tracer_Draw( ParticleDraw* pDraw, Vector const& start, Vector const& delta, 
				 float width, float* color, float startV = 0.0, float endV = 1.0 );

void Tracer_Draw( CMeshBuilder *pMeshBuilder, Vector const& start, Vector const& delta, float width, float* color, float startV = 0.0, float endV = 1.0 );

//-----------------------------------------------------------------------------
// Computes the four verts to draw the tracer with, in the following order:
//	start vertex left side, start vertex right side 
//	end vertex left side, end vertex right side 
// returne false if the tracer is offscreen
//-----------------------------------------------------------------------------
bool Tracer_ComputeVerts( const Vector &start, const Vector &delta, float width, Vector *pVerts );

class CClient_Precipitation;

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class SimpleParticle;

using namespace ::ks::reflect;


// Draw rain effects.
void DrawPrecipitation();

//-----------------------------------------------------------------------------
// Precipitation particle type
//-----------------------------------------------------------------------------

class CPrecipitationParticle
{
public:
	Vector	m_Pos;
	Vector	m_Velocity;
	float	m_SpawnTime;				// Note: Tweak with this to change lifetime
	float	m_Mass;
	float	m_Ramp;

	float	m_flMaxLifetime;
	int		m_nSplitScreenPlayerSlot;
};

inline std::vector< CClient_Precipitation* > g_Precipitations;

//===========
// Snow fall
//===========
class CSnowFallManager;
inline CSnowFallManager *s_pSnowFallMgr[ MAX_SPLITSCREEN_PLAYERS ];
bool SnowFallManagerCreate( CClient_Precipitation *pSnowEntity );
void SnowFallManagerDestroy( void );

class AshDebrisEffect : public CSimpleEmitter
{
public:
	explicit AshDebrisEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}

	static AshDebrisEffect* Create( const char *pDebugName );

	virtual float UpdateAlpha( const SimpleParticle *pParticle );
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta );

private:
	AshDebrisEffect( const AshDebrisEffect & );
};

//-----------------------------------------------------------------------------
// Precipitation blocker entity
//-----------------------------------------------------------------------------
class CPrecipitationBlocker : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PrecipitationBlocker" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPrecipitationBlocker, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CPrecipitationBlocker();
	virtual ~CPrecipitationBlocker();
};

//-----------------------------------------------------------------------------
// Precipitation base entity
//-----------------------------------------------------------------------------
class CClient_Precipitation : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Precipitation" } ]]
	static constexpr bool ks_class = true;

	class CPrecipitationEffect;
	friend class CClient_Precipitation::CPrecipitationEffect;

public:
	DECLARE_CLASS( CClient_Precipitation, CBaseEntity );
	DECLARE_CLIENTCLASS();

	CClient_Precipitation();
	virtual ~CClient_Precipitation();

	// Inherited from CBaseEntity
	virtual void Precache( );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType() { return RENDERABLE_IS_TRANSLUCENT; }

	void Render();

	// Computes where we're gonna emit
	bool ComputeEmissionArea( Vector& origin, Vector2D& size, CBaseCombatCharacter *pCharacter );

private:

	// Creates a single particle
	CPrecipitationParticle* CreateParticle();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();

	void Simulate( float dt );

	// Renders the particle
	void RenderParticle( CPrecipitationParticle* pParticle, CMeshBuilder &mb );

	void CreateWaterSplashes();

	// Emits the actual particles
	void EmitParticles( float fTimeDelta );

	// Gets the tracer width and speed
	float GetWidth() const;
	float GetLength() const;
	float GetSpeed() const;

	// Gets the remaining lifetime of the particle
	float GetRemainingLifetime( CPrecipitationParticle* pParticle ) const;

	// Computes the wind vector
	static void ComputeWindVector( );

	// simulation methods
	bool SimulateRain( CPrecipitationParticle* pParticle, float dt );
	bool SimulateSnow( CPrecipitationParticle* pParticle, float dt );

	void CreateParticlePrecip( void );
	void InitializeParticlePrecip( void );
	void DispatchOuterParticlePrecip( int nSlot, CBasePlayer *pPlayer, Vector vForward );
	void DispatchInnerParticlePrecip( int nSlot, CBasePlayer *pPlayer, Vector vForward );
	void DestroyOuterParticlePrecip( int nSlot );
	void DestroyInnerParticlePrecip( int nSlot );

	void UpdateParticlePrecip( CBasePlayer *pPlayer, int nSlot );
	float GetDensity() { return m_flDensity; }

private:
	void CreateAshParticle( void );
	void CreateRainOrSnowParticle( const Vector &vSpawnPosition, const Vector &vEndPosition, const Vector &vVelocity );	// TERROR: adding end pos for lifetime calcs

	// Information helpful in creating and rendering particles
	IMaterial		*m_MatHandle;	// material used 

	float			m_Color[4];		// precip color
	float			m_Lifetime;		// Precip lifetime
	float			m_InitialRamp;	// Initial ramp value
	float			m_Speed;		// Precip speed
	float			m_Width;		// Tracer width
	float			m_Remainder;	// particles we should render next time
	[[= ks::reflect::Net{} ]] PrecipitationType_t	m_nPrecipType;			// Precip type
	float			m_flHalfScreenWidth;	// Precalculated each frame.

	float			m_flDensity;

	// Some state used in rendering and simulation
	// Used to modify the rain density and wind from the console
	static ConVar s_raindensity;
	static ConVar s_rainwidth;
	static ConVar s_rainlength;
	static ConVar s_rainspeed;

	static Vector s_WindVector;			// Stores the wind speed vector

	std::vector<CPrecipitationParticle> m_Particles;
	std::vector< Vector > m_Splashes;

	struct AshSplit_t
	{
		AshSplit_t() : m_bActiveAshEmitter( false ), m_vAshSpawnOrigin( 0, 0, 0 ), m_iAshCount( 0 )
		{
		}

		ks::intrusive_ptr<AshDebrisEffect>		m_pAshEmitter;
		TimedEvent						m_tAshParticleTimer;
		TimedEvent						m_tAshParticleTraceTimer;
		bool							m_bActiveAshEmitter;
		Vector							m_vAshSpawnOrigin;

		int								m_iAshCount;
	};

protected:
	AshSplit_t						m_Ash[ MAX_SPLITSCREEN_PLAYERS ];

	float							m_flParticleInnerDist;	//The distance at which to start drawing the inner system
	const char						*m_pParticleInnerNearDef; //Name of the first inner system
	const char						*m_pParticleInnerFarDef;  //Name of the second inner system
	const char						*m_pParticleOuterDef;     //Name of the outer system
	HPARTICLEFFECT					m_pParticlePrecipInnerNear[ MAX_SPLITSCREEN_PLAYERS ];
	HPARTICLEFFECT					m_pParticlePrecipInnerFar[ MAX_SPLITSCREEN_PLAYERS ];
	HPARTICLEFFECT					m_pParticlePrecipOuter[ MAX_SPLITSCREEN_PLAYERS ];
	TimedEvent						m_tParticlePrecipTraceTimer[ MAX_SPLITSCREEN_PLAYERS ];
	bool							m_bActiveParticlePrecipEmitter[ MAX_SPLITSCREEN_PLAYERS ];
	bool							m_bParticlePrecipInitialized;

private:
	CClient_Precipitation( const CClient_Precipitation & ); // not defined, not accessible
};

extern float g_flOverlayRange;

class CGlowSprite
{
public:
	Vector				m_vColor;		// 0-1
	float				m_flHorzSize;	// Horizontal and vertical sizes.
	float				m_flVertSize;	// 1 = size of the sun
	IMaterial			*m_pMaterial;	// Material to use
};

class CGlowOverlay
{
public:

					CGlowOverlay();
	virtual			~CGlowOverlay();

	// Return false to remove (and delete) the overlay.
	virtual bool	Update();

// Data the creator should fill in.
public:

	// Position of the light source. If m_bDirectional is set, then it ignores
	// this and uses m_vDirection as the direction of the light source.
	Vector		m_vPos;

	// Optional direction. Used for really far away things like the sun.
	// The direction should point AT the light source.
	bool		m_bDirectional;
	Vector		m_vDirection;

	// If this is set, then the overlay is only visible if the ray to it hits the sky.
	bool		m_bInSky;
	float		m_skyObstructionScale;

	CGlowSprite	m_Sprites[MAX_SUN_LAYERS];
	int			m_nSprites;

	float		m_flProxyRadius;

	float		m_flHDRColorScale;

public:

	// After creating the overlay, call this to add it to the active list.
	// You can also call Activate and Deactivate as many times as you want.
	void			Activate();
	void			Deactivate();
	
	// Render all the active overlays.
	static void		DrawOverlays( bool bCacheFullSceneState );
	static void		UpdateSkyOverlays( float zFar, bool bCacheFullSceneState );

protected:

	void			UpdateGlowObstruction( const Vector &vToGlow, bool bCacheFullSceneState );
	void			UpdateSkyGlowObstruction( float zFar, bool bCacheFullSceneState );

	virtual void	CalcSpriteColorAndSize( 
		float flDot,
		CGlowSprite *pSprite, 
		float *flHorzSize, 
		float *flVertSize, 
		Vector *vColor );

	virtual void	CalcBasis( 
		const Vector &vToGlow,
		float flHorzSize,
		float flVertSize,
		Vector &vBasePt,
		Vector &vUp,
		Vector &vRight );

	virtual void	Draw( bool bCacheFullSceneState );
	float			CalcGlowAspect();
	
	float			m_flGlowObstructionScale;	
	bool			m_bCacheGlowObstruction;			// Flags to cache obstruction scales
	bool			m_bCacheSkyObstruction;				// Used in IFM poster rendering

private:
	short			m_bActivated;
	std::optional< std::list< CGlowOverlay * >::iterator > m_ListIndex; // position in g_GlowOverlaySystem, if added
	pixelvis_handle_t m_queryHandle;
};

//Override for warping
class CWarpOverlay : public CGlowOverlay
{
protected:
	
	virtual void Draw( bool bCacheFullSceneState );
};

//struct trace_t;

enum
{
	FX_ENERGYSPLASH_EXPLOSIVE		= 0x1,
	FX_ENERGYSPLASH_SMOKE			= 0x2,
	FX_ENERGYSPLASH_LITTLESPARKS	= 0x4,
	FX_ENERGYSPLASH_BIGSPARKS		= 0x8,
	FX_ENERGYSPLASH_BIGSPARKSCOLLIDE = 0x10,
	FX_ENERGYSPLASH_ENERGYBALLS		= 0x20,
	FX_ENERGYSPLASH_DLIGHT			= 0x40,
	
	FX_ENERGYSPLASH_DEFAULT = ~FX_ENERGYSPLASH_EXPLOSIVE,
	FX_ENERGYSPLASH_DEFAULT_EXPLOSIVE = ~0,
};

bool FX_GetAttachmentTransform( ClientEntityHandle_t hEntity, int attachmentIndex, matrix3x4_t &transform );
bool FX_GetAttachmentTransform( ClientEntityHandle_t hEntity, int attachmentIndex, Vector *origin, QAngle *angles );

void FX_RicochetSound( const Vector& pos );

void FX_AntlionImpact( const Vector &pos, trace_t *tr );
void FX_DebrisFlecks( const Vector& origin, trace_t *trace, char materialType, int iScale, bool bNoFlecks = false );
void FX_Tracer( Vector& start, Vector& end, int velocity, bool makeWhiz = true );
void FX_GunshipTracer( Vector& start, Vector& end, int velocity, bool makeWhiz = true );
void FX_StriderTracer( Vector& start, Vector& end, int velocity, bool makeWhiz = true );
void FX_HunterTracer( Vector& start, Vector& end, int velocity, bool makeWhiz = true );
void FX_PlayerTracer( Vector& start, Vector& end );
void FX_BulletPass( Vector& start, Vector& end );
void FX_MetalSpark( const Vector &position, const Vector &direction, const Vector &surfaceNormal, int iScale = 1 );
void FX_MetalScrape( Vector &position, Vector &normal );
void FX_Sparks( const Vector &pos, int nMagnitude, int nTrailLength, const Vector &vecDir, float flWidth, float flMinSpeed, float flMaxSpeed, char *pSparkMaterial = nullptr );
void FX_ElectricSpark( const Vector &pos, int nMagnitude, int nTrailLength, const Vector *vecDir );
void FX_BugBlood( Vector &pos, Vector &dir, Vector &vWorldMins, Vector &vWorldMaxs );
void FX_Blood( Vector &pos, Vector &dir, float r, float g, float b, float a );
void FX_CreateImpactDust( Vector &origin, Vector &normal );
void FX_EnergySplash( const Vector &pos, const Vector &normal, int nFlags = FX_ENERGYSPLASH_DEFAULT );
void FX_MicroExplosion( Vector &position, Vector &normal );
void FX_Explosion( Vector& origin, Vector& normal, char materialType );
void FX_ConcussiveExplosion( Vector& origin, Vector& normal ); 
void FX_MuzzleEffect( const Vector &origin, const QAngle &angles, float scale, ClientEntityHandle_t hEntity, unsigned char *pFlashColor = nullptr, bool bOneFrame = false );
void FX_MuzzleEffectAttached( float scale, ClientEntityHandle_t hEntity, int attachmentIndex, unsigned char *pFlashColor = nullptr, bool bOneFrame = false  );
void FX_StriderMuzzleEffect( const Vector &origin, const QAngle &angles, float scale, ClientEntityHandle_t hEntity, unsigned char *pFlashColor = nullptr );
void FX_GunshipMuzzleEffect( const Vector &origin, const QAngle &angles, float scale, ClientEntityHandle_t hEntity, unsigned char *pFlashColor = nullptr );
ks::intrusive_ptr<CSimpleEmitter> FX_Smoke( const Vector &origin, const Vector &velocity, float scale, int numParticles, float flDietime, unsigned char *pColor, int iAlpha, const char *pMaterial, float flRoll, float flRollDelta );
void FX_Smoke( const Vector &origin, const QAngle &angles, float scale, int numParticles, unsigned char *pColor = nullptr, int iAlpha = -1 );
void FX_Dust( const Vector &vecOrigin, const Vector &vecDirection, float flSize, float flSpeed );
void FX_CreateGaussExplosion( const Vector &pos, const Vector &dir, int type );
void FX_GaussTracer( Vector& start, Vector& end, int velocity, bool makeWhiz = true );
void FX_TracerSound( const Vector &start, const Vector &end, int iTracerType );

// Lighting information utility
void UTIL_GetNormalizedColorTintAndLuminosity( const Vector &color, Vector *tint = nullptr, float *luminosity = nullptr );

// Useful function for testing whether to draw noZ effects
bool EffectOccluded( const Vector &pos, pixelvis_handle_t *queryHandle = 0 );

class CTeslaInfo
{
public:
	Vector			m_vPos;
	QAngle			m_vAngles;
	int				m_nEntIndex;
	char			*m_pszSpriteName;
	float			m_flBeamWidth;
	int				m_nBeams;
	Vector			m_vColor;
	float			m_flTimeVisible;
	float			m_flRadius;
};

void FX_Tesla( const CTeslaInfo &teslaInfo );
extern ConVar r_decals;

extern PMaterialHandle g_Mat_Fleck_Wood[2];
extern PMaterialHandle g_Mat_Fleck_Cement[2];
extern PMaterialHandle g_Mat_Fleck_Antlion[2];
extern PMaterialHandle g_Mat_Fleck_Tile[2];
extern PMaterialHandle g_Mat_DustPuff[2];
extern PMaterialHandle g_Mat_BloodPuff[2];
extern PMaterialHandle g_Mat_Fleck_Glass[2];
extern PMaterialHandle g_Mat_SMG_Muzzleflash[4];
extern PMaterialHandle g_Mat_Combine_Muzzleflash[3];

class SimpleParticle;

class CBloodSprayEmitter : public CSimpleEmitter
{
public:
	
	CBloodSprayEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	static CBloodSprayEmitter *Create( const char *pDebugName )
	{
		return new CBloodSprayEmitter( pDebugName );
	}

	inline void SetGravity( float flGravity )
	{
		m_flGravity = flGravity;
	}

	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta );

	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta );

private:

	float m_flGravity;

	CBloodSprayEmitter( const CBloodSprayEmitter & );
};

class CFXDiscreetLine : public CClientSideEffect
{
public:

	CFXDiscreetLine ( const char *name, const Vector& start, const Vector& direction, float velocity, 
		float length, float clipLength, float scale, float life, const char *shader );
	~CFXDiscreetLine ( void );

	virtual void	Draw( double frametime );
	virtual bool	IsActive( void );
	virtual void	Destroy( void );
	virtual	void	Update( double frametime );

protected:

	IMaterial		*m_pMaterial;
	float			m_fLife;
	Vector			m_vecOrigin, m_vecDirection;
	float			m_fVelocity;
	float			m_fStartTime;
	float			m_fClipLength;
	float			m_fScale;
	float			m_fLength;
};

//JDW: For now we're clamping this value
inline constexpr auto EXPLOSION_FORCE_MAX = 2;
inline constexpr auto EXPLOSION_FORCE_MIN = 2;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseExplosionEffect
{
private:
	static CBaseExplosionEffect	m_instance;

public:
			~CBaseExplosionEffect( void ) {}

	static	CBaseExplosionEffect &Instance( void )	{	return m_instance;	}

	virtual void	Create( const Vector &position, float force, float scale, int flags );

protected:
					CBaseExplosionEffect( void );

	virtual void	PlaySound( void );
	virtual void	CreateCore( void );
	virtual void	CreateDebris( void );
	virtual void	CreateMisc( void );
	virtual void	CreateDynamicLight( void );

	float			ScaleForceByDeviation( Vector &deviant, Vector &source, float spread, float *force = nullptr );

	float			Probe( const Vector &origin, Vector *direction, float strength );
	void			GetForceDirection( const Vector &origin, float magnitude, Vector *resultDirection, float *resultForce );

protected:

	Vector	m_vecOrigin;
	Vector	m_vecDirection;
	float	m_flForce;
	float	m_flScale;
	int		m_fFlags;

	PMaterialHandle	m_Material_Smoke;
	PMaterialHandle m_Material_Embers[2];
	PMaterialHandle m_Material_FireCloud;
};

//Singleton accessor
extern CBaseExplosionEffect &BaseExplosionEffect( void );

//
// CExplosionOverlay
//

class CExplosionOverlay : public CWarpOverlay
{
public:
	
	virtual bool Update( void );

public:

	float	m_flLifetime;
	Vector	m_vBaseColors[MAX_SUN_LAYERS];

};

//-----------------------------------------------------------------------------
// Purpose: Water explosion
//-----------------------------------------------------------------------------
class CWaterExplosionEffect : public CBaseExplosionEffect
{
	typedef CBaseExplosionEffect BaseClass;
public:
	static	CWaterExplosionEffect &Instance( void )	{	return m_waterinstance;	}

	virtual void	Create( const Vector &position, float force, float scale, int flags );

protected:
	virtual void	CreateCore( void );
	virtual void	CreateDebris( void );
	virtual void	CreateMisc( void );
	virtual void	PlaySound( void );

private:
	Vector	m_vecWaterSurface;
	float	m_flDepth;				// Depth below the water surface (used for surface effect)
	Vector	m_vecColor;				// Lighting tint information
	float	m_flLuminosity;			// Luminosity information

	static CWaterExplosionEffect	m_waterinstance;
};

//Singleton accessor
extern CWaterExplosionEffect &WaterExplosionEffect( void );

//-----------------------------------------------------------------------------
// Purpose: Water explosion
//-----------------------------------------------------------------------------
class CMegaBombExplosionEffect : public CBaseExplosionEffect
{
	typedef CBaseExplosionEffect BaseClass;
public:
	static	CMegaBombExplosionEffect &Instance( void )	{	return m_megainstance;	}

protected:
	virtual void	CreateCore( void );

	virtual void	CreateDebris( void ) { };
	virtual void	CreateMisc( void ) { };
	virtual void	PlaySound( void ) { };

private:
	static CMegaBombExplosionEffect	m_megainstance;
};

//Singleton accessor
extern CMegaBombExplosionEffect &MegaBombExplosionEffect( void );

class CParticleRenderIterator;
class CParticleSimulateIterator;
struct Particle;


// FleckParticle

class FleckParticle : public Particle
{
public:
	Vector		m_vecVelocity;
	float		m_flRoll;
	float		m_flRollDelta;
	float		m_flDieTime;	// How long it lives for.
	float		m_flLifetime;	// How long it has been alive for so far.
	byte		m_uchColor[3];
	byte		m_uchSize;
};

//
// CFleckParticles
//

class CFleckParticles : public CSimpleEmitter
{
public:

	explicit 				CFleckParticles( const char *pDebugName );
							~CFleckParticles();
	static ks::intrusive_ptr<CFleckParticles> Create( const char *pDebugName, const Vector &vCenter, const Vector &extents );

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	//Setup for point emission
	virtual void		Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags = 0 );

	CParticleCollision m_ParticleCollision;

	CFleckParticles *m_pNextParticleSystem;
private:
	CFleckParticles( const CFleckParticles & ); // not defined, not accessible
};

class CEffectData;
class CBaseEntity;


// Parse the impact data from the server's data block
CBaseEntity *ParseImpactData( const CEffectData &data, Vector *vecOrigin, Vector *vecStart, Vector *vecShotDir, short &nSurfaceProp, int &iMaterial, int &iDamageType, int &iHitbox );

// Get the decal name to use based on an impact with the specified entity, surface material, and damage type
char const *GetImpactDecal( CBaseEntity *pEntity, int iMaterial, int iDamageType );

// Basic decal handling
// Returns true if it hit something
enum
{
	IMPACT_NODECAL = 0x1,
	IMPACT_REPORT_RAGDOLL_IMPACTS = 0x2,
};

bool Impact( Vector &vecOrigin, Vector &vecStart, int iMaterial, int iDamageType, int iHitbox, CBaseEntity *pEntity, trace_t &tr, int nFlags = 0, int maxLODToDecal = ADDDECAL_TO_ALL_LODS );

// Flags for PerformCustomEffects
enum
{
	FLAGS_CUSTIOM_EFFECTS_NOFLECKS = 0x1,
};

// Do spiffy things according to the material hit
void PerformCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags = 0 );

// Play the correct impact sound according to the material hit
void PlayImpactSound( CBaseEntity *pServerEntity, trace_t &tr, Vector &vecServerOrigin, int nServerSurfaceProp );

// This can be used to hook impact sounds and play them at a later time.
// Shotguns do this so it doesn't play 10 identical sounds in the same spot.
typedef void (*ImpactSoundRouteFn)( const char *pSoundName, const Vector &vEndPos );
void SetImpactSoundRoute( ImpactSoundRouteFn fn );

//-----------------------------------------------------------------------------
// Purpose: Enumerator class for ragdolls being affected by bullet forces
//-----------------------------------------------------------------------------
class CRagdollEnumerator : public IPartitionEnumerator
{
public:
	// Forced constructor
	CRagdollEnumerator( Ray_t& shot, int iDamageType );

	// Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	bool Hit( void ) const { return m_bHit; }

private:
	Ray_t			m_rayShot;
	int				m_iDamageType;
	bool			m_bHit;
};

// Types of supported interpolation
enum InterpType_t
{
	INTERP_LINEAR = 0,
	INTERP_SPLINE,
};

class CInterpolatedValue 
{
public:
			CInterpolatedValue( void );
			CInterpolatedValue( float startTime, float endTime, float startValue, float endValue, InterpType_t type );

	void	SetTime( float start, float end );
	void	SetRange( float start, float end );
	void	SetType( InterpType_t type );
	
	// Set the value with no range
	void SetAbsolute( float value );

	// Set the value with range and time supplied
	void Init( float startValue, float endValue, float dt, InterpType_t type = INTERP_LINEAR );

	// Start from the current value and move towards the end value
	void InitFromCurrent( float endValue, float dt, InterpType_t type = INTERP_LINEAR );

	// Find our interpolated value at the given point in time
	float Interp( float curTime );

private:

	float	m_flStartTime;
	float	m_flEndTime;
	float	m_flStartValue;
	float	m_flEndValue;

	int		m_nInterpType;
};

struct FXLineData_t
{
	Vector		m_vecStart;
	Vector		m_vecEnd;
	Vector		m_vecStartVelocity;
	Vector		m_vecEndVelocity;
	float		m_flStartAlpha;
	float		m_flEndAlpha;
	float		m_flStartScale;
	float		m_flEndScale;
	float		m_flDieTime;
	float		m_flLifeTime;
	
	IMaterial	*m_pMaterial;
};

class CFXLine : public CClientSideEffect
{
public:

	CFXLine( const char *name, const FXLineData_t &data );
	
	~CFXLine( void );

	virtual void	Draw( double frametime );
	virtual bool	IsActive( void );
	virtual void	Destroy( void );
	virtual	void	Update( double frametime );

protected:

	FXLineData_t	m_FXData;
};

void FX_DrawLine( const Vector &start, const Vector &end, float scale, IMaterial *pMaterial, const color32 &color );
void FX_DrawLineFade( const Vector &start, const Vector &end, float scale, IMaterial *pMaterial, const color32 &color, float fadeDist );

// Flags
inline constexpr auto FXQUAD_BIAS_SCALE = 0x0001; //Bias the scale's interpolation function
inline constexpr auto FXQUAD_BIAS_ALPHA = 0x0002; //Bias the alpha's interpolation function
inline constexpr auto FXQUAD_COLOR_FADE = 0x0004; //Blend the color towards black via the alpha (overcomes additive ignoring alpha)

struct FXQuadData_t
{
	FXQuadData_t( void );

	void SetFlags( unsigned int flags )			{ m_uiFlags |= flags; }
	void SetOrigin( const Vector &origin )		{ m_vecOrigin = origin; }
	void SetNormal( const Vector &normal )		{ m_vecNormal = normal; }
	void SetScale( float start, float end )		{ m_flStartScale = start; m_flEndScale = end; }
	void SetAlpha( float start, float end )		{ m_flStartAlpha = start; m_flEndAlpha = end; }
	void SetLifeTime( float lifetime )			{ m_flDieTime = lifetime; }
	void SetColor( float r, float g, float b )	{ m_Color = Vector( r, g, b ); }
	void SetAlphaBias( float bias )				{ m_flAlphaBias = bias; }
	void SetScaleBias( float bias )				{ m_flScaleBias = bias; }
	void SetYaw( float yaw, float delta = 0.0f ){ m_flYaw = yaw; m_flDeltaYaw = delta; }

	void SetMaterial( const char *shader );

	unsigned int	m_uiFlags;
	IMaterial		*m_pMaterial;
	Vector			m_vecOrigin;
	Vector			m_vecNormal;
	float			m_flStartScale;
	float			m_flEndScale;
	float			m_flDieTime;
	float			m_flLifeTime;
	float			m_flStartAlpha;
	float			m_flEndAlpha;
	Vector			m_Color;
	float			m_flYaw;
	float			m_flDeltaYaw;
	
	// Only used with FXQUAD_BIAS_ALPHA and FXQUAD_BIAS_SCALE
	float			m_flScaleBias;
	float			m_flAlphaBias;
};

class CFXQuad : public CClientSideEffect
{
public:

	CFXQuad( const FXQuadData_t &data );
	
	~CFXQuad( void );

	virtual void	Draw( double frametime );
	virtual bool	IsActive( void );
	virtual void	Destroy( void );
	virtual	void	Update( double frametime );

protected:

	FXQuadData_t	m_FXData;
};

class CParticleRenderIterator;
class CParticleSimulateIterator;
struct Particle;


inline constexpr auto bitsPARTICLE_TRAIL_VELOCITY_DAMPEN = 0x00000001; //Dampen the velocity as the particles move
inline constexpr auto bitsPARTICLE_TRAIL_COLLIDE = 0x00000002; //Do collision with simulation
inline constexpr auto bitsPARTICLE_TRAIL_FADE = 0x00000004; //Fade away
inline constexpr auto bitsPARTICLE_TRAIL_FADE_IN = 0x00000008; //Fade in

class TrailParticle : public Particle
{
public:

	Vector		m_vecVelocity;
	color32		m_color;				// Particle color
	float		m_flDieTime;			// How long it lives for.
	float		m_flLifetime;			// How long it has been alive for so far.
	float		m_flLength;				// Length of the tail (in seconds!)
	float		m_flWidth;				// Width of the spark
};

inline void Color32ToFloat4( float colorOut[4], const color32 & colorIn )
{
	colorOut[0] = colorIn.r * (1.0f/255.0f);
	colorOut[1] = colorIn.g * (1.0f/255.0f);
	colorOut[2] = colorIn.b * (1.0f/255.0f);
	colorOut[3] = colorIn.a * (1.0f/255.0f);
}

inline void FloatToColor32( color32 &out, float r, float g, float b, float a )
{
	out.r = r * 255;
	out.g = g * 255;
	out.b = b * 255;
	out.a = a * 255;
}

inline void Float4ToColor32( color32 &out, float colorIn[4] )
{
	out.r = colorIn[0] * 255;
	out.g = colorIn[1] * 255;
	out.b = colorIn[2] * 255;
	out.a = colorIn[3] * 255;
}

inline void Color32Init( color32 &out, int r, int g, int b, int a )
{
	out.r = r;
	out.g = g;
	out.b = b;
	out.a = a;
}
//
// CTrailParticles
//

class CTrailParticles : public CSimpleEmitter
{
	DECLARE_CLASS( CTrailParticles, CSimpleEmitter );
public:
	explicit CTrailParticles( const char *pDebugName );
	
	static CTrailParticles	*Create( const char *pDebugName )	{	return new CTrailParticles( pDebugName );	}

	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

	//Setup for point emission
	virtual void	Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags, bool bNotCollideable = false );
	
	void SetFlag( int flags )				{	m_fFlags |= flags;	}
	void SetVelocityDampen( float dampen )	{	m_flVelocityDampen = dampen;	}
	void SetGravity( float gravity )		{	m_ParticleCollision.SetGravity( gravity );	}
	void SetCollisionDamped( float dampen )	{	m_ParticleCollision.SetCollisionDampen( dampen );	}
	void SetAngularCollisionDampen( float dampen )	{ m_ParticleCollision.SetAngularCollisionDampen( dampen );	}

	CParticleCollision	m_ParticleCollision;

protected:

	int					m_fFlags;
	float				m_flVelocityDampen;

private:
	CTrailParticles( const CTrailParticles & ); // not defined, not accessible
};

//
// Sphere trails
//

class CSphereTrails;   // declared only: it was never implemented

// Simple glow emitter won't draw any of it's particles until/unless the pixel visibility test succeeds
class CSimpleGlowEmitter : public CSimpleEmitter
{
	DECLARE_CLASS( CSimpleGlowEmitter, CSimpleEmitter );
public:
	CSimpleGlowEmitter( const char *pDebugName, const Vector &sortOrigin, float flDeathTime );
	static CSimpleGlowEmitter	*Create( const char *pDebugName, const Vector &sortOrigin, float flDeathTime );

	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
protected:

	bool WasTestedInView( unsigned char viewMask );
	bool IsVisibleInView( unsigned char viewMask );
	void SetTestedInView( unsigned char viewMask, bool bTested );
	void SetVisibleInView( unsigned char viewMask, bool bVisible );
	unsigned char CurrentViewMask() const;

	float				m_flDeathTime;			// How long it has been alive for so far.
	float				m_startTime;
	pixelvis_handle_t	m_queryHandle;
private:
	unsigned char		m_wasTested;
	unsigned char		m_isVisible;

private:
	CSimpleGlowEmitter( const CSimpleGlowEmitter & ); // not defined, not accessible
};

inline constexpr auto FXSTATICLINE_FLIP_HORIZONTAL = 0x00000001; //Swaps the TC's so the texture is flipped horizontally
inline constexpr auto FXSTATICLINE_FLIP_VERTICAL = 0x00000002; //Swaps the TC's so the texture is flipped vertically

class CFXStaticLine : public CClientSideEffect
{
public:

	CFXStaticLine( const char *name, const Vector& start, const Vector& end, float scale, float life, const char *shader, unsigned int flags );
	~CFXStaticLine( void );

	virtual void	Draw( double frametime );
	virtual bool	IsActive( void );
	virtual void	Destroy( void );
	virtual	void	Update( double frametime );

protected:

	IMaterial		*m_pMaterial;
	Vector			m_vecStart, m_vecEnd;
	unsigned int	m_uiFlags;
	float			m_fLife;
	float			m_fScale;
};

class CParticleMgr;
class CBaseParticleEntity;

using namespace ::ks::reflect;


class CParticleTrail : public CBaseParticleEntity, public IPrototypeAppEffect
{
public:

	DECLARE_CLASS( CParticleTrail, CBaseParticleEntity );

				CParticleTrail( void );
	virtual		~CParticleTrail( void );

	void		GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );
	
	void		SetEmit( bool bEmit );
	bool		ShouldEmit( void ) { return m_bEmit; }

	void		SetSpawnRate( float rate );

// CBaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

	virtual void	Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs );

	int				m_nAttachment;
	float			m_flLifetime;			// How long this effect will last

private:

	float			m_SpawnRate;			// How many particles per second.
	bool			m_bEmit;				// Keep emitting particles?

	TimedEvent		m_ParticleSpawn;
	CParticleMgr	*m_pParticleMgr;

private:

	CParticleTrail( const CParticleTrail & );
};

class CParticleSimulateIterator;
class SimpleParticle;


class CSplashParticle : public CSimpleEmitter
{
public:
	
	explicit CSplashParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName ), m_bUseClipHeight( false ) {}
	
	// Create
	static CSplashParticle *Create( const char *pDebugName )
	{
		return new CSplashParticle( pDebugName );
	}

	// Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta );

	// Velocity
	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta );

	// Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle );

	void SetClipHeight( float flClipHeight );

	// Simulation
	void SimulateParticles( CParticleSimulateIterator *pIterator );

private:
	CSplashParticle( const CSplashParticle & );
	
	float	m_flClipHeight;
	bool	m_bUseClipHeight;
};

extern void FX_WaterRipple( const Vector &origin, float scale, Vector *pColor, float flLifetime=1.5, float flAlpha=1 );
extern void FX_GunshotSplash( const Vector &origin, const Vector &normal, float scale );
extern void FX_GunshotSlimeSplash( const Vector &origin, const Vector &normal, float scale );

//-----------------------------------------------------------------------------
// Purpose: Retrieve and alter lighting for splashes
// Input  : position - point to check
//			*color - tint of the lighting at this point
//			*luminosity - adjusted luminosity at this point
//-----------------------------------------------------------------------------
inline void FX_GetSplashLighting( Vector position, Vector *color, float *luminosity )
{
	// Compute our lighting at our position
	Vector totalColor = engine->GetLightForPoint( position, true );
	
	// Get our lighting information
	UTIL_GetNormalizedColorTintAndLuminosity( totalColor, color, luminosity );
	
	// Fake a specular highlight (too dim otherwise)
	if ( luminosity != nullptr )
	{
		*luminosity = std::min( 1.0f, (*luminosity) * 4.0f );
		
		// Clamp so that we never go completely translucent
		if ( *luminosity < 0.25f )
		{
			*luminosity = 0.25f;
		}
	}
	
	// Only take a quarter of the tint, mostly we want to be white
	if ( color != nullptr )
	{
		(*color) = ( (*color) * 0.25f ) + Vector( 0.75f, 0.75f, 0.75f );
	}
}

class CUserCmd;
class CBaseCombatWeapon;
struct kbutton_t;


struct CameraThirdData_t
{
	float	m_flPitch;
	float	m_flYaw;
	float	m_flDist;
	float	m_flLag;
	Vector	m_vecHullMin;
	Vector	m_vecHullMax;
};

class IInput
{
public:
	// Initialization/shutdown of the subsystem
	virtual	void		Init_All( void ) = 0;
	virtual void		Shutdown_All( void ) = 0;
	// Latching button states
	virtual int			GetButtonBits( bool bResetState ) = 0;
	// Create movement command
	virtual void		CreateMove ( int sequence_number, float input_sample_frametime, bool active ) = 0;
	virtual void		ExtraMouseSample( float frametime, bool active ) = 0;
	virtual bool		WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand ) = 0;
	virtual void		EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot ) = 0;
	virtual void		DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot ) = 0;

	virtual CUserCmd	*GetUserCmd( int nSlot, int sequence_number ) = 0;

	virtual void		MakeWeaponSelection( CBaseCombatWeapon *weapon ) = 0;

	// Retrieve key state
	virtual float		KeyState ( kbutton_t *key ) = 0;
	// Issue key event
	virtual int			KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding ) = 0;
	// Look for key
	virtual kbutton_t	*FindKey( const char *name ) = 0;

	// Issue commands from controllers
	virtual void		ControllerCommands( void ) = 0;
	// Extra initialization for some joysticks
	virtual bool		ControllerModeActive() = 0;
	virtual void		Joystick_Advanced( bool bSilent ) = 0;
	virtual void		Joystick_SetSampleTime( float frametime ) = 0;
	virtual float		Joystick_GetPitch( void ) = 0;
	virtual float		Joystick_GetYaw( void ) = 0;
	virtual void		Joystick_Querry( float &forward, float &side, float &pitch, float &yaw ) = 0;
	virtual void		Joystick_ForceRecentering( int nStick, bool bSet = true ) = 0;
	virtual void		IN_SetSampleTime( float frametime ) = 0;

	// Accumulate mouse delta
	virtual void		AccumulateMouse( int nSlot ) = 0;
	// Activate/deactivate mouse
	virtual void		ActivateMouse( void ) = 0;
	virtual void		DeactivateMouse( void ) = 0;

	// Clear mouse state data
	virtual void		ClearStates( void ) = 0;
	// Retrieve lookspring setting
	virtual float		GetLookSpring( void ) = 0;

	// Retrieve mouse position
	virtual void		GetFullscreenMousePos( int *mx, int *my, int *unclampedx = 0, int *unclampedy = 0 ) = 0;
	virtual void		SetFullscreenMousePos( int mx, int my ) = 0;
	virtual void		ResetMouse() = 0;
	virtual	float		GetLastForwardMove( void ) = 0;

	// Third Person camera ( TODO/FIXME:  Move this to a separate interface? )
	virtual void		CAM_Think( void ) = 0;
	virtual int			CAM_IsThirdPerson( int nSlot = -1 ) = 0;
	virtual bool		CAM_IsThirdPersonOverview( int nSlot = -1 ) = 0;
	virtual void		CAM_GetCameraOffset( Vector& ofs ) = 0;
	virtual void		CAM_ToThirdPerson(void) = 0;
	virtual void		CAM_ToFirstPerson(void) = 0;
	virtual void		CAM_ToThirdPersonShoulder(void) = 0;
	virtual void		CAM_ToThirdPersonOverview(void) = 0;
	virtual void		CAM_StartMouseMove(void) = 0;
	virtual void		CAM_EndMouseMove(void) = 0;
	virtual void		CAM_StartDistance(void) = 0;
	virtual void		CAM_EndDistance(void) = 0;
	virtual int			CAM_InterceptingMouse( void ) = 0;
	virtual	void		CAM_Command( int command ) = 0;

	// orthographic camera info	( TODO/FIXME:  Move this to a separate interface? )
	virtual void		CAM_ToOrthographic() = 0;
	virtual	bool		CAM_IsOrthographic() const = 0;
	virtual	void		CAM_OrthographicSize( float& w, float& h ) const = 0;

	virtual void		LevelInit( void ) = 0;

	// Causes an input to have to be re-pressed to become active
	virtual void		ClearInputButton( int bits ) = 0;

	virtual	void		CAM_SetCameraThirdData( CameraThirdData_t *pCameraData, const QAngle &vecCameraOffset ) = 0;
	virtual void		CAM_CameraThirdThink( void ) = 0;
};

extern ::IInput *input;

typedef IMaterialProxy *MaterialProxyFactory_t();

class IMaterialProxyDict
{
public:
	// This is used to instance a proxy.
	virtual IMaterialProxy *CreateProxy( const char *proxyName ) = 0;
	// virtual destructor
	virtual	~IMaterialProxyDict() {}
	// Used by EXPOSE_MATERIAL_PROXY to insert all proxies into the dictionary.
	virtual void Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory ) = 0;
};

extern IMaterialProxyDict &GetMaterialProxyDict();

// This side's move helper. srceng links both sides, so the instance cannot live in ks.game.
inline IMoveHelper *g_pMoveHelper = nullptr;

inline IMoveHelper *MoveHelper()
{
	return g_pMoveHelper;
}

//-----------------------------------------------------------------------------
// Purpose: impact effect helpers
//-----------------------------------------------------------------------------

void GetColorForSurface( trace_t *trace, Vector *color );

// Initializers are a way to register your object to be initialized at startup time.
// They're a good way to have global variables without worrying about dependent
// constructors being called. They also make it so init code doesn't depend on the
// global objects it's initializing.

// To use initializers, just use REGISTER_INITIALIZER to register your global variable like this:
// class SomeClass {....}
// SomeClass *g_pSomeClassSingleton = NULL;
// REGISTER_INITIALIZER(SomeClass, &g_pSomeClassSingleton);

typedef void* (*CreateInitializerObjectFn)();
typedef void (*DeleteInitializerObjectFn)(void *ptr);

class Initializer
{
public:
					Initializer(void **pVar, CreateInitializerObjectFn createFn, DeleteInitializerObjectFn deleteFn);

	// Allocates all the global objects.
	static bool		InitializeAllObjects();
	
	// Free all the global objects.
	static void		FreeAllObjects();

private:
	static Initializer			*s_pInitializers;
	
	void						**m_pVar;
	CreateInitializerObjectFn	m_CreateFn;
	DeleteInitializerObjectFn	m_DeleteFn;
	Initializer					*m_pNext;
};

#define REGISTER_INITIALIZER(className, varPointer)												\
	static void* __Initializer__Create##className##Fn()				{return new className;}		\
	static void* __Initializer__Delete##className##Fn(void *ptr)	{delete (className*)ptr;}	\
	static Initializer g_Initializer_##className##(varPointer, __Initializer__Create##className##Fn, __Initializer__Delete##className##Fn);

#define REGISTER_FUNCTION_INITIALIZER(functionName)	\
	static void* __Initializer__Create##functionName##Fn()			{ functionName(); return 0; } \
	static Initializer g_Initializer_##functionName##(0, __Initializer__Create##functionName##Fn, 0);

#undef REGISTER_INITIALIZER
#undef REGISTER_FUNCTION_INITIALIZER

class CUserCmd;
class CBaseCombatWeapon;
struct kbutton_t;


// TrackIR
extern QAngle	g_angleCenter;
// TrackIR

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CKeyboardKey
{
public:
	// Name for key
	char				name[ 32 ];
	// Pointer to the underlying structure
	kbutton_t			*pkey;
	// Next key in key list.
	CKeyboardKey		*next;
};

class CVerifiedUserCmd
{
public:
	CUserCmd	m_cmd;
	CRC32_t		m_crc;
};

class CInput : public ::IInput
{
// Interface
public:
							CInput( void );
							~CInput( void );
	void					InitMouse( void );
	virtual		void		Init_All( void );
	virtual		void		Shutdown_All( void );
	virtual		int			GetButtonBits( bool bResetState );
	virtual		void		CreateMove ( int sequence_number, float input_sample_frametime, bool active );
	virtual		void		ExtraMouseSample( float frametime, bool active );
	virtual		bool		WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand );
	virtual		void		EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int slot );
	virtual		void		DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int slot );

	virtual		CUserCmd	*GetUserCmd( int nSlot, int sequence_number );

	virtual		void		MakeWeaponSelection( CBaseCombatWeapon *weapon );

	virtual		float		KeyState( kbutton_t *key );
	virtual		int			KeyEvent( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual		kbutton_t	*FindKey( const char *name );

	virtual		void		ControllerCommands( void );
	virtual		void		Joystick_Advanced( bool bSilent );
	virtual		void		Joystick_SetSampleTime(float frametime);
	virtual		float		Joystick_GetPitch( void );
	virtual		float		Joystick_GetYaw( void );
	virtual		void		Joystick_Querry( float &forward, float &side, float &pitch, float &yaw );
	virtual		void		Joystick_ForceRecentering( int nStick, bool bSet = true );
	virtual		void		IN_SetSampleTime( float frametime );

	virtual		void		AccumulateMouse( int nSlot );
	virtual		void		ActivateMouse( void );
	virtual		void		DeactivateMouse( void );
	virtual		void		ClearStates( void );
	virtual		float		GetLookSpring( void );

	virtual		void		GetFullscreenMousePos( int *mx, int *my, int *unclampedx = nullptr, int *unclampedy = nullptr );
	virtual		void		SetFullscreenMousePos( int mx, int my );
	virtual		void		ResetMouse( void );

//	virtual		bool		IsNoClipping( void );
	virtual		float		GetLastForwardMove( void );
	virtual		void		ClearInputButton( int bits );

	virtual		void		CAM_Think( void );
	virtual		int			CAM_IsThirdPerson( int nSlot = -1 );
	virtual		bool		CAM_IsThirdPersonOverview( int nSlot = -1 );
	virtual		void		CAM_GetCameraOffset( Vector& ofs );
	virtual		void		CAM_ToThirdPerson(void);
	virtual		void		CAM_ToFirstPerson(void);
	virtual		void		CAM_ToThirdPersonShoulder(void);
	virtual		void		CAM_ToThirdPersonOverview(void);
	virtual		void		CAM_StartMouseMove(void);
	virtual		void		CAM_EndMouseMove(void);
	virtual		void		CAM_StartDistance(void);
	virtual		void		CAM_EndDistance(void);
	virtual		int			CAM_InterceptingMouse( void );
	virtual		void		CAM_Command( int command );

	// orthographic camera info
	virtual		void		CAM_ToOrthographic();
	virtual		bool		CAM_IsOrthographic() const;
	virtual		void		CAM_OrthographicSize( float& w, float& h ) const;
	
	virtual		void		LevelInit( void );

	virtual		void		CAM_SetCameraThirdData( CameraThirdData_t *pCameraData, const QAngle &vecCameraOffset );
	virtual		void		CAM_CameraThirdThink( void );	

	virtual		void		CheckPaused( CUserCmd *cmd );
	virtual		void		CheckSplitScreenMimic( int nSlot, CUserCmd *cmd, CUserCmd *pPlayer0Command );

// Private Implementation
protected:
	// Implementation specific initialization
	virtual void		Init_Camera( void );
	void		Init_Keyboard( void );
	void		Init_Mouse( void );
	void		Shutdown_Keyboard( void );
	// Add a named key to the list queryable by the engine
	void		AddKeyButton( const char *name, kbutton_t *pkb );
	// Mouse/keyboard movement input helpers
	void		ScaleMovements( CUserCmd *cmd );
	void		ComputeForwardMove( int nSlot, CUserCmd *cmd );
	void		ComputeUpwardMove( int nSlot, CUserCmd *cmd );
	void		ComputeSideMove( int nSlot, CUserCmd *cmd );
	void		AdjustAngles ( int nSlot, float frametime );
	void		ClampAngles( QAngle& viewangles );
	void		AdjustPitch( int nSlot, float speed, QAngle& viewangles );
	void		AdjustYaw( int nSlot, float speed, QAngle& viewangles );
	float		DetermineKeySpeed( int nSlot, float frametime );
	void		GetAccumulatedMouseDeltasAndResetAccumulators( int nSlot, float *mx, float *my );
	void		GetMouseDelta( int nSlot, float inmousex, float inmousey, float *pOutMouseX, float *pOutMouseY );
	void		ScaleMouse( int nSlot, float *x, float *y );
	virtual void		ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y );
	void		MouseMove ( int nSlot, CUserCmd *cmd );
	void		MotionControllerMove( float frametime, CUserCmd *cmd );
	float		HandleMotionControllerInputSmoothing( float flDeadZonePct, float val );

	// Joystick  movement input helpers
	void			ControllerMove( int nSlot, float frametime, CUserCmd *cmd );	
	float			ScaleAxisValue( const float axisValue, const float axisThreshold );
	virtual void	JoyStickMove( float frametime, CUserCmd *cmd );

	virtual bool	ControllerModeActive( void );
	virtual bool	JoyStickActive();
	virtual void	JoyStickSampleAxes( float &forward, float &side, float &pitch, float &yaw, bool &bAbsoluteYaw, bool &bAbsolutePitch );
	virtual void	JoyStickThirdPersonPlatformer( CUserCmd *cmd, float &forward, float &side, float &pitch, float &yaw );
	virtual void	JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch );
	virtual void	JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove );
	virtual void	JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove );
	float			ResponseCurveLookDefault( int nSlot, float x, int axis, float otherAxis, float dist, float frametime );
	float			ResponseCurveLook( int nSlot, int curve, float x, int axis, float otherAxis, float dist, float frametime );
	float			ResponseCurveLookAccelerated( int nSlot, float x, int axis, float otherAxis, float dist, float frametime );
	float			ResponseCurveLookPolynomial( int nSlot, float x, int axis, float otherAxis, float dist, float frametime );

	float			AutoAimDampening( float x, int axis, float dist );
	float			UpdateAndGetQuickTurnYaw( int nSlot, float lookFrametime, float angle );

	// Call this to get the cursor position. The call will be logged in the VCR file if there is one.
	void		GetMousePos(int &x, int &y);
	void		SetMousePos(int x, int y);
	virtual void GetWindowCenter( int&x, int& y );
	// Called once per frame to allow convar overrides to acceleration settings when mouse is active
	void		CheckMouseAcclerationVars();

	void		ValidateUserCmd( CUserCmd *usercmd, int sequence_number );

	// TrackIR
	void		TrackIRMove ( float frametime, CUserCmd *cmd );
	void		Init_TrackIR( void );
	void		Shutdown_TrackIR( void );

	bool		m_fTrackIRAvailable;
	// TrackIR

protected:
	typedef struct
	{
		unsigned int AxisFlags;
		unsigned int AxisMap;
		unsigned int ControlMap;
	} joy_axis_t;

	void		DescribeJoystickAxis( int nJoystick, char const *axis, joy_axis_t *mapping );
	char const	*DescribeAxis( int index );

	enum
	{
		GAME_AXIS_NONE = 0,
		GAME_AXIS_FORWARD,
		GAME_AXIS_PITCH,
		GAME_AXIS_SIDE,
		GAME_AXIS_YAW,
		MAX_GAME_AXES
	};

	enum
	{
		CAM_COMMAND_NONE = 0,
		CAM_COMMAND_TOTHIRDPERSON = 1,
		CAM_COMMAND_TOFIRSTPERSON = 2
	};

	enum
	{
		MOUSE_ACCEL_THRESHHOLD1 = 0,	// if mouse moves > this many mickey's double it
		MOUSE_ACCEL_THRESHHOLD2,		// if mouse moves > this many mickey's double it a second time
		MOUSE_SPEED_FACTOR,				// 0 = disabled, 1 = threshold 1 enabled, 2 = threshold 2 enabled

		NUM_MOUSE_PARAMS,
	};

	// Has the mouse been initialized?
	bool		m_fMouseInitialized;
	// Is the mosue active?
	bool		m_fMouseActive;
	// Has the joystick advanced initialization been run?
	bool		m_fJoystickAdvancedInit;
	// Between controller and mouse, what's the primary input
	bool		m_bControllerMode;
	float		m_fAccumulatedMouseMove;
	float		m_lastAutoAimValue;

	// Accumulated mouse deltas

	struct PerUserInput_t
	{
		PerUserInput_t()
		{
			m_flAccumulatedMouseXMovement = 0;
			m_flAccumulatedMouseYMovement = 0;
			m_flPreviousMouseXPosition = 0;
			m_flPreviousMouseYPosition = 0;
			m_flRemainingJoystickSampleTime = 0;
			m_flKeyboardSampleTime = 0;

			m_flSpinFrameTime = 0;
			m_flSpinRate = 0;
			m_flLastYawAngle = 0;

			// Is the 3rd person camera using the mouse?
			m_fCameraInterceptingMouse = 0;
			// Are we in 3rd person view?
			m_fCameraInThirdPerson = 0;
			// Should we move view along with mouse?
			m_fCameraMovingWithMouse = 0;
			// What is the current camera offset from the view origin?
			m_vecCameraOffset.Init();
			// Is the camera in distance moving mode?
			m_fCameraDistanceMove = 0;
			// Old and current mouse position readings.
			m_nCameraOldX = 0;
			m_nCameraOldY = 0;
			m_nCameraX = 0;
			m_nCameraY = 0;

			// orthographic camera settings
			m_CameraIsOrthographic = 0;

			m_CameraIsThirdPersonOverview = 0;

			m_angPreviousViewAngles.Init();
			m_angPreviousViewAnglesTilt.Init();

			m_flLastForwardMove = 0;

			m_pCommands = 0;
			m_pVerifiedCommands = nullptr;

			m_nClearInputState = 0;
			m_pCameraThirdData = nullptr;
			m_nCamCommand = 0;

			m_flPreviousJoystickForwardMove = 0;
			m_flPreviousJoystickSideMove = 0;
			m_flPreviousJoystickYaw = 0;
			m_flPreviousJoystickPitch = 0;
			m_bPreviousJoystickUseAbsoluteYaw = 0;
			m_bPreviousJoystickUseAbsolutePitch = 0;
			m_bForceJoystickRecentering[ 0 ] = false;
			m_bForceJoystickRecentering[ 1 ] = false;
		}

		float		m_flAccumulatedMouseXMovement;
		float		m_flAccumulatedMouseYMovement;
		float		m_flPreviousMouseXPosition;
		float		m_flPreviousMouseYPosition;
		float		m_flRemainingJoystickSampleTime;
		float		m_flKeyboardSampleTime;

		float		m_flSpinFrameTime;
		float		m_flSpinRate;
		float		m_flLastYawAngle;

		// Joystick Axis data
		joy_axis_t m_rgAxes[ MAX_JOYSTICK_AXES ];

		// Is the 3rd person camera using the mouse?
		bool		m_fCameraInterceptingMouse;
		// Are we in 3rd person view?
		bool		m_fCameraInThirdPerson;
		// Should we move view along with mouse?
		bool		m_fCameraMovingWithMouse;
		// What is the current camera offset from the view origin?
		Vector		m_vecCameraOffset;
		// Is the camera in distance moving mode?
		bool		m_fCameraDistanceMove;
		// Old and current mouse position readings.
		int			m_nCameraOldX;
		int			m_nCameraOldY;
		int			m_nCameraX;
		int			m_nCameraY;

		// orthographic camera settings
		bool		m_CameraIsOrthographic;

		// orthographic camera settings
		bool		m_CameraIsThirdPersonOverview;

		QAngle		m_angPreviousViewAngles;
		QAngle		m_angPreviousViewAnglesTilt;

		float		m_flLastForwardMove;

		int			m_nClearInputState;

		CUserCmd	*m_pCommands;
		CVerifiedUserCmd	*m_pVerifiedCommands;

		// Set until polled by CreateMove and cleared
		CHandle< CBaseCombatWeapon > m_hSelectedWeapon;

		CameraThirdData_t	*m_pCameraThirdData;
		int					m_nCamCommand;

		// Cached movement from the previous sample
		// Note: To be used when we run out of sample time,
		//		 instead of leaving zeros in CUserCmd's.
		float	m_flPreviousJoystickForwardMove;
		float	m_flPreviousJoystickSideMove;
		float	m_flPreviousJoystickYaw;
		float	m_flPreviousJoystickPitch;
		bool	m_bPreviousJoystickUseAbsoluteYaw;
		bool	m_bPreviousJoystickUseAbsolutePitch;
		bool	m_bForceJoystickRecentering[ 2 ];
	};

	PerUserInput_t &GetPerUser( int nSlot = -1 );
	const PerUserInput_t &GetPerUser( int nSlot = -1 ) const;

	// Flag to restore systemparameters when exiting
	bool		m_fRestoreSPI;
	// Original mouse parameters
	int			m_rgOrigMouseParms[ NUM_MOUSE_PARAMS ];
	// Current mouse parameters.
	int			m_rgNewMouseParms[ NUM_MOUSE_PARAMS ];
	bool		m_rgCheckMouseParam[ NUM_MOUSE_PARAMS ];
	// Are the parameters valid
	bool		m_fMouseParmsValid;
	// List of queryable keys
	CKeyboardKey *m_pKeys;
	
	PerUserInput_t	m_PerUser[ MAX_SPLITSCREEN_PLAYERS ];

	InputContextHandle_t m_hInputContext;
	std::recursive_mutex m_IKContactPointMutex;
};

extern kbutton_t in_strafe;
extern kbutton_t in_speed;
extern kbutton_t in_zoom;
extern kbutton_t in_jlook;
extern kbutton_t in_graph;  
extern kbutton_t in_moveleft;
extern kbutton_t in_moveright;
extern kbutton_t in_forward;
extern kbutton_t in_back;
extern kbutton_t in_joyspeed;
extern kbutton_t in_lookspin;

extern class ConVar in_joystick;
extern class ConVar joy_autosprint;

extern void KeyDown( kbutton_t *b, const char *c );
extern void KeyUp( kbutton_t *b, const char *c );

class CBaseEntity;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: Interfaces derived from this are able to filter out the local player
//  when doing prediction on the client, this includes not sending network data to
//  the local player from the server if needed.
//-----------------------------------------------------------------------------
class IPredictionSystem
{
public:
	IPredictionSystem()
	{
		m_pNextSystem = g_pPredictionSystems;
		g_pPredictionSystems = this;

		m_bSuppressEvent = false;
		m_pSuppressHost = nullptr;

		m_nStatusPushed = 0;
	};

	virtual ~IPredictionSystem() {};

	IPredictionSystem *GetNext()
	{
		return m_pNextSystem;
	}

	void SetSuppressEvent( bool state )
	{
		m_bSuppressEvent = state;
	}

	void SetSuppressHost( CBaseEntity *host )
	{
		m_pSuppressHost = host;
	}

	CBaseEntity const *GetSuppressHost( void )
	{
		if ( DisableFiltering() )
		{
			return nullptr;
		}

		return m_pSuppressHost;
	}

	bool CanPredict( void ) const
	{
		if ( DisableFiltering() )
		{
			return false;
		}

		return !m_bSuppressEvent;
	}

	static IPredictionSystem *g_pPredictionSystems;

	static void SuppressEvents( bool state )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->SetSuppressEvent( state );
			sys = sys->GetNext();
		}
	}

	static void SuppressHostEvents( CBaseEntity *host )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->SetSuppressHost( host );
			sys = sys->GetNext();
		}
	}

private:

	static void Push( void )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->_Push();
			sys = sys->GetNext();
		}
	}

	static void Pop( void )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->_Pop();
			sys = sys->GetNext();
		}
	}

	void _Push( void )
	{
		++m_nStatusPushed;
	}
	void _Pop( void )
	{
		--m_nStatusPushed;
	}

	bool DisableFiltering( void ) const
	{
		return ( m_nStatusPushed > 0  ) ? true : false;
	}

	IPredictionSystem	*m_pNextSystem;
	bool				m_bSuppressEvent;
	CBaseEntity			*m_pSuppressHost;

	int					m_nStatusPushed;

	friend class CDisablePredictionFiltering;
};

class CDisablePredictionFiltering
{
public:
	CDisablePredictionFiltering( bool disable = true )
	{
		m_bDisabled = disable;
		if ( m_bDisabled )
		{
			IPredictionSystem::Push();
		}
	}

	~CDisablePredictionFiltering( void )
	{
		if ( m_bDisabled )
		{
			IPredictionSystem::Pop();
		}
	}
private:
	bool	m_bDisabled;
};

class CEconItemDefinition;

// Purpose: Stub for item inventory system (econ system removed)

// Type definitions
typedef uint64 globalindex_t;
inline constexpr auto INVALID_GLOBAL_INDEX = ((globalindex_t)0);

enum EItemRequestResult
{
	k_EItemRequestResultOK = 0,
	k_EItemRequestResultPending = 1,
	k_EItemRequestResultFailed = 2,
};

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Stub for script-created items
//-----------------------------------------------------------------------------
class CScriptCreatedItem
{
public:
	CScriptCreatedItem() {}
	virtual ~CScriptCreatedItem() {}

	CEconItemDefinition *GetItemDefinition() const { return nullptr; }
};

//-----------------------------------------------------------------------------
// Purpose: Base class for player inventories
//-----------------------------------------------------------------------------
class CPlayerInventory
{
public:
	CPlayerInventory() {}
	virtual ~CPlayerInventory() {}

	virtual bool ItemShouldBeIncluded( int iItemPosition ) { return true; }
	virtual void InventoryReceived( EItemRequestResult eResult, int iItems ) {}
	virtual void DumpInventoryToConsole( bool bRoot ) {}

	virtual bool MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition ) { return true; }
	virtual bool MoveItemToBackpack( globalindex_t iGlobalIndex ) { return true; }
};

//-----------------------------------------------------------------------------
// Purpose: Base class for inventory managers
//-----------------------------------------------------------------------------
class CInventoryManager
{
public:
	CInventoryManager() {}
	virtual ~CInventoryManager() {}

	virtual void PostInit( void ) {}
};

class CBaseEntity;

using namespace ::ks::reflect;



//-----------------------------------------------------------------------------
// Purpose:  Shared interface to temp entities
//-----------------------------------------------------------------------------
class ITempEntsSystem : public IPredictionSystem
{
public:
	virtual bool SuppressTE( IRecipientFilter& filter ) = 0;
	virtual void ArmorRicochet( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir ) = 0;
	virtual void BeamEntPoint( IRecipientFilter& filer, float delay,
		int	nStartEntity, const Vector *start, int nEndEntity, const Vector* end, 
		int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamEnts( IRecipientFilter& filer, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamFollow( IRecipientFilter& filter, float delay,
		int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
		float fadeLength, float r, float g, float b, float a ) = 0;
	virtual void BeamPoints( IRecipientFilter& filer, float delay,
		const Vector* start, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamLaser( IRecipientFilter& filer, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamRing( IRecipientFilter& filer, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 ) = 0;
	virtual void BeamRingPoint( IRecipientFilter& filer, float delay,
		const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 ) = 0;
	virtual void BeamSpline( IRecipientFilter& filer, float delay,
		int points, Vector* rgPoints ) = 0;
	virtual void BloodStream( IRecipientFilter& filer, float delay,
		const Vector* org, const Vector* dir, int r, int g, int b, int a, int amount ) = 0;
	virtual void BloodSprite( IRecipientFilter& filer, float delay,
		const Vector* org, const Vector *dir, int r, int g, int b, int a, int size ) = 0;
	virtual void BreakModel( IRecipientFilter& filer, float delay,
		const Vector& pos, const QAngle &angle, const Vector& size, const Vector& vel, 
		int modelindex, int randomization, int count, float time, int flags ) = 0;
	virtual void BSPDecal( IRecipientFilter& filer, float delay,
		const Vector* pos, int entity, int index ) = 0;
	virtual void ProjectDecal( IRecipientFilter& filter, float delay,
		const Vector* pos, const QAngle *angles, float distance, int index ) = 0;
	virtual void Bubbles( IRecipientFilter& filer, float delay,
		const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed ) = 0;
	virtual void BubbleTrail( IRecipientFilter& filer, float delay,
		const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed ) = 0;
	virtual void Decal( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* start, int entity, int hitbox, int index ) = 0;
	virtual void DynamicLight( IRecipientFilter& filer, float delay,
		const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay ) = 0;
	virtual void Explosion( IRecipientFilter& filer, float delay,
		const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, const Vector* normal = nullptr, unsigned char materialType = 'C' ) = 0;
	virtual void ShatterSurface( IRecipientFilter& filer, float delay,
		const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
		float width, float height, float shardsize, ShatterSurface_t surfacetype,
		int front_r, int front_g, int front_b, int back_r, int back_g, int back_b) = 0;
	virtual void GlowSprite( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, float life, float size, int brightness ) = 0;
	virtual void FootprintDecal( IRecipientFilter& filer, float delay, const Vector *origin, const Vector* right, 
		int entity, int index, unsigned char materialType ) = 0;
	virtual void Fizz( IRecipientFilter& filer, float delay,
		const CBaseEntity *ed, int modelindex, int density, int current ) = 0;
	virtual void KillPlayerAttachments( IRecipientFilter& filer, float delay,
		int player ) = 0;
	virtual void LargeFunnel( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, int reversed ) = 0;
	virtual void MetalSparks( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir ) = 0;
	virtual void EnergySplash( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir, bool bExplosive ) = 0;
	virtual void PlayerDecal( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox ) = 0;
	virtual void ShowLine( IRecipientFilter& filer, float delay,
		const Vector* start, const Vector* end ) = 0;
	virtual void Smoke( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, float scale, int framerate ) = 0;
	virtual void Sparks( IRecipientFilter& filer, float delay,
		const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir ) = 0;
	virtual void Sprite( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, float size, int brightness ) = 0;
	virtual void SpriteSpray( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count ) = 0;
	virtual void WorldDecal( IRecipientFilter& filer, float delay,
		const Vector* pos, int index ) = 0;
	virtual void MuzzleFlash( IRecipientFilter& filer, float delay,
		const Vector &start, const QAngle &angles, float scale, int type ) = 0;
	virtual void Dust( IRecipientFilter& filer, float delay,
				 const Vector &pos, const Vector &dir, float size, float speed ) = 0;
	virtual void GaussExplosion( IRecipientFilter& filer, float delay,
				const Vector &pos, const Vector &dir, int type ) = 0;
	virtual void PhysicsProp( IRecipientFilter& filter, float delay, int modelindex, int skin, 
		const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor ) = 0;

	// For playback from external tools
	virtual void TriggerTempEntity( KeyValues *pKeyValues ) = 0;

	virtual void ClientProjectile( IRecipientFilter& filter, float delay,
		const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner ) = 0;
};

extern ITempEntsSystem *te;

//-----------------------------------------------------------------------------
// Purpose: Apply effects to view origin/angles, etc.  Screen fade and shake
//-----------------------------------------------------------------------------
class IViewEffects
{
public:
	// Initialize subsystem
	virtual void	Init( void ) = 0;
	// Initialize after each level change
	virtual void	LevelInit( void ) = 0;
	// Called each frame to determine the current view fade parameters ( color and alpha )
	virtual void	GetFadeParams( unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a, bool *blend ) = 0;
	// Apply directscreen shake
	virtual void	Shake( const ScreenShake_t &data ) = 0;
	// Apply direct screen fade
	virtual void	Fade( ScreenFade_t &data ) = 0;
	// Clear all permanent fades in our fade list
	virtual void	ClearPermanentFades( void ) = 0;
	// Clear all fades in our fade list
	virtual void	ClearAllFades( void ) = 0;
	// Compute screen shake values for this frame
	virtual void	CalcShake( void ) = 0;
	// Apply those values to the passed in vector(s).
	virtual void	ApplyShake( Vector& origin, QAngle& angles, float factor ) = 0;
	// Compute screen tilt values for this frame
	virtual void	CalcTilt( void ) = 0;
	// Apply those values to the passed in vector(s).
	virtual void	ApplyTilt( QAngle& angles, float factor ) = 0;
	// Save / Restore
	virtual void	ClearAllShakes( void ) = 0;
};

extern IViewEffects *GetViewEffects();

class CNoopAnimController;
class CUserCmd;
class CBaseEntity;
class CBasePlayer;



// Message mode types
enum
{
	MM_NONE = 0,
	MM_SAY,
	MM_SAY_TEAM,
};

class IClientMode
{
// Misc.
public:

	virtual			~IClientMode() {}

	// Called before the HUD is initialized.
	virtual void	InitViewport()=0;

	// One time init when .dll is first loaded.
	virtual void	Init()=0;

	// Called when vgui is shutting down.
	virtual void	UI_Shutdown() = 0;

	// One time call when dll is shutting down
	virtual void	Shutdown()=0;

	// Called when switching from one IClientMode to another.
	// This can re-layout the view and such.
	// Note that Enable and Disable are called when the DLL initializes and shuts down.
	virtual void	Enable()=0;

	// Called when it's about to go into another client mode.
	virtual void	Disable()=0;

	// Called when initializing or when the view changes.
	// This should move the viewport into the correct position.
	virtual void	Layout( bool bForce = false )=0;

	// Gets at the viewports vgui panel animation controller, if there is one...
	virtual CNoopAnimController *GetViewportAnimationController() = 0;

	// called every time shared client dll/engine data gets changed,
	// and gives the cdll a chance to modify the data.
	virtual void	ProcessInput( bool bActive ) = 0;

	// The mode can choose to draw/not draw entities.
	virtual bool	ShouldDrawDetailObjects( ) = 0;
	virtual bool	ShouldDrawEntity(CBaseEntity *pEnt) = 0;
	virtual bool	ShouldDrawLocalPlayer( CBasePlayer *pPlayer ) = 0;
	virtual bool	ShouldDrawParticles( ) = 0;

	// The mode can choose to not draw fog
	virtual bool	ShouldDrawFog( void ) = 0;

	virtual void	OverrideView( CViewSetup *pSetup ) = 0;
	virtual void	OverrideAudioState( AudioState_t *pAudioState ) = 0;
	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) = 0;
	virtual void	StartMessageMode( int iMessageModeType ) = 0;
	virtual void	OverrideMouseInput( float *x, float *y ) = 0;
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *cmd ) = 0;

	virtual void	LevelInit( const char *newmap ) = 0;
	virtual void	LevelShutdown( void ) = 0;

	// Certain modes hide the view model
	virtual bool	ShouldDrawViewModel( void ) = 0;
	virtual bool	ShouldDrawCrosshair( void ) = 0;

	// Let mode override viewport for engine
	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height ) = 0;

	// Called before rendering a view.
	virtual void	PreRender( CViewSetup *pSetup ) = 0;

	// Called after everything is rendered.
	virtual void	PostRender( void ) = 0;

	virtual void	PostRenderVGui() = 0;

	virtual float	GetViewModelFOV( void ) = 0;

	virtual bool	CanRecordDemo( char *errorMsg, int length ) const = 0;

	virtual wchar_t*	GetServerName( void ) = 0;
	virtual void		SetServerName( wchar_t *name ) = 0;
	virtual wchar_t*	GetMapName( void ) = 0;
	virtual void		SetMapName( wchar_t *name ) = 0;

	virtual void	OnColorCorrectionWeightsReset( void ) = 0;
	virtual float	GetColorCorrectionScale( void ) const = 0;

	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) = 0;

	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup ) = 0;

	virtual void	UpdateCameraManUIState( int iType, int nOptionalParam, uint64 xuid ) = 0;
	virtual void	ScoreboardOff( void ) = 0;
	virtual void	GraphPageChanged( void ) = 0;

// Updates.
public:

	// Called every frame.
	virtual void	Update()=0;	

	virtual void	SetBlurFade( float scale ) = 0;
	virtual float	GetBlurFade( void ) = 0;
};	

extern IClientMode *GetClientMode();
extern IClientMode *GetFullscreenClientMode();

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

//-----------------------------------------------------------------------------
// Forward decls
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Handles to a client shadow
//-----------------------------------------------------------------------------
enum ShadowReceiver_t
{
	SHADOW_RECEIVER_BRUSH_MODEL = 0,
	SHADOW_RECEIVER_STATIC_PROP,
	SHADOW_RECEIVER_STUDIO_MODEL,
};

//-----------------------------------------------------------------------------
// The class responsible for dealing with shadows on the client side
//-----------------------------------------------------------------------------
class IClientShadowMgr : public IGameSystemPerFrame
{
public:
	// Create, destroy shadows
	virtual ClientShadowHandle_t CreateShadow( ClientEntityHandle_t entity, int nEntIndex, int flags, std::bitset< MAX_SPLITSCREEN_PLAYERS > *pSplitScreenBits = nullptr ) = 0;
	virtual void DestroyShadow( ClientShadowHandle_t handle ) = 0;

	// Create flashlight.
	// FLASHLIGHTFIXME: need to rename all of the shadow stuff to projectedtexture and have flashlights and shadows as instances.
	virtual ClientShadowHandle_t CreateFlashlight( const FlashlightState_t &lightState ) = 0;
	virtual void UpdateFlashlightState( ClientShadowHandle_t shadowHandle, const FlashlightState_t &lightState ) = 0;
	virtual void DestroyFlashlight( ClientShadowHandle_t handle ) = 0;

	// Create simple projected texture.  it is not a light or a shadow, but this system does most of the work already for it
	virtual ClientShadowHandle_t CreateProjection( const FlashlightState_t &lightState ) = 0;
	virtual void UpdateProjectionState( ClientShadowHandle_t shadowHandle, const FlashlightState_t &lightState ) = 0;
	virtual void DestroyProjection( ClientShadowHandle_t handle ) = 0;

	// Indicate that the shadow should be recomputed due to a change in
	// the client entity
	virtual void UpdateProjectedTexture( ClientShadowHandle_t handle, bool force = false ) = 0;

	// Used to cause shadows to be re-projected against the world.
	virtual void AddToDirtyShadowList( ClientShadowHandle_t handle, bool force = false ) = 0;
	virtual void AddToDirtyShadowList( IClientRenderable *pRenderable, bool force = false ) = 0;

	// deals with shadows being added to shadow receivers
	virtual void AddShadowToReceiver( ClientShadowHandle_t handle,
		IClientRenderable* pRenderable, ShadowReceiver_t type ) = 0;

	virtual void RemoveAllShadowsFromReceiver( 
		IClientRenderable* pRenderable, ShadowReceiver_t type ) = 0;

	// Re-renders all shadow textures for shadow casters that lie in the leaf list
	virtual void ComputeShadowTextures( const CViewSetup &view, int leafCount, WorldListLeafData_t* pLeafList ) = 0;

	// Frees shadow depth textures for use in subsequent view/frame
	virtual void UnlockAllShadowDepthTextures() = 0;
	
	// Renders the shadow texture to screen...
	virtual void RenderShadowTexture( int w, int h ) = 0;

	// Sets the shadow direction + color
	virtual void SetShadowDirection( const Vector& dir ) = 0;
	virtual const Vector &GetShadowDirection() const = 0;
	
	virtual void SetShadowColor( unsigned char r, unsigned char g, unsigned char b ) = 0;
	virtual void SetShadowDistance( float flMaxDistance ) = 0;
	virtual void SetShadowBlobbyCutoffArea( float flMinArea ) = 0;
	virtual void SetFalloffBias( ClientShadowHandle_t handle, unsigned char ucBias ) = 0;

	// Marks the render-to-texture shadow as needing to be re-rendered
	virtual void MarkRenderToTextureShadowDirty( ClientShadowHandle_t handle ) = 0;

	// Advance the frame
	virtual void AdvanceFrame() = 0;

	// Set and clear flashlight target renderable
	virtual void SetFlashlightTarget( ClientShadowHandle_t shadowHandle, EHANDLE targetEntity ) = 0;

	// Set flashlight light world flag
	virtual void SetFlashlightLightWorld( ClientShadowHandle_t shadowHandle, bool bLightWorld ) = 0;

	virtual void SetShadowsDisabled( bool bDisabled ) = 0;

	virtual void ComputeShadowDepthTextures( const CViewSetup &pView, bool bSetup = false ) = 0;

	virtual void DrawVolumetrics( const CViewSetup &view ) = 0;

	// Toggle shadow casting from world light sources
	virtual void SetShadowFromWorldLightsEnabled( bool bEnable ) = 0;

	virtual void DrawDeferredShadows( const CViewSetup &view, int leafCount, WorldListLeafData_t* pLeafList ) = 0;

	virtual void InitRenderTargets() = 0;

	// Reprojects moved shadows against the world
	virtual void ReprojectShadows() = 0;

	virtual void UpdateSplitscreenLocalPlayerShadowSkip() = 0;

	virtual void GetFrustumExtents( ClientShadowHandle_t handle, Vector &vecMin, Vector &vecMax ) = 0;

	virtual void ShutdownRenderTargets( void ) =0;
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern IClientShadowMgr* g_pClientShadowMgr;

using namespace ::ks::reflect;

// Purpose: Client-server neutral effects interface

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Client-server neutral effects interface
//-----------------------------------------------------------------------------
inline constexpr const char *IEFFECTS_INTERFACE_VERSION = "IEffects001";
class IEffects : public IPredictionSystem
{
public:
	//
	// Particle effects
	//
	virtual void Beam( const Vector &Start, const Vector &End, int nModelIndex, 
		int nHaloIndex, unsigned char frameStart, unsigned char frameRate,
		float flLife, unsigned char width, unsigned char endWidth, unsigned char fadeLength, 
		unsigned char noise, unsigned char red, unsigned char green,
		unsigned char blue, unsigned char brightness, unsigned char speed) = 0;

	//-----------------------------------------------------------------------------
	// Purpose: Emits smoke sprites.
	// Input  : origin - Where to emit the sprites.
	//			scale - Sprite scale * 10.
	//			framerate - Framerate at which to animate the smoke sprites.
	//-----------------------------------------------------------------------------
	virtual void Smoke( const Vector &origin, int modelIndex, float scale, float framerate ) = 0;

	virtual void Sparks( const Vector &position, int nMagnitude = 1, int nTrailLength = 1, const Vector *pvecDir = nullptr ) = 0;

	virtual void Dust( const Vector &pos, const Vector &dir, float size, float speed ) = 0;

	virtual void MuzzleFlash( const Vector &vecOrigin, const QAngle &vecAngles, float flScale, int iType ) = 0;

	// like ricochet, but no sound
	virtual void MetalSparks( const Vector &position, const Vector &direction ) = 0; 

	virtual void EnergySplash( const Vector &position, const Vector &direction, bool bExplosive = false ) = 0;

	virtual void Ricochet( const Vector &position, const Vector &direction ) = 0;

	// FIXME: Should these methods remain in this interface? Or go in some 
	// other client-server neutral interface?
	virtual float Time() = 0;
	virtual bool IsServer() = 0;

	// Used by the playback system to suppress sounds
	virtual void SuppressEffectsSounds( bool bSuppress ) = 0;
};

//-----------------------------------------------------------------------------
// Client-server neutral effects interface accessor
//-----------------------------------------------------------------------------
extern IEffects *g_pEffects;

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Entity that propagates general data needed by clients for every player.

inline constexpr const char *PLAYER_UNCONNECTED_NAME = "unconnected";
inline constexpr const char *PLAYER_ERROR_NAME = "ERRORNAME";

void RecvProxy_ChangedTeam( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CPlayerResource : public CBaseEntity, public IGameResources, public IShaderDeviceDependentObject
{
	[[= ks::reflect::NetTable{ .name = "DT_PlayerResource", .base = false } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPlayerResource, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					CPlayerResource();
	virtual			~CPlayerResource();

public : // IGameResources intreface

	// Team data access 
	virtual int		GetTeamScore( int index );
	virtual const char *GetTeamName( int index );
	virtual const Color&GetTeamColor( int index );

	// Player data access
	virtual bool	IsConnected( int index );
	virtual bool	IsAlive( int index );
	virtual bool	IsFakePlayer( int index );
	virtual bool	IsLocalPlayer( int index  );
	virtual bool	IsHLTV(int index);

	virtual const char *GetPlayerName( int index );
	virtual int		GetPing( int index );
//	virtual int		GetPacketloss( int index );
	virtual int		GetKills( int index );
	virtual int		GetAssists( int index );
	virtual int		GetDeaths( int index );
	virtual int		GetTeam( int index );
	virtual int		GetPendingTeam( int index );
	virtual int		GetFrags( int index );
	virtual int		GetHealth( int index );
	virtual int		GetCoachingTeam( int index );

	virtual void	ClientThink();
	virtual	void	OnDataChanged(DataUpdateType_t updateType);
	virtual void	DeviceLost( void );
	virtual void	DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
	virtual void	ScreenSizeChanged( int width, int height ) { }
	virtual void	TeamChanged( void ){ }

	// Returns the xuid for the given player, but only if they are connected
	XUID			GetXuid( int index );
	void			FillXuidText( int iIndex, char *buf, int bufSize );

protected:
	virtual void	UpdatePlayerName( int slot );
	void	UpdateAsLocalizedFakePlayerName( int slot, char const *pchRawPlayerName );
	void	UpdateXuids( void );

	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	[[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] string_t	m_szName[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iPing[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iKills[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iAssists[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iDeaths[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] bool	m_bConnected[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_ChangedTeam, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iTeam[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_ChangedTeam, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iPendingTeam[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] bool	m_bAlive[MAX_PLAYERS+1];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iHealth[MAX_PLAYERS+1];
	Color	m_Colors[MAX_TEAMS];
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_PRIVATE } ]] int		m_iCoachingTeam[MAX_PLAYERS+1];
	
	XUID	m_Xuids[MAX_PLAYERS+1];
};

extern CPlayerResource *g_PR;

IGameResources *GameResources();

using namespace ::ks::reflect;

// Only care about this stuff in game/client .dlls

class CPlayerState
{
	[[= ks::reflect::NetTable{ .name = "DT_PlayerState", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CPlayerState );
	DECLARE_EMBEDDED_NETWORKVAR();
	
	// This virtual method is necessary to generate a vtable in all cases
	// (DECLARE_PREDICTABLE will generate a vtable also)!
	virtual ~CPlayerState() {}

	// true if the player is dead
	CNetworkVar( bool, deadflag, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]
	                             [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );	
	// Viewing angle (player only)
	QAngle		v_angle;		
	
// The client .dll only cares about deadflag
//  the game and engine .dlls need to worry about the rest of this data

// NOTE:  Only care about this stuff in game/client dlls
// Put at end in case it has any effect on size of structure

	DECLARE_PREDICTABLE();
};

class CBaseEntity;

using namespace ::ks::reflect;

class CPointCamera : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PointCamera" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPointCamera, CBaseEntity );
	DECLARE_CLIENTCLASS();

public:
	CPointCamera();
	~CPointCamera();

	bool IsActive();
	
	// CBaseEntity.
	virtual bool	ShouldDraw();

	float			GetFOV();
	float			GetResolution();
	bool			IsFogEnabled();
	void			GetFogColor( unsigned char &r, unsigned char &g, unsigned char &b );
	float			GetFogStart();
	float			GetFogMaxDensity();
	float			GetFogEnd();
	bool			UseScreenAspectRatio() const { return m_bUseScreenAspectRatio; }

	virtual void	GetToolRecordingState( KeyValues *msg );

protected:
	[[= ks::reflect::Net{} ]] float m_FOV;
	[[= ks::reflect::Net{} ]] float m_Resolution;
	[[= ks::reflect::Net{} ]] bool m_bFogEnable;
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32 m_FogColor;
	[[= ks::reflect::Net{} ]] float m_flFogStart;
	[[= ks::reflect::Net{} ]] float m_flFogEnd;
	[[= ks::reflect::Net{} ]] float m_flFogMaxDensity;
	[[= ks::reflect::Net{} ]] bool m_bActive;
	[[= ks::reflect::Net{} ]] bool m_bUseScreenAspectRatio;

public:
	CPointCamera	*m_pNext;
};

CPointCamera *GetPointCameraList();

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Controls the pose parameters of a model

inline constexpr auto MAX_POSE_CONTROLLED_PROPS = 4; // Number of entities by the same name that can be controlled

// Type of frequency modulations
enum PoseController_FModType_t
{
	POSECONTROLLER_FMODTYPE_NONE = 0,
	POSECONTROLLER_FMODTYPE_SINE,
	POSECONTROLLER_FMODTYPE_SQUARE,
	POSECONTROLLER_FMODTYPE_TRIANGLE,
	POSECONTROLLER_FMODTYPE_SAWTOOTH,
	POSECONTROLLER_FMODTYPE_NOISE,

	POSECONTROLLER_FMODTYPE_TOTAL,
};

//-----------------------------------------------------------------------------
// CLIENT CLASS
//-----------------------------------------------------------------------------

class CPoseController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PoseController" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPoseController, CBaseEntity );
	DECLARE_CLIENTCLASS();

	virtual void	Spawn( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	virtual void	ClientThink( void );

private:

	void	UpdateModulation( void );
	void	UpdatePoseCycle( float fCycleAmount );
	void	SetCurrentPose( float fCurrentPoseValue );

	// Networked variables
	[[= ks::reflect::Net{} ]] EHANDLE						m_hProps[MAX_POSE_CONTROLLED_PROPS];
	[[= ks::reflect::Net{} ]] unsigned char				m_chPoseIndex[MAX_POSE_CONTROLLED_PROPS];
	[[= ks::reflect::Net{} ]] bool						m_bPoseValueParity;
	[[= ks::reflect::Net{} ]] float						m_fPoseValue;
	[[= ks::reflect::Net{} ]] float						m_fInterpolationTime;
	[[= ks::reflect::Net{} ]] bool						m_bInterpolationWrap;
	[[= ks::reflect::Net{} ]] float						m_fCycleFrequency;
	[[= ks::reflect::Net{} ]] PoseController_FModType_t	m_nFModType;
	[[= ks::reflect::Net{} ]] float						m_fFModTimeOffset;
	[[= ks::reflect::Net{} ]] float						m_fFModRate;
	[[= ks::reflect::Net{} ]] float						m_fFModAmplitude;
	bool	m_bOldPoseValueParity;

	float	m_fCurrentPoseValue;	// Actual pose value cycled by the frequency and modulation
	float	m_fCurrentFMod;			// The current fequency modulation amount (stored for noise walk)

	CInterpolatedValue	m_PoseTransitionValue;
};

class CBaseEntity;



// inherit from this interface to be able to call WatchPositionChanges
class IWatcherCallback
{
public:
	virtual ~IWatcherCallback() {}
};

class IPositionWatcher : public IWatcherCallback
{
public:
	virtual void NotifyPositionChanged( CBaseEntity *pEntity ) = 0;
};

// NOTE: The table of watchers is NOT saved/loaded!  Recreate these links on restore
void ReportPositionChanged( CBaseEntity *pMovedEntity );
void WatchPositionChanges( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity );
void RemovePositionWatcher( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity );

// inherit from this interface to be able to call WatchPositionChanges
class IVPhysicsWatcher : public IWatcherCallback
{
public:
	virtual void NotifyVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake ) = 0;
};

// NOTE: The table of watchers is NOT saved/loaded!  Recreate these links on restore
void ReportVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake );
void WatchVPhysicsStateChanges( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity );
void RemoveVPhysicsStateWatcher( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity );

class CBaseEntity;

using namespace ::ks::reflect;

//=============================================================================
//
// Class Postprocess Controller:
//
class CPostProcessController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PostProcessController" } ]]
	[[= ks::reflect::From<"m_PostProcessParameters.m_flParameters",
	ks::reflect::Net{ .wire = "m_flPostProcessParameters" }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPostProcessController, CBaseEntity );
public:
	DECLARE_CLIENTCLASS();

	CPostProcessController();
	virtual ~CPostProcessController();

	virtual void PostDataUpdate( DataUpdateType_t updateType );

	static CPostProcessController* GetMasterController() { return ms_pMasterController; }

	PostProcessParameters_t	m_PostProcessParameters;
	
private:
	[[= ks::reflect::Net{} ]] bool m_bMaster;

	static CPostProcessController* ms_pMasterController;
};

class CSpatialEntity;

// Purpose: Singleton manager for spatial entities on the client


//------------------------------------------------------------------------------
// Purpose : Singleton manager for spatial entities on the client
//------------------------------------------------------------------------------
class CSpatialEntityMgr : public CAutoGameSystemPerFrame
{
	// Inherited from IGameSystemPerFrame
public:
	virtual char const *Name() { return "Spatial Entity Mgr"; }

	// Gets called each frame
	virtual void Update( float frametime );

	// Other public methods
public:

	// Create, destroy spatial entity
	void AddSpatialEntity( CSpatialEntity *pSpatialEntity );
	void RemoveSpatialEntity( CSpatialEntity *pSpatialEntity );

	std::vector< CSpatialEntity* > m_SpatialEntities;
};

// Purpose: A little helper class that computes a spline patch

//-----------------------------------------------------------------------------
// Spline patch: 
//-----------------------------------------------------------------------------

class CSplinePatch
{
public:
	// The last argument represents the number of float channels in addition to position
	CSplinePatch( );
	~CSplinePatch();

	// Call this to initialize the patch
	void Init( int w, int h, int extraChannels );

	// 0 = linear, 1 = spliney!
	void SetLinearBlend( float factor );

	// Hooks the patch up to externally controlled data...
	void SetControlPositions( Vector const** pPositions );
	void SetChannelData( int channel, float* pChannel );

	// This interface isn't wonderful; it's limited by optimization issues...

	// Call this before querying the patch for data at (i,j)
	void SetupPatchQuery( float i, float j );

	// Gets the point and normal at (i,j) specified above
	void GetPointAndNormal( Vector& position, Vector& normal ) const;

	// Gets at other channels
	float GetChannel( int channel ) const;

	// Gets at the dimensions
	int	Width() const { return m_Width; }
	int Height() const { return m_Height; }

public:
	// The integer + float values for the patch query
	int		m_is, m_it;
	float	m_fs, m_ft;

private:
	enum
	{
		MAX_CHANNELS = 4
	};

	// no copy constructor
	CSplinePatch( const CSplinePatch& );

	// Computes indices of the samples to read for this interpolation
	void ComputeIndices( );

	// input data
	int m_Width;
	int m_Height;
	int m_ChannelCount;
	Vector const** m_ppPositions;
	float const* m_pChannel[MAX_CHANNELS];

	// temporary data used for a single patch query
	int		m_SampleIndices[4][4];
	Vector4D m_SVec;
	Vector4D m_TVec;

	float m_LinearFactor;
};

class CEngineSprite;
class CBaseEntity;

using namespace ::ks::reflect;

inline constexpr auto SF_SPRITE_STARTON = 0x0001;
inline constexpr auto SF_SPRITE_ONCE = 0x0002;
inline constexpr auto SF_SPRITE_TEMPORARY = 0x8000;

constexpr float MAX_GLOW_PROXY_SIZE = 64.0f;

constexpr float MAX_SPRITE_SCALE = 64.0f;

void RecvProxy_SpriteScale( const CRecvProxyData *pData, void *pStruct, void *pOut );



class CSpriteRenderer
{
public:
	//-----------------------------------------------------------------------------
	// Purpose: Sprite orientations
	// WARNING!  Change these in common/MaterialSystem/Sprite.cpp if you change them here!
	//-----------------------------------------------------------------------------
	typedef enum
	{
		SPR_VP_PARALLEL_UPRIGHT		= 0,
		SPR_FACING_UPRIGHT			= 1,
		SPR_VP_PARALLEL				= 2,
		SPR_ORIENTED				= 3,
		SPR_VP_PARALLEL_ORIENTED	= 4
	} SPRITETYPE;
	
	// Determine sprite orientation
	static void							GetSpriteAxes( SPRITETYPE type, 
										const Vector& origin,
										const QAngle& angles,
										Vector& forward, 
										Vector& right, 
										Vector& up );

	// Sprites can alter blending amount
	virtual float					GlowBlend( CEngineSprite *psprite, const Vector& entorigin, int rendermode, int renderfx, int alpha, float *scale );

	// Draws tempent as a sprite
	int								DrawSprite( 
										IClientEntity *entity,
										const model_t *model, 
										const Vector& origin, 
										const QAngle& angles,
										float frame,
										IClientEntity *attachedto,
										int attachmentindex,
										int rendermode,
										int renderfx,
										int alpha,
										int r, 
										int g, 
										int b,
										float scale,
										float flHDRColorScale = 1.0f
										);

protected:
	pixelvis_handle_t	m_queryHandle;
	float				m_flGlowProxySize;
	float				m_flHDRColorScale;
};

class CSprite : public CBaseEntity
	, public CSpriteRenderer
{
	[[= ks::reflect::NetTable{ .name = "DT_Sprite" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CSprite, CBaseEntity );
public:
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	CSprite();
	virtual ~CSprite();

	virtual void SetModel( const char *szModelName );

	virtual bool IsSprite( void ) const
	{
		return true;
	};

	bool IsClientOnly() const { return m_bClientOnly; }

	void Spawn( void );
	void Precache( void );
	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	void SetGlowProxySize( float flSize ) { m_flGlowProxySize = flSize; }

	void AnimateThink( void );
	void ExpandThink( void );
	void Animate( float frames );
	void Expand( float scaleSpeed, float fadeSpeed );
	void SpriteInit( const char *pSpriteName, const Vector &origin );

	inline void SetAttachment( CBaseEntity *pEntity, int attachment )
	{
		if ( pEntity )
		{
			m_hAttachedToEntity = pEntity;
			m_nAttachment = attachment;
			FollowEntity( pEntity );
		}
	}

	void TurnOff( void );
	void TurnOn( void );
	bool IsOn() { return !IsEffectActive( EF_NODRAW ); }

	inline float Frames( void ) { return m_flMaxFrame; }
	inline void SetTransparency( int rendermode, int r, int g, int b, int a, int fx )
	{
		SetRenderMode( (RenderMode_t)rendermode );
		SetColor( r, g, b );
		SetBrightness( a );
		SetRenderFX( (RenderFx_t)fx );
	}
	inline void SetTexture( int spriteIndex ) { SetModelIndex( spriteIndex ); }
	inline void SetColor( int r, int g, int b ) { SetRenderColor( r, g, b ); }
	
	void SetBrightness( int brightness, float duration = 0.0f );
	void SetScale( float scale, float duration = 0.0f );
	void SetSpriteScale( float scale );
	void EnableWorldSpaceScale( bool bEnable );

	float GetScale( void ) { return m_flSpriteScale; }
	int	GetBrightness( void ) { return m_nBrightness; }
	float GetHDRColorScale( void ) { return m_flHDRColorScale; }

	inline void FadeAndDie( float duration ) 
	{ 
		SetBrightness( 0, duration );
		SetThink(&CSprite::AnimateUntilDead); 
		m_flDieTime = gpGlobals->curtime + duration; 
		SetNextThink( gpGlobals->curtime );  
	}

	inline void AnimateAndDie( float framerate ) 
	{ 
		SetThink(&CSprite::AnimateUntilDead); 
		m_flSpriteFramerate = framerate;
		m_flDieTime = gpGlobals->curtime + (m_flMaxFrame / m_flSpriteFramerate); 
		SetNextThink( gpGlobals->curtime ); 
	}

	inline void AnimateForTime( float framerate, float time ) 
	{ 
		SetThink(&CSprite::AnimateUntilDead); 
		m_flSpriteFramerate = framerate;
		m_flDieTime = gpGlobals->curtime + time;
		SetNextThink( gpGlobals->curtime ); 
	}

	// FIXME: This completely blows.
	// Surely there's gotta be a better way.
	void FadeOutFromSpawn( )
	{
		SetThink(&CSprite::BeginFadeOutThink); 
		SetNextThink( gpGlobals->curtime + 0.01f ); 
	}

	void BeginFadeOutThink( )
	{
		FadeAndDie( 0.25f ); 
	}

	void AnimateUntilDead( void );
	static CSprite *SpriteCreatePredictable( const char *module, int line, const char *pSpriteName, const Vector &origin, bool animate );

	float	GetRenderScale( void );
	float	GetMaxRenderScale( void );
	int		GetRenderBrightness( void );

	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	virtual const	Vector& GetRenderOrigin();
	virtual void	GetRenderBounds( Vector &vecMins, Vector &vecMaxs );
	virtual float	GlowBlend( CEngineSprite *psprite, const Vector& entorigin, int rendermode, int renderfx, int alpha, float *scale );
	virtual void	GetToolRecordingState( KeyValues *msg );

	virtual void	ClientThink( void );
	virtual void	OnDataChanged( DataUpdateType_t updateType );

	static void RecreateAllClientside();
	static void DestroyAllClientside();
	static void ParseAllClientsideEntities(const char *pMapData);
	static const char *ParseClientsideEntity( const char *pEntData );

	bool InitializeClientside();

	virtual bool KeyValue( const char *szKeyName, const char *szValue ) ;	

public:
	CNetworkHandle( CBaseEntity, m_hAttachedToEntity, [[= ks::reflect::Net{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 8 } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flSpriteFramerate, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 60.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "framerate" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flFrame, [[= ks::reflect::Net{ .bits = 20, .low = 0.0f, .high = 256.0f, .flags = SPROP_ROUNDDOWN } ]] [[= ks::reflect::Key{ .name = "frame" } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

	float		m_flDieTime;

private:

	CNetworkVar( int, m_nBrightness, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flBrightnessTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	
	CNetworkVar( float, m_flSpriteScale, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = MAX_SPRITE_SCALE, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Proxy<RecvProxy_SpriteScale, ks::reflect::WireSide::Recv>{} ]] [[= ks::reflect::Key{ .name = "scale" } ]] [[= ks::reflect::Key{ .name = "SetScale", .input = true } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]]);
	CNetworkVar( float, m_flScaleTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( bool, m_bWorldSpaceScale, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flGlowProxySize, [[= ks::reflect::Net{ .bits = 6, .low = 0.0f, .high = MAX_GLOW_PROXY_SIZE, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "GlowProxySize" } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );

	float		m_flLastTime;
	float		m_flMaxFrame;

	float		m_flStartScale;
	float		m_flDestScale;		//Destination scale
	float		m_flScaleTimeStart;	//Real time for start of scale
	int			m_nStartBrightness;
	int			m_nDestBrightness;		//Destination brightness
	float		m_flBrightnessTimeStart;//Real time for brightness

	bool		m_bClientOnly;
};

class CSpriteOriented : public CSprite
{
	[[= ks::reflect::NetTable{ .name = "DT_SpriteOriented" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSpriteOriented, CSprite );
	DECLARE_CLIENTCLASS();
	virtual RenderableTranslucencyType_t ComputeTranslucencyType();
};

// Macro to wrap creation
template <typename T1, typename T2, typename T3>
inline auto SPRITE_CREATE_PREDICTABLE( T1 name, T2 origin, T3 animate, std::source_location loc = std::source_location::current() )
{
	return CSprite::SpriteCreatePredictable( loc.file_name(), static_cast<int>( loc.line() ), name, origin, animate );
}

using namespace ::ks::reflect;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Sprite trail
//-----------------------------------------------------------------------------
struct TrailPoint_t
{
	DECLARE_SIMPLE_DATADESC();

	Vector	m_vecScreenPos;
	float	m_flDieTime;
	float	m_flTexCoord;
	float	m_flWidthVariance;
};

class CSpriteTrail : public CSprite
{
	[[= ks::reflect::NetTable{ .name = "DT_SpriteTrail" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CSpriteTrail, CSprite );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

public:
	CSpriteTrail( void );

	// Sets parameters of the sprite trail
	void SetLifeTime( float time );
	void SetStartWidth( float flStartWidth );
	void SetEndWidth( float flEndWidth );
	void SetStartWidthVariance( float flStartWidthVariance );
	void SetTextureResolution( float flTexelsPerInch );
	void SetMinFadeLength( float flMinFadeLength );
	void SetSkybox( const Vector &vecSkyboxOrigin, float flSkyboxScale );

	// Is the trail in the skybox?
	bool IsInSkybox() const;
	void Spawn( void );
	void Precache( void );

	// Client only code
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual const Vector &GetRenderOrigin( void );
	virtual const QAngle &GetRenderAngles( void );

	// On data update
	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void GetRenderBounds( Vector& mins, Vector& maxs );
	virtual void ClientThink();

private:
	enum
	{
		// NOTE: # of points max must be a power of two!
		MAX_SPRITE_TRAIL_POINTS	= 64,
		MAX_SPRITE_TRAIL_MASK = 0x3F,
	};

	TrailPoint_t *GetTrailPoint( int n );
	void	UpdateTrail( void );
	void	ComputeScreenPosition( Vector *pScreenPos );
	void	ConvertSkybox();
	void	UpdateBoundingBox( void );

	TrailPoint_t	m_vecSteps[MAX_SPRITE_TRAIL_POINTS];
	int	m_nFirstStep;
	int m_nStepCount;
	float m_flUpdateTime;
	Vector m_vecPrevSkyboxOrigin;
	float m_flPrevSkyboxScale;
	Vector m_vecRenderMins;
	Vector m_vecRenderMaxs;

	CNetworkVar( float, m_flLifeTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );	// Amount of time before a new trail segment fades away
	CNetworkVar( float, m_flStartWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "startwidth" } ]] );	// The starting scale
	CNetworkVar( float, m_flEndWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "endwidth" } ]] );	// The ending scale
	CNetworkVar( float, m_flStartWidthVariance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// The starting scale
	CNetworkVar( float, m_flTextureRes, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Texture resolution along the trail
	CNetworkVar( float, m_flMinFadeLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// The end of the trail must fade out for this many units
	CNetworkVector( m_vecSkyboxOrigin, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );	// What's our skybox origin?
	CNetworkVar( float, m_flSkyboxScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// What's our skybox scale?

	[[= ks::reflect::Key{ .name = "spritename" } ]] string_t m_iszSpriteName;
	[[= ks::reflect::Key{ .name = "animate" } ]] bool	m_bAnimate;
};

class CBaseEntity;

using namespace ::ks::reflect;

//
// Special glow overlay
//

class CSunGlowOverlay : public CGlowOverlay
{
	virtual void CalcSpriteColorAndSize( float flDot, CGlowSprite *pSprite, float *flHorzSize, float *flVertSize, Vector *vColor );

public:

	void SetModulateByDot( bool state = true )
	{
		m_bModulateByDot = state;
	}

protected:

	bool m_bModulateByDot;
};

//
// Sun entity
//

// named by the Bare<> annotation below, so it cannot be a .cpp static
void RecvProxy_SunHDRColorScale( const CRecvProxyData *pData, void *pStruct, void *pOut );

class CSun : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Sun", .base = false } ]]
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::Bare<"HDRColorScale",
	ks::reflect::Net{ .enc = ks::reflect::WireEnc::Float }, RecvProxy_SunHDRColorScale>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSun, CBaseEntity );
	DECLARE_CLIENTCLASS();

					CSun();
					~CSun();

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:
	CSunGlowOverlay	m_Overlay;
	CSunGlowOverlay	m_GlowOverlay;
	
	[[= ks::reflect::Net{} ]] [[= ks::reflect::Proxy<RecvProxy_Int32ToColor32, ks::reflect::WireSide::Recv>{} ]] color32				m_clrOverlay;
	[[= ks::reflect::Net{} ]] int					m_nSize;
	[[= ks::reflect::Net{} ]] int					m_nOverlaySize;
	[[= ks::reflect::Net{} ]] Vector				m_vDirection;
	[[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int } ]] bool				m_bOn;

	[[= ks::reflect::Net{} ]] int					m_nMaterial;
	[[= ks::reflect::Net{} ]] int					m_nOverlayMaterial;
};

#ifdef USE_BLOBULATOR

void Surface_Draw( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth, bool bSurfaceNoParticleCull = false );

void Surface_SafeLightCubeUpdate( const Vector &vecRenderOrigin, Vector4D *cachedCubeColours );

extern ks::aligned_vector< ImpParticleWithFourInterpolants, 16 > g_SurfaceRenderParticles;
extern const QAngle g_SurfaceRenderAnglesAngles;

#endif

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

// Used to initialize m_flBaseDamage to something that we know pretty much for sure
// hasn't been modified by a user. 
inline constexpr auto BASEDAMAGE_NOT_SPECIFIED = std::numeric_limits<float>::max();

class CTakeDamageInfo
{
public:
	DECLARE_CLASS_NOBASE( CTakeDamageInfo );

					CTakeDamageInfo();
					CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType = 0, int iObjectsPenetrated = 0 );
					CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, float flDamage, int bitsDamageType, int iKillType = 0, int iObjectsPenetrated = 0 );
					CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector *reportedPosition = nullptr, int iObjectsPenetrated = 0 );
					CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector *reportedPosition = nullptr, int iObjectsPenetrated = 0 );
	
	// Inflictor is the weapon or rocket (or player) that is dealing the damage.
	CBaseEntity*	GetInflictor() const;
	void			SetInflictor( CBaseEntity *pInflictor );

	// Weapon is the weapon that did the attack.
	// For hitscan weapons, it'll be the same as the inflictor. For projectile weapons, the projectile 
	// is the inflictor, and this contains the weapon that created the projectile.
	CBaseEntity*	GetWeapon() const;
	void			SetWeapon( CBaseEntity *pWeapon );

	// Attacker is the character who originated the attack (like a player or an AI).
	CBaseEntity*	GetAttacker() const;
	void			SetAttacker( CBaseEntity *pAttacker );

	float			GetDamage() const;
	void			SetDamage( float flDamage );
	float			GetMaxDamage() const;
	void			SetMaxDamage( float flMaxDamage );
	void			ScaleDamage( float flScaleAmount );
	void			AddDamage( float flAddAmount );
	void			SubtractDamage( float flSubtractAmount );

	float			GetBaseDamage() const;
	bool			BaseDamageIsValid() const;

	Vector			GetDamageForce() const;
	void			SetDamageForce( const Vector &damageForce );
	void			ScaleDamageForce( float flScaleAmount );

	Vector			GetDamagePosition() const;
	void			SetDamagePosition( const Vector &damagePosition );

	Vector			GetReportedPosition() const;
	void			SetReportedPosition( const Vector &reportedPosition );

	int				GetDamageType() const;
	void			SetDamageType( int bitsDamageType );
	void			AddDamageType( int bitsDamageType );
	int				GetDamageCustom( void ) const;
	void			SetDamageCustom( int iDamageCustom );
	int				GetDamageStats( void ) const;
	void			SetDamageStats( int iDamageStats );

	int				GetAmmoType() const;
	void			SetAmmoType( int iAmmoType );
	const char *	GetAmmoName() const;

	float			GetRadius() const;
	void			SetRadius( float fRadius );

	int				GetObjectsPenetrated() const;	// Returns number of objects this damage has passed through
	void			SetObjectsPenetrated( int iObjectrPenetrated );

	int				GetDamagedOtherPlayers() const     { return m_iDamagedOtherPlayers; }
	void			SetDamagedOtherPlayers( int iVal ) { m_iDamagedOtherPlayers = iVal; }

	void			Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType = 0, int iObjectsPenetrated = 0);
	void			Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, float flDamage, int bitsDamageType, int iKillType = 0, int iObjectsPenetrated = 0 );
	void			Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector *reportedPosition = nullptr, int iObjectsPenetrated = 0 );
	void			Set( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector *reportedPosition = nullptr, int iObjectsPenetrated = 0 );

	void			AdjustPlayerDamageInflictedForSkillLevel();
	void			AdjustPlayerDamageTakenForSkillLevel();

	// Given a damage type (composed of the #defines above), fill out a string with the appropriate text.
	// For designer debug output.
	static void		DebugGetDamageTypeString(unsigned int DamageType, char *outbuf, int outbuflength );

//private:
	void			CopyDamageToBaseDamage();

	uint32			GetBulletID() const				{ return m_uiBulletID; }
	uint8			GetRecoilIndex() const			{ return m_uiRecoilIndex; }
	void			SetBulletID( uint32 bulletid, uint8 uiRecoilIndex ); 
protected:
	void			Init( CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, const Vector &reportedPosition, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated );

	Vector			m_vecDamageForce;
	Vector			m_vecDamagePosition;
	Vector			m_vecReportedPosition;	// Position players are told damage is coming from
	EHANDLE			m_hInflictor;
	EHANDLE			m_hAttacker;
	EHANDLE			m_hWeapon;
	float			m_flDamage;
	float			m_flMaxDamage;
	float			m_flBaseDamage;			// The damage amount before skill leve adjustments are made. Used to get uniform damage forces.
	int				m_bitsDamageType;
	int				m_iDamageCustom;
	int				m_iDamageStats;
	int				m_iAmmoType;			// AmmoType of the weapon used to cause this damage, if any
	float			m_flRadius;
	int				m_iDamagedOtherPlayers;
	int				m_iObjectsPenetrated;	// Number of objects penetrated
	uint32			m_uiBulletID;
	uint8			m_uiRecoilIndex;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: Multi damage. Used to collect multiple damages in the same frame (i.e. shotgun pellets)
//-----------------------------------------------------------------------------
class CMultiDamage : public CTakeDamageInfo
{
	DECLARE_CLASS( CMultiDamage, CTakeDamageInfo );
public:
	CMultiDamage();

	bool			IsClear( void ) { return (m_hTarget == nullptr); }
	CBaseEntity		*GetTarget() const;
	void			SetTarget( CBaseEntity *pTarget );

	void			Init( CBaseEntity *pTarget, CBaseEntity *pInflictor, CBaseEntity *pAttacker, CBaseEntity *pWeapon, const Vector &damageForce, const Vector &damagePosition, const Vector &reportedPosition, float flDamage, int bitsDamageType, int iKillType, int iObjectsPenetrated );

protected:
	EHANDLE			m_hTarget;

	DECLARE_SIMPLE_DATADESC();
};

extern CMultiDamage g_MultiDamage;

// Multidamage accessors
void ClearMultiDamage( void );
void ApplyMultiDamage( void );
void AddMultiDamage( const CTakeDamageInfo &info, CBaseEntity *pEntity );

//-----------------------------------------------------------------------------
// Purpose: Utility functions for physics damage force calculation 
//-----------------------------------------------------------------------------
float ImpulseScale( float flTargetMass, float flDesiredSpeed );
void CalculateExplosiveDamageForce( CTakeDamageInfo *info, const Vector &vecDir, const Vector &vecForceOrigin, float flScale = 1.0 );
void CalculateBulletDamageForce( CTakeDamageInfo *info, int iBulletType, const Vector &vecBulletDir, const Vector &vecForceOrigin, float flScale = 1.0 );
void CalculateMeleeDamageForce( CTakeDamageInfo *info, const Vector &vecMeleeDir, const Vector &vecForceOrigin, float flScale = 1.0 );
void GuessDamageForce( CTakeDamageInfo *info, const Vector &vecForceDir, const Vector &vecForceOrigin, float flScale = 1.0 );

// -------------------------------------------------------------------------------------------------- //
// Inlines.
// -------------------------------------------------------------------------------------------------- //
