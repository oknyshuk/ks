//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BEAM_H
#define BEAM_H

#include "baseentity_shared.h"
#include "baseplayer_shared.h"
#if !defined( CLIENT_DLL )
#include "entityoutput.h"
#endif

#include "beam_flags.h"
// dt_recv.h for CRecvProxyData, which the recv proxies below are declared with; worldsize.h for the
// MIN/MAX_COORD_INTEGER range DT_Beam sends m_vecOrigin in.
#include "dt_recv.h"
#include "worldsize.h"

#define MAX_BEAM_WIDTH			102.3f
#define MAX_BEAM_SCROLLSPEED	100.0f
#define MAX_BEAM_NOISEAMPLITUDE		64

#define SF_BEAM_STARTON			0x0001
#define SF_BEAM_TOGGLE			0x0002
#define SF_BEAM_RANDOM			0x0004
#define SF_BEAM_RING			0x0008
#define SF_BEAM_SPARKSTART		0x0010
#define SF_BEAM_SPARKEND		0x0020
#define SF_BEAM_DECALS			0x0040
#define SF_BEAM_SHADEIN			0x0080
#define SF_BEAM_SHADEOUT		0x0100
#define	SF_BEAM_TAPEROUT		0x0200	// Tapers to zero
#define SF_BEAM_TEMPORARY		0x8000

#define ATTACHMENT_INDEX_BITS	5
#define ATTACHMENT_INDEX_MASK	((1 << ATTACHMENT_INDEX_BITS) - 1)

#if defined( CLIENT_DLL )
#define CBeam C_Beam
#include "c_pixel_visibility.h"
#endif

// A member annotation lives inside a CNetworkVar() argument list, where a preprocessor directive is
// not allowed, so m_fSpeed's recv proxy has to be declared on both sides. It is defined in the
// client half of beam_shared.cpp.
void RecvProxy_Beam_ScrollSpeed( const CRecvProxyData *pData, void *pStruct, void *pOut );
#if defined( CLIENT_DLL )
void RecvProxy_ClrRender( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_IntToMoveParent( const CRecvProxyData *pData, void *pStruct, void *pOut );
#endif

// I've seen CBeams glitter in the dark near Tannhauser gate...
class
      [[= ks::reflect::NetTable{ .name = "DT_Beam", .base = false } ]]
      [[= ks::reflect::From<"m_nRenderFX", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_nRenderMode", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_nModelIndex", ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX }>{} ]]
// The remaining inherited props differ by side in the member they read and in their proxy, and a
// class-level annotation may be guarded where a member-level one may not.
#if !defined( CLIENT_DLL )
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED | SPROP_CHANGES_OFTEN }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .flags = SPROP_CHANGES_OFTEN, .enc = ks::reflect::ENC_VECTOR }>{} ]]
      [[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
#else
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{}, RecvProxy_ClrRender>{} ]]
      [[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
      [[= ks::reflect::From<"m_hNetworkMoveParent", ks::reflect::Net{ .wire = "moveparent" }, RecvProxy_IntToMoveParent>{} ]]
#endif
      [[= ks::reflect::PredFrom<"m_nRenderFX", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
      [[= ks::reflect::PredFrom<"m_nRenderMode", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
      [[= ks::reflect::PredFrom<"m_clrRender", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } >{} ]]
      [[= ks::reflect::PredFrom<"m_nModelIndex", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX } >{} ]]
      [[= ks::reflect::PredFrom<"m_vecOrigin", ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } >{} ]]
      CBeam : public CBaseEntity
{
	DECLARE_CLASS( CBeam, CBaseEntity );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	CBeam();

	virtual void SetModel( const char *szModelName );

	void	Spawn( void );
	void	Precache( void );
#if !defined( CLIENT_DLL )
	int		ObjectCaps( void );
	void	SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	int		UpdateTransmitState( void );
	int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
#endif

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

#if defined( CLIENT_DLL )
// IClientEntity overrides.
public:
	virtual int			DrawModel( int flags, const RenderableInstance_t &instance );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType();
	virtual void		OnDataChanged( DataUpdateType_t updateType );

	virtual bool		OnPredictedEntityRemove( bool isbeingremoved, C_BaseEntity *predicted );

	// Add beam to visible entities list?
	virtual bool		Simulate( void );
	virtual bool		ShouldReceiveProjectedTextures( int flags )
	{
		return false;
	}

	virtual void					GetToolRecordingState( KeyValues *msg );
	void RestoreToToolRecordedState( KeyValues *pKV );

// Beam Data Elements
private:
	// Computes the bounding box of a beam local to the origin of the beam
	void ComputeBounds( Vector& mins, Vector& maxs );

	friend void RecvProxy_Beam_ScrollSpeed( const CRecvProxyData *pData, void *pStruct, void *pOut );
	friend class CViewRenderBeams;

#endif

protected:
	CNetworkVar( float, m_flFrameRate, [[= ks::reflect::Net{ .bits = 10, .low = -25.0f, .high = 25.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( float, m_flHDRColorScale, [[= ks::reflect::Net{ .bits = 0, .low = 0.0f, .high = 100.0f, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "HDRColorScale" } ]] );
	float		m_flFireTime;
	[[= ks::reflect::Key{ .name = "damage" } ]] float		m_flDamage;			// Damage per second to touchers.
	CNetworkVar( int, m_nNumBeamEnts, [[= ks::reflect::Net{ .bits = 5, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
#if defined( CLIENT_DLL )
	pixelvis_handle_t	m_queryHandleHalo;
#endif

private:
#if !defined( CLIENT_DLL )
	[[= ks::reflect::Input{ .name = "Noise", .type = FIELD_FLOAT } ]] void InputNoise( inputdata_t &inputdata );
 	[[= ks::reflect::Input{ .name = "Width", .type = FIELD_FLOAT } ]] void InputWidth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorRedValue", .type = FIELD_FLOAT } ]] void InputColorRedValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorBlueValue", .type = FIELD_FLOAT } ]] void InputColorBlueValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ColorGreenValue", .type = FIELD_FLOAT } ]] void InputColorGreenValue( inputdata_t &inputdata );
#endif

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
	CNetworkVar( float, m_fSpeed, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = MAX_BEAM_SCROLLSPEED, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Proxy<RecvProxy_Beam_ScrollSpeed, ks::reflect::WIRE_RECV>{} ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] [[= ks::reflect::Key{ .name = "ScrollSpeed", .input = true } ]] );
	CNetworkVar( float, m_flFrame, [[= ks::reflect::Net{ .bits = 20, .low = 0.0f, .high = 256.0f, .flags = SPROP_ROUNDDOWN | SPROP_CHANGES_OFTEN } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );
	CNetworkVar( BeamClipStyle_t, m_nClipStyle, [[= ks::reflect::Net{ .bits = kBEAMCLIPSTYLE_NUMBITS + 1, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVector( m_vecEndPos, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = 0.125f } ]] );

	EHANDLE		m_hEndEntity;

#if !defined( CLIENT_DLL )
	[[= ks::reflect::Key{ .name = "dissolvetype" } ]] int			m_nDissolveType;
#endif
};

#if !defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
inline int CBeam::ObjectCaps( void )
{ 
	int flags = 0;
	if ( HasSpawnFlags( SF_BEAM_TEMPORARY ) )
		flags = FCAP_DONT_SAVE;
	return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | flags; 
}
#endif

inline void	CBeam::SetFireTime( float flFireTime )		
{ 
	m_flFireTime = flFireTime; 
}

//-----------------------------------------------------------------------------
// NOTE: Start + End Pos are specified in *relative* coordinates 
//-----------------------------------------------------------------------------
inline void CBeam::SetStartPos( const Vector &pos ) 
{ 
#if defined( CLIENT_DLL )
	SetNetworkOrigin( pos );
#endif
	SetLocalOrigin( pos );
}

inline void CBeam::SetEndPos( const Vector &pos ) 
{ 
	m_vecEndPos = pos; 
}
	 
 // center point of beam
inline const Vector &CBeam::WorldSpaceCenter( void ) const 
{
	Vector &vecResult = AllocTempVector();
	VectorAdd( GetAbsStartPos(), GetAbsEndPos(), vecResult );
	vecResult *= 0.5f;
	return vecResult;
}

inline void CBeam::SetStartAttachment( int attachment )	
{
	Assert( (attachment & ~ATTACHMENT_INDEX_MASK) == 0 );
	m_nAttachIndex.Set( 0, attachment );
}

inline void CBeam::SetEndAttachment( int attachment )		
{ 
	Assert( (attachment & ~ATTACHMENT_INDEX_MASK) == 0 );
	m_nAttachIndex.Set( m_nNumBeamEnts-1, attachment );
}

inline void CBeam::SetTexture( int spriteIndex )		
{ 
	SetModelIndex( spriteIndex ); 
}

inline void CBeam::SetHaloTexture( int spriteIndex )	
{ 
	m_nHaloIndex = spriteIndex; 
}

inline void CBeam::SetHaloScale( float haloScale )		
{ 
	m_fHaloScale = haloScale; 
}

inline void CBeam::SetWidth( float width )				
{
	Assert( width <= MAX_BEAM_WIDTH );
	m_fWidth = MIN( MAX_BEAM_WIDTH, width );
}

inline void CBeam::SetEndWidth( float endWidth )		
{ 
	Assert( endWidth <= MAX_BEAM_WIDTH );
	m_fEndWidth	= MIN( MAX_BEAM_WIDTH, endWidth );
}

inline void CBeam::SetFadeLength( float fadeLength )	
{ 
	m_fFadeLength = fadeLength; 
}

inline void CBeam::SetNoise( float amplitude )			
{ 
	m_fAmplitude = amplitude; 
}

inline void CBeam::SetColor( int r, int g, int b )		
{ 
	SetRenderColor( r, g, b );
}

inline void CBeam::SetBrightness( int brightness )		
{ 
	SetRenderAlpha( brightness ); 
}

inline void CBeam::SetFrame( float frame )				
{ 
	m_fStartFrame = frame; 
}

inline void CBeam::SetScrollRate( int speed )			
{ 
	m_fSpeed = speed; 
}

inline CBaseEntity* CBeam::GetStartEntityPtr( void ) const 
{ 
	return m_hAttachEntity[0].Get(); 
}

inline int CBeam::GetStartEntity( void ) const 
{ 
	CBaseEntity *pEntity = m_hAttachEntity[0].Get();
	return pEntity ? pEntity->entindex() : 0; 
}

inline CBaseEntity* CBeam::GetEndEntityPtr( void ) const 
{ 
	return m_hAttachEntity[1].Get(); 
}

inline int CBeam::GetEndEntity( void ) const	
{ 
	CBaseEntity *pEntity = m_hAttachEntity[m_nNumBeamEnts-1].Get();
	return pEntity ? pEntity->entindex() : 0; 
}

inline int CBeam::GetStartAttachment() const
{
	return m_nAttachIndex[0] & ATTACHMENT_INDEX_MASK;
}

inline int CBeam::GetEndAttachment() const
{
	return m_nAttachIndex[m_nNumBeamEnts-1] & ATTACHMENT_INDEX_MASK;
}

inline int CBeam::GetTexture( void )		
{ 
	return GetModelIndex(); 
}

inline int CBeam::GetHaloTexture( void ) const
{
	return m_nHaloIndex;
}

inline float CBeam::GetWidth( void ) const		
{
	return m_fWidth; 
}

inline float CBeam::GetEndWidth( void ) const	
{ 
	return m_fEndWidth; 
}

inline float CBeam::GetFadeLength( void ) const	
{ 
	return m_fFadeLength; 
}

inline float CBeam::GetNoise( void ) const		
{ 
	return m_fAmplitude; 
}

inline float CBeam::GetHaloScale( void ) const
{
	return m_fHaloScale;
}

inline int CBeam::GetBrightness( void ) const	
{ 
	return GetRenderAlpha();
}

inline float CBeam::GetFrame( void ) const		
{ 
	return m_fStartFrame; 
}

inline float CBeam::GetScrollRate( void ) const	
{
	return m_fSpeed; 
}

inline float CBeam::GetHDRColorScale( void ) const
{
	return m_flHDRColorScale;
}

inline void CBeam::LiveForTime( float time ) 
{ 
	SetThink(&CBeam::SUB_Remove); 
	SetNextThink( gpGlobals->curtime + time ); 
}

inline void	CBeam::BeamDamageInstant( trace_t *ptr, float damage ) 
{ 
	m_flDamage = damage; 
	m_flFireTime = gpGlobals->curtime - 1;
	BeamDamage(ptr); 
}

bool IsStaticPointEntity( CBaseEntity *pEnt );

// Macro to wrap creation
#define BEAM_CREATE_PREDICTABLE( name, width, player ) \
	CBeam::BeamCreatePredictable( __FILE__, __LINE__, false, name, width, player )

#define BEAM_CREATE_PREDICTABLE_PERSIST( name, width, player ) \
	CBeam::BeamCreatePredictable( __FILE__, __LINE__, true, name, width, player )

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


#endif // BEAM_H
