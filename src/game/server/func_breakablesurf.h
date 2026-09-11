//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FUNC_BREAKABLESURF_H
#define FUNC_BREAKABLESURF_H

#include "reflect_annotations.h"


#define MAX_NUM_PANELS 16	//Must match client

#include "func_break.h"

//#############################################################################
//  > CWindowPane
//
//  A piece that falls out of the window
//#############################################################################
class CWindowPane : public CBaseAnimating
{
public:
	DECLARE_CLASS( CWindowPane, CBaseAnimating );

	static CWindowPane* CreateWindowPane(  const Vector &vecOrigin, const QAngle &vecAngles );

	void			Spawn( void );
	void			Precache( void );
	void			PaneTouch( CBaseEntity *pOther );
	void			Die( void );
};

//#############################################################################
//  > CBreakableSurface
//
//  A breakable surface
//#############################################################################
class [[= ks::reflect::NetTable{ .name = "DT_BreakableSurface" } ]]
      CBreakableSurface : public CBreakable
{
	DECLARE_CLASS( CBreakableSurface, CBreakable );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nNumWide, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nNumHigh, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flPanelWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flPanelHeight, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVector( m_vNormal, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_vCorner, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( bool, m_bIsBroken, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( ShatterSurface_t, m_nSurfaceType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "surfacetype" } ]] );
	int					m_nNumBrokenPanes;
	float				m_flSupport[MAX_NUM_PANELS][MAX_NUM_PANELS]; //UNDONE: allocate dynamically?

	[[= ks::reflect::Key{ .name = "fragility" } ]] int					m_nFragility;
	[[= ks::reflect::Key{ .name = "lowerleft" } ]] Vector				m_vLLVertex;
	[[= ks::reflect::Key{ .name = "upperleft" } ]] Vector				m_vULVertex;
	[[= ks::reflect::Key{ .name = "lowerright" } ]] Vector				m_vLRVertex;
	[[= ks::reflect::Key{ .name = "upperright" } ]] Vector				m_vURVertex;
	[[= ks::reflect::Key{ .name = "error" } ]] int					m_nQuadError;

	void			SurfaceTouch( CBaseEntity *pOther );
	void			PanePos(const Vector &vPos, float *flWidth, float *flHeight);

	bool			IsBroken(int nWidth, int nHeight);
	void			SetSupport(int w, int h, float support);

	float			GetSupport(int nWidth, int nHeight);
	float			RecalcSupport(int nWidth, int nHeight);

	void			BreakPane(int nWidth, int nHeight);
	void			DropPane(int nWidth, int nHeight);
	bool			ShatterPane(int nWidth, int nHeight, const Vector &force, const Vector &vForcePos);
	void			BreakAllPanes(void);

	void			CreateShards(const Vector &vBreakPos, const QAngle &vAngles,
								 const Vector &vForce,	  const Vector &vForcePos,
								 float flWidth,			  float flHeight,
								 int   nShardSize);

	void			Spawn(void);
	void			Precache(void);
	void			Die( CBaseEntity *pBreaker, const Vector &vAttackDir );
	void			BreakThink(void);
	void			Event_Killed( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType );
	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	int				OnTakeDamage( const CTakeDamageInfo &info );
	[[= ks::reflect::Input{ .name = "Shatter", .type = FIELD_VECTOR } ]] void			InputShatter( inputdata_t &inputdata );
	void			VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
private:
	// One bit per pane
	CNetworkArray( bool, m_RawPanelBitVec, MAX_NUM_PANELS * MAX_NUM_PANELS, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

#endif // FUNC_BREAKABLESURF_H

