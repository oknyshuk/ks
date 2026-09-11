//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSOBJ_H
#define PHYSOBJ_H

#include "reflect_annotations.h"

#ifndef PHYSICS_H
#include "physics.h"
#endif

#include "entityoutput.h"
#include "func_break.h"
#include "player_pickup.h"

// ---------------------------------------------------------------------
//
// CPhysBox -- physically simulated brush rectangular solid
//
// ---------------------------------------------------------------------
// Physbox Spawnflags. Start at 0x01000 to avoid collision with CBreakable's
#define SF_PHYSBOX_ASLEEP					0x01000
#define SF_PHYSBOX_IGNOREUSE				0x02000
#define SF_PHYSBOX_DEBRIS					0x04000
#define SF_PHYSBOX_MOTIONDISABLED			0x08000
#define SF_PHYSBOX_USEPREFERRED				0x10000
#define SF_PHYSBOX_ENABLE_ON_PHYSCANNON		0x20000
#define SF_PHYSBOX_NO_ROTORWASH_PUSH		0x40000		// The rotorwash doesn't push these
#define SF_PHYSBOX_ENABLE_PICKUP_OUTPUT		0x80000
#define SF_PHYSBOX_ALWAYS_PICK_UP		    0x100000		// Physcannon can always pick this up, no matter what mass or constraints may apply.
#define SF_PHYSBOX_NEVER_PICK_UP			0x200000		// Physcannon will never be able to pick this up.
#define SF_PHYSBOX_NEVER_PUNT				0x400000		// Physcannon will never be able to punt this object.
#define SF_PHYSBOX_PREVENT_PLAYER_TOUCH_ENABLE 0x800000		// If set, the player will not cause the object to enable its motion when bumped into

// UNDONE: Hook collisions into the physics system to generate touch functions and take damage on falls
// UNDONE: Base class PhysBrush
class [[= ks::reflect::NetTable{ .name = "DT_PhysBox" } ]]
      CPhysBox : public CBreakable
{
DECLARE_CLASS( CPhysBox, CBreakable );

public:
	DECLARE_SERVERCLASS();

	void	Spawn ( void );
	bool	CreateVPhysics();
	void	Move( const Vector &force );
	virtual int ObjectCaps();
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	virtual int DrawDebugTextOverlays(void);

	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	int		OnTakeDamage( const CTakeDamageInfo &info );
	void		 EnableMotion( void );

	bool CanBePickedUpByPhyscannon();

	// IPlayerPickupVPhysics
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );

	bool		 HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle PreferredCarryAngles( void ) { return m_angPreferredCarryAngles; }

	int			ExploitableByPlayer() const { return m_iExploitableByPlayer; }

	// inputs
	[[= ks::reflect::Input{ .name = "Wake", .type = FIELD_VOID } ]] void InputWake( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Sleep", .type = FIELD_VOID } ]] void InputSleep( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableMotion", .type = FIELD_VOID } ]] void InputEnableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableMotion", .type = FIELD_VOID } ]] void InputDisableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ForceDrop", .type = FIELD_VOID } ]] void InputForceDrop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableFloating", .type = FIELD_VOID } ]] void InputDisableFloating( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BecomeDebris", .type = FIELD_VOID } ]] void InputBecomeDebris( inputdata_t &inputdata );

	DECLARE_DATADESC();
	
protected:
	[[= ks::reflect::Key{ .name = "Damagetype" } ]] int				m_damageType;
	[[= ks::reflect::Key{ .name = "massScale" } ]] float			m_massScale;
	[[= ks::reflect::Key{ .name = "overridescript" } ]] string_t		m_iszOverrideScript;
	[[= ks::reflect::Key{ .name = "damagetoenablemotion" } ]] int				m_damageToEnableMotion;
	[[= ks::reflect::Key{ .name = "forcetoenablemotion" } ]] float			m_flForceToEnableMotion;
	[[= ks::reflect::Key{ .name = "preferredcarryangles" } ]] QAngle			m_angPreferredCarryAngles;
	[[= ks::reflect::Key{ .name = "notsolid" } ]] bool			m_bNotSolidToWorld;
	[[= ks::reflect::Key{ .name = "ExploitableByPlayer" } ]] int				m_iExploitableByPlayer;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnDamaged" } ]] COutputEvent	m_OnDamaged;
	[[= ks::reflect::Key{ .name = "OnAwakened" } ]] COutputEvent	m_OnAwakened;
	[[= ks::reflect::Key{ .name = "OnMotionEnabled" } ]] COutputEvent	m_OnMotionEnabled;
	[[= ks::reflect::Key{ .name = "OnPhysGunPickup" } ]] COutputEvent	m_OnPhysGunPickup;
	[[= ks::reflect::Key{ .name = "OnPhysGunPunt" } ]] COutputEvent	m_OnPhysGunPunt;
	[[= ks::reflect::Key{ .name = "OnPhysGunOnlyPickup" } ]] COutputEvent	m_OnPhysGunOnlyPickup;
	[[= ks::reflect::Key{ .name = "OnPhysGunDrop" } ]] COutputEvent	m_OnPhysGunDrop;
	[[= ks::reflect::Key{ .name = "OnPlayerUse" } ]] COutputEvent	m_OnPlayerUse;

	CHandle<CBasePlayer>	m_hCarryingPlayer;	// Player who's carrying us
};

// ---------------------------------------------------------------------
//
// CPhysExplosion -- physically simulated explosion
//
// ---------------------------------------------------------------------
#define SF_PHYSEXPLOSION_NODAMAGE			0x0001
#define SF_PHYSEXPLOSION_PUSH_PLAYER		0x0002
#define SF_PHYSEXPLOSION_RADIAL				0x0004
#define	SF_PHYSEXPLOSION_TEST_LOS			0x0008
#define SF_PHYSEXPLOSION_DISORIENT_PLAYER	0x0010

class CPhysExplosion : public CPointEntity
{
public:
	DECLARE_CLASS( CPhysExplosion, CPointEntity );

	void	Spawn ( void );
	void	Explode( CBaseEntity *pActivator, CBaseEntity *pCaller );
	void	ExplodeAndRemove( CBaseEntity *pActivator, CBaseEntity *pCaller );

	CBaseEntity *FindEntity( CBaseEntity *pEntity, CBaseEntity *pActivator, CBaseEntity *pCaller );

	int DrawDebugTextOverlays(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "Explode", .type = FIELD_VOID } ]] void InputExplode( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ExplodeAndRemove", .type = FIELD_VOID } ]] void InputExplodeAndRemove( inputdata_t &inputdata );

	DECLARE_DATADESC();
	
	float		GetRadius( void );
	[[= ks::reflect::Key{ .name = "magnitude" } ]] float		m_damage;
	[[= ks::reflect::Key{ .name = "radius" } ]] float		m_radius;
	[[= ks::reflect::Key{ .name = "targetentityname" } ]] string_t	m_targetEntityName;
	[[= ks::reflect::Key{ .name = "inner_radius" } ]] float		m_flInnerRadius;
	
	[[= ks::reflect::Key{ .name = "OnPushedPlayer" } ]] COutputEvent	m_OnPushedPlayer;	
};

void CreatePhysExplosion( Vector origin, float magnitude, float radius, string_t target, float innerRadius, int flags );

//==================================================
// CPhysImpact
//==================================================

class CPhysImpact : public CPointEntity
{
public:
	DECLARE_CLASS( CPhysImpact, CPointEntity );

	void		Spawn( void );
	//void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		Activate( void );

	[[= ks::reflect::Input{ .name = "Impact", .type = FIELD_VOID } ]] void		InputImpact( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	void		PointAtEntity( void );

	[[= ks::reflect::Key{ .name = "magnitude" } ]] float		m_damage;
	[[= ks::reflect::Key{ .name = "distance" } ]] float		m_distance;
	[[= ks::reflect::Key{ .name = "directionentityname" } ]] string_t	m_directionEntityName;
};

//-----------------------------------------------------------------------------
// Purpose: A magnet that creates constraints between itself and anything it touches 
//-----------------------------------------------------------------------------

struct magnetted_objects_t
{
	IPhysicsConstraint *pConstraint;
	EHANDLE			   hEntity;

	DECLARE_SIMPLE_DATADESC();
};

class [[= ks::reflect::NetTable{ .name = "DT_PhysMagnet" } ]]
      CPhysMagnet : public CBaseAnimating, public IPhysicsConstraintEvent
{
	DECLARE_CLASS( CPhysMagnet, CBaseAnimating );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPhysMagnet();
	~CPhysMagnet();

	void	Spawn( void );
	void	Precache( void );
	void	Touch( CBaseEntity *pOther );
	void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	void	DoMagnetSuck( CBaseEntity *pOther );
	void	SetConstraintGroup( IPhysicsConstraintGroup *pGroup );

	bool	IsOn( void ) { return m_bActive; }
	int		GetNumAttachedObjects( void );
	float	GetTotalMassAttachedObjects( void );
	CBaseEntity *GetAttachedObject( int iIndex );

	// Checking for hitting something
	void	ResetHasHitSomething( void ) { m_bHasHitSomething = false; }
	bool	HasHitSomething( void ) { return m_bHasHitSomething; }

	// Inputs
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff( inputdata_t &inputdata );

	void	InputConstraintBroken( inputdata_t &inputdata );

	void	DetachAll( void );

// IPhysicsConstraintEvent
public:
	void	ConstraintBroken( IPhysicsConstraint *pConstraint );
	
protected:
	// Outputs
	[[= ks::reflect::Key{ .name = "OnAttach" } ]] COutputEvent	m_OnMagnetAttach;
	[[= ks::reflect::Key{ .name = "OnDetach" } ]] COutputEvent	m_OnMagnetDetach;

	// Keys
	[[= ks::reflect::Key{ .name = "massScale" } ]] float			m_massScale;
	[[= ks::reflect::Key{ .name = "overridescript" } ]] string_t		m_iszOverrideScript;
	[[= ks::reflect::Key{ .name = "forcelimit" } ]] float			m_forceLimit;
	[[= ks::reflect::Key{ .name = "torquelimit" } ]] float			m_torqueLimit;

	CUtlVector< magnetted_objects_t >	m_MagnettedEntities;
	IPhysicsConstraintGroup				*m_pConstraintGroup;

	bool			m_bActive;
	bool			m_bHasHitSomething;
	float			m_flTotalMass;
	float			m_flRadius;
	float			m_flNextSuckTime;
	[[= ks::reflect::Key{ .name = "maxobjects" } ]] int				m_iMaxObjectsAttached;
};

#endif // PHYSOBJ_H
