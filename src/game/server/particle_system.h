//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "networkstringtable_gamedll.h"

void SendProxy_Angles( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ParticleSystem", .base = false } ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .low = 0.0f, .high = HIGH_DEFAULT, .flags = SPROP_COORD|SPROP_CHANGES_OFTEN, .enc = ks::reflect::ENC_VECTOR }, SendProxy_Origin>{} ]]
      [[= ks::reflect::From<"m_fEffects", ks::reflect::Net{ .bits = EF_MAX_BITS, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_hOwnerEntity", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
      [[= ks::reflect::From<"m_iParentAttachment", ks::reflect::Net{ .bits = NUM_PARENTATTACHMENT_BITS, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 13, .flags = SPROP_CHANGES_OFTEN, .enc = ks::reflect::ENC_QANGLES }, SendProxy_Angles>{} ]]
      CParticleSystem : public CBaseEntity
{
	DECLARE_CLASS( CParticleSystem, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CParticleSystem( void );

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual int  UpdateTransmitState(void);
	virtual int	 ObjectCaps( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );

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

#endif // PARTICLE_SYSTEM_H
