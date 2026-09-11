//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef EFFECT_DISPATCH_DATA_H
#define EFFECT_DISPATCH_DATA_H
#ifdef _WIN32
#pragma once
#endif


#include "const.h"
#include "reflect_annotations.h"

#ifdef CLIENT_DLL

	#include "dt_recv.h"
	#include "client_class.h"
	#include "recvproxy.h"

	EXTERN_RECV_TABLE( DT_EffectData );

	// Serves the "entindex" prop, which is named by a string rather than by a member and writes
	// m_hEntity itself. An annotation on the class needs it visible here, so it is not static.
	void RecvProxy_EntIndex( const CRecvProxyData *pData, void *pStruct, void *pOut );

#else

	#include "dt_send.h"
	#include "server_class.h"
	#include "sendproxy.h"

	EXTERN_SEND_TABLE( DT_EffectData );

#endif

#define EFFECTDATA_SERVER_IGNOREPREDICTIONCULL 0x4
// NOTE: These flags are specifically *not* networked; so it's placed above the max effect flag bits
#define EFFECTDATA_NO_RECORD 0x80000000

#define MAX_EFFECT_FLAG_BITS 8

// Note, must match common/qlimits.h!!!
// Must have this value in sync(-1) with qlimits.h and const.h
#define MAX_MODEL_INDEX_BITS	12

#define MAX_EFFECT_DISPATCH_STRING_BITS	10
#define MAX_EFFECT_DISPATCH_STRINGS		( 1 << MAX_EFFECT_DISPATCH_STRING_BITS )

// This is the class that holds whatever data we're sending down to the client to make the effect.
class [[= ks::reflect::NetTable{ .name = "DT_EffectData", .base = false } ]]
      // Six flat props reached through a dotted path: SENDINFO_NOCHECK( m_vOrigin.x ) and
      // RECVINFO( m_vOrigin.x ) both name the prop "m_vOrigin.x" in this table, not a sub-table.
      [[= ks::reflect::From<"m_vOrigin.x", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
      [[= ks::reflect::From<"m_vOrigin.y", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
      [[= ks::reflect::From<"m_vOrigin.z", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
      [[= ks::reflect::From<"m_vStart.x", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
      [[= ks::reflect::From<"m_vStart.y", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
      [[= ks::reflect::From<"m_vStart.z", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
#ifdef CLIENT_DLL
      // "entindex" is not a member: the legacy prop is RecvPropInt( "entindex", 0, ... ) whose
      // proxy writes m_hEntity itself, so it is anchored on m_vOrigin, the member at offset 0.
      [[= ks::reflect::From<"m_vOrigin",
            ks::reflect::Net{ .enc = ks::reflect::ENC_INT, .wire = "entindex" },
            RecvProxy_EntIndex>{} ]]
#endif
      CEffectData
{
public:
	Vector m_vOrigin;
	Vector m_vStart;
	[[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NORMAL } ]] Vector m_vNormal;
	// Sent with SendPropQAngles, received with RecvPropQAngles, which is RecvPropVector.
	[[= ks::reflect::Net{ .bits = 7 } ]] QAngle m_vAngles;
	[[= ks::reflect::Net{ .bits = MAX_EFFECT_FLAG_BITS, .flags = SPROP_UNSIGNED } ]] int		m_fFlags;
#ifdef CLIENT_DLL
	ClientEntityHandle_t m_hEntity;
#else
	[[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED, .wire = "entindex" } ]] int		m_nEntIndex;
#endif
	[[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] float	m_flScale;
	[[= ks::reflect::Net{ .bits = 12, .low = 0.0f, .high = 1023.0f, .flags = SPROP_ROUNDDOWN } ]] float	m_flMagnitude;
	[[= ks::reflect::Net{ .bits = 10, .low = 0.0f, .high = 1023.0f, .flags = SPROP_ROUNDDOWN } ]] float	m_flRadius;
	[[= ks::reflect::Net{ .bits = 5 } ]] int		m_nAttachmentIndex;
	// Send/RecvPropIntWithMinusOneFlag are Send/RecvPropInt wrappers whose whole effect is the
	// default proxy, and this call site overrides it -- with a different function on each side.
#ifdef CLIENT_DLL
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]]
	[[= ks::reflect::Proxy<RecvProxy_ShortSubOne, ks::reflect::WIRE_RECV>{} ]]
#else
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]]
	[[= ks::reflect::Proxy<SendProxy_ShortAddOne, ks::reflect::WIRE_SEND>{} ]]
#endif
	short	m_nSurfaceProp;

	// Some TF2 specific things
	[[= ks::reflect::Net{ .bits = MAX_MODEL_INDEX_BITS, .flags = SPROP_UNSIGNED } ]] int		m_nMaterial;
	[[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] int		m_nDamageType;
	[[= ks::reflect::Net{ .bits = 11, .flags = SPROP_UNSIGNED } ]] int		m_nHitBox;

	[[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] int		m_nOtherEntIndex;
	
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] unsigned char	m_nColor;

	[[= ks::reflect::Net{} ]] bool	m_bPositionsAreRelativeToEntity;

// Don't mess with stuff below here. DispatchEffect handles all of this.
public:
	CEffectData()
	{
		m_vOrigin.Init();
		m_vStart.Init();
		m_vNormal.Init();
		m_vAngles.Init();

		m_fFlags = 0;
#ifdef CLIENT_DLL
		m_hEntity = INVALID_EHANDLE;
#else
		m_nEntIndex = 0;
#endif
		m_flScale = 1.f;
		m_nAttachmentIndex = 0;
		m_nSurfaceProp = 0;

		m_flMagnitude = 0.0f;
		m_flRadius = 0.0f;

		m_nMaterial = 0;
		m_nDamageType = 0;
		m_nHitBox = 0;

		m_nColor = 0;

		m_nOtherEntIndex = 0;

		m_bPositionsAreRelativeToEntity = false;
	}

	int GetEffectNameIndex() { return m_iEffectName; }

#ifdef CLIENT_DLL
	IClientRenderable *GetRenderable() const;
	C_BaseEntity *GetEntity() const;
	int entindex() const;
#endif

private:

	#ifdef CLIENT_DLL
		DECLARE_CLIENTCLASS_NOBASE()
	#else
		DECLARE_SERVERCLASS_NOBASE()
	#endif

	[[= ks::reflect::Net{ .bits = MAX_EFFECT_DISPATCH_STRING_BITS, .flags = SPROP_UNSIGNED } ]] int m_iEffectName;	// Entry in the EffectDispatch network string table. The is automatically handled by DispatchEffect().
};


#ifdef CLIENT_DLL
bool SuppressingParticleEffects();
void SuppressParticleEffects( bool bSuppress );
#endif

#endif // EFFECT_DISPATCH_DATA_H
