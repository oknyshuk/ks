//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ROPE_H
#define ROPE_H

#include "reflect_annotations.h"
#include "rope_shared.h"
#include "baseentity_shared.h"
#include "videocfg/videocfg.h"
#ifdef _WIN32
#pragma once
#endif


#include "baseentity.h"

#include "positionwatcher.h"

class [[= ks::reflect::NetTable{ .name = "DT_RopeKeyframe", .base = false } ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR }>{} ]]
      [[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
      [[= ks::reflect::From<"m_iParentAttachment", ks::reflect::Net{ .bits = NUM_PARENTATTACHMENT_BITS, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_nMinCPULevel", ks::reflect::Net{ .bits = CPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_nMaxCPULevel", ks::reflect::Net{ .bits = CPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_nMinGPULevel", ks::reflect::Net{ .bits = GPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_nMaxGPULevel", ks::reflect::Net{ .bits = GPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
      CRopeKeyframe : public CBaseEntity, public IPositionWatcher
{
	DECLARE_CLASS( CRopeKeyframe, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CRopeKeyframe();
	virtual			~CRopeKeyframe();

	// Create a rope and attach it to two entities.
	// Attachment points on the entities are optional.
	static CRopeKeyframe* Create(
		CBaseEntity *pStartEnt,
		CBaseEntity *pEndEnt,
		int iStartAttachment=0,
		int iEndAttachment=0,
		int ropeWidth = 2,
		const char *pMaterialName = "cable/cable.vmt",		// Note: whoever creates the rope must
															// use PrecacheModel for whatever material
															// it specifies here.
		int numSegments = 5,
		const char *pClassName = "keyframe_rope"
		);

	static CRopeKeyframe* CreateWithSecondPointDetached(
		CBaseEntity *pStartEnt,
		int iStartAttachment = 0,	// must be 0 if you don't want to use a specific model attachment.
		int ropeLength = 20,
		int ropeWidth = 2,
		const char *pMaterialName = "cable/cable.vmt",		// Note: whoever creates the rope
															// use PrecacheModel for whatever material
															// it specifies here.
		int numSegments = 5,
		bool bInitialHang = false,
		const char *pClassName = "keyframe_rope"
		);

	bool		SetupHangDistance( float flHangDist );
	void		ActivateStartDirectionConstraints( bool bEnable );
	void		ActivateEndDirectionConstraints( bool bEnable );

	
	// Shakes all ropes near vCenter. The higher flMagnitude is, the larger the shake will be.
	static void ShakeRopes( const Vector &vCenter, float flRadius, float flMagnitude );
	static void PrecacheShakeRopes();

// CBaseEntity overrides.
public:
	
	// don't cross transitions
	virtual int		ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Activate();
	virtual void	Precache();
	virtual void	Spawn( void );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual bool	KeyValue( const char *szKeyName, const char *szValue );

	void			PropagateForce(CBaseEntity *pActivator, CBaseEntity *pCaller, CBaseEntity *pFirstLink, float x, float y, float z);

	// Once-off length recalculation
	void			RecalculateLength( void );

	// Kill myself when I next come to rest
	void			DieAtNextRest( void );

	virtual int		UpdateTransmitState(void);
	virtual void	SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	virtual void	SetParent( CBaseEntity *pParentEntity, int iAttachment );

// Input functions.
public:

	[[= ks::reflect::Input{ .name = "SetScrollSpeed", .type = FIELD_FLOAT } ]] void InputSetScrollSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetForce", .type = FIELD_VECTOR } ]] void InputSetForce( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Break", .type = FIELD_VOID } ]] void InputBreak( inputdata_t &inputdata );

public:

	bool			Break( void );
	void			DetachPoint( int iPoint );
	
	void			EndpointsChanged();

	// By default, ropes don't collide with the world. Call this to enable it.
	void			EnableCollision();

	// Toggle wind.
	void			EnableWind( bool bEnable );

	CBaseEntity*	GetStartPoint() { return m_hStartPoint.Get(); }
	int				GetStartAttachment() { return m_iStartAttachment; };

	CBaseEntity*	GetEndPoint() { return m_hEndPoint.Get(); }
	int				GetEndAttachment() { return m_iEndAttachment; };

	void			SetStartPoint( CBaseEntity *pStartPoint, int attachment = 0 );
	void			SetEndPoint( CBaseEntity *pEndPoint, int attachment = 0 );

	// See ROPE_PLAYER_WPN_ATTACH for info.
	void			EnablePlayerWeaponAttach( bool bAttach );


	// IPositionWatcher
	virtual void NotifyPositionChanged( CBaseEntity *pEntity );

private:
	// Unless this is called during initialization, the caller should have done
	// PrecacheModel on whatever material they specify in here.
	void			SetMaterial( const char *pName );

protected:
	void			SetAttachmentPoint( CBaseHandle &hOutEnt, short &iOutAttachment, CBaseEntity *pEnt, int iAttachment );

	// This is normally called by Activate but if you create the rope at runtime,
	// you must call it after you have setup its variables.
	virtual void	Init();

private:
	// These work just like the client-side versions.
	bool			GetEndPointPos2( CBaseEntity *pEnt, int iAttachment, Vector &v );
	bool			GetEndPointPos( int iPt, Vector &v );

	void			UpdateBBox( bool bForceRelink );


public:

	CNetworkVar( int, m_RopeFlags, [[= ks::reflect::Net{ .bits = ROPE_NUMFLAGS, .flags = SPROP_UNSIGNED } ]] );		// Combination of ROPE_ defines in rope_shared.h
	
	[[= ks::reflect::Key{ .name = "NextKey" } ]] string_t	m_iNextLinkName;
	CNetworkVar( int, m_Slack, [[= ks::reflect::Net{ .bits = 13 } ]] [[= ks::reflect::Key{ .name = "Slack" } ]] );
	CNetworkVar( float, m_Width, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Width" } ]] );
	CNetworkVar( float, m_TextureScale, [[= ks::reflect::Net{ .bits = 10, .low = 0.1f, .high = 10.0f } ]] [[= ks::reflect::Key{ .name = "TextureScale" } ]] );
	CNetworkVar( int, m_nSegments, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );		// Number of segments.
	CNetworkVar( bool, m_bConstrainBetweenEndpoints, [[= ks::reflect::Net{} ]] );

	string_t m_strRopeMaterialModel;
	CNetworkVar( int, m_iRopeMaterialModelIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );	// Index of sprite model with the rope's material.
	
	// Number of subdivisions in between segments.
	CNetworkVar( int, m_Subdiv, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "Subdiv" } ]] );
	
	// Used simply to wake up rope on the client side if it has gone to sleep
	CNetworkVar( unsigned char, m_nChangeCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );

	//EHANDLE		m_hNextLink;
	
	CNetworkVar( int, m_RopeLength, [[= ks::reflect::Net{ .bits = 15 } ]] );	// Rope length at startup, used to calculate tension.

	CNetworkVar( int, m_fLockedPoints, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );

	bool		m_bCreatedFromMapFile; // set to false when creating at runtime

	CNetworkVar( float, m_flScrollSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "ScrollSpeed" } ]] );

	CNetworkVar( int, m_iDefaultRopeMaterialModelIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );

private:
	// Used to detect changes.
	bool		m_bStartPointValid;
	bool		m_bEndPointValid;
	
	CNetworkHandle( CBaseEntity, m_hStartPoint, [[= ks::reflect::Net{} ]] );		// StartPoint/EndPoint are entities
	CNetworkHandle( CBaseEntity, m_hEndPoint, [[= ks::reflect::Net{} ]] );
	CNetworkVar( short, m_iStartAttachment, [[= ks::reflect::Net{ .bits = 5 } ]] );	// StartAttachment/EndAttachment are attachment points.
	CNetworkVar( short, m_iEndAttachment, [[= ks::reflect::Net{ .bits = 5 } ]] );
};


#endif // ROPE_H
