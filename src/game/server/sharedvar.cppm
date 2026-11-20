// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

module;
#include <time.h>
#include "dt_recv.h"
#include "iservernetworkable.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "string_t.h"
#include "predictable_entity.h"
#include "eiface.h"
#include "worldsize.h"
#include "baseentity.h"
#include "studio.h"
#include "vphysics/constraints.h"
#include "basecombatweapon_shared.h"
#include "dt_send.h"
#include "env_debughistory.h"
#include "declare_class.h"
#include "soundflags.h"
#include "eventlist.h"
#include "igameevents.h"
#include "datamap.h"
#include "bspflags.h"
#include "networkstringtabledefs.h"
#include "bspfile.h"
#include "ispsharedmemory.h"
#include "edict.h"
#include "vcollide_parse.h"
#include "irecipientfilter.h"
#include "stringregistry.h"
#include "vphysics_interface.h"
#include "vphysics/vehicles.h"
#include "tier2/resourceprecacher.h"
#include "vscript/ivscript.h"
#include "trains.h"
#include "vscript_game.h"
#include "vscript_shared.h"
module ks.server:sharedvar;

import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.playerid;
import ks.game.ai_activity;
import :vscript;
import :fish;
import :baseentity;
import ks.game.shareddefs;
import std;
import ks.core.types;
import ks.game.beam_flags;
import ks.core.threadtools;
import ks.tier1.createinterface;
import ks.core.random;
import ks.tier1.fixed_pool;
import ks.core.wchartypes;
import ks.math.mathlib;
import ks.tier1.convar;
import ks.tier1.envelope;
import ks.tier1.keyvalues;
import ks.core.platform;
import ks.core.tslist;
import ks.tier1.bitbuf;
import ks.tier1.checksum_crc;

using namespace ::ks::reflect;

// Purpose: Server side view model object

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

	bool IsViewable_Impl(void) { return false; }

	 void					UpdateOnRemove_Impl( void );

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

	 void			Precache_Impl( void );

	 void			Spawn_Impl( void );

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

	 int				UpdateTransmitState_Impl( void );
	 int				ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );
	 void			SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

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
struct inputdata_t;

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

// I've seen CBeams glitter in the dark near Tannhauser gate...
class CBeam : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Beam", .base = false } ]]
	[[= ks::reflect::From<"m_nRenderFX", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nRenderMode", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex }>{} ]]
	// The remaining inherited props differ by side in the member they read and in their proxy, and a
	// class-level annotation may be guarded where a member-level one may not.
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED | SPROP_CHANGES_OFTEN }, SendProxy_Color32ToInt32>{} ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .flags = SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
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
	DECLARE_DATADESC();

	CBeam();

	 void SetModel_Impl( const char *szModelName );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	int		ObjectCaps_Impl( void );
	void	SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );
	int		UpdateTransmitState_Impl( void );
	int		ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );

	 int DrawDebugTextOverlays_Impl(void);

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

	 const Vector &WorldSpaceCenter_Impl( void ) const;

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

protected:
	CNetworkVar( float, m_flFrameRate, [[= ks::reflect::Net{ .bits = 10, .low = -25.0f, .high = 25.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );
	float		m_flFireTime;
	[[= ks::reflect::Key{ .name = "damage" } ]] float		m_flDamage;			// Damage per second to touchers.
	CNetworkVar( int, m_nNumBeamEnts, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );

private:
	[[= ks::reflect::Input{ .name = "Noise", .type = FIELD_FLOAT } ]] void InputNoise( inputdata_t &inputdata );
 	[[= ks::reflect::Input{ .name = "Width", .type = FIELD_FLOAT } ]] void InputWidth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorRedValue", .type = FIELD_FLOAT } ]] void InputColorRedValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorBlueValue", .type = FIELD_FLOAT } ]] void InputColorBlueValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorGreenValue", .type = FIELD_FLOAT } ]] void InputColorGreenValue( inputdata_t &inputdata );

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

	[[= ks::reflect::Key{ .name = "dissolvetype" } ]] int			m_nDissolveType;
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
#define BEAMENT_ENTITY(x)		((x)&0xFFF)
#define BEAMENT_ATTACHMENT(x)	(((x)>>12)&0xF)

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

#undef BEAMENT_ENTITY
#undef BEAMENT_ATTACHMENT

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Bullet types
//-----------------------------------------------------------------------------

// -----------------------------------------
//	Sounds
// -----------------------------------------


extern void	SpawnBlood(Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

class CBaseCombatWeapon;

class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CBaseViewModel;
class CEconItemView;
struct animevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


extern void OnBaseCombatWeaponCreated( CBaseCombatWeapon * );
extern void OnBaseCombatWeaponDestroyed( CBaseCombatWeapon * );

void *SendProxy_SendLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
void *SendProxy_SendActiveLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );

// The two exclusive sub-tables are named as props of DT_BaseCombatWeapon rather than derived from a
// member, so both the tables and the proxies that gate them have to be visible where the annotation
// sits.
namespace DT_LocalWeaponData { extern SendTable g_SendTable; }
namespace DT_LocalActiveWeaponData { extern SendTable g_SendTable; }

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

class CHudTexture;

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

	virtual void HandleAnimEvent( animevent_t *pEvent );
	 int  ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo ) ;
	 int	UpdateTransmitState_Impl() ;

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
	[[= ks::reflect::SubTable<"LocalWeaponData", &DT_LocalWeaponData::g_SendTable,
	SendProxy_SendLocalWeaponDataTable, true>{} ]]
	[[= ks::reflect::SubTable<"LocalActiveWeaponData", &DT_LocalActiveWeaponData::g_SendTable,
	SendProxy_SendActiveLocalWeaponDataTable, true>{} ]]
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

	 bool			IsBaseCombatWeapon_Impl( void ) const { return true; }
	 CBaseCombatWeapon *MyCombatWeaponPointer_Impl( void ) { return this; }

	// A derived weapon class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const { return false; }

	 void			Spawn_Impl( void );
	 void			Precache_Impl( void );

	void					MakeTracer_Impl( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );

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

	 void			Activate_Impl( void );

public:

// Server Only Methods

	CBaseWeaponWorldModel*	CreateWeaponWorldModel( void );

	DECLARE_DATADESC();
	virtual void			FallInit( void );						// prepare to fall to the ground
	virtual void			FallThink( void );						// make the weapon fall to the ground after spawning

	// Weapon spawning
	bool					IsConstrained() { return m_pConstraint != nullptr; }
	bool					IsInBadPosition ( void );				// Is weapon in bad position to pickup?
	bool					RepositionWeapon ( void );				// Attempts to reposition the weapon in a location where it can be
	virtual void			Materialize( void );					// make a weapon visible and tangible
	void					AttemptToMaterialize( void );			// see if the game rules will let the weapon become visible and tangible
	virtual void			CheckRespawn( void );					// see if this weapon should respawn after being picked up
	CBaseEntity				*Respawn_Impl ( void );						// copy a weapon

	static int				GetAvailableWeaponsInBox( CBaseCombatWeapon **pList, int listMax, const Vector &mins, const Vector &maxs );

	// Weapon dropping / destruction
	virtual void			Delete( void );
	void					DestroyItem( void );
	virtual void			Kill( void );

	virtual int				CapabilitiesGet( void ) { return 0; }
	 int				ObjectCaps_Impl( void );

	bool					IsRemoveable() { return m_bRemoveable; }
	void					SetRemoveable( bool bRemoveable ) { m_bRemoveable = bRemoveable; }
	
	// Returns bits for	weapon conditions

	virtual void			Operator_FrameUpdate( CBaseCombatCharacter  *pOperator );
	virtual void			Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	virtual void			Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary, CBaseEntity *pTarget = nullptr ) { return; }
	// NOTE: This should never be called when a character is operating the weapon.  Animation events should be
	// routed through the character, and then back into CharacterAnimEvent() 
	void					HandleAnimEvent( animevent_t *pEvent );

	 int				UpdateTransmitState_Impl( void );

	[[= ks::reflect::Input{ .name = "HideWeapon", .type = FIELD_VOID } ]] void					InputHideWeapon( inputdata_t &inputdata );
	void					Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual void			MakeWeaponNameFromEntity( CBaseEntity *pOther );

	//weapon module body groups
	virtual void			SetWeaponModules( void );

// Client only methods

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

	void ShowWeaponWorldModel( bool bVisible );
			
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

	float					m_flLastTimeInAir;
	 void			PhysicsSimulate_Impl( void );

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

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nNextThinkTick );

	int						WeaponState() const { return m_iState; }

	int						m_iSubType;

	float					m_flUnlockTime;
	EHANDLE					m_hLocker;				// Who locked this weapon.

	CNetworkVar( bool, m_bFlipViewModel, [[= ks::reflect::Net{ .bits = -1, .table = "DT_LocalWeaponData" } ]] );
	
	CNetworkVar( int, m_iWeaponOrigin, [[= ks::reflect::Net{ .bits = -1, .table = "DT_LocalWeaponData" } ]] );			// How the player acquired the weapon

	IPhysicsConstraint		*GetConstraint() { return m_pConstraint; }

	 CStudioHdr			*OnNewModel_Impl() ;
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

	// Outputs
protected:
	[[= ks::reflect::Key{ .name = "OnPlayerUse" } ]] COutputEvent			m_OnPlayerUse;		// Fired when the player uses the weapon.
	[[= ks::reflect::Key{ .name = "OnPlayerPickup" } ]] COutputEvent			m_OnPlayerPickup;	// Fired when the player picks up the weapon.
	[[= ks::reflect::Key{ .name = "OnNPCPickup" } ]] COutputEvent			m_OnNPCPickup;		// Fired when an NPC picks up the weapon.
	[[= ks::reflect::Key{ .name = "OnCacheInteraction" } ]] COutputEvent			m_OnCacheInteraction;	// For awarding lambda cache achievements in HL2 on 360. See .FGD file for details 

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

//-----------------------------------------------------------------------------
// For networking this bad boy
//-----------------------------------------------------------------------------
EXTERN_SEND_TABLE( DT_EntityParticleTrailInfo );

//-----------------------------------------------------------------------------
// Particle trail info
//-----------------------------------------------------------------------------
struct EntityParticleTrailInfo_t
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityParticleTrailInfo", .base = false } ]]
	[[= ks::reflect::From<"m_flLifetime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flStartSize", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flEndSize", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	static constexpr bool ks_class = true;

	EntityParticleTrailInfo_t();

	DECLARE_CLASS_NOBASE( EntityParticleTrailInfo_t );
	DECLARE_SIMPLE_DATADESC();
	DECLARE_EMBEDDED_NETWORKVAR();

	[[= ks::reflect::Key{ .name = "ParticleTrailMaterial" } ]] string_t m_strMaterialName;
	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Key{ .name = "ParticleTrailLifetime" } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Key{ .name = "ParticleTrailStartSize" } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Key{ .name = "ParticleTrailEndSize" } ]] );
};

enum debughistorycategories_t
{
	HISTORY_ENTITY_IO,
	HISTORY_AI_DECISIONS,
	HISTORY_SCENE_PRINT,
	HISTORY_ALYX_BLIND,		// TEMP: until we find and fix this bug
	HISTORY_PLAYER_DAMAGE,  // record all damage done to the player

	// Add new categories here

	MAX_HISTORY_CATEGORIES,
};

// Debug history is compiled out.
#define DISABLE_DEBUG_HISTORY

void ClearDebugHistory();

#undef DISABLE_DEBUG_HISTORY

class CBaseEntity;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Implementation of the class that controls detail prop fade distances
//-----------------------------------------------------------------------------
class CEnvDetailController : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_DetailController", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvDetailController, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CEnvDetailController();
	virtual ~CEnvDetailController();

using BaseClass::KeyValue_Impl;
	 bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	CNetworkVar( float, m_flFadeStartDist, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flFadeEndDist, [[= ks::reflect::Net{ .bits = 32 } ]] );

	// ALWAYS transmit to all clients.
	 int UpdateTransmitState_Impl( void );
};

CEnvDetailController * GetDetailController();

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: Entity that fires outputs whenever the player stands on a different surface
//-----------------------------------------------------------------------------
class CEnvPlayerSurfaceTrigger : public CPointEntity
{
	DECLARE_CLASS( CEnvPlayerSurfaceTrigger, CPointEntity );
public:
	DECLARE_DATADESC();

	~CEnvPlayerSurfaceTrigger( void );
	void	Spawn_Impl( void );
	void	OnRestore_Impl( void );

	// Main interface to all surface triggers
	static void	SetPlayerSurface( CBasePlayer *pPlayer, char gameMaterial );

	void	UpdateMaterialThink( void );

private:
	void	PlayerSurfaceChanged( CBasePlayer *pPlayer, char gameMaterial );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "gamematerial" } ]] int		m_iTargetGameMaterial;
	int		m_iCurrentGameMaterial;
	bool	m_bDisabled;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnSurfaceChangedToTarget" } ]] COutputEvent m_OnSurfaceChangedToTarget;
	[[= ks::reflect::Key{ .name = "OnSurfaceChangedFromTarget" } ]] COutputEvent m_OnSurfaceChangedFromTarget;
};

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CSoundPatch;

//-----------------------------------------------------------------------------
// Implementation of the class that computes windspeed
//-----------------------------------------------------------------------------
class CEnvWindShared
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvWindShared", .base = false } ]]
	[[= ks::reflect::From<"m_iMinWind", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iMaxWind", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iMinGust", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iMaxGust", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_flMinGustDelay", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flMaxGustDelay", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_iGustDirChange", ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iWindSeed", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iInitialWindDir", ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_flInitialWindSpeed", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flStartTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flGustDuration", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS_NOBASE( CEnvWindShared );
	DECLARE_EMBEDDED_NETWORKVAR();
	
	CEnvWindShared();
	~CEnvWindShared();

	CEnvWindShared( const CEnvWindShared & ) = delete;
	CEnvWindShared& operator= ( const CEnvWindShared& ) = delete;

	void Init( int iEntIndex, int iRandomSeed, float flTime, int iWindDir, float flInitialWindSpeed );

	void SetLocation( const Vector &location );

	// Method to update the wind speed
	// Time passed in here is global time, not delta time
	// The function returns the time at which it must be called again
	float WindThink( float flTime );

	// FIXME: These really should be private
	CNetworkVar( float, m_flStartTime );

	CNetworkVar( int, m_iWindSeed );		// random number seed...

	CNetworkVar( int, m_iMinWind );			// the slowest the wind can normally blow
	CNetworkVar( int, m_iMaxWind );			// the fastest the wind can normally blow
	CNetworkVar( int, m_windRadius );		// the radius this entity affects with its windiness, so a map can have multiple
	CNetworkVar( int, m_iMinGust );			// the slowest that a gust can be
	CNetworkVar( int, m_iMaxGust );			// the fastest that a gust can be

	CNetworkVar( float, m_flMinGustDelay );	// min time between gusts
	CNetworkVar( float, m_flMaxGustDelay );	// max time between gusts

	CNetworkVar( float, m_flGustDuration );	// max time between gusts

	CNetworkVar( int, m_iGustDirChange );	// max number of degrees wind dir changes on gusts.
	CNetworkVector( m_location );			// The location of this wind controller

	int m_iszGustSound;		// name of the wind sound to play for gusts.
	int m_iWindDir;			// wind direction (yaw)
	float m_flWindSpeed;	// the wind speed

	Vector m_currentWindVector;	// For all the talk of proper prediction, we ended up just storing and returning through a static vector.  Now we can have multiple env_wind, so we need this in here.
	Vector m_CurrentSwayVector;
	Vector m_PrevSwayVector;

	CNetworkVar( int, m_iInitialWindDir );
	CNetworkVar( float, m_flInitialWindSpeed );

	COutputEvent m_OnGustStart;
	COutputEvent m_OnGustEnd;

private:
	void ComputeWindVariation( float flTime );

	// Updates the wind sound
	void UpdateWindSound( float flTotalWindSpeed );

	void UpdateTreeSway( float flTime );

	float	m_flVariationTime;
	float	m_flSwayTime;
	float	m_flSimTime;		// What's the time I last simulated up to?
	float	m_flSwitchTime;		// when do I actually switch from gust to not gust
	float	m_flAveWindSpeed;	// the average wind speed
	bool	m_bGusting;			// is the wind gusting right now?

	float m_flWindAngleVariation;
	float m_flWindSpeedVariation;

	int m_iEntIndex;

	// Used to generate random numbers
	CUniformRandomStream m_Stream;

	// NOTE: In order to make this algorithm independent of calling frequency
	// I have to decouple the stream used to generate average wind speed
	// and the stream used to generate wind variation since they are
	// simulated using different timesteps
	CUniformRandomStream m_WindVariationStream;

	// Used to generate the wind sound...
	CSoundPatch *m_pWindSound;
};

//-----------------------------------------------------------------------------
// Method to sample the wind speed at a particular location
//-----------------------------------------------------------------------------
Vector GetWindspeedAtLocation( const Vector &location );

//-----------------------------------------------------------------------------
// Method to sample the windspeed at a particular time
//-----------------------------------------------------------------------------
void GetWindspeedAtTime( float flTime, Vector &vecVelocity );

//-----------------------------------------------------------------------------
// Method to reset wind speed..
//-----------------------------------------------------------------------------
void ResetWindspeed();

class CBaseEntity;



bool CanOverrideEnvZoomOwner( CBaseEntity *pZoomOwner );
float GetZoomOwnerDesiredFOV( CBaseEntity *pZoomOwner );

class CSprite;
struct inputdata_t;

using namespace ::ks::reflect;



class CEnvLaser : public CBeam
{
	DECLARE_CLASS( CEnvLaser, CBeam );
public:
	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	void	TurnOn( void );
	void	TurnOff( void );
	int		IsOn( void );

	void	FireAtPoint( trace_t &point );
	void	StrikeThink( void );

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "LaserTarget" } ]] string_t m_iszLaserTarget;	// Name of entity or entities to strike at, randomly picked if more than one match.
	CSprite	*m_pSprite;
	[[= ks::reflect::Key{ .name = "EndSprite" } ]] string_t m_iszSpriteName;
	Vector  m_firePosition;

	[[= ks::reflect::Key{ .name = "framestart" } ]] float	m_flStartFrame;
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

inline constexpr auto SF_MESSAGE_ONCE = 0x0001; // Fade in, not out
inline constexpr auto SF_MESSAGE_ALL = 0x0002; // Send to all clients

class CMessage : public CPointEntity
{
public:
	DECLARE_CLASS( CMessage, CPointEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	inline void SetMessage( string_t iszMessage ) { m_iszMessage = iszMessage; }

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:

	[[= ks::reflect::Input{ .name = "ShowMessage", .type = FIELD_VOID } ]] void InputShowMessage( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "message" } ]] string_t m_iszMessage;		// Message to display.
	[[= ks::reflect::Key{ .name = "messagevolume" } ]] float m_MessageVolume;
	[[= ks::reflect::Key{ .name = "messageattenuation" } ]] int m_MessageAttenuation;
	float m_Radius;

	DECLARE_DATADESC();

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "messagesound" } ]] string_t m_sNoise;
	[[= ks::reflect::Key{ .name = "OnShowMessage" } ]] COutputEvent m_OnShowMessage;
};

class CBaseFilter;
class CSound;
struct inputdata_t;

using namespace ::ks::reflect;



const int SF_MICROPHONE_SOUND_COMBAT			= 0x01;
const int SF_MICROPHONE_SOUND_WORLD				= 0x02;
const int SF_MICROPHONE_SOUND_PLAYER			= 0x04;
const int SF_MICROPHONE_SOUND_BULLET_IMPACT		= 0x08;
const int SF_MICROPHONE_SWALLOW_ROUTED_SOUNDS	= 0x10;
const int SF_MICROPHONE_SOUND_EXPLOSION			= 0x20;
const int SF_MICROPHONE_IGNORE_NONATTENUATED	= 0x40;

// Return codes from SoundPlayed
enum MicrophoneResult_t
{
    MicrophoneResult_Ok = 0,
    MicrophoneResult_Swallow,       // The microphone swallowed the sound. Don't play it.
    MicrophoneResult_Remove,        // The microphone should be removed from the list of microphones.
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvMicrophone : public CPointEntity
{
	DECLARE_CLASS( CEnvMicrophone, CPointEntity );

public:
	~CEnvMicrophone();

	void Spawn_Impl(void);
	void Activate_Impl(void);
	void OnRestore_Impl( void );
	void ActivateSpeaker( void );
	void Think_Impl(void);
	bool CanHearSound(CSound *pSound, float &flVolume);
	bool CanHearSound( int entindex, soundlevel_t soundlevel, float &flVolume, const Vector *pOrigin );

	void SetSensitivity( float flSensitivity );
	void SetMaxRange( float flMaxRange );
	void SetSpeakerName( string_t iszSpeakerName );
	void SetSpeaker( string_t iszSpeakerName, EHANDLE hSpeaker );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeakerName", .type = FIELD_STRING } ]] void InputSetSpeakerName( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// Hook for the sound system to tell us when a sound's been played. Returns true if it's to swallow the passed in sound.
	static bool OnSoundPlayed( int entindex, const char *soundname, soundlevel_t soundlevel, 
		float flVolume, int iFlags, int iPitch, const Vector *pOrigin, float soundtime, std::vector< Vector >& soundorigins );

	static void OnSoundStopped( const char *soundname );

private:

	// Per-microphone notification that a sound has played.
	MicrophoneResult_t SoundPlayed( int entindex, const char *soundname, soundlevel_t soundlevel, 
		float flVolume, int iFlags, int iPitch, const Vector *pOrigin, float soundtime, std::vector< Vector >& soundorigins );

	void SoundStopped( const char *soundname );

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool		m_bDisabled;			// If true, the microphone will not measure sound.
	EHANDLE		m_hMeasureTarget;		// Point at which to measure sound level.
	[[= ks::reflect::Key{ .name = "SoundMask" } ]] int			m_nSoundMask;			// Which sound types we are interested in.
	[[= ks::reflect::Key{ .name = "Sensitivity" } ]] float		m_flSensitivity;		// 0 = deaf, 1 = default, 10 = maximum sensitivity
	[[= ks::reflect::Key{ .name = "SmoothFactor" } ]] float		m_flSmoothFactor;		// 0 = no smoothing of samples, 0.9 = maximum smoothing
	[[= ks::reflect::Key{ .name = "MaxRange" } ]] float		m_flMaxRange;			// Maximum sound hearing range, irrelevant of attenuation
	[[= ks::reflect::Key{ .name = "SpeakerName" } ]] string_t	m_iszSpeakerName;		// Name of a speaker to output any heard sounds through
	EHANDLE		m_hSpeaker;				// Speaker to output any heard sounds through
	bool		m_bAvoidFeedback;
	[[= ks::reflect::Key{ .name = "speaker_dsp_preset" } ]] int			m_iSpeakerDSPPreset;	// Speaker DSP preset to use when this microphone is enabled
	[[= ks::reflect::Key{ .name = "ListenFilter" } ]] string_t	m_iszListenFilter;
	CHandle<CBaseFilter>	m_hListenFilter;

	[[= ks::reflect::Key{ .name = "SoundLevel" } ]] COutputFloat m_SoundLevel;			// Fired when the sampled volume level changes.
	[[= ks::reflect::Key{ .name = "OnRoutedSound" } ]] COutputEvent m_OnRoutedSound;		// Fired when a sound has been played through our speaker
	[[= ks::reflect::Key{ .name = "OnHeardSound" } ]] COutputEvent m_OnHeardSound;		// Heard sound.

	char		m_szLastSound[256];
};

class CBaseEntity;
class CBaseTempEntity;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: A server object that is used to test all registered temp entities
//-----------------------------------------------------------------------------
class CTempEntTester : public CPointEntity
{
public:
	DECLARE_CLASS( CTempEntTester, CPointEntity );

	void				Spawn_Impl( void );
	void				Think_Impl( void );

	static	CBaseEntity *Create( const Vector &vecOrigin, const QAngle &vecAngles, const char *lifetime, const char *single_te );
private:
	// Current temp entity to test
	CBaseTempEntity		*m_pCurrent;

	// Lifetime
	float				m_fLifeTime;

	char				m_szClass[ 64 ];
};

inline constexpr auto AE_TYPE_SERVER = ( 1 << 0 );
inline constexpr auto AE_TYPE_SCRIPTED = ( 1 << 1 ); // see scriptevent.h
inline constexpr auto AE_TYPE_SHARED = ( 1 << 2 );
inline constexpr auto AE_TYPE_WEAPON = ( 1 << 3 );
inline constexpr auto AE_TYPE_CLIENT = ( 1 << 4 );
inline constexpr auto AE_TYPE_FACEPOSER = ( 1 << 5 );

inline constexpr auto AE_TYPE_NEWEVENTSYSTEM = ( 1 << 10 ); //Temporary flag.

inline constexpr auto AE_NOT_AVAILABLE = -1;

typedef enum
{
	AE_INVALID = -1,			// So we have something more succint to check for than '-1'
	AE_EMPTY,
	AE_NPC_LEFTFOOT, // #define	NPC_EVENT_LEFTFOOT			2050
	AE_NPC_RIGHTFOOT, // #define NPC_EVENT_RIGHTFOOT			2051
	AE_NPC_BODYDROP_LIGHT, //#define NPC_EVENT_BODYDROP_LIGHT	2001
	AE_NPC_BODYDROP_HEAVY, //#define NPC_EVENT_BODYDROP_HEAVY	2002
	AE_NPC_SWISHSOUND, //#define NPC_EVENT_SWISHSOUND		2010
	AE_NPC_180TURN, //#define NPC_EVENT_180TURN			2020
	AE_NPC_ITEM_PICKUP, //#define NPC_EVENT_ITEM_PICKUP					2040
	AE_NPC_WEAPON_DROP, //#define NPC_EVENT_WEAPON_DROP					2041
	AE_NPC_WEAPON_SET_SEQUENCE_NAME, //#define NPC_EVENT_WEAPON_SET_SEQUENCE_NAME		2042
	AE_NPC_WEAPON_SET_SEQUENCE_NUMBER, //#define NPC_EVENT_WEAPON_SET_SEQUENCE_NUMBER	2043
	AE_NPC_WEAPON_SET_ACTIVITY, //#define NPC_EVENT_WEAPON_SET_ACTIVITY			2044
	AE_NPC_HOLSTER,
	AE_NPC_DRAW,
	AE_NPC_WEAPON_FIRE,

	AE_CL_PLAYSOUND, // #define CL_EVENT_SOUND				5004	// Emit a sound 
	AE_SV_PLAYSOUND,
	AE_CL_STOPSOUND,

	AE_START_SCRIPTED_EFFECT,
	AE_STOP_SCRIPTED_EFFECT,

	AE_CLIENT_EFFECT_ATTACH,
	
	AE_MUZZLEFLASH,			// Muzzle flash from weapons held by the player
	AE_NPC_MUZZLEFLASH,		// Muzzle flash from weapons held by NPCs
	
	AE_THUMPER_THUMP,		//Thumper Thump!
	AE_AMMOCRATE_PICKUP_AMMO,	//Ammo crate pick up ammo!

	AE_NPC_RAGDOLL,

	AE_NPC_ADDGESTURE,
	AE_NPC_RESTARTGESTURE,

	AE_NPC_ATTACK_BROADCAST,

	AE_NPC_HURT_INTERACTION_PARTNER,
	AE_NPC_SET_INTERACTION_CANTDIE,

	AE_SV_DUSTTRAIL,

	AE_CL_CREATE_PARTICLE_EFFECT,
	AE_CL_STOP_PARTICLE_EFFECT,
	AE_CL_ADD_PARTICLE_EFFECT_CP,
	AE_CL_CREATE_PARTICLE_BRASS,

	AE_RAGDOLL,

	AE_CL_ENABLE_BODYGROUP,
	AE_CL_DISABLE_BODYGROUP,
	AE_CL_BODYGROUP_SET_VALUE,
	AE_CL_BODYGROUP_SET_VALUE_CMODEL_WPN,

	AE_CL_BODYGROUP_SET_TO_CLIP,	// Updates the value of bodygroup 1 to the number of bullets in m_iClip1
	AE_CL_BODYGROUP_SET_TO_NEXTCLIP,	// Updates the value of bodygroup 1 to the number of bullets that can be reloaded next

	AE_CL_HIDE_SILENCER,
	AE_CL_ATTACH_SILENCER_COMPLETE,
	AE_CL_SHOW_SILENCER,
	AE_CL_DETACH_SILENCER_COMPLETE,

	AE_CL_EJECT_MAG, // Create a physical magazine to drop in the world and hide the fake-animated one
	AE_CL_EJECT_MAG_UNHIDE, //Unhide the non-physical mag that AE_CL_EJECT_MAG hid

	AE_WPN_PRIMARYATTACK,	// Used by weapons that want their primary attack to occur during an attack anim (i.e. grenade throwing)
	AE_WPN_INCREMENTAMMO,

	AE_WPN_HIDE,		// Used to hide player weapons
	AE_WPN_UNHIDE,		// Used to unhide player weapons

	AE_WPN_PLAYWPNSOUND,	// Play a weapon sound from the weapon script file

	AE_WPN_COMPLETE_RELOAD,	// the event that fires when a weapon has completed a gun reload visually according to the animation

	// Alien Swarm Infested shared events
	AE_ASW_FOOTSTEP,		// asw, played as each foot steps down
	AE_MARINE_FOOTSTEP,
	AE_MARINE_RELOAD_SOUND_A,	// anim event fired reloading sound
	AE_MARINE_RELOAD_SOUND_B,	// anim event fired reloading sound
	AE_MARINE_RELOAD_SOUND_C,	// anim event fired reloading sound
	AE_REMOVE_CLIENT_AIM,	// asw, removes this entity from the client autoaim list

	AE_MELEE_DAMAGE,
	AE_MELEE_START_COLLISION_DAMAGE,
	AE_MELEE_STOP_COLLISION_DAMAGE,
	AE_SCREEN_SHAKE,
	AE_START_DETECTING_COMBO,
	AE_STOP_DETECTING_COMBO,
	AE_COMBO_TRANSITION,
	AE_ALLOW_MOVEMENT,
	AE_SKILL_EVENT,				// marine skill event triggered (event options describes the skill)

	AE_TUG_INCAP,

	AE_CLIENT_EJECT_BRASS,

	AE_BEGIN_TAUNT_LOOP,

	AE_CL_SET_STATTRAK_GLOW,

	AE_WPN_CZ_DUMP_CURRENT_MAG,
	AE_WPN_CZ_UPDATE_BODYGROUP,

	AE_WPN_CLIP_TO_POSEPARAM,
	AE_WPN_NEXTCLIP_TO_POSEPARAM,
	AE_WPN_EMPTYSHOTS_TO_POSEPARAM,

	LAST_SHARED_ANIMEVENT,
} Animevent;

typedef struct evententry_s evententry_t;

//=========================================================
//=========================================================
extern void EventList_Init( void );
extern void EventList_Free( void );
extern bool EventList_RegisterSharedEvent( const char *pszEventName, int iEventIndex, int iType = 0 );
extern Animevent EventList_RegisterPrivateEvent( const char *pszEventName );
extern int EventList_IndexForName( const char *pszEventName );
extern const char *EventList_NameForIndex( int iEventIndex );
Animevent EventList_RegisterPrivateEvent( const char *pszEventName );

// This macro guarantees that the names of each event and the constant used to
// reference it in the code are identical.

// Implemented in shared code
extern void EventList_RegisterSharedEvents( void );
extern int EventList_GetEventType( int eventIndex );

class CBaseEntity;

using namespace ::ks::reflect;


class CEventLog : public CGameEventListener, public CBaseGameSystem
{
	
public:
	CEventLog();
	virtual ~CEventLog();

public: // game event handling
	
	void FireGameEvent( IGameEvent * event );
	
public: // CBaseGameSystem overrides

	virtual bool Init();
	//virtual void Shutdown() {}

	virtual void FormatPlayer( CBaseEntity *ent, char *str, int len ) const;
	const char *FormatPlayer( CBaseEntity *ent ) const;

	enum
	{
		PLAYER_LOGINFO_SIZE = 256,
	};

protected:

	virtual bool PrintEvent( IGameEvent * event );
	virtual bool PrintGameEvent( IGameEvent * event );
	virtual bool PrintPlayerEvent( IGameEvent * event );
	virtual bool PrintTeamEvent( IGameEvent * event );
	virtual bool PrintOtherEvent( IGameEvent * event );
};

extern CEventLog *GameLogSystem();

class CBaseEntity;


// Purpose: A global class that holds a prioritized queue of entity I/O events.
//			Events can be posted with a nonzero delay, which determines how long
//			they are held before being dispatched to their recipients.
//			The queue is serviced once per server frame.

struct EventQueuePrioritizedEvent_t : public ks::pooled< EventQueuePrioritizedEvent_t >
{
	float m_flFireTime;
	string_t m_iTarget;
	string_t m_iTargetInput;
	EHANDLE m_pActivator;
	EHANDLE m_pCaller;
	int m_iOutputID;
	EHANDLE m_pEntTarget;  // a pointer to the entity to target; overrides m_iTarget

	variant_t m_VariantValue;	// variable-type parameter

	EventQueuePrioritizedEvent_t *m_pNext;
	EventQueuePrioritizedEvent_t *m_pPrev;

	DECLARE_SIMPLE_DATADESC();

	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< EventQueuePrioritizedEvent_t >;
};

class CEventQueue
{
public:
	// pushes an event into the queue, targeting a string name (m_iName), or directly by a pointer
	void AddEvent( const char *target, const char *action, variant_t Value, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID = 0 );
	void AddEvent( CBaseEntity *target, const char *action, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID = 0 );
	void AddEvent( CBaseEntity *target, const char *action, variant_t Value, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID = 0 );

	void CancelEvents( CBaseEntity *pCaller );
	void CancelEventOn( CBaseEntity *pTarget, const char *sInputName );
	bool HasEventPending( CBaseEntity *pTarget, const char *sInputName );

	// services the queue, firing off any events who's time hath come
	void ServiceEvents( void );

	// debugging
	void ValidateQueue( void );

	// serialization

	CEventQueue();
	~CEventQueue();

	void Init( void );
	void Clear( void ); // resets the list

	void Dump( void );

private:

	void AddEvent( EventQueuePrioritizedEvent_t *event );
	void RemoveEvent( EventQueuePrioritizedEvent_t *pe );

	DECLARE_SIMPLE_DATADESC();
	EventQueuePrioritizedEvent_t m_Events;
	int m_iListCount;
};

extern CEventQueue g_EventQueue;

class CBaseEntity;


inline constexpr auto SF_ENVEXPLOSION_NODAMAGE = 0x00000001; // when set, ENV_EXPLOSION will not actually inflict damage
inline constexpr auto SF_ENVEXPLOSION_REPEATABLE = 0x00000002; // can this entity be refired?
inline constexpr auto SF_ENVEXPLOSION_NOFIREBALL = 0x00000004; // don't draw the fireball
inline constexpr auto SF_ENVEXPLOSION_NOSMOKE = 0x00000008; // don't draw the smoke
inline constexpr auto SF_ENVEXPLOSION_NODECAL = 0x00000010; // don't make a scorch mark
inline constexpr auto SF_ENVEXPLOSION_NOSPARKS = 0x00000020; // don't make sparks
inline constexpr auto SF_ENVEXPLOSION_NOSOUND = 0x00000040; // don't play explosion sound.
inline constexpr auto SF_ENVEXPLOSION_RND_ORIENT = 0x00000080; // randomly oriented sprites
inline constexpr auto SF_ENVEXPLOSION_NOFIREBALLSMOKE = 0x0100;
inline constexpr auto SF_ENVEXPLOSION_NOPARTICLES = 0x00000200;
inline constexpr auto SF_ENVEXPLOSION_NODLIGHTS = 0x00000400;
inline constexpr auto SF_ENVEXPLOSION_NOCLAMPMIN = 0x00000800; // don't clamp the minimum size of the fireball sprite
inline constexpr auto SF_ENVEXPLOSION_NOCLAMPMAX = 0x00001000; // don't clamp the maximum size of the fireball sprite
inline constexpr auto SF_ENVEXPLOSION_SURFACEONLY = 0x00002000; // don't damage the player if he's underwater.
inline constexpr auto SF_ENVEXPLOSION_GENERIC_DAMAGE = 0x00004000; // don't do BLAST damage
inline constexpr auto SF_ENVEXPLOSION_ICE = 0x00008000; // freeze stuff and do ice type effects

extern int	g_sModelIndexFireball;
extern int	g_sModelIndexSmoke;

void ExplosionCreate( const Vector &center, const QAngle &angles, 
	CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, float flExplosionForce = 0.0f, bool bSurfaceOnly = false, bool bSilent = false, int iCustomDamageType = -1 );

void ExplosionCreate( const Vector &center, const QAngle &angles, 
					 CBaseEntity *pOwner, int magnitude, int radius, int nSpawnFlags, 
					 float flExplosionForce = 0.0f, CBaseEntity *pInflictor = nullptr, int iCustomDamageType = -1,  const EHANDLE *ignoredEntity = nullptr, Class_T ignoredClass = CLASS_NONE);

// this version lets you specify classes or entities to be ignored
void ExplosionCreate( const Vector &center, const QAngle &angles, 
					 CBaseEntity *pOwner, int magnitude, int radius, bool doDamage, 
					 const EHANDLE *ignoredEntity, Class_T ignoredClass,
					 float flExplosionForce = 0.0f, bool bSurfaceOnly = false, bool bSilent = false, int iCustomDamageType = -1 );

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: Filters are outboard entities that hold a set of rules that other
//			entities can use to determine behaviors.
//			For example, triggers can use an activator filter to determine who
//			activates them. NPCs and breakables can use a damage filter to
//			determine what can damage them.
//			Current filter criteria are:
//				Activator name
//				Activator class
//				Activator team
//				Damage type (for damage filters only)
//			More than one filter can be combined to create a more complex boolean
//			expression by using filter_multi.

// ###################################################################
//	> BaseFilter
// ###################################################################
class CBaseFilter : public CLogicalEntity
{
	DECLARE_CLASS( CBaseFilter, CLogicalEntity );

public:

	DECLARE_DATADESC();

	bool PassesFilter( CBaseEntity *pCaller, CBaseEntity *pEntity );
	bool PassesDamageFilter_Impl( const CTakeDamageInfo &info );

	[[= ks::reflect::Key{ .name = "Negated" } ]] bool m_bNegated;

	// Inputs
	[[= ks::reflect::Input{ .name = "TestActivator", .type = FIELD_INPUT } ]] void InputTestActivator( inputdata_t &inputdata );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnPass" } ]] COutputEvent	m_OnPass;		// Fired when filter is passed
	[[= ks::reflect::Key{ .name = "OnFail" } ]] COutputEvent	m_OnFail;		// Fired when filter is failed

protected:

	virtual bool PassesFilterImpl( CBaseEntity *pCaller, CBaseEntity *pEntity );
	virtual bool PassesDamageFilterImpl(const CTakeDamageInfo &info);
};

class CBaseEntity;

using namespace ::ks::reflect;


//Spawnflags
inline constexpr auto SF_FIRE_INFINITE = 0x00000001;
inline constexpr auto SF_FIRE_SMOKELESS = 0x00000002;
inline constexpr auto SF_FIRE_START_ON = 0x00000004;
inline constexpr auto SF_FIRE_START_FULL = 0x00000008;
inline constexpr auto SF_FIRE_DONT_DROP = 0x00000010;
inline constexpr auto SF_FIRE_NO_GLOW = 0x00000020;
inline constexpr auto SF_FIRE_DIE_PERMANENT = 0x00000080;
inline constexpr auto SF_FIRE_VISIBLE_FROM_ABOVE = 0x00000100;

//==================================================
// CFire
//==================================================

enum fireType_e
{
	FIRE_NATURAL = 0,
	FIRE_PLASMA,
};

//==================================================

// NPCs and grates do not prevent fire from travelling
inline constexpr auto MASK_FIRE_SOLID = ( MASK_SOLID & (~(CONTENTS_MONSTER|CONTENTS_GRATE)) );

//==================================================
// FireSystem
//==================================================
bool FireSystem_StartFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type = FIRE_NATURAL);
void FireSystem_ExtinguishInRadius( const Vector &origin, float radius, float rate );
void FireSystem_AddHeatInRadius( const Vector &origin, float radius, float heat );

bool FireSystem_GetFireDamageDimensions( CBaseEntity *pFire, Vector *pFireMins, Vector *pFireMaxs );

class CBaseEntity;

using namespace ::ks::reflect;

//==================================================
// CBaseFire
//==================================================

//NOTENOTE: Reserved for all descendants
inline constexpr auto bitsFIRE_NONE = 0x00000000;
inline constexpr auto bitsFIRE_ACTIVE = 0x00000001;

class CBaseFire : public CBaseEntity
{
public:
	DECLARE_CLASS( CBaseFire, CBaseEntity );

	CBaseFire( void );
	virtual	~CBaseFire( void );

	virtual void	Scale( float size, float time );
	virtual void	Scale( float start, float size, float time );
	virtual void	Enable( int state = true );

	//Client-side
	CNetworkVar( float, m_flStartScale );
	CNetworkVar( float, m_flScale );
	CNetworkVar( float, m_flScaleTime );
	CNetworkVar( int, m_nFlags );
};

//==================================================
// CFireSmoke
//==================================================

//NOTENOTE: Mirrored in cl_dll/c_fire_smoke.cpp
inline constexpr auto bitsFIRESMOKE_SMOKE = 0x00000002;
inline constexpr auto bitsFIRESMOKE_SMOKE_COLLISION = 0x00000004;
inline constexpr auto bitsFIRESMOKE_GLOW = 0x00000008;
inline constexpr auto bitsFIRESMOKE_VISIBLE_FROM_ABOVE = 0x00000010;

class CFireSmoke : public CBaseFire
{
	[[= ks::reflect::NetTable{ .name = "DT_FireSmoke" } ]]
	[[= ks::reflect::From<"m_flStartScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flScaleTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_nFlags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFireSmoke, CBaseFire );

	CFireSmoke( void );
	virtual	~CFireSmoke( void );

	void	Spawn_Impl();
	void	Precache_Impl();
	void	EnableSmoke( int state = true );
	void	EnableGlow( int state = true );
	void	EnableVisibleFromAbove( int state = true );
	
	DECLARE_SERVERCLASS();

public:

	//Client-side
	CNetworkVar( int, m_nFlameModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nFlameFromAboveModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );

	//Server-side
};

// Purpose: Namespace for functions dealing with Debug Overlays

// An overlay line
struct OverlayLine_t 
{
	Vector			origin;
	Vector			dest;
	int				r;
	int				g;
	int				b;
	bool			noDepthTest;
	bool			draw;
};

extern void	UTIL_AddDebugLine( const Vector &startPos, const Vector &endPos, bool noDepthTest, bool testLOS );
extern void UTIL_DrawOverlayLines( void );

extern void DebugDrawLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, int r, int g, int b, bool test, float duration );

// String tables used by the game DLL
inline constexpr auto MAX_VGUI_SCREEN_STRING_BITS = 8;
inline constexpr auto MAX_VGUI_SCREEN_STRINGS = ( 1 << MAX_VGUI_SCREEN_STRING_BITS );
inline constexpr auto VGUI_SCREEN_INVALID_STRING = ( MAX_VGUI_SCREEN_STRINGS - 1 );

inline constexpr auto MAX_MATERIAL_STRING_BITS = 10;
inline constexpr auto MAX_MATERIAL_STRINGS = ( 1 << MAX_MATERIAL_STRING_BITS );
inline constexpr auto OVERLAY_MATERIAL_INVALID_STRING = ( MAX_MATERIAL_STRINGS - 1 );

inline constexpr auto MAX_CHOREO_SCENES_STRING_BITS = 13;
inline constexpr auto MAX_CHOREO_SCENES_STRINGS = ( 1 << MAX_CHOREO_SCENES_STRING_BITS );
inline constexpr auto CHOREO_SCENES_INVALID_STRING = ( MAX_CHOREO_SCENES_STRINGS - 1 );

inline constexpr auto MAX_PARTICLESYSTEMS_STRING_BITS = 10;
inline constexpr auto MAX_PARTICLESYSTEMS_STRINGS = ( 1 << MAX_PARTICLESYSTEMS_STRING_BITS );
inline constexpr auto PARTICLESYSTEMS_INVALID_STRING = ( MAX_PARTICLESYSTEMS_STRINGS - 1 );

inline constexpr auto MAX_MOVIE_STRING_BITS = 4;
inline constexpr auto MAX_MOVIE_STRINGS = ( 1 << MAX_MOVIE_STRING_BITS );
inline constexpr auto OVERLAY_MOVIE_INVALID_STRING = ( MAX_MOVIE_STRINGS - 1 );

extern INetworkStringTableContainer *networkstringtable;
extern INetworkStringTable *g_pStringTableVguiScreen;
extern INetworkStringTable *g_pStringTableClientSideChoreoScenes;

inline constexpr auto MAX_INFOPANEL_STRINGS = 128;

// save/load

class CBaseAnimating;


struct animevent_t
{
	// see mstudioevent_for_client_server_t comment below
	unsigned short	_event_highword;
	union
	{
		unsigned short	_event_lowword;
		unsigned short	event_newsystem;
	};

	const char		*options;
	float			cycle;
	float			eventtime;
	int				type;
	CBaseAnimating	*pSource;
	bool			m_bHandledByScript;

	// see mstudioevent_for_client_server_t comment below
	int Event_OldSystem( void ) const { return *static_cast< const int* >( static_cast< const void* >( &_event_highword ) ); }
	void Event_OldSystem( int nEvent ) { *static_cast< int* >( static_cast< void* >( &_event_highword ) ) = nEvent; }
	int Event( void ) const
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
			return event_newsystem;

		return Event_OldSystem();
	}
	void Event( int nEvent )
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
		{
			event_newsystem = nEvent;
		}
		else
		{
			Event_OldSystem( nEvent );
		}
	}
};
inline constexpr auto EVENT_SPECIFIC = 0;
inline constexpr auto EVENT_SCRIPTED = 1000; // see scriptevent.h
inline constexpr auto EVENT_SHARED = 2000;
inline constexpr auto EVENT_WEAPON = 3000;
inline constexpr auto EVENT_CLIENT = 5000;

inline constexpr auto NPC_EVENT_BODYDROP_LIGHT = 2001;
inline constexpr auto NPC_EVENT_BODYDROP_HEAVY = 2002;

inline constexpr auto NPC_EVENT_SWISHSOUND = 2010;

inline constexpr auto NPC_EVENT_180TURN = 2020;

inline constexpr auto NPC_EVENT_ITEM_PICKUP = 2040;
inline constexpr auto NPC_EVENT_WEAPON_DROP = 2041;
inline constexpr auto NPC_EVENT_WEAPON_SET_SEQUENCE_NAME = 2042;
inline constexpr auto NPC_EVENT_WEAPON_SET_SEQUENCE_NUMBER = 2043;
inline constexpr auto NPC_EVENT_WEAPON_SET_ACTIVITY = 2044;

inline constexpr auto NPC_EVENT_LEFTFOOT = 2050;
inline constexpr auto NPC_EVENT_RIGHTFOOT = 2051;

inline constexpr auto NPC_EVENT_OPEN_DOOR = 2060;

// !! DON'T CHANGE TO ORDER OF THESE.  THEY ARE HARD CODED IN THE WEAPON QC FILES (YUCK!) !!
inline constexpr auto EVENT_WEAPON_MELEE_HIT = 3001;
inline constexpr auto EVENT_WEAPON_SMG1 = 3002;
inline constexpr auto EVENT_WEAPON_MELEE_SWISH = 3003;
inline constexpr auto EVENT_WEAPON_SHOTGUN_FIRE = 3004;
inline constexpr auto EVENT_WEAPON_THROW = 3005;
inline constexpr auto EVENT_WEAPON_AR1 = 3006;
inline constexpr auto EVENT_WEAPON_AR2 = 3007;
inline constexpr auto EVENT_WEAPON_HMG1 = 3008;
inline constexpr auto EVENT_WEAPON_SMG2 = 3009;
inline constexpr auto EVENT_WEAPON_MISSILE_FIRE = 3010;
inline constexpr auto EVENT_WEAPON_SNIPER_RIFLE_FIRE = 3011;
inline constexpr auto EVENT_WEAPON_AR2_GRENADE = 3012;
inline constexpr auto EVENT_WEAPON_THROW2 = 3013;
inline constexpr auto EVENT_WEAPON_PISTOL_FIRE = 3014;
inline constexpr auto EVENT_WEAPON_RELOAD = 3015;
inline constexpr auto EVENT_WEAPON_THROW3 = 3016;
inline constexpr auto EVENT_WEAPON_RELOAD_SOUND = 3017; // Use this + EVENT_WEAPON_RELOAD_FILL_CLIP to prevent shooting during the reload animation
inline constexpr auto EVENT_WEAPON_RELOAD_FILL_CLIP = 3018;
inline constexpr auto EVENT_WEAPON_SMG1_BURST1 = 3101; // first round in a 3-round burst
inline constexpr auto EVENT_WEAPON_SMG1_BURSTN = 3102; // 2, 3 rounds
inline constexpr auto EVENT_WEAPON_AR2_ALTFIRE = 3103;

inline constexpr auto EVENT_WEAPON_SEQUENCE_FINISHED = 3900;

// NOTE: MUST BE THE LAST WEAPON EVENT -- ONLY WEAPON EVENTS BETWEEN EVENT_WEAPON AND THIS
inline constexpr auto EVENT_WEAPON_LAST = 3999;

// Because the client and server have a shared memory space for event data,
// but use different indexes for the event (servers cache them all upfront, 
// while client creates them as sequences fire) we're going to store the server
// index in the low word and the client index in the high word.
//
// studio.h is a public header, so we'll use this matching struct to do the
// conversion and not force us to macro all the code that checks the event member.
// This struct's data MUST match mstudioevent_t
//
// BUT events that use the old event system use the entire dword which will always
// match between client and server. Instead of wrapping everything in if checks, 
// for ( type & AE_TYPE_NEWEVENTSYSTEM ) we use the Event accessors to set/get 
// while doing the check under the hood.
//
// -Jeep
struct mstudioevent_for_client_server_t
{
	float				cycle;
	unsigned short	_event_highword;	// Placeholder for the high word
	union
	{
		// Server shares with the low word
		unsigned short	_event_lowword;
		unsigned short	event_newsystem;
	};

	int					type;
	inline const char * pszOptions( void ) const { return options; }
	char				options[64];

	int					szeventindex;
	inline char * const pszEventName( void ) const { return ((char *)this) + szeventindex; }

	// For setting and getting through Event, check the AE_TYPE_NEWEVENTSYSTEM flag to decide
	// if we should set/get just the short used by the client/server or use the whole int.
	int Event_OldSystem( void ) const { return *static_cast< const int* >( static_cast< const void* >( &_event_highword ) ); }
	void Event_OldSystem( int nEvent ) { *static_cast< int* >( static_cast< void* >( &_event_highword ) ) = nEvent; }
	int Event( void ) const
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
			return event_newsystem;

		return Event_OldSystem();
	}
	void Event( int nEvent )
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
		{
			event_newsystem = nEvent;
		}
		else
		{
			Event_OldSystem( nEvent );
		}
	}
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CEntityDissolve : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityDissolve" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityDissolve, CBaseEntity );

	CEntityDissolve( void );
	~CEntityDissolve( void );

	static CEntityDissolve	*Create( CBaseEntity *pTarget, const char *pMaterialName, 
		float flStartTime, int nDissolveType = 0, bool *pRagdollCreated = nullptr );
	static CEntityDissolve	*Create( CBaseEntity *pTarget, CBaseEntity *pSource );
	
	void	Precache_Impl();
	void	Spawn_Impl();
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetStartTime( float flStartTime );
	void	SetDissolverOrigin( Vector vOrigin ) { m_vDissolverOrigin = vOrigin; }
	void	SetMagnitude( int iMagnitude ){ m_nMagnitude = iMagnitude; }
	void	SetDissolveType( int iType ) { m_nDissolveType = iType;	}

	Vector	GetDissolverOrigin( void );
	int		GetMagnitude( void ) { return m_nMagnitude;	}
	int		GetDissolveType( void ) { return m_nDissolveType;	}

	DECLARE_DATADESC();

	CNetworkVar( float, m_flStartTime, [[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flFadeInStart,        [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeInLength,       [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutModelStart,  [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutModelLength, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutStart,       [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFadeOutLength,      [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );

protected:
	[[= ks::reflect::Input{ .name = "Dissolve", .type = FIELD_STRING } ]] void	InputDissolve( inputdata_t &inputdata );
	void	DissolveThink( void );
	void	ElectrocuteThink( void );

	CNetworkVar( int, m_nDissolveType, [[= ks::reflect::Net{ .bits = ENTITY_DISSOLVE_BITS, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "dissolvetype" } ]] );
	CNetworkVector( m_vDissolverOrigin, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nMagnitude, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "magnitude" } ]] );
};

class CBaseEntity;

using namespace ::ks::reflect;

inline constexpr auto FLAME_DAMAGE_INTERVAL = 0.2f; // How often to deal damage.
inline constexpr auto FLAME_DIRECT_DAMAGE_PER_SEC = 5.0f;
inline constexpr auto FLAME_RADIUS_DAMAGE_PER_SEC = 4.0f;

inline constexpr auto FLAME_DIRECT_DAMAGE = ( FLAME_DIRECT_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL );
inline constexpr auto FLAME_RADIUS_DAMAGE = ( FLAME_RADIUS_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL );

inline constexpr auto FLAME_MAX_LIFETIME_ON_DEAD_NPCS = 10.0f;


class CEntityFlame : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityFlame" } ]]
	static constexpr bool ks_class = true;

	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityFlame, CBaseEntity );

public:
	static CEntityFlame	*Create( CBaseEntity *pTarget, float flLifetime, float flSize = 0.0f, bool bUseHitboxes = true );

	CEntityFlame( void );

	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetLifetime( float lifetime );
	void	SetUseHitboxes( bool use );

	void	SetNumHitboxFires( int iNumHitBoxFires );
	void	SetHitboxFireScale( float flHitboxFireScale );

	float	GetRemainingLife( void ) const;
	int		GetNumHitboxFires( void );
	float	GetHitboxFireScale( void );

	void Precache_Impl();
	void UpdateOnRemove_Impl();

	void Spawn_Impl();
	void Activate_Impl();

	void	UseCheapEffect( bool bCheap );

	void	SetSize( float size ) { m_flSize = size; }
	
	void	SetAttacker( CBaseEntity *pAttacker ) { m_hAttacker = pAttacker; }
	CBaseEntity *GetAttacker( void ) const;

protected:
	void	FlameThink( void );

	CNetworkHandle( CBaseEntity, m_hEntAttached, [[= ks::reflect::Net{} ]] );		// The entity that we are burning (attached to).
	CNetworkVar( bool, m_bCheapEffect, [[= ks::reflect::Net{} ]] );

	CNetworkVar( float, m_flSize );
	CNetworkVar( bool, m_bUseHitboxes );
	CNetworkVar( int, m_iNumHitboxFires );
	CNetworkVar( float, m_flHitboxFireScale );

	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Key{ .name = "lifetime" } ]] );
	string_t	m_iszPlayingSound;	// Track the sound so we can StopSound later

	EHANDLE m_hAttacker;

	int m_iDangerSound;
	bool m_bPlayingSound;
};

class CBaseAnimating;
class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;


class CEntityFreezing : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityFreezing" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityFreezing, CBaseEntity );

	static CEntityFreezing	*Create( CBaseAnimating *pTarget );
	
	void	Precache_Impl();
	void	Spawn_Impl();
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetFreezingOrigin( Vector vOrigin ) { m_vFreezingOrigin = vOrigin; }
	Vector	GetFreezingOrigin( void ) { return m_vFreezingOrigin; }
	void	SetFrozen( float flFrozen ){ m_flFrozen = flFrozen; }
	int		GetFrozen( void ) { return m_flFrozen; }
	void	FinishFreezing( void ) { m_bFinishFreezing = true; }

	DECLARE_DATADESC();

protected:
	[[= ks::reflect::Input{ .name = "Freeze", .type = FIELD_STRING } ]] void	InputFreeze( inputdata_t &inputdata );

	CNetworkVector( m_vFreezingOrigin, [[= ks::reflect::Net{ .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flFrozen, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "frozen" } ]] );
	CNetworkVar( bool, m_bFinishFreezing, [[= ks::reflect::Net{} ]] );

public:
	// Declared last, sent second. Left in Valve's order deliberately: the migration does not
	// reorder members, and the name-keyed gate proves that is safe.
	CNetworkArray( float, m_flFrozenPerHitbox, 50, [[= ks::reflect::Net{} ]] );
};

class CBaseEntity;
struct notify_system_event_params_t;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Spawns particles after  the entity
//-----------------------------------------------------------------------------
class CEntityParticleTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EntityParticleTrail" } ]]
	[[= ks::reflect::From<"m_Info", ks::reflect::Net{}>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEntityParticleTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

public:
	static CEntityParticleTrail	*Create( CBaseEntity *pTarget, const EntityParticleTrailInfo_t &info, CBaseEntity *pConstraint );
	static void Destroy( CBaseEntity *pTarget, const EntityParticleTrailInfo_t &info );

	void Spawn_Impl();
	void UpdateOnRemove_Impl();

	// Force our constraint entity to be trasmitted
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	// Clean up when the entity goes away.
	void NotifySystemEvent_Impl( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

private:
	void	AttachToEntity( CBaseEntity *pTarget );
	void	IncrementRefCount();
	void	DecrementRefCount();
	
	CNetworkVar( int, m_iMaterialName, [[= ks::reflect::Net{ .bits = MAX_MATERIAL_STRING_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVarEmbedded( EntityParticleTrailInfo_t, m_Info );
	CNetworkHandle( CBaseEntity, m_hConstraintEntity, [[= ks::reflect::Net{} ]] );

	int	m_nRefCount;
};

class CBaseEntity;

typedef CHandle<CBaseEntity> EHANDLE;

	inline int Server_EHandleToInt( const EHANDLE &hIn )
	{
		if ( hIn.Get() )
		{
			int iSerialNum = hIn.GetSerialNumber() & (1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1;
			return ( hIn.GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS) );
		}
		else
		{
			return INVALID_NETWORKED_EHANDLE_VALUE;
		}
	}

template< class Type, class Changer >
class CSharedVarBase
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

	void Initialize( char *pchName, int iEntNum )
	{
		contract_assert( !m_pSharedMemory && !m_pValue );

		m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		m_pSharedMemory->Init( sizeof( Type ) );
		m_pValue = (Type*)m_pSharedMemory->Base();
	}

		// Data can only be set on the server
		template< class C >
		const Type& operator=( const C &val ) 
		{ 
			return Set( static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator=( const CNetworkVarBase< C, Changer > &val ) 
		{ 
			return Set( static_cast<const Type>( *val.m_pValue ) ); 
		}

		const Type& Set( const Type &val )
		{
			*m_pValue = val;
			return *m_pValue;
		}

		template< class C >
		const Type& operator+=( const C &val ) 
		{
			return Set( *m_pValue + static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator-=( const C &val ) 
		{
			return Set( *m_pValue - static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator/=( const C &val ) 
		{
			return Set( *m_pValue / static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator*=( const C &val ) 
		{
			return Set( *m_pValue * static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator^=( const C &val ) 
		{
			return Set( *m_pValue ^ static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator|=( const C &val ) 
		{
			return Set( *m_pValue | static_cast<const Type>( val ) ); 
		}

		const Type& operator++()
		{
			return (*this += 1);
		}

		Type operator--()
		{
			return (*this -= 1);
		}

		Type operator++( int ) // postfix version..
		{
			Type val = *m_pValue;
			(*this += 1);
			return val;
		}

		Type operator--( int ) // postfix version..
		{
			Type val = *m_pValue;
			(*this -= 1);
			return val;
		}

		// For some reason the compiler only generates type conversion warnings for this operator when used like 
		// CNetworkVarBase<unsigned char> = 0x1
		// (it warns about converting from an int to an unsigned char).
		template< class C >
		const Type& operator&=( const C &val ) 
		{	
			return Set( *m_pValue & static_cast<const Type>( val ) ); 
		}

		operator Type&()
		{
			return *m_pValue; 
		}

		Type& GetForModify()
		{
			return *m_pValue;
		}

		Type* operator->()
		{
			return m_pValue; 
		}

	operator const Type&() const 
	{
		return *m_pValue; 
	}

	const Type& Get() const 
	{
		return *m_pValue; 
	}

	const Type* operator->() const 
	{
		return m_pValue; 
	}

private:
public:
	ISPSharedMemory		*m_pSharedMemory;
	Type				*m_pValue;
};

// Shared color wrapper.
template< class Type, class Changer >
class CSharedColor32Base : public CSharedVarBase< Type, Changer >
{
public:
		// Data can only be set on the server
		inline void Init( byte rVal, byte gVal, byte bVal )
		{
			SetR( rVal );
			SetG( gVal );
			SetB( bVal );
		}
		inline void Init( byte rVal, byte gVal, byte bVal, byte aVal )
		{
			SetR( rVal );
			SetG( gVal );
			SetB( bVal );
			SetA( aVal );
		}

		const Type& operator=( const Type &val ) 
		{ 
			return Set( val ); 
		}

		const Type& operator=( const CSharedColor32Base<Type,Changer> &val ) 
		{ 
			return CSharedVarBase<Type,Changer>::Set( *val.m_pValue );
		}

		inline void SetR( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->r, val ); }
		inline void SetG( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->g, val ); }
		inline void SetB( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->b, val ); }
		inline void SetA( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->a, val ); }

	inline byte GetR() const { return CSharedColor32Base<Type,Changer>::m_pValue->r; }
	inline byte GetG() const { return CSharedColor32Base<Type,Changer>::m_pValue->g; }
	inline byte GetB() const { return CSharedColor32Base<Type,Changer>::m_pValue->b; }
	inline byte GetA() const { return CSharedColor32Base<Type,Changer>::m_pValue->a; }

	// Data can only be set on the server
	private:
		inline void SetVal( byte &out, const byte &in )
		{
			out = in;
		}
};

// Shared vector wrapper.
template< class Type, class Changer >
class CSharedVectorBase : public CSharedVarBase< Type, Changer >
{
public:
		// Data can only be set on the server
		inline void Init( float ix=0, float iy=0, float iz=0 ) 
		{
			SetX( ix );
			SetY( iy );
			SetZ( iz );
		}

		const Type& operator=( const Type &val ) 
		{ 
			return CSharedVarBase< Type, Changer >::Set( val ); 
		}

		const Type& operator=( const CSharedVectorBase<Type,Changer> &val ) 
		{ 
			return CSharedVarBase<Type,Changer>::Set( *val.m_pValue );
		}

		inline void SetX( float val ) { SetVal( CSharedVectorBase<Type,Changer>::m_pValue->x, val ); }
		inline void SetY( float val ) { SetVal( CSharedVectorBase<Type,Changer>::m_pValue->y, val ); }
		inline void SetZ( float val ) { SetVal( CSharedVectorBase<Type,Changer>::m_pValue->z, val ); }
		inline void Set( int i, float val ) { SetVal( (*CSharedVectorBase<Type,Changer>::m_pValue)[i], val ); }
		inline float& operator[]( int i ) { return (*CSharedVectorBase<Type,Changer>::m_pValue)[i]; }

		const Type& operator*=( float val )
		{
			return CSharedVarBase< Type, Changer >::Set( (*CSharedVectorBase<Type,Changer>::m_pValue) * val );
		}

	inline float GetX() const { return CSharedVectorBase<Type,Changer>::m_pValue->x; }
	inline float GetY() const { return CSharedVectorBase<Type,Changer>::m_pValue->y; }
	inline float GetZ() const { return CSharedVectorBase<Type,Changer>::m_pValue->z; }
	inline const float& operator[]( int i ) const { return (*CSharedVectorBase<Type,Changer>::m_pValue)[i]; }

	bool operator==( const Type &val ) const 
	{ 
		return (*CSharedVectorBase<Type,Changer>::m_pValue) == (Type)val; 
	}

	bool operator!=( const Type &val ) const 
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) != (Type)val; 
	}

	const Type operator+( const Type &val ) const 
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return (*CSharedVectorBase<Type,Changer>::m_pValue) - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator*( float val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) / val; 
	}

	// Data can only be set on the server
	private:
		inline void SetVal( float &out, float in ) 
		{
			out = in;
		}
};

// Shared quaternion wrapper.
template< class Type, class Changer >
class CSharedQuaternionBase : public CSharedVarBase< Type, Changer >
{
public:
		// Data can only be set on the server
		inline void Init( float ix=0, float iy=0, float iz=0, float iw = 0 ) 
		{
			SetX( ix );
			SetY( iy );
			SetZ( iz );
			SetZ( iw );
		}

		const Type& operator=( const Type &val ) 
		{ 
			return CSharedVarBase< Type, Changer >::Set( val ); 
		}

		const Type& operator=( const CSharedQuaternionBase<Type,Changer> &val ) 
		{ 
			return CSharedVarBase<Type,Changer>::Set( *val.m_pValue );
		}

		const Type& operator*=( float val )
		{
			return CSharedVarBase< Type, Changer >::Set( (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val );
		}

		inline void SetX( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->x, val ); }
		inline void SetY( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->y, val ); }
		inline void SetZ( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->z, val ); }
		inline void SetW( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->w, val ); }
		inline void Set( int i, float val ) { SetVar( (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i], val ); }
		inline float& operator[]( int i ) { return (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i]; }

	inline float GetX() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->x; }
	inline float GetY() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->y; }
	inline float GetZ() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->z; }
	inline float GetW() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->w; }
	inline const float& operator[]( int i ) const { return (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i]; }

	bool operator==( const Type &val ) const 
	{ 
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) == (Type)val; 
	}

	bool operator!=( const Type &val ) const 
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) != (Type)val; 
	}

	const Type operator+( const Type &val ) const 
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator*( float val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) / val; 
	}

	// Data can only be set on the server
	private:
		inline void SetVar( float &out, float in ) 
		{
			out = in;
		}
};

// Prevent Shared Vars from using handle templates without the handle wrapper
template< class Type, class Changer >
class CSharedVarBase< CHandle<Type>, Changer >
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

protected:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CHandle<Type>		*m_pValue;
};

// Prevent Shared Vars from using handle templates without the handle wrapper
template< class Changer >
class CSharedVarBase< CBaseHandle, Changer >
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

protected:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CBaseHandle			*m_pValue;
};

// Shared handle wrapper.
template< class Type, class Changer >
class CSharedHandleBase : public CSharedVarBase< EHANDLE, Changer >
{
public:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() {}

	void Initialize( char *pchName, int iEntNum )
	{
		contract_assert( !this->m_pSharedMemory && !this->m_pValue );

		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( EHANDLE ) + sizeof( int ) );		// Also allocate memory for the int that converts to a Client handle
		this->m_pValue = (EHANDLE*)this->m_pSharedMemory->Base();
		m_piClientIndex = (int*)((void*)(this->m_pValue + 1));
	}

	bool operator !() const 
	{ 
		return !GetHandle_Internal()->Get(); 
	}

		// Data can only be set on the server
		const Type* operator=( const Type *val ) 
		{ 
			return Set( val ); 
		}

		const Type& operator=( const CSharedHandleBase<Type,Changer> &val ) 
		{ 
			const EHANDLE &handle = CSharedVarBase<EHANDLE,Changer>::Set( *val.m_pValue );
			return *(const Type*)handle.Get();
		}

		const Type* Set( const Type *val )
		{
			(*CSharedHandleBase<Type,Changer>::m_pValue) = val;

			// Store the int that we'll need to translate to the EHandle on the client
			*m_piClientIndex = Server_EHandleToInt( *CSharedHandleBase<Type,Changer>::m_pValue );
			return val;
		}

	operator const Type*() const 
	{ 
		return Get_Internal();
	}

	const Type* Get() const 
	{ 
		return Get_Internal();
	}

	const Type* operator->() const 
	{ 
		return Get_Internal();
	}

	bool operator==( const Type *val ) const 
	{
		return Get_Internal() == val; 
	}

	bool operator!=( const Type *val ) const 
	{
		return Get_Internal() != val;
	}

private:
	EHANDLE GetHandle_Internal() const 
	{
			// Server can get the handle directly
			return (*CSharedHandleBase<Type,Changer>::m_pValue);
	}

	Type* Get_Internal() const 
	{
		return static_cast< Type* >( GetHandle_Internal().Get() );
	}

public:
	int		*m_piClientIndex;	// Pointer to ints for Client handle translations
};

// Shared array wrapper.
template< class Type, class Changer >
class CSharedArrayBase : public CSharedVarBase< Type, Changer >
{
public:
	CSharedArrayBase( void )
	{
		m_iCount = 0;
	}

	void Initialize( char *pchName, int iEntNum, int iCount )
	{
		contract_assert( !this->m_pSharedMemory && !(CSharedArrayBase<Type,Changer>::m_pValue) && iCount > 0 );

		m_iCount = iCount;
		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( Type ) * m_iCount );
		CSharedArrayBase<Type,Changer>::m_pValue = (Type*)this->m_pSharedMemory->Base();
	}

	const int& Count() const
	{
		return m_iCount;
	}

	const Type* Get() const 
	{
		return CSharedArrayBase<Type,Changer>::m_pValue; 
	}

	const Type& Get( int i ) const
	{
		contract_assert( i >= 0 && i < m_iCount );
		return CSharedArrayBase<Type,Changer>::m_pValue[i];
	}

	const Type& operator[]( int i ) const
	{
		return Get( i );
	}

		// Data can only be set on the server
		Type& operator[]( int i )
		{
			return GetForModify( i );
		}

		Type& GetForModify( int i )
		{
			contract_assert( i >= 0 && i < m_iCount );
			return CSharedArrayBase<Type,Changer>::m_pValue[i];
		}

		void Set( int i, const Type &val )
		{
			contract_assert( i >= 0 && i < m_iCount );
			CSharedArrayBase<Type,Changer>::m_pValue[i] = val;
		}

private:
public:
	int		m_iCount;
};

// Shared handle array wrapper.
template< class Type, class Changer >
class CSharedHandleArrayBase : public CSharedArrayBase< EHANDLE, Changer >
{
public:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() {}

	void Initialize( char *pchName, int iEntNum, int iCount )
	{
		contract_assert( !this->m_pSharedMemory && !(CSharedHandleArrayBase<Type,Changer>::m_pValue) && iCount > 0 );

		this->m_iCount = iCount;
		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( EHANDLE ) * this->m_iCount + sizeof( int ) * this->m_iCount );	// Also allocate memory for the ints that convert to Client handles
		this->m_pValue = (EHANDLE*)this->m_pSharedMemory->Base();
		m_piClientIndex = (int*)((void*)(CSharedHandleArrayBase<Type,Changer>::m_pValue + this->m_iCount));
	}

	const int& Count() const
	{
		return this->m_iCount;
	}

	const Type& Get( int i ) const
	{
		contract_assert( i >= 0 && i < this->m_iCount );
		return Get_Internal( i );
	}

	const Type* operator[]( int i ) const
	{
		return Get_Internal( i );
	}

		// Data can only be set on the server
		const Type* Set( int i, const Type *val )
		{
			contract_assert( i >= 0 && i < this->m_iCount );
			CSharedHandleArrayBase<Type,Changer>::m_pValue[i] = val;

			// Store the int that we'll need to translate to the EHandle on the client
			m_piClientIndex[i] = Server_EHandleToInt( CSharedHandleArrayBase<Type,Changer>::m_pValue[i] );
			return val;
		}

private:
	EHANDLE GetHandle_Internal( int i ) const 
	{
			// Server can get the handle directly
			return CSharedHandleArrayBase<Type,Changer>::m_pValue[i];
	}

	Type* Get_Internal( int i ) const 
	{
		return static_cast< Type* >( GetHandle_Internal(i).Get() );
	}

public:
	int		*m_piClientIndex;	// Pointer to ints for Client handle translations
};

// Prevent Shared Vars from using UtlVector templates without the UtlVector wrapper
template< class Type, class Changer >
class CSharedVarBase< std::vector< Type >, Changer >
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = nullptr;
		m_pValue = nullptr;
	}

	~CSharedVarBase( void )
	{
		if ( m_pSharedMemory )
		{
			m_pSharedMemory->Release();
			m_pSharedMemory = nullptr;
		}
		m_pValue = nullptr;
	}

protected:
	virtual void Shared_CUtlVector_Vars_Must_Use_CSharedUtlVector() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	std::vector< Type >	*m_pValue;
};

// Shared UtlVector wrapper.
template< class Type, class Changer >
class CSharedUtlVectorBase : public CSharedVarBase< std::vector< Type >, Changer >
{
public:
	void Initialize( char *pchName, int iEntNum )
	{
		contract_assert( !this->m_pSharedMemory && !(CSharedUtlVectorBase<Type,Changer>::m_pValue) );

		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( std::vector< Type > ) );
		CSharedUtlVectorBase<Type,Changer>::m_pValue = (std::vector< Type >*)this->m_pSharedMemory->Base();
	}

	// Data can only be set on the server
	operator std::vector< Type >&()
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	std::vector< Type >& GetForModify()
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue;
	}

	std::vector< Type >* operator->()
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	Type& operator[]( int i )
	{
		return GetForModify( i );
	}

	Type& GetForModify( int i )
	{
		contract_assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		return (*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i];
	}

	void Set( int i, const Type &val )
	{
		contract_assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		(*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i] = val;
	}

	operator const std::vector< Type >&() const 
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const std::vector< Type >& Get() const 
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const std::vector< Type >* operator->() const 
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const Type& operator[]( int i ) const
	{
		return Get( i );
	}

	const Type& Get( int i ) const
	{
		contract_assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		return (*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i];
	}

	const int Count() const
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue->Count();
	}

protected:
	virtual void Shared_CUtlVector_Vars_Must_Use_CSharedUtlVector() {}

private:
};

// Use this macro to define a network variable.
#define CSharedVar( type, name ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedVarBase )

// Helper for color32's. Contains GetR(), SetR(), etc.. functions.
#define CSharedColor32( name ) \
	SHARED_VAR_START( color32, name ) \
	SHARED_VAR_END( color32, name, CSharedColor32Base )

// Vectors + some convenient helper functions.
#define CSharedVector( name ) CSharedVectorInternal( Vector, name )
#define CSharedQAngle( name ) CSharedVectorInternal( QAngle, name )

#define CSharedVectorInternal( type, name ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedVectorBase )

#define CSharedQuaternion( name ) \
	SHARED_VAR_START( Quaternion, name ) \
	SHARED_VAR_END( Quaternion, name, CSharedQuaternionBase )

#define CSharedHandle( handletype, name ) \
	SHARED_VAR_START( handletype, name ) \
	SHARED_VAR_END( handletype, name, CSharedHandleBase )

#define CSharedArray( type, name, count ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedArrayBase ) \
	enum { m_SharedArray_count_##name = count };

#define CSharedString( name, length ) \
	CSharedArray( char, name, length )

#define CSharedHandleArray( handletype, name, count ) \
	SHARED_VAR_START( handletype, name ) \
	SHARED_VAR_END( handletype, name, CSharedHandleArrayBase ) \
	enum { m_SharedArray_count_##name = count };

#define CSharedUtlVector( utlvectortype, name ) \
	SHARED_VAR_START( utlvectortype, name ) \
	SHARED_VAR_END( utlvectortype, name, CSharedUtlVectorBase ) \


// Internal macros used in definitions of network vars.
#define SHARED_VAR_START( type, name ) \
	class SharedVar_##name; \
	friend class SharedVar_##name; \
	typedef ThisClass MakeASharedVar_##name; \
	class SharedVar_##name \
	{ \
	public: \
		template <typename T> friend int ServerClassInit(T *);

#define SHARED_VAR_END( type, name, base ) \
	}; \
	base< type, SharedVar_##name > name;

#define DECLARE_SHAREDCLASS() \
	virtual void InitSharedVars( void );

#define IMPLEMENT_SHAREDCLASS_DT( type ) \
	void type::InitSharedVars( void ) \
	{ \
		BaseClass::InitSharedVars();

#define END_SHARED_TABLE() \
	}

#define SharedProp( var ) \
	var.Initialize( "SharedVar_"#var, entindex() );

#define SharedPropArray( var ) \
	var.Initialize( "SharedVar_"#var, entindex(), m_SharedArray_count_##var );

#undef CSharedVar
#undef CSharedColor32
#undef CSharedVector
#undef CSharedQAngle
#undef CSharedVectorInternal
#undef CSharedQuaternion
#undef CSharedHandle
#undef CSharedArray
#undef CSharedString
#undef CSharedHandleArray
#undef CSharedUtlVector
#undef SHARED_VAR_START
#undef SHARED_VAR_END
#undef DECLARE_SHAREDCLASS
#undef IMPLEMENT_SHAREDCLASS_DT
#undef END_SHARED_TABLE
#undef SharedProp
#undef SharedPropArray

extern ConVar ai_shot_bias_min;
extern ConVar ai_shot_bias_max;

//---------------------------------------------------------
// Caches off a shot direction and allows you to perform
// various operations on it without having to recalculate
// vecRight and vecUp each time. 
//---------------------------------------------------------
class CShotManipulator
{
public:
	CShotManipulator( const Vector &vecForward )
	{
		SetShootDir( vecForward );
	};

	void SetShootDir( const Vector &vecForward )
	{
		m_vecShotDirection = vecForward;
		VectorVectors( m_vecShotDirection, m_vecRight, m_vecUp );
	}

	const Vector &ApplySpread( const Vector &vecSpread, float bias = 1.0 );
	const Vector &ApplyAngularSpread( const Vector &vecSpread, float bias = 1.0 );

	const Vector &GetShotDirection()	{ return m_vecShotDirection; }
	const Vector &GetResult()			{ return m_vecResult; }
	const Vector &GetRightVector()		{ return m_vecRight; }
	const Vector &GetUpVector()			{ return m_vecUp;}

private:
	Vector m_vecShotDirection;
	Vector m_vecRight;
	Vector m_vecUp;
	Vector m_vecResult;
};

//---------------------------------------------------------
// Take a vector (direction) and another vector (spread) 
// and modify the direction to point somewhere within the 
// spread. This used to live inside FireBullets.
//---------------------------------------------------------
const Vector &CShotManipulator::ApplySpread( const Vector &vecSpread, float bias )
{
	// get circular gaussian spread
	float x, y, z;

	if ( bias > 1.0 )
		bias = 1.0;
	else if ( bias < 0.0 )
		bias = 0.0;

	float shotBiasMin = ai_shot_bias_min.GetFloat();
	float shotBiasMax = ai_shot_bias_max.GetFloat();

	// 1.0 gaussian, 0.0 is flat, -1.0 is inverse gaussian
	float shotBias = ( ( shotBiasMax - shotBiasMin ) * bias ) + shotBiasMin;

	float flatness = ( std::fabsf(shotBias) * 0.5 );

	do
	{
		x = random_valve->RandomFloat(-1,1) * flatness + random_valve->RandomFloat(-1,1) * (1 - flatness);
		y = random_valve->RandomFloat(-1,1) * flatness + random_valve->RandomFloat(-1,1) * (1 - flatness);
		if ( shotBias < 0 )
		{
			x = ( x >= 0 ) ? 1.0 - x : -1.0 - x;
			y = ( y >= 0 ) ? 1.0 - y : -1.0 - y;
		}
		z = x*x+y*y;
	} while (z > 1);

	m_vecResult = m_vecShotDirection + x * vecSpread.x * m_vecRight + y * vecSpread.y * m_vecUp;

	return m_vecResult;
}

const Vector &CShotManipulator::ApplyAngularSpread( const Vector &vecSpread, float bias )
{
	float x, y, z;
	x = vecSpread[0] * random_valve->RandomFloat(-0.5f, 0.5f);
	y = vecSpread[1] * random_valve->RandomFloat(-0.5f, 0.5f);
	z = vecSpread[2] * random_valve->RandomFloat(-0.5f, 0.5f);

	matrix3x4_t matrix;
	QAngle qa(x,y,z);
	AngleMatrix( qa, matrix );
	VectorTransform(m_vecShotDirection, matrix, m_vecResult);
	return m_vecResult;
}

extern CGlobalVars *gpGlobals;

inline constexpr auto ST_EPS = 0.001;

//-----------------------------------------------------------------------------

class CSimpleSimTimer
{
public:
	CSimpleSimTimer()
	 : m_next( -1 )
	{ 
	}

	void Force()
	{
		m_next = -1;
	}

	bool Expired() const
	{
		return ( gpGlobals->curtime - m_next > -ST_EPS );
	}

	float Delay( float delayTime )
	{
		return (m_next += delayTime);
	}
	
	float GetNext() const
	{
		return m_next;
	}

	void Set( float interval )
	{
		m_next = gpGlobals->curtime + interval;
	}

	void Set( float minInterval, float maxInterval )
	{ 
		if ( maxInterval > 0.0 )
			m_next = gpGlobals->curtime + random_valve->RandomFloat( minInterval, maxInterval );
		else
			m_next = gpGlobals->curtime + minInterval;
	}

	float GetRemaining() const;

	DECLARE_SIMPLE_DATADESC();
	
protected:
	float m_next;
};

//-----------------------------------------------------------------------------

class CSimTimer : public CSimpleSimTimer
{
public:
	CSimTimer( float interval = 0.0, bool startExpired = true )	
	{ 
		Set( interval, startExpired );
	}
	
	void Set( float interval, bool startExpired = true )
	{ 
		m_interval = interval;
		m_next = (startExpired) ? -1.0 : gpGlobals->curtime + m_interval;
	}

	void Reset( float interval = -1.0 );

	float GetInterval() const
	{
		return m_interval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_interval;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class CRandSimTimer : public CSimpleSimTimer
{
public:
	CRandSimTimer( float minInterval = 0.0, float maxInterval = 0.0, bool startExpired = true )	
	{ 
		Set( minInterval, maxInterval, startExpired );
	}
	
	void Set( float minInterval, float maxInterval = 0.0, bool startExpired = true )
	{ 
		m_minInterval = minInterval;
		m_maxInterval = maxInterval;
		
		if (startExpired)
		{
			m_next = -1;
		}
		else
		{
			if ( m_maxInterval == 0 )
				m_next = gpGlobals->curtime + m_minInterval;
			else
				m_next = gpGlobals->curtime + random_valve->RandomFloat( m_minInterval, m_maxInterval );
		}
	}

	void Reset();

	float GetMinInterval() const
	{
		return m_minInterval;
	}

	float GetMaxInterval() const
	{
		return m_maxInterval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_minInterval;
	float m_maxInterval;
};

//-----------------------------------------------------------------------------

class CStopwatchBase  : public CSimpleSimTimer
{
public:
	CStopwatchBase()	
	{ 
		m_fIsRunning = false;
	}

	bool IsRunning() const
	{
		return m_fIsRunning;
	}
	
	void Stop()
	{
		m_fIsRunning = false;
	}

	bool Expired() const
	{
		return ( m_fIsRunning && CSimpleSimTimer::Expired() );
	}
	
	DECLARE_SIMPLE_DATADESC();
	
protected:
	bool m_fIsRunning;
	
};

//-------------------------------------
class CSimpleStopwatch  : public CStopwatchBase
{
public:
	void Start( float minCountdown, float maxCountdown = 0.0 );

	void Stop()
	{
		m_fIsRunning = false;
	}

	bool Expired() const
	{
		return ( m_fIsRunning && CSimpleSimTimer::Expired() );
	}
};
//-------------------------------------

class CStopwatch : public CStopwatchBase
{
public:
	CStopwatch ( float interval = 0.0 )
	{ 
		Set( interval );
	}
	
	void Set( float interval )
	{ 
		m_interval = interval;
	}

	void Start( float intervalOverride );

	void Start()
	{
		Start( m_interval );
	}
	
	float GetInterval() const
	{
		return m_interval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_interval;
};

//-------------------------------------

class CRandStopwatch : public CStopwatchBase
{
public:
	CRandStopwatch( float minInterval = 0.0, float maxInterval = 0.0 )	
	{ 
		Set( minInterval, maxInterval );
	}
	
	void Set( float minInterval, float maxInterval = 0.0 )
	{ 
		m_minInterval = minInterval;
		m_maxInterval = maxInterval;
	}

	void Start( float minOverride, float maxOverride = 0.0 );

	void Start()
	{
		Start( m_minInterval, m_maxInterval );
	}
	
	float GetInterval() const
	{
		return m_minInterval;
	}

	float GetMinInterval() const
	{
		return m_minInterval;
	}

	float GetMaxInterval() const
	{
		return m_maxInterval;
	}

	DECLARE_SIMPLE_DATADESC();
	
private:
	float m_minInterval;
	float m_maxInterval;
};

//-----------------------------------------------------------------------------

class CThinkOnceSemaphore
{
public:
	CThinkOnceSemaphore()
	 :	m_lastTime( -1 )
	{
	}

	bool EnterThink();

	bool DidThink() const;

	void SetDidThink()
	{
		m_lastTime = gpGlobals->curtime;
	}

private:
	float m_lastTime;
};

//-----------------------------------------------------------------------------

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Resource collection entity


//=============================================================================
//
// Sky Camera Class
//
class CSkyCamera : public CLogicalEntity
{
	[[= ks::reflect::KeyFrom<"m_skyboxData.scale", ks::reflect::Key{ .name = "scale" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.enable", ks::reflect::Key{ .name = "fogenable" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.blend", ks::reflect::Key{ .name = "fogblend" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.dirPrimary", ks::reflect::Key{ .name = "fogdir" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.colorPrimary", ks::reflect::Key{ .name = "fogcolor" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.colorSecondary", ks::reflect::Key{ .name = "fogcolor2" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.start", ks::reflect::Key{ .name = "fogstart" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.end", ks::reflect::Key{ .name = "fogend" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.maxdensity", ks::reflect::Key{ .name = "fogmaxdensity" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_skyboxData.fog.HDRColorScale", ks::reflect::Key{ .name = "HDRColorScale" } >{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CSkyCamera, CLogicalEntity );

public:

	DECLARE_DATADESC();
	CSkyCamera();
	~CSkyCamera();
	void Spawn_Impl( void );
	void Activate_Impl();

	[[= ks::reflect::Input{ .name = "ActivateSkybox", .type = FIELD_VOID } ]] void InputActivateSkybox( inputdata_t &inputdata );

public:
	sky3dparams_t	m_skyboxData;
	[[= ks::reflect::Key{ .name = "use_angles" } ]] bool			m_bUseAngles;
	CSkyCamera		*m_pNext;
};

//-----------------------------------------------------------------------------
// Retrives the current skycamera
//-----------------------------------------------------------------------------
CSkyCamera*		GetCurrentSkyCamera();
CSkyCamera*		GetSkyCameraList();

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;


//==================================================
// SmokeTrail
//==================================================

class SmokeTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeTrail" } ]]
	static constexpr bool ks_class = true;

	DECLARE_DATADESC();
public:
	DECLARE_CLASS( SmokeTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	SmokeTrail();
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue ); 
	void					SetEmit(bool bVal);
	void					FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName = nullptr);
	static	SmokeTrail*		CreateSmokeTrail();

public:
	// Effect parameters. These will assume default values but you can change them.
	CNetworkVector( m_StartColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::WireEnc::Vector } ]] );			// Fade between these colors.
	CNetworkVector( m_EndColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_Opacity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "opacity" } ]] );

	CNetworkVar( float, m_SpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] [[= ks::reflect::Key{ .name = "spawnrate" } ]] );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );		// How long do the particles live?
	CNetworkVar( float, m_StopEmitTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );			// When do I stop emitting particles?
	CNetworkVar( float, m_MinSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxspeed" } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "startsize" } ]] );			// Size ramp.
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "endsize" } ]] );	
	CNetworkVar( float, m_SpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "spawnradius" } ]] );
	CNetworkVar( float, m_MinDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "mindirectedspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxdirectedspeed" } ]] );
	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 32 } ]] );
};

//==================================================
// RocketTrail
//==================================================

class RocketTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_RocketTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( RocketTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	RocketTrail();
	void					SetEmit(bool bVal);
	void					FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName = nullptr);
	static RocketTrail		*CreateRocketTrail();

public:
	// Effect parameters. These will assume default values but you can change them.
	CNetworkVector( m_StartColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::WireEnc::Vector } ]] );			// Fade between these colors.
	CNetworkVector( m_EndColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_Opacity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( float, m_SpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] );		// How long do the particles live?
	CNetworkVar( float, m_StopEmitTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );			// When do I stop emitting particles?
	CNetworkVar( float, m_MinSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );				// Speed range.
	CNetworkVar( float, m_MaxSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );			// Size ramp.
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );	
	CNetworkVar( float, m_SpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	
	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 32 } ]] );
	
	CNetworkVar( bool, m_bDamaged, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( float, m_flFlareScale, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );			// Size of the flare
};

//==================================================
// SporeTrail
//==================================================

class SporeTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SporeTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( SporeTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	SporeTrail( void );

	static SporeTrail*		CreateSporeTrail();

//Data members
public:

	CNetworkVector( m_vecEndColor, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::WireEnc::Vector } ]] );

	CNetworkVar( float, m_flSpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] );
	CNetworkVar( float, m_flParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flSpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );
};

//==================================================
// SporeExplosion
//==================================================

class SporeExplosion : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SporeExplosion" } ]]
	static constexpr bool ks_class = true;

	DECLARE_DATADESC();
public:
	DECLARE_CLASS( SporeExplosion, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	SporeExplosion( void );
	void Spawn_Impl( void );

	static SporeExplosion*		CreateSporeExplosion();

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

//Data members
public:

	[[= ks::reflect::Key{ .name = "startdisabled" } ]] bool m_bDisabled;

	CNetworkVar( float, m_flSpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] [[= ks::reflect::Key{ .name = "spawnrate" } ]] );
	CNetworkVar( float, m_flParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] );
	CNetworkVar( float, m_flStartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flEndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flSpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bDontRemove, [[= ks::reflect::Net{} ]] );
};

//==================================================
// CFireTrail
//==================================================

class CFireTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FireTrail" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CFireTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	static CFireTrail	*CreateFireTrail( void );
	void				FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName );
	void				Precache_Impl( void );

	CNetworkVar( int, m_nAttachment, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flLifetime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};

//==================================================
// DustTrail
//==================================================

class DustTrail : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_DustTrail" } ]]
	static constexpr bool ks_class = true;

	DECLARE_DATADESC();
public:
	DECLARE_CLASS( DustTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	DustTrail();
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue ); 
	void					SetEmit(bool bVal);
	static	DustTrail*		CreateDustTrail();

public:
	// Effect parameters. These will assume default values but you can change them.
	CNetworkVector( m_Color, [[= ks::reflect::Net{ .bits = 8, .low = 0, .high = 1, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_Opacity, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "opacity" } ]] );

	CNetworkVar( float, m_SpawnRate, [[= ks::reflect::Net{ .bits = 8, .low = 1, .high = 1024 } ]] [[= ks::reflect::Key{ .name = "spawnrate" } ]] );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime, [[= ks::reflect::Net{ .bits = 16, .low = 0.1, .high = 100, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );		// How long do the particles live?
	CNetworkVar( float, m_StopEmitTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );			// When do I stop emitting particles?
	CNetworkVar( float, m_MinSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "minspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxspeed" } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "startsize" } ]] );			// Size ramp.
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "endsize" } ]] );	
	CNetworkVar( float, m_SpawnRadius, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "spawnradius" } ]] );
	CNetworkVar( float, m_MinDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "mindirectedspeed" } ]] );				// Speed range.
	CNetworkVar( float, m_MaxDirectedSpeed, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "maxdirectedspeed" } ]] );
	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{} ]] );

	CNetworkVar( int, m_nAttachment );
};

struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: Defines the server side of a steam jet particle system entity.

//==================================================
// CSmokeStack
//==================================================

class CSmokeStackLightInfo
{
public:
	DECLARE_CLASS_NOBASE( CSmokeStackLightInfo );
	DECLARE_SIMPLE_DATADESC();
	DECLARE_NETWORKVAR_CHAIN();

	CNetworkVector( m_vPos );
	CNetworkVector( m_vColor );
	CNetworkVar( float, m_flIntensity );
};

class CSmokeStack : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SmokeStack" } ]]
	[[= ks::reflect::From<"m_DirLight.m_vPos", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
	[[= ks::reflect::From<"m_DirLight.m_vColor", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
	[[= ks::reflect::From<"m_DirLight.m_flIntensity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
	[[= ks::reflect::From<"m_AmbientLight.m_vPos", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
	[[= ks::reflect::From<"m_AmbientLight.m_vColor", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
	[[= ks::reflect::From<"m_AmbientLight.m_flIntensity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSmokeStack, CBaseParticleEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

					CSmokeStack();
					~CSmokeStack();

	void	Spawn_Impl( void );
	void	Activate_Impl();
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );
	void	Precache_Impl();

protected:

	// Input handlers.
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle(inputdata_t &data);

	void	RecalcWindVector();

// Stuff from the datatable.
public:
	CNetworkVar( float, m_SpreadSpeed, [[= ks::reflect::Key{ .name = "SpreadSpeed", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_Speed, [[= ks::reflect::Key{ .name = "Speed", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Key{ .name = "StartSize" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Key{ .name = "EndSize" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_Rate, [[= ks::reflect::Key{ .name = "Rate", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_JetLength, [[= ks::reflect::Key{ .name = "JetLength", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Length of the jet. Lifetime is derived from this.
	CNetworkVar( float, m_flRollSpeed, [[= ks::reflect::Key{ .name = "Roll" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( int, m_bEmit, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );		// Emit particles?
	CNetworkVar( float, m_flBaseSpread, [[= ks::reflect::Key{ .name = "BaseSpread" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	
	CSmokeStackLightInfo		m_AmbientLight;
	CSmokeStackLightInfo		m_DirLight;

	CNetworkVar( float, m_flTwist, [[= ks::reflect::Key{ .name = "Twist" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	
	string_t		m_strMaterialModel;
	CNetworkVar( int, m_iMaterialModel, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_IntAddOne, ks::reflect::WireSide::Send>{} ]] );

	[[= ks::reflect::Key{ .name = "WindAngle" } ]] int				m_WindAngle;
	[[= ks::reflect::Key{ .name = "WindSpeed" } ]] int				m_WindSpeed;
	CNetworkVector( m_vWind, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );		// m_vWind is just calculated from m_WindAngle and m_WindSpeed.

	[[= ks::reflect::Key{ .name = "InitialState" } ]] bool			m_InitialState;
};

// solid_t parsing
class CSolidSetDefaults : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) {}
	virtual void SetDefaults( void *pData );
};

extern CSolidSetDefaults g_SolidSetup;

class CSound;

class CBaseEntity;

using namespace ::ks::reflect;

// Soundent.h - the entity that spawns when the world 
// spawns, and handles the world's active and free sound
// lists.

enum
{
	MAX_WORLD_SOUNDS_SP	= 64,	// Maximum number of sounds handled by the world at one time in single player.
	// This is also the number of entries saved in a savegame file (for b/w compatibility).

	MAX_WORLD_SOUNDS_MP	= 128	// The sound array size is set this large but we'll only use gpGlobals->maxPlayers+32 entries in mp.
};

enum
{
	SOUND_NONE				= 0,
	SOUND_COMBAT			= 0x00000001,
	SOUND_WORLD				= 0x00000002,
	SOUND_PLAYER			= 0x00000004,
	SOUND_DANGER			= 0x00000008,
	SOUND_BULLET_IMPACT		= 0x00000010,
	SOUND_CARCASS			= 0x00000020,
	SOUND_MEAT				= 0x00000040,
	SOUND_GARBAGE			= 0x00000080,
	SOUND_THUMPER			= 0x00000100, // keeps certain creatures at bay
	SOUND_BUGBAIT			= 0x00000200, // gets the antlion's attention
	SOUND_PHYSICS_DANGER	= 0x00000400,
	SOUND_DANGER_SNIPERONLY	= 0x00000800, // only scares the sniper NPC.
	SOUND_MOVE_AWAY			= 0x00001000,
	SOUND_PLAYER_VEHICLE	= 0x00002000,
	SOUND_READINESS_LOW		= 0x00004000, // Changes listener's readiness (Player Companion only)
	SOUND_READINESS_MEDIUM	= 0x00008000,
	SOUND_READINESS_HIGH	= 0x00010000,

	// Contexts begin here.
	SOUND_CONTEXT_FROM_SNIPER		= 0x00100000, // additional context for SOUND_DANGER
	SOUND_CONTEXT_GUNFIRE			= 0x00200000, // Added to SOUND_COMBAT
	SOUND_CONTEXT_MORTAR			= 0x00400000, // Explosion going to happen here.
	SOUND_CONTEXT_COMBINE_ONLY		= 0x00800000, // Only combine can hear sounds marked this way
	SOUND_CONTEXT_REACT_TO_SOURCE	= 0x01000000, // React to sound source's origin, not sound's location
	SOUND_CONTEXT_EXPLOSION			= 0x02000000, // Context added to SOUND_COMBAT, usually.
	SOUND_CONTEXT_EXCLUDE_COMBINE	= 0x04000000, // Combine do NOT hear this
	SOUND_CONTEXT_DANGER_APPROACH   = 0x08000000, // Treat as a normal danger sound if you see the source, otherwise turn to face source.
	SOUND_CONTEXT_ALLIES_ONLY		= 0x10000000, // Only player allies can hear this sound
	SOUND_CONTEXT_PLAYER_VEHICLE	= 0x20000000, // HACK: need this because we're not treating the SOUND_xxx values as true bit values! See switch in OnListened.
	SOUND_CONTEXT_FROM_FIRE			= 0x40000000,	// A fire is nearby
	SOUND_CONTEXT_FOLLOW_OWNER		= 0x80000000,	// The sound origin is at the owner

	ALL_CONTEXTS			= 0xFFF00000,

	ALL_SCENTS				= SOUND_CARCASS | SOUND_MEAT | SOUND_GARBAGE,

	ALL_SOUNDS				= 0x000FFFFF & ~ALL_SCENTS,

};

// Make as many of these as you want. 
enum
{
	SOUNDENT_CHANNEL_UNSPECIFIED = 0,
	SOUNDENT_CHANNEL_REPEATING,
	SOUNDENT_CHANNEL_REPEATED_DANGER,	// for things that make danger sounds frequently.
	SOUNDENT_CHANNEL_REPEATED_PHYSICS_DANGER,
	SOUNDENT_CHANNEL_WEAPON,
	SOUNDENT_CHANNEL_INJURY,
	SOUNDENT_CHANNEL_BULLET_IMPACT,
	SOUNDENT_CHANNEL_NPC_FOOTSTEP,
	SOUNDENT_CHANNEL_SPOOKY_NOISE,		// made by zombies in darkness
	SOUNDENT_CHANNEL_ZOMBINE_GRENADE,
};

enum
{
	SOUNDLIST_EMPTY = -1
};

inline constexpr auto SOUNDENT_VOLUME_MACHINEGUN = 1500.0;
inline constexpr auto SOUNDENT_VOLUME_SHOTGUN = 1500.0;
inline constexpr auto SOUNDENT_VOLUME_PISTOL = 1500.0;
inline constexpr auto SOUNDENT_VOLUME_EMPTY = 500.0; // volume of the "CLICK" when you have no bullets

enum
{
	SOUND_PRIORITY_VERY_LOW = -2,
	SOUND_PRIORITY_LOW,
	SOUND_PRIORITY_NORMAL = 0,
	SOUND_PRIORITY_HIGH,
	SOUND_PRIORITY_VERY_HIGH,
	SOUND_PRIORITY_HIGHEST,
};

//=========================================================
// CSound - an instance of a sound in the world.
//=========================================================
class CSound
{
	DECLARE_SIMPLE_DATADESC();

public:
	bool	DoesSoundExpire() const;
	float	SoundExpirationTime() const;
	void	SetSoundOrigin( const Vector &vecOrigin ) { m_vecOrigin = vecOrigin; }
	const	Vector& GetSoundOrigin( void );
	const	Vector& GetSoundReactOrigin( void );
	bool	FIsSound( void );
	bool	FIsScent( void );
	bool	IsSoundType( int nSoundFlags ) const;
	int		SoundType( ) const;
	int		SoundContext() const;
	int		SoundTypeNoContext( ) const;
	int		Volume( ) const;
	float	OccludedVolume() { return m_iVolume * m_flOcclusionScale; }
	int		NextSound() const;
	void	Reset ( void );
	int		SoundChannel( void ) const;
	bool	ValidateOwner() const;

	EHANDLE	m_hOwner;				// sound's owner
	EHANDLE	m_hTarget;				// Sounds's target - an odd concept. For a gunfire sound, the target is the entity being fired at
	int		m_iVolume;				// how loud the sound is
	float	m_flOcclusionScale;		// How loud the sound is when occluded by the world. (volume * occlusionscale)
	int		m_iType;				// what type of sound this is
	int		m_iNextAudible;			// temporary link that NPCs use to build a list of audible sounds

private:
	void	Clear ( void );

	float	m_flExpireTime;	// when the sound should be purged from the list
	short	m_iNext;		// index of next sound in this list ( Active or Free )
	bool	m_bNoExpirationTime;
	int		m_ownerChannelIndex;

	Vector	m_vecOrigin;	// sound's location in space

	bool	m_bHasOwner;	// Lets us know if this sound was created with an owner. In case the owner goes null.

#ifdef DEBUG
	int		m_iMyIndex;		// debugging
#endif

	friend class CSoundEnt;
};

//=========================================================
// CSoundEnt - a single instance of this entity spawns when
// the world spawns. The SoundEnt's job is to update the 
// world's Free and Active sound lists.
//=========================================================
class CSoundEnt : public CPointEntity
{

public:
	DECLARE_CLASS( CSoundEnt, CPointEntity );

	// Construction, destruction
	static bool InitSoundEnt();
	static void ShutdownSoundEnt();

	CSoundEnt();
	virtual ~CSoundEnt();

	void OnRestore_Impl();
	void Precache_Impl ( void );
	void Spawn_Impl( void );
	void Think_Impl( void );
	void Initialize ( void );
	int ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	static int		InsertSound ( int iType, const Vector &vecOrigin, int iVolume, float flDuration, CBaseEntity *pOwner = nullptr, int soundChannelIndex = SOUNDENT_CHANNEL_UNSPECIFIED, CBaseEntity *pSoundTarget = nullptr );
	static int		ActiveList( void );// return the head of the active list
	static CSound*	SoundPointerForIndex( int iIndex );// return a pointer for this index in the sound list
	static CSound*	GetLoudestSoundOfType( int iType, const Vector &vecEarPosition );
	static int		ClientSoundIndex ( edict_t *pClient );
	static void		FreeSound( int iSound );

	bool	IsEmpty( void );
	int		ISoundsInList ( int iListType );
	int		IAllocSound ( void );
	int		FindOrAllocateSound( CBaseEntity *pOwner, int soundChannelIndex );
	
private:
	static void		FreeSound ( int iSound, int iPrevious );
	static int		FreeList( void );// return the head of the free list

	int		m_iFreeSound;	// index of the first sound in the free sound list
	int		m_iActiveSound; // indes of the first sound in the active sound list
	int		m_cLastActiveSounds; // keeps track of the number of active sounds at the last update. (for diagnostic work)
	CSound	m_SoundPool[ MAX_WORLD_SOUNDS_MP ];
};

class CBasePlayer;
struct EmitSound_t;



class CSoundPatch;

enum soundcommands_t
{
	SOUNDCTRL_CHANGE_VOLUME,
	SOUNDCTRL_CHANGE_PITCH,
	SOUNDCTRL_STOP,
	SOUNDCTRL_DESTROY,
};

//Envelope point
struct envelopePoint_t
{
	float	amplitudeMin, amplitudeMax;
	float	durationMin, durationMax;
};

//Envelope description
struct envelopeDescription_t
{
	envelopePoint_t	*pPoints;
	int				nNumPoints;
};

class CSoundEnvelopeController
{
public:
	virtual void		SystemReset( void ) = 0;
	virtual void		SystemUpdate( void ) = 0;
	virtual void		Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime = 0 ) = 0;
	virtual void		CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float value ) = 0;
	virtual void		CommandClear( CSoundPatch *pSound ) = 0;
	virtual void		Shutdown( CSoundPatch *pSound ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
							float attenuation, float scriptVolume = 1.0f ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, const Vector *pSoundOrigin, float scriptVolume = 1.0f ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
							soundlevel_t soundlevel ) = 0;

	virtual CSoundPatch	*SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es ) = 0;

	virtual void		SoundDestroy( CSoundPatch	* ) = 0;
	virtual void		SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime ) = 0;
	virtual void		SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime ) = 0;
	virtual void		SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout = false ) = 0;
	virtual float		SoundGetPitch( CSoundPatch *pSound ) = 0;
	virtual float		SoundGetVolume( CSoundPatch *pSound ) = 0;

	virtual float		SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints ) = 0;
	virtual float		SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope ) = 0;

	virtual void		CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer ) = 0;

	virtual string_t	SoundGetName( CSoundPatch *pSound ) = 0;
	virtual string_t	SoundGetScriptName( CSoundPatch *pSound ) = 0;
	static	CSoundEnvelopeController &GetController( void );

	virtual void		SoundSetCloseCaptionDuration( CSoundPatch *pSound, float flDuration ) = 0;
};

class CEnvSoundscape;

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;



struct ss_update_t
{
	CBasePlayer *pPlayer;
	CEnvSoundscape	*pCurrentSoundscape;
	Vector		playerPosition;
	float		currentDistance;
	int			traceCount;
	bool		bInRange;
};

class CEnvSoundscape : public CServerOnlyEntity
{
public:
	DECLARE_CLASS( CEnvSoundscape, CServerOnlyEntity );
	DECLARE_DATADESC();

	CEnvSoundscape();
	~CEnvSoundscape();

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Spawn_Impl( void );
	void Precache_Impl( void );
	void UpdateForPlayer( ss_update_t &update );
	void WriteAudioParamsTo( audioparams_t &audio );
	bool InRangeOfPlayer( CBasePlayer *pPlayer );
	void DrawDebugGeometryOverlays_Impl( void );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleEnabled", .type = FIELD_VOID } ]] void InputToggleEnabled( inputdata_t &inputdata );

	string_t GetSoundscapeName() const {return m_soundscapeName;}

private:

	bool IsEnabled( void ) const;
	void Disable( void );
	void Enable( void );

public:
	[[= ks::reflect::Key{ .name = "OnPlay" } ]] COutputEvent	m_OnPlay;
	[[= ks::reflect::Key{ .name = "radius" } ]] float	m_flRadius;
	string_t m_soundscapeName;
	int		m_soundscapeIndex;
	int		m_soundscapeEntityId;
	[[= ks::reflect::Key{ .name = "position7", .index = 7 } ]] [[= ks::reflect::Key{ .name = "position6", .index = 6 } ]] [[= ks::reflect::Key{ .name = "position5", .index = 5 } ]] [[= ks::reflect::Key{ .name = "position4", .index = 4 } ]] [[= ks::reflect::Key{ .name = "position3", .index = 3 } ]] [[= ks::reflect::Key{ .name = "position2", .index = 2 } ]] [[= ks::reflect::Key{ .name = "position1", .index = 1 } ]] [[= ks::reflect::Key{ .name = "position0", .index = 0 } ]] string_t m_positionNames[NUM_AUDIO_LOCAL_SOUNDS];
	
	// If this is set, then this soundscape ignores all its parameters and uses
	// those of this soundscape.
	CHandle<CEnvSoundscape> m_hProxySoundscape;

private:

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bDisabled;
};

class CEnvSoundscapeProxy : public CEnvSoundscape
{
public:
	DECLARE_CLASS( CEnvSoundscapeProxy, CEnvSoundscape );
	DECLARE_DATADESC();

	CEnvSoundscapeProxy();
	void Activate_Impl();

	// Here just to stop it falling back to CEnvSoundscape's, and
	// printing bogus errors about missing soundscapes.
	void Precache_Impl() { return; }

private:
	[[= ks::reflect::Key{ .name = "MainSoundscapeName" } ]] string_t m_MainSoundscapeName;
};

class CEnvSoundscapeTriggerable : public CEnvSoundscape
{
friend class CTriggerSoundscape;

public:
	DECLARE_CLASS( CEnvSoundscapeTriggerable, CEnvSoundscape );

	CEnvSoundscapeTriggerable();
	
	// Overrides the base class's think and prevents it from running at all.
	void Think_Impl();

private:

	// Passed through from CTriggerSoundscape.
	void DelegateStartTouch( CBaseEntity *pEnt );
	void DelegateEndTouch( CBaseEntity *pEnt );
};

class CEnvSoundscape;


struct clusterSoundscapeList_t
{
	unsigned short	soundscapeCount;
	unsigned short	firstSoundscape;
};

class CSoundscapeSystem : public CAutoGameSystemPerFrame
{
public:
	CSoundscapeSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
	}

	// game system
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void FrameUpdatePostEntityThink( void );
	virtual void LevelInitPreEntity( void );
	virtual void LevelInitPostEntity();

	virtual void AddSoundscapeFile( const char *filename );
	int	GetSoundscapeIndex( const char *pName );
	bool IsValidIndex( int index );

	void FlushSoundscapes( void );
	void AddSoundscapeEntity( CEnvSoundscape *pSoundscape );
	void RemoveSoundscapeEntity( CEnvSoundscape *pSoundscape );
	void PrintDebugInfo( void );

	void AddSoundscapeSounds( KeyValues *pSoundscape, int soundscapeIndex );
	void PrecacheSounds( int soundscapeIndex );

private:
	CStringRegistry							m_soundscapes;
	int										m_soundscapeCount;
	std::vector< CEnvSoundscape * >			m_soundscapeEntities;
	std::vector< clusterSoundscapeList_t >		m_soundscapesInCluster;
	std::vector< unsigned short >				m_soundscapeIndexList;
	int										m_activeIndex;
	std::vector< std::vector< std::string > >	m_soundscapeSounds;
};

extern CSoundscapeSystem g_SoundscapeSystem;

class CBaseEntity;


void DoSpark( CBaseEntity *ent, const Vector &location, int nMagnitude, int nTrailLength, bool bPlaySound, const Vector &vecDir );

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Spatial entity.

//------------------------------------------------------------------------------
// Purpose : Spatial entity
//------------------------------------------------------------------------------
void SendProxy_Origin( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class CSpatialEntity : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SpatialEntity", .base = false } ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector }, SendProxy_Origin>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CSpatialEntity, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CSpatialEntity();

	void Spawn_Impl( void );
	int  UpdateTransmitState_Impl();

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	// Inputs
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFadeInDuration", .type = FIELD_FLOAT } ]] void	InputSetFadeInDuration ( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFadeOutDuration", .type = FIELD_FLOAT } ]] void	InputSetFadeOutDuration ( inputdata_t &inputdata );

private:
	void	FadeIn ( void );
	void	FadeOut ( void );

	void FadeInThink( void );	// Fades lookup weight from Cur->MaxWeight 
	void FadeOutThink( void );	// Fades lookup weight from CurWeight->0.0

	[[= ks::reflect::Key{ .name = "fadeInDuration" } ]] float	m_flFadeInDuration;		// Duration for a full 0->MaxWeight transition
	[[= ks::reflect::Key{ .name = "fadeOutDuration" } ]] float	m_flFadeOutDuration;	// Duration for a full Max->0 transition
	float	m_flStartFadeInWeight;
	float	m_flStartFadeOutWeight;
	float	m_flTimeStartFadeIn;
	float	m_flTimeStartFadeOut;

	[[= ks::reflect::Key{ .name = "maxweight" } ]] float	m_flMaxWeight;

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool	m_bStartDisabled;
	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enabled" } ]] );

	CNetworkVar( float, m_MinFalloff, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "minfalloff" } ]] );
	CNetworkVar( float, m_MaxFalloff, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "maxfalloff" } ]] );
	CNetworkVar( float, m_flCurWeight, [[= ks::reflect::Net{ .bits = 32 } ]] );

	[[= ks::reflect::Key{ .name = "filename" } ]] string_t	m_lookupFilename;
};

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Dynamic light at the end of a spotlight

class CSpotlightEnd : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SpotlightEnd" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSpotlightEnd, CBaseEntity );

	void				Spawn_Impl( void );

	int					ObjectCaps_Impl( void );

	DECLARE_SERVERCLASS();

public:
	CNetworkVar( float, m_flLightScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_Radius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
//	CNetworkVector( m_vSpotlightDir );
//	CNetworkVector( m_vSpotlightOrg );
	Vector			m_vSpotlightDir;
	Vector			m_vSpotlightOrg;
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

inline constexpr auto SF_SPRITE_STARTON = 0x0001;
inline constexpr auto SF_SPRITE_ONCE = 0x0002;
inline constexpr auto SF_SPRITE_TEMPORARY = 0x8000;

constexpr float MAX_GLOW_PROXY_SIZE = 64.0f;

constexpr float MAX_SPRITE_SCALE = 64.0f;

void RecvProxy_SpriteScale( const CRecvProxyData *pData, void *pStruct, void *pOut );


class CSprite : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Sprite" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CSprite, CBaseEntity );
public:
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	CSprite();
	virtual ~CSprite();

	 void SetModel_Impl( const char *szModelName );

	void Spawn_Impl( void );
	void Precache_Impl( void );
	 void ComputeWorldSpaceSurroundingBox_Impl( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	void SetGlowProxySize( float flSize ) { m_flGlowProxySize = flSize; }

	 int ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );
	 int UpdateTransmitState_Impl( void );
	
	void SetAsTemporary( void ) { AddSpawnFlags( SF_SPRITE_TEMPORARY ); }
	bool IsTemporary( void ) { return ( HasSpawnFlags( SF_SPRITE_TEMPORARY ) ); }
	
	int	ObjectCaps_Impl( void );

	void OnRestore_Impl();

	void AnimateThink( void );
	void ExpandThink( void );
	void Animate( float frames );
	void Expand( float scaleSpeed, float fadeSpeed );
	void SpriteInit( const char *pSpriteName, const Vector &origin );

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	// Input handlers
	[[= ks::reflect::Input{ .name = "HideSprite", .type = FIELD_VOID } ]] void InputHideSprite( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ShowSprite", .type = FIELD_VOID } ]] void InputShowSprite( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleSprite", .type = FIELD_VOID } ]] void InputToggleSprite( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorRedValue", .type = FIELD_FLOAT } ]] void InputColorRedValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorBlueValue", .type = FIELD_FLOAT } ]] void InputColorBlueValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorGreenValue", .type = FIELD_FLOAT } ]] void InputColorGreenValue( inputdata_t &inputdata );

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
	DECLARE_DATADESC();

	static CSprite *SpriteCreate( const char *pSpriteName, const Vector &origin, bool animate );
	static CSprite *SpriteCreatePredictable( const char *module, int line, const char *pSpriteName, const Vector &origin, bool animate );

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

};

class CSpriteOriented : public CSprite
{
	[[= ks::reflect::NetTable{ .name = "DT_SpriteOriented" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CSpriteOriented, CSprite );
	DECLARE_SERVERCLASS();
	void Spawn_Impl( void );
};

// Macro to wrap creation
template <typename T1, typename T2, typename T3>
inline auto SPRITE_CREATE_PREDICTABLE( T1 name, T2 origin, T3 animate, std::source_location loc = std::source_location::current() )
{
	return CSprite::SpriteCreatePredictable( loc.file_name(), static_cast<int>( loc.line() ), name, origin, animate );
}

class CSprite;

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
	void Spawn_Impl( void );
	void Precache_Impl( void );

	// Server only code
	static CSpriteTrail *SpriteTrailCreate( const char *pSpriteName, const Vector &origin, bool animate );

private:

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

class CSteamJet;

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: Defines the server side of a steam jet particle system entity.

//NOTENOTE: Mirrored in cl_dlls\c_steamjet.cpp
inline constexpr auto STEAM_NORMAL = 0;
inline constexpr auto STEAM_HEATWAVE = 1;

//==================================================
// CSteamJet
//==================================================

class CSteamJet : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SteamJet" } ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

public:
	CSteamJet();
	DECLARE_CLASS( CSteamJet, CBaseParticleEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

protected:

	// Input handlers.
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &data);

// Stuff from the datatable.
public:
	CNetworkVar( float, m_SpreadSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "SpreadSpeed", .input = true } ]] );
	CNetworkVar( float, m_Speed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Speed", .input = true } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "StartSize" } ]] );
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "EndSize" } ]] );
	CNetworkVar( float, m_Rate, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Rate", .input = true } ]] );
	CNetworkVar( float, m_JetLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "JetLength", .input = true } ]] );	// Length of the jet. Lifetime is derived from this.

	CNetworkVar( int, m_bEmit, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );		// Emit particles?
	CNetworkVar( bool, m_bFaceLeft, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );	// For support of legacy env_steamjet, which faced left instead of forward.
	[[= ks::reflect::Key{ .name = "InitialState" } ]] bool			m_InitialState;

	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "Type" } ]] );		// Type of steam (normal, heatwave)
	CNetworkVar( float, m_flRollSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "RollSpeed" } ]] );

	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

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

class CBaseEntity;


//-----------------------------------------------------------------------------
// Particle attachment methods
//-----------------------------------------------------------------------------
enum ParticleAttachment_t
{
	PATTACH_ABSORIGIN = 0,			// Create at absorigin, but don't follow
	PATTACH_ABSORIGIN_FOLLOW,		// Create at absorigin, and update to follow the entity
	PATTACH_CUSTOMORIGIN,			// Create at a custom origin, but don't follow
	PATTACH_CUSTOMORIGIN_FOLLOW,	// Create at a custom origin, follow relative position to specified entity
	PATTACH_POINT,					// Create on attachment point, but don't follow
	PATTACH_POINT_FOLLOW,			// Create on attachment point, and update to follow the entity
	PATTACH_EYES_FOLLOW,			// Create on eyes of the attached entity, and update to follow the entity
	PATTACH_OVERHEAD_FOLLOW,		// Create at the top of the entity's bbox
	PATTACH_WORLDORIGIN,			// Used for control points that don't attach to an entity
	PATTACH_ROOTBONE_FOLLOW,		// Create at the root bone of the entity, and update to follow

	MAX_PATTACH_TYPES,
};

extern int GetAttachTypeFromString( const char *pszString );

inline constexpr auto PARTICLE_DISPATCH_FROM_ENTITY = (1<<0);
inline constexpr auto PARTICLE_DISPATCH_RESET_PARTICLES = (1<<1);
inline constexpr auto PARTICLE_DISPATCH_ALLOW_DORMANT = (1<<2);

//-----------------------------------------------------------------------------
// Particle parsing methods
//-----------------------------------------------------------------------------
// Parse the particle manifest file & register the effects within it
// Only needs to be called once per game, unless tools change particle definitions
void ParseParticleEffects( bool bLoadSheets );

// Precaches standard particle systems (only necessary on server)
// Should be called once per level
void PrecacheStandardParticleSystems( );

//-----------------------------------------------------------------------------
// Particle spawning methods
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr, bool bAllowDormantSpawn = false );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffect( const char *pszParticleName, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = nullptr );
void DispatchParticleEffectLink( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, CBaseEntity *pOtherEntity = nullptr, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( int iEffectIndex, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = nullptr, int nSplitScreenPlayerSlot = -1 );
void StopParticleEffects( CBaseEntity *pEntity );
void StopParticleEffect( CBaseEntity *pEntity, const char *pszParticleName );

class CBasePlayer;

using namespace ::ks::reflect;


inline constexpr const char *PARTICLESMOKEGRENADE_ENTITYNAME = "env_particlesmokegrenade";

inline constexpr auto MIN_SMOKE_TINT = 0.5f;
inline constexpr auto MAX_SMOKE_TINT = 0.6f;

class ParticleSmokeGrenade : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleSmokeGrenade" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( ParticleSmokeGrenade, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

						ParticleSmokeGrenade();
	
	virtual				~ParticleSmokeGrenade();

	void		Spawn_Impl( void );
	int			UpdateTransmitState_Impl( void );
	void				SetCreator(CBasePlayer *creator);
	CBasePlayer*		GetCreator();

public:

	// Tell the client entity to start filling the volume.
	void				FillVolume();

	// Set the times it fades out at.
	void				SetFadeTime(float startTime, float endTime);

	// Set time to fade out relative to current time
	void				SetRelativeFadeTime(float startTime, float endTime);

	// Set the tint color of the grenade smoke.
	void				SetSmokeColor(Vector color);

	void				Think_Impl( void );

public:
	
	// Stage 0 (default): make a smoke trail that follows the entity it's following.
	// Stage 1          : fill a volume with smoke.
	CNetworkVar( unsigned char, m_CurrentStage, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( float, m_flSpawnTime, [[= ks::reflect::Net{} ]] );

	// When to fade in and out.
	CNetworkVar( float, m_FadeStartTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_FadeEndTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( Vector, m_MinColor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( Vector, m_MaxColor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

protected:
    
	EHANDLE m_creatorPlayer;
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

void SendProxy_Angles( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system
//-----------------------------------------------------------------------------
class CParticleSystem : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleSystem", .base = false } ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = kHighDefault, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::Vector }, SendProxy_Origin>{} ]]
	[[= ks::reflect::From<"m_fEffects", ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_hOwnerEntity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
	[[= ks::reflect::From<"m_iParentAttachment", ks::reflect::Net{ .bits = NUM_PARENTATTACHMENT_BITS, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .flags = SPROP_CHANGES_OFTEN, .enc = ks::reflect::WireEnc::QAngles }, SendProxy_Angles>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CParticleSystem, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CParticleSystem( void );

	void Precache_Impl( void );
	void Spawn_Impl( void );
	void Activate_Impl( void );
	int  UpdateTransmitState_Impl(void);
	int	 ObjectCaps_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	bool GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen );

	void		StartParticleSystem( void );
	void		StopParticleSystem( int nStopType = STOP_NORMAL );

	[[= ks::reflect::Input{ .name = "Start", .type = FIELD_VOID } ]] void		InputStart( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop", .type = FIELD_VOID } ]] void		InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopPlayEndCap", .type = FIELD_VOID } ]] void		InputStopEndCap( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DestroyImmediately", .type = FIELD_VOID } ]] void		InputDestroy( inputdata_t &inputdata );
	void		StartParticleSystemThink( void );
	bool		SetControlPointValue( int iControlPoint, const Vector &vValue ); //server controlled control points (variables in particle effects instead of literal follow points)
	void		DisableSaveRestore( bool bState ) { m_bNoSave = bState; }

	enum
	{	
		kSERVERCONTROLLEDPOINTS = 4,
		kMAXCONTROLPOINTS = 63, ///< actually one less than the total number of cpoints since 0 is assumed to be me
	}; 
	
	// stop types
	enum 
	{
		STOP_NORMAL = 0,
		STOP_DESTROY_IMMEDIATELY,
		STOP_PLAY_ENDCAP,
		NUM_STOP_TYPES
	};

protected:

	/// Load up and resolve the entities that are supposed to be the control points 
	void ReadControlPointEnts( void );

	bool				m_bNoSave;
	[[= ks::reflect::Key{ .name = "start_active" } ]] bool				m_bStartActive;
	[[= ks::reflect::Key{ .name = "effect_name" } ]] string_t			m_iszEffectName;
	CNetworkString(		m_szSnapshotFileName, MAX_PATH, [[= ks::reflect::Net{} ]] );
	
	CNetworkVar( bool,	m_bActive, [[= ks::reflect::Net{} ]] );
	// bits: Q_log2( NUM_STOP_TYPES ) + 1, spelled out because Q_log2 is a runtime call.
	CNetworkVar( int,	m_nStopType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int,	m_iEffectIndex, [[= ks::reflect::Net{ .bits = MAX_PARTICLESYSTEMS_STRING_BITS, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float,	m_flStartTime, [[= ks::reflect::Net{ .bits = 32 } ]] );	// Time at which this effect was started.  This is used after restoring an active effect.
	
	//server controlled control points (variables in particle effects instead of literal follow points)
	CNetworkArray( Vector, m_vServerControlPoints, kSERVERCONTROLLEDPOINTS,
	               [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkArray( uint8, m_iServerControlPointAssignments, kSERVERCONTROLLEDPOINTS,
	               [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_UNSIGNED } ]] );

	[[= ks::reflect::Key{ .name = "cpoint63", .index = 62 } ]] [[= ks::reflect::Key{ .name = "cpoint62", .index = 61 } ]] [[= ks::reflect::Key{ .name = "cpoint61", .index = 60 } ]] [[= ks::reflect::Key{ .name = "cpoint60", .index = 59 } ]] [[= ks::reflect::Key{ .name = "cpoint59", .index = 58 } ]] [[= ks::reflect::Key{ .name = "cpoint58", .index = 57 } ]] [[= ks::reflect::Key{ .name = "cpoint57", .index = 56 } ]] [[= ks::reflect::Key{ .name = "cpoint56", .index = 55 } ]] [[= ks::reflect::Key{ .name = "cpoint55", .index = 54 } ]] [[= ks::reflect::Key{ .name = "cpoint54", .index = 53 } ]] [[= ks::reflect::Key{ .name = "cpoint53", .index = 52 } ]] [[= ks::reflect::Key{ .name = "cpoint52", .index = 51 } ]] [[= ks::reflect::Key{ .name = "cpoint51", .index = 50 } ]] [[= ks::reflect::Key{ .name = "cpoint50", .index = 49 } ]] [[= ks::reflect::Key{ .name = "cpoint49", .index = 48 } ]] [[= ks::reflect::Key{ .name = "cpoint48", .index = 47 } ]] [[= ks::reflect::Key{ .name = "cpoint47", .index = 46 } ]] [[= ks::reflect::Key{ .name = "cpoint46", .index = 45 } ]] [[= ks::reflect::Key{ .name = "cpoint45", .index = 44 } ]] [[= ks::reflect::Key{ .name = "cpoint44", .index = 43 } ]] [[= ks::reflect::Key{ .name = "cpoint43", .index = 42 } ]] [[= ks::reflect::Key{ .name = "cpoint42", .index = 41 } ]] [[= ks::reflect::Key{ .name = "cpoint41", .index = 40 } ]] [[= ks::reflect::Key{ .name = "cpoint40", .index = 39 } ]] [[= ks::reflect::Key{ .name = "cpoint39", .index = 38 } ]] [[= ks::reflect::Key{ .name = "cpoint38", .index = 37 } ]] [[= ks::reflect::Key{ .name = "cpoint37", .index = 36 } ]] [[= ks::reflect::Key{ .name = "cpoint36", .index = 35 } ]] [[= ks::reflect::Key{ .name = "cpoint35", .index = 34 } ]] [[= ks::reflect::Key{ .name = "cpoint34", .index = 33 } ]] [[= ks::reflect::Key{ .name = "cpoint33", .index = 32 } ]] [[= ks::reflect::Key{ .name = "cpoint32", .index = 31 } ]] [[= ks::reflect::Key{ .name = "cpoint31", .index = 30 } ]] [[= ks::reflect::Key{ .name = "cpoint30", .index = 29 } ]] [[= ks::reflect::Key{ .name = "cpoint29", .index = 28 } ]] [[= ks::reflect::Key{ .name = "cpoint28", .index = 27 } ]] [[= ks::reflect::Key{ .name = "cpoint27", .index = 26 } ]] [[= ks::reflect::Key{ .name = "cpoint26", .index = 25 } ]] [[= ks::reflect::Key{ .name = "cpoint25", .index = 24 } ]] [[= ks::reflect::Key{ .name = "cpoint24", .index = 23 } ]] [[= ks::reflect::Key{ .name = "cpoint23", .index = 22 } ]] [[= ks::reflect::Key{ .name = "cpoint22", .index = 21 } ]] [[= ks::reflect::Key{ .name = "cpoint21", .index = 20 } ]] [[= ks::reflect::Key{ .name = "cpoint20", .index = 19 } ]] [[= ks::reflect::Key{ .name = "cpoint19", .index = 18 } ]] [[= ks::reflect::Key{ .name = "cpoint18", .index = 17 } ]] [[= ks::reflect::Key{ .name = "cpoint17", .index = 16 } ]] [[= ks::reflect::Key{ .name = "cpoint16", .index = 15 } ]] [[= ks::reflect::Key{ .name = "cpoint15", .index = 14 } ]] [[= ks::reflect::Key{ .name = "cpoint14", .index = 13 } ]] [[= ks::reflect::Key{ .name = "cpoint13", .index = 12 } ]] [[= ks::reflect::Key{ .name = "cpoint12", .index = 11 } ]] [[= ks::reflect::Key{ .name = "cpoint11", .index = 10 } ]] [[= ks::reflect::Key{ .name = "cpoint10", .index = 9 } ]] [[= ks::reflect::Key{ .name = "cpoint9", .index = 8 } ]] [[= ks::reflect::Key{ .name = "cpoint8", .index = 7 } ]] [[= ks::reflect::Key{ .name = "cpoint7", .index = 6 } ]] [[= ks::reflect::Key{ .name = "cpoint6", .index = 5 } ]] [[= ks::reflect::Key{ .name = "cpoint5", .index = 4 } ]] [[= ks::reflect::Key{ .name = "cpoint4", .index = 3 } ]] [[= ks::reflect::Key{ .name = "cpoint3", .index = 2 } ]] [[= ks::reflect::Key{ .name = "cpoint2", .index = 1 } ]] [[= ks::reflect::Key{ .name = "cpoint1", .index = 0 } ]] string_t			m_iszControlPointNames[kMAXCONTROLPOINTS];
	CNetworkArray( EHANDLE, m_hControlPointEnts, kMAXCONTROLPOINTS, [[= ks::reflect::Net{} ]] );
	CNetworkArray( unsigned char, m_iControlPointParents, kMAXCONTROLPOINTS,
	               [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]]
	               [[= ks::reflect::Key{ .name = "cpoint1_parent", .index = 0 } ]] [[= ks::reflect::Key{ .name = "cpoint2_parent", .index = 1 } ]] [[= ks::reflect::Key{ .name = "cpoint3_parent", .index = 2 } ]] [[= ks::reflect::Key{ .name = "cpoint4_parent", .index = 3 } ]] [[= ks::reflect::Key{ .name = "cpoint5_parent", .index = 4 } ]] [[= ks::reflect::Key{ .name = "cpoint6_parent", .index = 5 } ]] [[= ks::reflect::Key{ .name = "cpoint7_parent", .index = 6 } ]] );
};

struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Spawnflag for CPathTrack
//-----------------------------------------------------------------------------
inline constexpr auto SF_PATH_DISABLED = 0x00000001;
//#define SF_PATH_FIREONCE		0x00000002
inline constexpr auto SF_PATH_ALTREVERSE = 0x00000004;
inline constexpr auto SF_PATH_DISABLE_TRAIN = 0x00000008;
inline constexpr auto SF_PATH_TELEPORT = 0x00000010;
inline constexpr auto SF_PATH_ALTERNATE = 0x00008000;

enum TrackOrientationType_t
{
	TrackOrientation_Fixed = 0,
	TrackOrientation_FacePath,
	TrackOrientation_FacePathAngles,
};

//-----------------------------------------------------------------------------
// Paths!
//-----------------------------------------------------------------------------
class CPathTrack : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPathTrack, CServerOnlyPointEntity );

public:
	CPathTrack();

	void		Spawn_Impl( void );
	void		Activate_Impl( void );
	void		DrawDebugGeometryOverlays_Impl();
	
	void		ToggleAlternatePath( void );
	void		EnableAlternatePath( void );
	void		DisableAlternatePath( void );
	bool		HasAlternathPath() const;

	void		TogglePath( void );
	void		EnablePath( void );
	void		DisablePath( void );

	static CPathTrack	*ValidPath( CPathTrack *ppath, int testFlag = true );		// Returns ppath if enabled, NULL otherwise

	CPathTrack	*GetNextInDir( bool bForward );
	CPathTrack	*GetNext( void );
	CPathTrack	*GetPrevious( void );
	
	CPathTrack	*Nearest( const Vector &origin );
	//CPathTrack *LookAhead( Vector &origin, float dist, int move );
	CPathTrack *LookAhead( Vector &origin, float dist, int move, CPathTrack **pNextNext = nullptr );

	TrackOrientationType_t GetOrientationType();
	QAngle GetOrientation( bool bForwardDir );

	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;

	float GetRadius() const { return m_flRadius; }

	// These four methods help for circular path checking. Call BeginIteration
	// before iterating, EndInteration afterwards. Call Visit on each path in the
	// list. Then you can use HasBeenVisited to see if you've visited the node
	// already, which means you've got a circular or lasso path. You can use the
	// macro BEGIN_PATH_TRACK_ITERATION below to simplify the calls to
	// BeginInteration + EndIteration.
	static void BeginIteration();
	static void EndIteration();
	void		Visit();
	bool		HasBeenVisited() const;

private:
	void		Project( CPathTrack *pstart, CPathTrack *pend, Vector &origin, float dist );
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	
	static CPathTrack *Instance( edict_t *pent );

	[[= ks::reflect::Input{ .name = "InPass", .type = FIELD_VOID } ]] void InputPass( inputdata_t &inputdata );
	
	[[= ks::reflect::Input{ .name = "ToggleAlternatePath", .type = FIELD_VOID } ]] void InputToggleAlternatePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableAlternatePath", .type = FIELD_VOID } ]] void InputEnableAlternatePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableAlternatePath", .type = FIELD_VOID } ]] void InputDisableAlternatePath( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "TogglePath", .type = FIELD_VOID } ]] void InputTogglePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnablePath", .type = FIELD_VOID } ]] void InputEnablePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisablePath", .type = FIELD_VOID } ]] void InputDisablePath( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "radius" } ]] float		m_flRadius;
	float		m_length;
	[[= ks::reflect::Key{ .name = "altpath" } ]] string_t	m_altName;
    int			m_nIterVal;
	[[= ks::reflect::Key{ .name = "orientationtype" } ]] TrackOrientationType_t m_eOrientationType;

	[[= ks::reflect::Key{ .name = "OnPass" } ]] COutputEvent m_OnPass;

	static int	s_nCurrIterVal;
	static bool s_bIsIterating;
};

//-----------------------------------------------------------------------------
// Used to make sure circular iteration works all nice
//-----------------------------------------------------------------------------
#define BEGIN_PATH_TRACK_ITERATION() CPathTrackVisitor _visit

class CPathTrackVisitor
{
public:
	CPathTrackVisitor() { CPathTrack::BeginIteration(); }
	~CPathTrackVisitor() { CPathTrack::EndIteration(); }
};

#undef BEGIN_PATH_TRACK_ITERATION

class CBaseEntity;


inline constexpr auto SF_THRUST_STARTACTIVE = 0x0001;
inline constexpr auto SF_THRUST_FORCE = 0x0002;
inline constexpr auto SF_THRUST_TORQUE = 0x0004;
inline constexpr auto SF_THRUST_LOCAL_ORIENTATION = 0x0008;
inline constexpr auto SF_THRUST_MASS_INDEPENDENT = 0x0010;
inline constexpr auto SF_THRUST_IGNORE_POS = 0x0020;

CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive );
CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags );

AngularImpulse ComputeRotSpeedToAlignAxes( const Vector &testAxis, const Vector &alignAxis, const AngularImpulse &currentSpeed, 
										  float damping, float scale, float maxSpeed );

// Purpose: Data types used inside constraints for the purpose of playing sounds
//			during movement.

/** \brief Class to store a sampled history of velocity for an object -- used for certain sound calculations

Although this contains only one sample for now, it exists as an interface 
so as to make simpler the possibility of moving to a ring buffer 
implementation in the future. 

The "sample rate" variable is not nominal: it should be used to specify 
the ClientThink() interval. 

Be sure to use the beginSampling() function for the first sample, and 
addSample() thereafter: this will be relevant and necessary for a ring
buffer implementation (which will have to perform certain initialization).
*/
class VelocitySampler
{	
public:
	/*
	enum
	{
	HISTORY_DEPTH_LOG	= 3, // < log-base-2 of the sampler's array depth
	HISTORY_DEPTH		= (1 << VELOCITY_SAMPLER_HISTORY_DEPTH_LOG),
	};
	*/

	/// Return the internally stored sample rate.
	inline float getSampleRate() 
	{
		return m_fIdealSampleRate;
	}

	/// Store off the first recorded sample for the given object.
	inline void BeginSampling(const Vector &relativeVelocity);

	/// Record a sample. Do this LAST, after calling hasReversed() et al.
	inline void AddSample(const Vector &relativeVelocity);

	/// Using the sample history, determine if the object has reversed direction
	/// with at least the given acceleration (in units/sec^2).
	int HasReversed(const Vector &relativeVelocity, const float thresholdAcceleration[], const unsigned short numThresholds);

	/// Call this in spawn(). (Not a constructor because those are difficult to use in entities.)
	void Initialize(float samplerate);

	/// A convenience function for extracting the linear velocity of one object relative to another.
	inline static Vector GetRelativeVelocity(IPhysicsObject *pObj,	IPhysicsObject *pReferenceFrame);

	/// A convenience function for extracting the angular velocity of one object relative to another.
	inline static Vector GetRelativeAngularVelocity(IPhysicsObject *pObj,	IPhysicsObject *pReferenceFrame);

protected:
	Vector m_prevSample;
	float m_fPrevSampleTime;

	float m_fIdealSampleRate;

};

struct SimpleConstraintSoundProfile
{
	// define the indices of the sound points:
	enum
	{
		kMIN_THRESHOLD, ///< below this no sound is played
		kMIN_FULL,      ///< at this velocity sound is at its loudest

		kHIGHWATER,	///< high water mark for this enum
	} eKeypoints;

	float m_keyPoints[kHIGHWATER];

	/// Number of entries in the reversal sound array 
	enum { kREVERSAL_SOUND_ARRAY_SIZE = 3 };

	/// Acceleration threshold for playing the hard-reverse sound. Divided into sections.
	/// Below the 0th threshold no sound will play.
	float m_reversalSoundThresholds[kREVERSAL_SOUND_ARRAY_SIZE]; 

	/// Get volume for given velocity [0..1]
	float GetVolume(float inVel);
};

float SimpleConstraintSoundProfile::GetVolume(float inVel)
{
	// clamped lerp on 0-1
	if (inVel <= m_keyPoints[kMIN_THRESHOLD])
	{
		return 0;
	}
	else if (inVel >= m_keyPoints[kMIN_FULL])
	{
		return 1;
	}
	else	// lerp...
	{
		return (inVel - m_keyPoints[kMIN_THRESHOLD])/(m_keyPoints[kMIN_FULL] - m_keyPoints[kMIN_THRESHOLD]);
	}
}

class CPhysConstraint;
/** This class encapsulates the data and behavior necessary for a constraint to play sounds.

	An embedding entity does not inherit these keys: it has to name them itself. Give the
	entity class one KeyFrom<> annotation per key, with a dotted path from the entity to the
	member. The three float thresholds derive their type; the three sound names do not --
	they are string_t, and FIELD_SOUNDNAME versus FIELD_STRING is a semantic distinction no
	type carries, so those need an explicit .as:

	class CMyConstraint : public CPhysConstraint
{
	[[= ks::reflect::KeyFrom<"m_soundInfo.m_iszTravelSoundFwd",
	ks::reflect::Key{ .name = "slidesoundfwd", .as = FIELD_SOUNDNAME } >{} ]]
	[[= ks::reflect::KeyFrom<"m_soundInfo.m_soundProfile.m_reversalSoundThreshold",
	ks::reflect::Key{ .name = "reversalsoundthreshold" } >{} ]]
	static constexpr bool ks_class = true;
 ... };

	The two m_keyPoints entries are elements of an array, so they take Key{ .index = N }.
	Note that .index shifts the offset but does not appear in the entry's fieldName, whereas
	the old macro stringified the whole subscripted expression -- and find_member_path parses
	only decimal subscripts, not enumerators like kMIN_THRESHOLD. If a save-game field name
	has to match the old form exactly, that needs a fieldName override that does not exist yet.
	No entity keys these today: nothing in the tree reads minSoundThreshold and friends.

 */
class ConstraintSoundInfo
{
public:
	// no ctor.
	// dtor
	~ConstraintSoundInfo();

	/// Call from the constraint's Activate()
	void OnActivate( CPhysConstraint *pOuter );

	/// Constraint should have a think function that calls this. It should pass in relative velocity
	/// between child and parent. (This need not be linear velocity; it may be angular.)
	void OnThink( CPhysConstraint *pOuter, const Vector &relativeVelocity );

	/// This is how often the think function should be run:
	inline float getThinkRate() const { return 0.09f; }

	/// Call this before the first call to OnThink()
	void StartThinking( CPhysConstraint *pOuter, const Vector &relativeVelocity, const Vector &forwardVector );

	/// Call this if you intend to stop calling OnThink():
	void StopThinking( CPhysConstraint *pOuter );

	/// Call from owner's Precache().
	void OnPrecache( CPhysConstraint *pOuter );

	VelocitySampler m_vSampler;
	SimpleConstraintSoundProfile m_soundProfile;

	Vector m_forwardAxis; ///< velocity in this direction is forward. The opposite direction is backward.

	string_t m_iszTravelSoundFwd,m_iszTravelSoundBack;			// Path/filename of WAV file to play.
	CSoundPatch		*m_pTravelSound;
	bool			m_bPlayTravelSound;

	string_t m_iszReversalSounds[SimpleConstraintSoundProfile::kREVERSAL_SOUND_ARRAY_SIZE];			// Path/filename of WAV files to play -- one per entry in threshold.
	// CSoundPatch		*m_pReversalSound;
	bool			m_bPlayReversalSound;

protected:
	/// Maintain consistency of internal datastructures on start
	void ValidateInternals( CPhysConstraint *pOuter );

	/// Stop playing any active sounds.
	void DeleteAllSounds();
};

/////////////// INLINE FUNCTIONS
	
/// compute the relative velocity between an object and its parent. Just a convenience.
Vector VelocitySampler::GetRelativeVelocity( IPhysicsObject *pObj, IPhysicsObject *pReferenceFrame )
{
	Vector childVelocity, parentVelocity;
	pObj->GetImplicitVelocity( &childVelocity, nullptr );
	pReferenceFrame->GetImplicitVelocity(&parentVelocity, nullptr);

	return (childVelocity - parentVelocity);
}

Vector VelocitySampler::GetRelativeAngularVelocity( IPhysicsObject *pObj, IPhysicsObject *pReferenceFrame )
{
	contract_assert( pObj );

	if ( pReferenceFrame )
	{
		Vector childVelocityLocal, parentVelocityLocal, childVelocityWorld, parentVelocityWorld;
		pObj->GetImplicitVelocity( nullptr, &childVelocityLocal );
		pObj->LocalToWorldVector( &childVelocityWorld, childVelocityLocal );
		pReferenceFrame->GetImplicitVelocity( nullptr, &parentVelocityLocal );
		pObj->LocalToWorldVector( &parentVelocityWorld, parentVelocityLocal );

		return (childVelocityWorld - parentVelocityWorld);
	}
	else
	{
		Vector childVelocityLocal, childVelocityWorld;
		pObj->GetImplicitVelocity( nullptr, &childVelocityLocal );
		pObj->LocalToWorldVector( &childVelocityWorld, childVelocityLocal );

		return (childVelocityWorld);
	}
}

/************************************************************************/
// This function is nominal -- it's here as an interface because in the 
// future there will need to be special initialization for the first entry
// in a ring buffer. (I made a test implementation of this, then reverted it
// later; this is not an arbitrary assumption.)
/************************************************************************/
/// Store off the first recorded sample for the given object.
void VelocitySampler::BeginSampling(const Vector &relativeVelocity)
{
	return AddSample(relativeVelocity);
}

// Record a sample for the given object
void VelocitySampler::AddSample(const Vector &relativeVelocity)
{
	m_prevSample = relativeVelocity;
	m_fPrevSampleTime = gpGlobals->curtime;
}

/* // abandoned -- too complicated, no way to set from keyfields
/// Stores information used for playing sounds based on 
/// constraint movement
class ConstraintSoundProfile
{
public:
/// Defines a point in the sound profile: volume and pitch for the sound to play.
/// Implicit crossfading between two sounds. Used to map velocity to a sound profile.
struct SoundInfoTuple
{
float minVelocity;
union {
struct{
float volume1,pitch1; //< volume and pitch of sound 1
float volume2,pitch2; //< volume and pitch of sound 2
};
fltx4 m_as4;
};

inline SoundInfoTuple(float _minVelocity, float _volume1, float _pitch1, float _volume2, float _pitch2) :
minVelocity(_minVelocity), volume1(_volume1), pitch1(_pitch1), volume2(_volume2), pitch2(_pitch2) 
{}
};

ConstraintSoundProfile(const SoundInfoTuple *soundTable, unsigned int tableSize) 
: m_pSoundInfos(soundTable), m_numSoundInfos(tableSize) 
{}

protected:

/// A table of sound info structs
const SoundInfoTuple * const m_pSoundInfos;
/// Size of the table
const unsigned int m_numSoundInfos;
};

static ConstraintSoundProfile::SoundInfoTuple CSDebugProfileTable[] =
{
ConstraintSoundProfile::SoundInfoTuple(12,0,0,0,0),
ConstraintSoundProfile::SoundInfoTuple(24,0,0,0,0),

};
*/

class CBaseEntity;
class CEntityList;
class CTakeDamageInfo;


// Purpose: This is the abstraction layer for the physics simulation system
// Any calls to the external physics library (ipion) should be made through this
// layer.  Eventually, the physics system will probably become a DLL and made
// accessible to the client & server side code.

class IPhysicsMaterial;

extern IPhysicsMaterial		*g_Material;
extern ConVar phys_pushscale;
extern ConVar phys_timescale;

extern IPhysicsGameTrace	*physgametrace;

extern IPhysicsCollisionSolver * const g_pCollisionSolver;
extern IPhysicsCollisionEvent * const g_pCollisionEventHandler;
extern IPhysicsObjectEvent * const g_pObjectEventHandler;

extern ConVar sv_phys_props_block_movers;

// HACKHACK: We treat anything >= 500kg as a special "large mass" that does more impact damage
// and has special recovery on crushing/killing other objects
// also causes screen shakes on impact with static/world objects
const float VPHYSICS_LARGE_OBJECT_MASS = 500.0f;

struct gamevcollisionevent_t : public vcollisionevent_t
{
	Vector			preVelocity[2];
	Vector			postVelocity[2];
	AngularImpulse	preAngularVelocity[2];
	CBaseEntity		*pEntities[2];

	void Init( vcollisionevent_t *pEvent ) 
	{ 
		*((vcollisionevent_t *)this) = *pEvent; 
		pEntities[0] = nullptr;
		pEntities[1] = nullptr;
	}
};

struct triggerevent_t
{
	CBaseEntity		*pTriggerEntity;
	IPhysicsObject	*pTriggerPhysics;
	CBaseEntity		*pEntity;
	IPhysicsObject	*pObject;
	bool			bStart;

	inline void Init( CBaseEntity *triggerEntity, IPhysicsObject *triggerPhysics, CBaseEntity *entity, IPhysicsObject *object, bool startTouch )
	{
		pTriggerEntity = triggerEntity;
		pTriggerPhysics= triggerPhysics;
		pEntity = entity;
		pObject = object;
		bStart = startTouch;
	}
	inline void Clear()
	{
		std::memset( this, 0, sizeof(*this) );
	}
};

// parse solid parameter overrides out of a string 
void PhysSolidOverride( solid_t &solid, string_t overrideScript );

extern CEntityList *g_pShadowEntities;
void PhysAddShadow( CBaseEntity *pEntity );
void PhysRemoveShadow( CBaseEntity *pEntity );
bool PhysHasShadow( CBaseEntity *pEntity );

void PhysEnableFloating( IPhysicsObject *pObject, bool bEnable );

void PhysCollisionSound( CBaseEntity *pEntity, IPhysicsObject *pPhysObject, int channel, int surfaceProps, int surfacePropsHit, float deltaTime, float speed );
void PhysCollisionScreenShake( gamevcollisionevent_t *pEvent, int index );
void PhysCollisionDust( gamevcollisionevent_t *pEvent, surfacedata_t *phit );
void PhysBreakSound( CBaseEntity *pEntity, IPhysicsObject *pPhysObject, Vector vecOrigin );

// plays the impact sound for a particular material
void PhysicsImpactSound( CBaseEntity *pEntity, IPhysicsObject *pPhysObject, int channel, int surfaceProps, int surfacePropsHit, float volume, float impactSpeed );

void PhysCallbackDamage( CBaseEntity *pEntity, const CTakeDamageInfo &info );
void PhysCallbackDamage( CBaseEntity *pEntity, const CTakeDamageInfo &info, gamevcollisionevent_t &event, int hurtIndex );

// Applies force impulses at a later time
void PhysCallbackImpulse( IPhysicsObject *pPhysicsObject, const Vector &vecCenterForce, const AngularImpulse &vecCenterTorque );

// Sets the velocity at a later time
void PhysCallbackSetVelocity( IPhysicsObject *pPhysicsObject, const Vector &vecVelocity );

// queue up a delete on this object
void PhysCallbackRemove(IServerNetworkable *pRemove);

bool PhysGetDamageInflictorVelocityStartOfFrame( IPhysicsObject *pInflictor, Vector &velocity, AngularImpulse &angVelocity );

// force a physics entity to sleep immediately
void PhysForceEntityToSleep( CBaseEntity *pEntity, IPhysicsObject *pObject );

// teleport an entity to it's position relative to an object it's constrained to
void PhysTeleportConstrainedEntity( CBaseEntity *pTeleportSource, IPhysicsObject *pObject0, IPhysicsObject *pObject1, const Vector &prevPosition, const QAngle &prevAngles, bool physicsRotate );

void PhysGetListOfPenetratingEntities( CBaseEntity *pSearch, std::vector< CBaseEntity * > &list );
bool PhysShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1 );

// returns true when processing a callback - so we can defer things that can't be done inside a callback
bool PhysIsInCallback();
bool PhysIsFinalTick();

bool PhysGetTriggerEvent( triggerevent_t *pEvent, CBaseEntity *pTrigger );
// note: pErrorEntity is used to report errors (object not found, more than one found).  It can be NULL
IPhysicsObject *FindPhysicsObjectByName( const char *pName, CBaseEntity *pErrorEntity );

// this is called to flush all queues when the delete list is cleared
void PhysOnCleanupDeleteList();

struct masscenteroverride_t
{
	enum align_type
	{
		ALIGN_POINT = 0,
		ALIGN_AXIS = 1,
	};

	void Defaults()
	{
		entityName = NULL_STRING;
	}

	void SnapToPoint( string_t name, const Vector &pointWS );

	void SnapToAxis( string_t name, const Vector &axisStartWS, const Vector &unitAxisDirWS );

	Vector		center;
	Vector		axis;
	int			alignType;
	string_t	entityName;
};

void PhysSetMassCenterOverride( masscenteroverride_t &override );
// NOTE: this removes the entry from the table as well as retrieving it
void PhysGetMassCenterOverride( CBaseEntity *pEntity, vcollide_t *pCollide, solid_t &solidOut );
void PhysSetEntityGameFlags( CBaseEntity *pEntity, unsigned short flags );

void DebugDrawContactPoints(IPhysicsObject *pPhysics);

class CBoneFollower;

class CBaseAnimating;
class CBaseEntity;
class CTakeDamageInfo;
struct gamevcollisionevent_t;

using namespace ::ks::reflect;




//
// To use bone followers in an entity, contain a CBoneFollowerManager in it. Then:
//		- Call InitBoneFollowers() in the entity's CreateVPhysics().
//		- Call UpdateBoneFollowers() after you move your bones.
//		- Call DestroyBoneFollowers() when your entity's removed

struct physfollower_t
{
	DECLARE_SIMPLE_DATADESC();
	int boneIndex;
	CHandle<CBoneFollower> hFollower;
};

// create a manager and a list of followers directly from a ragdoll
void CreateBoneFollowersFromRagdoll( CBaseAnimating *pEntity, class CBoneFollowerManager *pManager, vcollide_t *pCollide );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBoneFollowerManager
{
	DECLARE_SIMPLE_DATADESC();
public:
	CBoneFollowerManager();
	~CBoneFollowerManager();

	// Use either of these to create the bone followers in your entity's CreateVPhysics()
	void InitBoneFollowers( CBaseAnimating *pParentEntity, int iNumBones, const char **pFollowerBoneNames );
	void AddBoneFollower( CBaseAnimating *pParentEntity, const char *pFollowerBoneName, solid_t *pSolid = nullptr );	// Adds a single bone follower

	// Call this after you move your bones
	void UpdateBoneFollowers( CBaseAnimating *pParentEntity );

	// Call this when your entity's removed
	void DestroyBoneFollowers( void );

	physfollower_t *GetBoneFollower( int iFollowerIndex );
	int				GetBoneFollowerIndex( CBoneFollower *pFollower );
	int				GetNumBoneFollowers( void ) const { return m_iNumBones; }

private:
	bool CreatePhysicsFollower( CBaseAnimating *pParentEntity, physfollower_t &follow, const char *pBoneName, solid_t *pSolid );

private:
	int							m_iNumBones;
	std::vector< physfollower_t >	m_physBones;
};

class CBoneFollower : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BoneFollower" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBoneFollower, CBaseEntity );
	DECLARE_SERVERCLASS();
public:
	// CBaseEntity
	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	int  UpdateTransmitState_Impl(void);

	// NOTE: These are forwarded to the parent object!
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	void VPhysicsFriction_Impl( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit );
	void VPhysicsShadowCollision_Impl( int index, gamevcollisionevent_t *pEvent );

	bool TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );
	int	 ObjectCaps_Impl( void );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void Touch_Impl( CBaseEntity *pOther );

	void TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	// locals
	bool Init( CBaseEntity *pOwner, const char *pModelName, solid_t &solid, const Vector &position, const QAngle &orientation );
	void UpdateFollower( const Vector &position, const QAngle &orientation, float flInterval );
	void SetTraceData( int physicsBone, int hitGroup );

	// factory
	static CBoneFollower *Create( CBaseEntity *pOwner, const char *pModelName, solid_t &solid, const Vector &position, const QAngle &orientation );

private:
	CNetworkVar( int, m_modelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_solidIndex, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );
	int		m_physicsBone;
	int		m_hitGroup;
};

class IGameSystem;

//-----------------------------------------------------------------------------
// Responsible for kicking off the precaching of resources
//-----------------------------------------------------------------------------
class CPrecacheRegister : public IGameSystem
{
	// Inherited from IGameSystem
public:
	virtual char const *Name() { return "PrecacheRegister"; }
	virtual bool IsPerFrame() { return false; }
	virtual bool Init();
	virtual void PostInit() {}
	virtual void Shutdown() {}
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity();
	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	// Other public methods
public:
	// constructor, destructor
	CPrecacheRegister() {}
	virtual ~CPrecacheRegister() {}

private:
};

//-----------------------------------------------------------------------------
// Singletons
//-----------------------------------------------------------------------------
extern PrecacheHandler_t g_PrecacheHandler;
extern CPrecacheRegister *g_pPrecacheRegister;

using namespace ::ks::reflect;

// For introspection

[[gnu::always_inline]] inline constexpr auto UsePrediction() { return 1; }

// CLIENT DLL includes

#define DECLARE_NETWORKCLASS_NOBASE()									\
		DECLARE_SERVERCLASS_NOBASE()	

	// nothing, only client has a prediction system

#define IMPLEMENT_NETWORKCLASS(className, dataTable)			\
	IMPLEMENT_SERVERCLASS(className, dataTable)
#define IMPLEMENT_NETWORKCLASS_DT(className, dataTable)			\
	IMPLEMENT_SERVERCLASS_ST(className, dataTable)

#undef DECLARE_NETWORKCLASS_NOBASE
#undef IMPLEMENT_NETWORKCLASS
#undef IMPLEMENT_NETWORKCLASS_DT

class CBasePlayer;
class CBaseViewModel;

using namespace ::ks::reflect;


class CPredictedViewModel : public CBaseViewModel
{
	[[= ks::reflect::NetTable{ .name = "DT_PredictedViewModel" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPredictedViewModel, CBaseViewModel );
public:

	DECLARE_NETWORKCLASS();

	CPredictedViewModel( void );
	virtual ~CPredictedViewModel( void );
							
	virtual void CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles );
	virtual void AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles );
	virtual void ApplyViewModelPitchAndDip( CBasePlayer *owner, Vector& origin, QAngle& angles );

	virtual void CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles );

private:
	
};

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CTesla : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Tesla" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTesla, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CTesla();

	void Spawn_Impl();
	void Activate_Impl();
	void Precache_Impl();

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DoSpark", .type = FIELD_VOID } ]] void InputDoSpark( inputdata_t &inputdata );

	void DoSpark();
	void ShootArcThink();
	
	void SetupForNextArc();
	CBaseEntity* GetSourceEntity();

public:
	
	// Tesla parameters.
	[[= ks::reflect::Key{ .name = "m_SourceEntityName" } ]] string_t m_SourceEntityName;	// Which entity the arcs come from.
	CNetworkVar( string_t, m_SoundName, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "m_SoundName" } ]] );			// What sound to play when arcing.

	[[= ks::reflect::Key{ .name = "m_Color" } ]] color32 m_Color;
	[[= ks::reflect::Key{ .name = "beamcount_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "beamcount_min", .index = 0 } ]] int m_NumBeams[2];		// Number of beams per spark.
	
	[[= ks::reflect::Key{ .name = "m_flRadius" } ]] float m_flRadius;		// Radius it looks for surfaces to arc to.
	
	[[= ks::reflect::Key{ .name = "thick_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "thick_min", .index = 0 } ]] float m_flThickness[2];		// Beam thickness.
	[[= ks::reflect::Key{ .name = "lifetime_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "lifetime_min", .index = 0 } ]] float m_flTimeVisible[2];	// How long each beam stays around (min/max).
	[[= ks::reflect::Key{ .name = "interval_max", .index = 1 } ]] [[= ks::reflect::Key{ .name = "interval_min", .index = 0 } ]] float m_flArcInterval[2];	// Time between args (min/max).

	[[= ks::reflect::Key{ .name = "m_bOn" } ]] bool m_bOn;

	CNetworkVar( string_t, m_iszSpriteName, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "texture" } ]] );
};

class CBaseEntity;


typedef CBaseEntity* (*StressEntityFn)();	// Function to create an entity for the stress test.

// Each game DLL can instantiate these to register types of entities it can create
// for the entity stress test.
class CStressEntityReg
{
public:

							CStressEntityReg( StressEntityFn fn );

	static CStressEntityReg*GetListHead()	{ return s_pHead; }
	CStressEntityReg*		GetNext()		{ return m_pNext; }
	StressEntityFn			GetFn()			{ return m_pFn; }

private:
	static CStressEntityReg	*s_pHead;	// List of all CStressEntityReg's.
	CStressEntityReg		*m_pNext;
	StressEntityFn			m_pFn;
};

// Use this macro to register a function to create stresstest entities.
#define REGISTER_STRESS_ENTITY( fnName ) static CStressEntityReg s_##fnName##__( fnName );

// Helper function for the functions that create the stress entities.
// Moves the entity to a random place in the level and returns the entity.
CBaseEntity*	MoveToRandomSpot( CBaseEntity *pEnt );
Vector			GetRandomSpot();

#undef REGISTER_STRESS_ENTITY

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
	EHANDLE				entityTouched;
	int					touchStamp;
	touchlink_t			*nextLink;
	touchlink_t			*prevLink;
	int					flags;
};

// means this touchlink is managed external to the main physics system
inline constexpr auto TOUCHSTAMP_EVENT_DRIVEN = -1;

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Spawnflags of CPathCorner
inline constexpr auto SF_CORNER_WAITFORTRIG = 0x001;
inline constexpr auto SF_CORNER_TELEPORT = 0x002;

// Tracktrain spawn flags
inline constexpr auto SF_TRACKTRAIN_NOPITCH = 0x0001;
inline constexpr auto SF_TRACKTRAIN_NOCONTROL = 0x0002;
inline constexpr auto SF_TRACKTRAIN_FORWARDONLY = 0x0004;
inline constexpr auto SF_TRACKTRAIN_PASSABLE = 0x0008;
inline constexpr auto SF_TRACKTRAIN_FIXED_ORIENTATION = 0x0010;
inline constexpr auto SF_TRACKTRAIN_HL1TRAIN = 0x0080;
inline constexpr auto SF_TRACKTRAIN_USE_MAXSPEED_FOR_PITCH = 0x0100;
inline constexpr auto SF_TRACKTRAIN_UNBLOCKABLE_BY_PLAYER = 0x0200;
inline constexpr auto SF_TRACKTRAIN_ALLOWROLL = 0x0400;

enum TrainVelocityType_t
{
        TrainVelocity_Instantaneous = 0,
        TrainVelocity_LinearBlend,
        TrainVelocity_EaseInEaseOut,
};

enum TrainOrientationType_t
{
        TrainOrientation_Fixed = 0,
        TrainOrientation_AtPathTracks,
        TrainOrientation_LinearBlend,
        TrainOrientation_EaseInEaseOut,
};

class CFuncTrackTrain : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_FuncTrackTrain" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CFuncTrackTrain, CBaseEntity );
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_SERVERCLASS();

public:
	CFuncTrackTrain();

	void Spawn_Impl( void );
	bool CreateVPhysics_Impl( void );
	void Precache_Impl( void );
	void UpdateOnRemove_Impl();
	void MoveDone_Impl();

	void Blocked_Impl( CBaseEntity *pOther );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	int DrawDebugTextOverlays_Impl();
	void DrawDebugGeometryOverlays_Impl();

	void Next( void );
	void Find( void );
	void NearestPath( void );
	void DeadEnd( void );

	void SetTrack( CPathTrack *track ) { m_ppath = track->Nearest(GetLocalOrigin()); }
	void SetControls( CBaseEntity *pControls );
	bool OnControls_Impl( CBaseEntity *pControls );

	void SoundStop( void );
	void SoundUpdate( void );

	void Start( void );
	void Stop( void );

	bool IsDirForward();
	void SetDirForward( bool bForward );
	void SetSpeed( float flSpeed, bool bAccel = false );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	// Input handlers
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxSpeed", .type = FIELD_FLOAT } ]] void InputSetMaxSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeedDir", .type = FIELD_FLOAT } ]] void InputSetSpeedDir( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeedReal", .type = FIELD_FLOAT } ]] void InputSetSpeedReal( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Stop", .type = FIELD_VOID } ]] void InputStop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Resume", .type = FIELD_VOID } ]] void InputResume( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Reverse", .type = FIELD_VOID } ]] void InputReverse( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartForward", .type = FIELD_VOID } ]] void InputStartForward( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartBackward", .type = FIELD_VOID } ]] void InputStartBackward( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeedDirAccel", .type = FIELD_FLOAT } ]] void InputSetSpeedDirAccel( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "MoveToPathNode", .type = FIELD_STRING } ]] void InputMoveToPathNode( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TeleportToPathNode", .type = FIELD_STRING } ]] void InputTeleportToPathNode( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "LockOrientation", .type = FIELD_VOID } ]] void InputLockOrientation( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UnlockOrientation", .type = FIELD_VOID } ]] void InputUnlockOrientation( inputdata_t &inputdata );
	
	static CFuncTrackTrain *Instance( edict_t *pent );

	DECLARE_DATADESC();

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() | FCAP_DIRECTIONAL_USE | FCAP_USE_ONGROUND; }

	void	OnRestore_Impl( void );

	float GetMaxSpeed() const { return m_maxSpeed; }
	float GetCurrentSpeed() const { return m_flSpeed; }
	float GetDesiredSpeed() const { return m_flDesiredSpeed;}

	bool IsBaseTrain_Impl( void ) const { return true; }
	Vector ScriptGetFuturePosition( float flSeconds, float flMinSpeed );

private:

	void ArriveAtNode( CPathTrack *pNode );
	void FirePassInputs( CPathTrack *pStart, CPathTrack *pEnd, bool forward );

public:

	// UNDONE: Add accessors?
	CPathTrack	*m_ppath;
	[[= ks::reflect::Key{ .name = "wheels" } ]] float		m_length;
	
private:

	TrainVelocityType_t GetTrainVelocityType();
	void UpdateTrainVelocity( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );

	TrainOrientationType_t GetTrainOrientationType();
	void UpdateTrainOrientation( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationAtPathTracks( CPathTrack *pnext, CPathTrack *pNextNext, const Vector &nextPos, float flInterval );
	void UpdateOrientationBlend( TrainOrientationType_t eOrientationType, CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval );
	void DoUpdateOrientation( const QAngle &curAngles, const QAngle &angles, float flInterval );

	void TeleportToPathTrack( CPathTrack *pTeleport );

	Vector		m_controlMins;
	Vector		m_controlMaxs;
	Vector		m_lastBlockPos;				// These are used to build a heuristic decision about being temporarily blocked by physics objects
	int			m_lastBlockTick;			// ^^^^^^^
	float		m_flVolume;
	[[= ks::reflect::Key{ .name = "bank" } ]] float		m_flBank;
	float		m_oldSpeed;
	[[= ks::reflect::Key{ .name = "dmg" } ]] float		m_flBlockDamage;			// Damage to inflict when blocked.
	[[= ks::reflect::Key{ .name = "height" } ]] float		m_height;
	[[= ks::reflect::Key{ .name = "startspeed" } ]] float		m_maxSpeed;
	float		m_dir;

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "MoveSound" } ]] string_t	m_iszSoundMove;				// Looping sound to play while moving. Pitch shifted based on speed.
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "MovePingSound" } ]] string_t	m_iszSoundMovePing;			// Ping sound to play while moving. Interval decreased based on speed.
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "StartSound" } ]] string_t	m_iszSoundStart;			// Sound to play when starting to move.
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "StopSound" } ]] string_t	m_iszSoundStop;				// Sound to play when stopping.

	string_t	m_strPathTarget;			// Destination node 

	[[= ks::reflect::Key{ .name = "MoveSoundMinTime" } ]] float		m_flMoveSoundMinTime;		// The most often to play the move 'ping' sound (used at max speed)
	[[= ks::reflect::Key{ .name = "MoveSoundMaxTime" } ]] float		m_flMoveSoundMaxTime;		// The least often to play the move 'ping' sound (used approaching zero speed)
	float		m_flNextMoveSoundTime;

	[[= ks::reflect::Key{ .name = "MoveSoundMinPitch" } ]] int			m_nMoveSoundMinPitch;		// The sound pitch to approach as we come to a stop
	[[= ks::reflect::Key{ .name = "MoveSoundMaxPitch" } ]] int			m_nMoveSoundMaxPitch;		// The sound pitch to approach as we approach our max speed (actually, it's hardcoded to 1000 in/sec)

	[[= ks::reflect::Key{ .name = "orientationtype" } ]] TrainOrientationType_t m_eOrientationType;
	[[= ks::reflect::Key{ .name = "velocitytype" } ]] TrainVelocityType_t m_eVelocityType;
	bool		m_bSoundPlaying;

	[[= ks::reflect::Key{ .name = "OnStart" } ]] COutputEvent m_OnStart;
	[[= ks::reflect::Key{ .name = "OnNextPoint" } ]] COutputEvent m_OnNext;
	[[= ks::reflect::Key{ .name = "OnArrivedAtDestinationNode" } ]] COutputEvent m_OnArrivedAtDestinationNode;

	[[= ks::reflect::Key{ .name = "ManualSpeedChanges" } ]] bool		m_bManualSpeedChanges;		// set when we want to send entity IO to govern speed and obey our TrainVelocityType_t
	float		m_flDesiredSpeed;			// target speed, when m_bManualSpeedChanges is set
	float		m_flSpeedChangeTime;
	[[= ks::reflect::Key{ .name = "ManualAccelSpeed" } ]] float		m_flAccelSpeed;
	[[= ks::reflect::Key{ .name = "ManualDecelSpeed" } ]] float		m_flDecelSpeed;
	bool		m_bAccelToSpeed;

	float		m_flNextMPSoundTime;
};

class CBaseEntity;
class CBaseFilter;
struct inputdata_t;

using namespace ::ks::reflect;


// DVS TODO: get rid of CBaseToggle
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseTrigger : public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseTrigger" } ]]
	[[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseTrigger, CBaseToggle );
	DECLARE_SERVERCLASS();
public:
	CBaseTrigger();
	
	void Activate_Impl( void );
	void PostClientActive_Impl( void );
	void InitTrigger( void );

	virtual void Enable( void );
	virtual void Disable( void );

	void Spawn_Impl( void );
	void UpdateOnRemove_Impl( void );
	void TouchTest(  void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] virtual void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] virtual void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] virtual void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TouchTest", .type = FIELD_VOID } ]] virtual void InputTouchTest ( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "StartTouch", .type = FIELD_VOID } ]] virtual void InputStartTouch( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EndTouch", .type = FIELD_VOID } ]] virtual void InputEndTouch( inputdata_t &inputdata );

	virtual bool UsesFilter( void ){ return ( m_hFilter.Get() != nullptr ); }
	virtual bool PassesTriggerFilters(CBaseEntity *pOther);
	void StartTouch_Impl(CBaseEntity *pOther);
	void EndTouch_Impl(CBaseEntity *pOther);

	virtual void OnStartTouchAll(CBaseEntity *pOther);
	virtual void OnEndTouchAll(CBaseEntity *pOther);

	bool IsTouching( CBaseEntity *pOther );

	CBaseEntity *GetTouchedEntityOfType( const char *sClassName );

	int	 DrawDebugTextOverlays_Impl(void);

	// by default, triggers don't deal with TraceAttack
	void TraceAttack(CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType) {}

	CNetworkVarForDerived( bool, m_bDisabled, [[= ks::reflect::Key{ .name = "StartDisabled" } ]] );
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t	m_iFilterName;
	CHandle<class CBaseFilter>	m_hFilter;

	const std::vector< EHANDLE > *GetTouchingEntities( void ) const
	{
		return &m_hTouchingEntities;
	}

protected:

	// Outputs
	[[= ks::reflect::Key{ .name = "OnStartTouch" } ]] COutputEvent m_OnStartTouch;
	[[= ks::reflect::Key{ .name = "OnStartTouchAll" } ]] COutputEvent m_OnStartTouchAll;
	[[= ks::reflect::Key{ .name = "OnEndTouch" } ]] COutputEvent m_OnEndTouch;
	[[= ks::reflect::Key{ .name = "OnEndTouchAll" } ]] COutputEvent m_OnEndTouchAll;
	[[= ks::reflect::Key{ .name = "OnTouching" } ]] COutputEvent m_OnTouching;
	[[= ks::reflect::Key{ .name = "OnNotTouching" } ]] COutputEvent m_OnNotTouching;

	// Entities currently being touched by this trigger
	std::vector< EHANDLE >	m_hTouchingEntities;

	DECLARE_DATADESC();

	// True if trigger participates in client side prediction
	CNetworkVar( bool, m_bClientSidePredicted, [[= ks::reflect::Net{} ]] );
};

//-----------------------------------------------------------------------------
// Purpose: Variable sized repeatable trigger.  Must be targeted at one or more entities.
//			If "delay" is set, the trigger waits some time after activating before firing.
//			"wait" : Seconds between triggerings. (.2 default/minimum)
//-----------------------------------------------------------------------------
class CTriggerMultiple : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerMultiple, CBaseTrigger );
public:
	void Spawn_Impl( void );
	void MultiTouch( CBaseEntity *pOther );
	void MultiWaitOver( void );
	virtual void ActivateMultiTrigger(CBaseEntity *pActivator);

	DECLARE_DATADESC();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnTrigger" } ]] COutputEvent m_OnTrigger;
};

// Global list of triggers that care about weapon fire
extern std::vector< CHandle<CTriggerMultiple> >	g_hWeaponFireTriggers;

//------------------------------------------------------------------------------
// Base VPhysics trigger implementation
// NOTE: This uses vphysics to compute touch events.  It doesn't do a per-frame Touch call, so the 
// Entity I/O is different from a regular trigger
//------------------------------------------------------------------------------
inline constexpr auto SF_VPHYSICS_MOTION_MOVEABLE = 0x1000;

class CBaseVPhysicsTrigger : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseVPhysicsTrigger" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseVPhysicsTrigger , CBaseEntity );
	DECLARE_SERVERCLASS();

public:
	DECLARE_DATADESC();

	void Spawn_Impl();
	void UpdateOnRemove_Impl();
	bool CreateVPhysics_Impl();
	void Activate_Impl( void );
	virtual bool PassesTriggerFilters(CBaseEntity *pOther);

	// UNDONE: Pass trigger event in or change Start/EndTouch.  Add ITriggerVPhysics perhaps?
	// BUGBUG: If a player touches two of these, his movement will screw up.
	// BUGBUG: If a player uses crouch/uncrouch it will generate touch events and clear the motioncontroller flag
	void StartTouch_Impl( CBaseEntity *pOther );
	void EndTouch_Impl( CBaseEntity *pOther );

	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	
protected:
	CNetworkVarForDerived( bool, m_bDisabled, [[= ks::reflect::Key{ .name = "StartDisabled" } ]] );
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t					m_iFilterName;
	CHandle<class CBaseFilter>	m_hFilter;
};

//-----------------------------------------------------------------------------
// Purpose: Hurts anything that touches it. If the trigger has a targetname,
//			firing it will toggle state.
//-----------------------------------------------------------------------------
class CTriggerHurt : public CBaseTrigger
{
public:
	CTriggerHurt();

	DECLARE_CLASS( CTriggerHurt, CBaseTrigger );

	void Spawn_Impl( void );
	void RadiationThink( void );
	void HurtThink( void );
	void Touch_Impl( CBaseEntity *pOther );
	void EndTouch_Impl( CBaseEntity *pOther );
	bool HurtEntity( CBaseEntity *pOther, float damage );
	int HurtAllTouchers( float dt );

	void NavThink( void );

	DECLARE_DATADESC();

	float	m_flOriginalDamage;	// Damage as specified by the level designer.
	[[= ks::reflect::Key{ .name = "damage" } ]] [[= ks::reflect::Key{ .name = "SetDamage", .input = true } ]] float	m_flDamage;			// Damage per second.
	[[= ks::reflect::Key{ .name = "damagecap" } ]] float	m_flDamageCap;		// Maximum damage per second.
	float	m_flLastDmgTime;	// Time that we last applied damage.
	float	m_flDmgResetTime;	// For forgiveness, the time to reset the counter that accumulates damage.
	[[= ks::reflect::Key{ .name = "damagetype" } ]] int		m_bitsDamageInflict;	// DMG_ damage type that the door or tigger does
	[[= ks::reflect::Key{ .name = "damagemodel" } ]] int		m_damageModel;
	[[= ks::reflect::Key{ .name = "nodmgforce" } ]] bool	m_bNoDmgForce;		// Should damage from this trigger impart force on what it's hurting

	enum
	{
		DAMAGEMODEL_NORMAL = 0,
		DAMAGEMODEL_DOUBLE_FORGIVENESS,
	};

	// Outputs
	[[= ks::reflect::Key{ .name = "OnHurt" } ]] COutputEvent m_OnHurt;
	[[= ks::reflect::Key{ .name = "OnHurtPlayer" } ]] COutputEvent m_OnHurtPlayer;

	std::vector< EHANDLE >	m_hurtEntities;
};
//
//  Callback trigger
//

class CTriggerCallback : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerCallback, CBaseTrigger );
	DECLARE_DATADESC();
	
	void Spawn_Impl( void );
	void StartTouch_Impl( CBaseEntity *pOther );

	static CTriggerCallback *Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecMins, const Vector &vecMaxs, CBaseEntity *pOwner, void (CBaseEntity::*pfnCallback)(CBaseEntity *) );

private:
	void (CBaseEntity::*m_pfnCallback)(CBaseEntity *);
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTriggerCamera : public CBaseEntity
{
public:
	DECLARE_CLASS( CTriggerCamera, CBaseEntity );
	// script description
	DECLARE_ENT_SCRIPTDESC();

	CTriggerCamera();
	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Enable( void );
	void Disable( void );
	void SetPlayer( CBaseEntity *pPlayer );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void FindAttachment( void );
	void FollowTarget( void );
	void ReturnToEyes( void );
	void MoveViewTo( QAngle vecGoalView );
	void Move( void );
	void StartCameraShot( const char *pszShotType, CBaseEntity *pSceneEntity, CBaseEntity *pActor1, CBaseEntity *pActor2, float duration );
	int ScriptGetFov( void );
	void ScriptSetFov( int iFOV, float rate );

	// Always transmit to clients so they know where to move the view to
	int UpdateTransmitState_Impl();

	DECLARE_DATADESC();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetTarget", .type = FIELD_STRING } ]] void InputSetTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTargetAttachment", .type = FIELD_STRING } ]] void InputSetTargetAttachment( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "ReturnToEyes", .type = FIELD_VOID } ]] void InputReturnToEyes( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TeleportToView", .type = FIELD_VOID } ]] void InputTeleportToView( inputdata_t &inputdata );
	
	[[= ks::reflect::Input{ .name = "SetTrackSpeed", .type = FIELD_FLOAT } ]] void InputSetTrackSpeed( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetPath", .type = FIELD_STRING } ]] void InputSetPath( inputdata_t &inputdata );

private:
	EHANDLE m_hPlayer;
	EHANDLE m_hTarget;

	// used for moving the camera along a path (rail rides)
	CBaseEntity *m_pPath;
	string_t m_sPath;
	float m_flWait;
	float m_flReturnTime;
	float m_flStopTime;
	float m_moveDistance;
	float m_targetSpeed;
	float m_initialSpeed;
	float m_acceleration;
	float m_deceleration;
	int	  m_state;
	Vector m_vecMoveDir;

	[[= ks::reflect::Key{ .name = "fov" } ]] float m_fov;
	[[= ks::reflect::Key{ .name = "fov_rate" } ]] float m_fovSpeed;
	[[= ks::reflect::Key{ .name = "trackspeed" } ]] float m_trackSpeed;

	[[= ks::reflect::Key{ .name = "targetattachment" } ]] string_t m_iszTargetAttachment;
	int	  m_iAttachmentIndex;
	bool  m_bSnapToGoal;

	int   m_nPlayerButtons;
	int m_nOldTakeDamage;

private:
	[[= ks::reflect::Key{ .name = "OnEndFollow" } ]] COutputEvent m_OnEndFollow;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTriggerViewProxy : public CBaseEntity
{
public:
	DECLARE_CLASS( CTriggerViewProxy, CBaseEntity );

	CTriggerViewProxy();
	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Enable( void );
	void Disable( void );
	void SetPlayer( CBaseEntity *pPlayer );
	void TranslateViewToProxy( void );
	void Move( void );

	Vector GetPlayerOffset();

	// Always transmit to clients so they know where to move the view to
	int UpdateTransmitState_Impl();

	DECLARE_DATADESC();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "TeleportPlayerToProxy", .type = FIELD_VOID } ]] void InputTeleportPlayerToProxy( inputdata_t &inputdata );

private:
	EHANDLE m_hPlayer;

	CBaseEntity *m_pProxy;
	[[= ks::reflect::Key{ .name = "proxy" } ]] string_t m_sProxy;
	[[= ks::reflect::Key{ .name = "proxyattachment" } ]] string_t m_sProxyAttachment;
	int m_nParentAttachment;
	int	m_state;
	[[= ks::reflect::Key{ .name = "offsettype" } ]] int	m_nOffsetType;
	Vector m_vecInitialPosition;
	Vector m_vecLastPosition;
	Vector m_vecLastVelocity;
	Vector m_vecInitialOffset;

	[[= ks::reflect::Key{ .name = "tiltfraction" } ]] float m_flTiltFraction;
	float m_flStartTime;

	[[= ks::reflect::Key{ .name = "usefakeacceleration" } ]] bool m_bUseFakeAcceleration;
	[[= ks::reflect::Key{ .name = "skewaccelerationforward" } ]] bool m_bSkewAccelerationForward;
	[[= ks::reflect::Key{ .name = "accelerationscalar" } ]] float m_flAccelerationScalar;

	[[= ks::reflect::Key{ .name = "easeanglestocamera" } ]] bool m_bEaseAnglesToCamera;

	int m_nPlayerButtons;
	int m_nOldTakeDamage;
};

class CEntityGroundContact
{
public:
	int					entindex;
	float				minheight;
	float				maxheight;
};

//#define KEEP_COMMAND_REPREDICTION_COUNT //define to keep a count of how many times we've predicted each command (for debugging)

class CUserCmd
{
public:
	CUserCmd()
	{
		Reset();
	}

	~CUserCmd() { };

	void Reset();

	CUserCmd& operator =( const CUserCmd& src )
	{
		if ( this == &src )
			return *this;

		command_number		= src.command_number;
		tick_count			= src.tick_count;
		viewangles			= src.viewangles;
		aimdirection		= src.aimdirection;
		forwardmove			= src.forwardmove;
		sidemove			= src.sidemove;
		upmove				= src.upmove;
		buttons				= src.buttons;
		impulse				= src.impulse;
		weaponselect		= src.weaponselect;
		weaponsubtype		= src.weaponsubtype;
		random_seed			= src.random_seed;
		server_random_seed	= src.server_random_seed;
		mousedx				= src.mousedx;
		mousedy				= src.mousedy;

		hasbeenpredicted	= src.hasbeenpredicted;

		// TrackIR
		headangles			= src.headangles;
		headoffset			= src.headoffset;
		// TrackIR

		return *this;
	}

	CUserCmd( const CUserCmd& src )
	{
		*this = src;
	}

	CRC32_t GetChecksum( void ) const;

	// For matching server and client commands for debugging
	int		command_number;
	
	// the tick the client created this command
	int		tick_count;
	
	// Player instantaneous view angles.
	QAngle	viewangles;
	Vector	aimdirection;	// For pointing devices. 
	// Intended velocities
	//	forward velocity.
	float	forwardmove;   
	//  sideways velocity.
	float	sidemove;      
	//  upward velocity.
	float	upmove;         
	// Attack button states
	int		buttons;		
	// Impulse command issued.
	byte    impulse;        
	// Current weapon id
	int		weaponselect;	
	int		weaponsubtype;

	int		random_seed;	// For shared random functions

	int		server_random_seed; // Only the server populates this seed

	short	mousedx;		// mouse accum in x from create move
	short	mousedy;		// mouse accum in y from create move

	// Client only, tracks whether we've predicted this command at least once
	bool	hasbeenpredicted;

	// Back channel to communicate IK state

	// TrackIR
	QAngle headangles;
	Vector headoffset;
	// TrackIR

};

void ReadUsercmd( bf_read *buf, CUserCmd *move, CUserCmd *from );
void WriteUsercmd( bf_write *buf, const CUserCmd *to, const CUserCmd *from );

// eliminates a conflict with TYPE_BOOL in OSX
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

//-----------------------------------------------------------------------------
class IUserMessageBinder;

//-----------------------------------------------------------------------------
// Purpose: Interface for registering and dispatching usermessages
// Shred code creates same ordered list on client/server
//-----------------------------------------------------------------------------
class CUserMessages
{
public:

	CUserMessages();
	~CUserMessages();

	bool DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg );
	void BindMessage( IUserMessageBinder *pMessageBinder );
	bool UnbindMessage( IUserMessageBinder *pMessageBinder );

private:
	typedef std::vector< IUserMessageBinder * > UserMessageBinderVec_t;
	typedef std::map< int, std::inplace_vector< UserMessageBinderVec_t, MAX_SPLITSCREEN_PLAYERS > > UserMessageHandlerMap_t;
	UserMessageHandlerMap_t m_UserMessageBinderMap;
};

extern CUserMessages *UserMessages();

// The server never binds user messages; shared code only declares binders.
class CUserMessageBinder
{
public:
	void Unbind() {}
	bool IsBound() const { return false; }
};
