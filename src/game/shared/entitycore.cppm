// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;
#include "basehandle.h"
#include "bspflags.h"
#include "vphysics_interface.h"
#include "networkvar.h"
#include "reflect_annotations.h"
export module ks.game.entitycore;

import ks.game.shareddefs;
import ks.math.mathlib;
import ks.math.vector;

export
{
class CEntityCore;
class CPlayerMoveCore;
class CPlayerLocalMoveCore;

// What the core needs from the side an entity lives on. Function pointers rather than virtual
// functions, so the core has no vtable. Each side fills one in and every entity points to it; it
// lives on the entity because srceng links both sides into one image.
struct CoreServices
{
	// A core network var changed; pVar null means the whole entity. Null on the client.
	void ( *Changed )( CEntityCore *pEntity, void *pVar );
	// FL_* bits changed. Null on the client.
	void ( *FlagsChanged )( CEntityCore *pEntity, int oldFlags, int newFlags );

	const Vector &( *AbsOrigin )( const CEntityCore *pEntity );
	const Vector &( *AbsVelocity )( const CEntityCore *pEntity );
	const QAngle &( *AbsAngles )( const CEntityCore *pEntity );
	int ( *EntIndex )( const CEntityCore *pEntity );
	bool ( *IsPlayer )( const CEntityCore *pEntity );
	bool ( *IsFloating )( const CEntityCore *pEntity );
	int ( *CollisionGroup )( const CEntityCore *pEntity );
	int ( *TeamNumber )( const CEntityCore *pEntity );
	unsigned int ( *SolidMask )( const CEntityCore *pEntity );
};

// The side-free part of an entity. CBaseEntity on the server and on the client derive from it, so
// state and logic that do not depend on networking direction are written and compiled once.
class [[= ks::reflect::Flatten{} ]] CEntityCore
{
public:
	bool IsEFlagSet( int nEFlagMask ) const { return ( m_iEFlags & nEFlagMask ) != 0; }
	bool IsMarkedForDeletion() const { return ( m_iEFlags & EFL_KILLME ) != 0; }

	IPhysicsObject *VPhysicsGetObject() const { return m_pPhysicsObject; }
	float VPhysicsGetNonShadowMass() const { return m_flNonShadowMass; }

	MoveType_t GetMoveType() const { return (MoveType_t)(unsigned char)m_MoveType; }
	MoveCollide_t GetMoveCollide() const { return (MoveCollide_t)(unsigned char)m_MoveCollide; }
	void SetMoveCollide( MoveCollide_t val ) { m_MoveCollide = val; }
	const QAngle &GetLocalAngularVelocity() const { return m_vecAngVelocity; }
	const Vector &GetLocalOrigin() const { return m_vecOrigin.Get(); }
	const QAngle &GetLocalAngles() const { return m_angRotation.Get(); }
	const Vector &GetLocalVelocity() const { return m_vecVelocity.Get(); }
	const Vector &GetBaseVelocity() const { return m_vecBaseVelocity.Get(); }
	void SetBaseVelocity( const Vector &v ) { m_vecBaseVelocity = v; }
	int GetWaterLevel() const { return m_nWaterLevel; }
	void SetWaterLevel( int nLevel ) { m_nWaterLevel = nLevel; }

	int GetFlags() const { return m_fFlags; }
	void AddFlag( int flags ) { ChangeFlags( m_fFlags | flags ); }
	void RemoveFlag( int flagsToRemove ) { ChangeFlags( m_fFlags & ~flagsToRemove ); }
	void ToggleFlag( int flagToToggle ) { ChangeFlags( m_fFlags ^ flagToToggle ); }
	void ClearFlags() { ChangeFlags( 0 ); }

	// The CONTENTS_ water bits, stored compactly.
	int GetWaterType() const { return ( ( m_nWaterType & 1 ) ? CONTENTS_WATER : 0 ) | ( ( m_nWaterType & 2 ) ? CONTENTS_SLIME : 0 ); }
	void SetWaterType( int nType ) { m_nWaterType = ( ( nType & CONTENTS_WATER ) ? 1 : 0 ) | ( ( nType & CONTENTS_SLIME ) ? 2 : 0 ); }

	float GetGravity() const { return m_flGravity; }
	void SetGravity( float flGravity ) { m_flGravity = flGravity; }

	const CBaseHandle &GetRefEHandle() const { return m_RefEHandle; }

	// Answered by the side.
	const Vector &GetAbsOrigin() const { return m_pServices->AbsOrigin( this ); }
	const Vector &GetAbsVelocity() const { return m_pServices->AbsVelocity( this ); }
	const QAngle &GetAbsAngles() const { return m_pServices->AbsAngles( this ); }
	int entindex() const { return m_pServices->EntIndex( this ); }
	bool IsPlayer() const { return m_pServices->IsPlayer( this ); }
	bool IsFloating() const { return m_pServices->IsFloating( this ); }
	int GetCollisionGroup() const { return m_pServices->CollisionGroup( this ); }
	int GetTeamNumber() const { return m_pServices->TeamNumber( this ); }
	unsigned int PhysicsSolidMaskForEntity() const { return m_pServices->SolidMask( this ); }

	// The network var macros reach their containing class through ThisClass.
	typedef CEntityCore ThisClass;
	void NetworkStateChanged() { if ( m_pServices->Changed ) m_pServices->Changed( this, nullptr ); }
	void NetworkStateChanged( void *pVar ) { if ( m_pServices->Changed ) m_pServices->Changed( this, pVar ); }

protected:
	void ChangeFlags( int newFlags )
	{
		const int old = m_fFlags;
		m_fFlags = newFlags;
		if ( m_pServices->FlagsChanged ) m_pServices->FlagsChanged( this, old, newFlags );
	}

	const CoreServices *m_pServices = nullptr; // set first thing by each side's CBaseEntity constructor
	int m_iEFlags = 0; // entity flags EFL_*
	int m_nSimulationTick;
	int m_fDataObjectTypes;
	IPhysicsObject *m_pPhysicsObject; // the entity's physics object (vphysics)
	float m_flNonShadowMass; // cached mass (shadow controllers set mass to VPHYSICS_MAX_MASS, or 50000)
	unsigned char m_nWaterType;
	float m_flGroundChangeTime; // time that the ground entity changed
	matrix3x4_t m_rgflCoordinateFrame;
	CBaseHandle m_RefEHandle; // reference ehandle, used to generate ehandles off this entity

	// World-space transform, derived from the local one and the move parent.
	Vector m_vecAbsOrigin;
	QAngle m_angAbsRotation;
	Vector m_vecAbsVelocity;
	QAngle m_vecAngVelocity; // local angular velocity
	float m_flGravity; // gravity scale

	// Networked by the server, predicted by the client; each side says how in its own class.
	CNetworkVar( unsigned char, m_MoveType ); // MOVETYPE_*
	CNetworkVar( unsigned char, m_MoveCollide ); // MOVECOLLIDE_*
	CNetworkVectorXY_SeparateZ( m_vecOrigin ); // local, relative to the move parent
	CNetworkQAngleXYZ( m_angRotation );

	// Sent only by the derived classes that switch them on (players, grenades, movelinear).
	CNetworkVarForDerived( int, m_fFlags ); // FL_*
	CNetworkVectorXYZForDerived( m_vecVelocity ); // local
	CNetworkVectorForDerived( m_vecBaseVelocity );
	CNetworkVectorXYZForDerived( m_vecViewOffset ); // eye position, relative to the origin
	CNetworkVarForDerived( float, m_flFriction );
	CNetworkVarForDerived( unsigned char, m_nWaterLevel );
	CNetworkVarForDerived( CBaseHandle, m_hGroundEntity ); // each side resolves it to its entity
};

// What player movement needs from the side, for a player. As CoreServices.
struct PlayerMoveServices
{
	bool bServer;
	CEntityCore *( *GroundEntity )( const CPlayerMoveCore *pPlayer );
	void ( *SetGroundEntity )( CPlayerMoveCore *pPlayer, CEntityCore *pGround );
	void ( *SetMoveType )( CPlayerMoveCore *pPlayer, MoveType_t val );
	const Vector &( *ViewOffset )( const CPlayerMoveCore *pPlayer );
	void ( *SetViewOffset )( CPlayerMoveCore *pPlayer, const Vector &v );
	bool ( *IsObserver )( const CPlayerMoveCore *pPlayer );
	int ( *ObserverMode )( const CPlayerMoveCore *pPlayer );
	CEntityCore *( *ObserverTarget )( const CPlayerMoveCore *pPlayer );
	int ( *CurrentCommandNumber )( const CPlayerMoveCore *pPlayer );
	void ( *PlayStepSound )( CPlayerMoveCore *pPlayer, Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	void ( *UpdateStepSound )( CPlayerMoveCore *pPlayer, surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity );
	QAngle ( *ViewPunchAngle )( const CPlayerMoveCore *pPlayer );
	void ( *SetViewPunchAngle )( CPlayerMoveCore *pPlayer, const QAngle &angle );
	const Vector &( *LadderNormal )( const CPlayerMoveCore *pPlayer );
	void ( *SetLadderNormal )( CPlayerMoveCore *pPlayer, const Vector &v );
	int ( *Health )( const CPlayerMoveCore *pPlayer );
	int ( *TickBase )( const CPlayerMoveCore *pPlayer );
	bool ( *IsDeadFlag )( const CPlayerMoveCore *pPlayer );
	float ( *LaggedMovementValue )( const CPlayerMoveCore *pPlayer );
	bool ( *IsBot )( const CPlayerMoveCore *pPlayer );
	float ( *PlayerMaxSpeed )( CPlayerMoveCore *pPlayer );
};

// The player as movement sees it: the state movement reads and writes outside the networked local
// data, and the entity and side calls it makes. Both sides' CBasePlayer derive from it beside their
// entity chain, so CGameMovement is written against this alone and compiled once. CBasePlayer
// resolves the names it shares with CBaseEntity in favour of the entity.
class [[= ks::reflect::Flatten{} ]] CPlayerMoveCore
{
public:
	// Bound by each side's CBasePlayer constructor.
	void BindMoveCore( CEntityCore *pEntity, CPlayerLocalMoveCore *pLocal, const PlayerMoveServices *pServices )
	{
		m_pMoveEntity = pEntity;
		m_pMoveLocal = pLocal;
		m_pMoveServices = pServices;
	}

	CEntityCore *MoveEntity() const { return m_pMoveEntity; }
	CPlayerLocalMoveCore &Local() const { return *m_pMoveLocal; }

	int GetFlags() const { return m_pMoveEntity->GetFlags(); }
	void AddFlag( int flags ) { m_pMoveEntity->AddFlag( flags ); }
	void RemoveFlag( int flags ) { m_pMoveEntity->RemoveFlag( flags ); }
	MoveType_t GetMoveType() const { return m_pMoveEntity->GetMoveType(); }
	MoveCollide_t GetMoveCollide() const { return m_pMoveEntity->GetMoveCollide(); }
	void SetMoveCollide( MoveCollide_t val ) { m_pMoveEntity->SetMoveCollide( val ); }
	const Vector &GetBaseVelocity() const { return m_pMoveEntity->GetBaseVelocity(); }
	void SetBaseVelocity( const Vector &v ) { m_pMoveEntity->SetBaseVelocity( v ); }
	int GetWaterLevel() const { return m_pMoveEntity->GetWaterLevel(); }
	void SetWaterLevel( int nLevel ) { m_pMoveEntity->SetWaterLevel( nLevel ); }
	int GetWaterType() const { return m_pMoveEntity->GetWaterType(); }
	void SetWaterType( int nType ) { m_pMoveEntity->SetWaterType( nType ); }
	float GetGravity() const { return m_pMoveEntity->GetGravity(); }
	void SetGravity( float flGravity ) { m_pMoveEntity->SetGravity( flGravity ); }
	const Vector &GetAbsVelocity() const { return m_pMoveEntity->GetAbsVelocity(); }
	int entindex() const { return m_pMoveEntity->entindex(); }

	bool IsServer() const { return m_pMoveServices->bServer; }
	CEntityCore *GetGroundEntity() const { return m_pMoveServices->GroundEntity( this ); }
	void SetGroundEntity( CEntityCore *pGround ) { m_pMoveServices->SetGroundEntity( this, pGround ); }
	void SetMoveType( MoveType_t val ) { m_pMoveServices->SetMoveType( this, val ); }
	const Vector &GetViewOffset() const { return m_pMoveServices->ViewOffset( this ); }
	void SetViewOffset( const Vector &v ) { m_pMoveServices->SetViewOffset( this, v ); }
	bool IsObserver() const { return m_pMoveServices->IsObserver( this ); }
	int GetObserverMode() const { return m_pMoveServices->ObserverMode( this ); }
	CEntityCore *GetObserverTarget() const { return m_pMoveServices->ObserverTarget( this ); }
	int CurrentCommandNumber() const { return m_pMoveServices->CurrentCommandNumber( this ); }
	void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force ) { m_pMoveServices->PlayStepSound( this, vecOrigin, psurface, fvol, force ); }
	void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity ) { m_pMoveServices->UpdateStepSound( this, psurface, vecOrigin, vecVelocity ); }
	QAngle GetViewPunchAngle() const { return m_pMoveServices->ViewPunchAngle( this ); }
	void SetViewPunchAngle( const QAngle &angle ) { m_pMoveServices->SetViewPunchAngle( this, angle ); }
	const Vector &GetLadderNormal() const { return m_pMoveServices->LadderNormal( this ); }
	void SetLadderNormal( const Vector &v ) { m_pMoveServices->SetLadderNormal( this, v ); }
	int GetHealth() const { return m_pMoveServices->Health( this ); }
	int GetTickBase() const { return m_pMoveServices->TickBase( this ); }
	bool IsDeadFlag() const { return m_pMoveServices->IsDeadFlag( this ); }
	float GetLaggedMovementValue() const { return m_pMoveServices->LaggedMovementValue( this ); }
	bool IsBot() const { return m_pMoveServices->IsBot( this ); }
	float GetPlayerMaxSpeed() { return m_pMoveServices->PlayerMaxSpeed( this ); }
	float GetStepSize() const;

	// Movement state, public because CGameMovement lives in another module.
	int m_StuckLast; // GetRandomStuckOffsets cycles it
	float m_flWaterJumpTime; // used to be called teleport_time
	Vector m_vecWaterJumpVel;
	float m_flSwimSoundTime;
	float m_ignoreLadderJumpTime;
	bool m_bHasWalkMovedSinceLastJump;
	float m_flStepSoundTime; // time to check for next footstep sound
	float m_surfaceFriction;

	// Texture names and surface data of what the player stands on.
	int m_surfaceProps;
	surfacedata_t *m_pSurfaceData;
	char m_chTextureType;
	char m_chPreviousTextureType; // cleared when off the ground; server-side surface triggers watch it

private:
	CEntityCore *m_pMoveEntity = nullptr;
	CPlayerLocalMoveCore *m_pMoveLocal = nullptr;
	const PlayerMoveServices *m_pMoveServices = nullptr;
};

// The part of the player's local data that movement uses. Both sides' CPlayerLocalData derive from
// it; how each side sends and predicts a field is said there.
class [[= ks::reflect::Flatten{} ]] CPlayerLocalMoveCore
{
public:
	typedef CPlayerLocalMoveCore ThisClass;
	// The network vars below report to the derived local data, which forwards to the player. A
	// function pointer rather than a virtual function, so this class has no vtable of its own.
	void NetworkStateChanged() { if ( m_pfnStateChanged ) m_pfnStateChanged( this, nullptr ); }
	void NetworkStateChanged( void *pVar ) { if ( m_pfnStateChanged ) m_pfnStateChanged( this, pVar ); }
	void ( *m_pfnStateChanged )( CPlayerLocalMoveCore *pCore, void *pVar ) = nullptr;

	int m_nStepside; // step sound side flip/flop
	int m_nOldButtons; // previous button state

	CNetworkVar( int, m_nDuckTimeMsecs ); // time left in the duck transition
	CNetworkVar( int, m_nDuckJumpTimeMsecs );
	CNetworkVar( int, m_nJumpTimeMsecs ); // time left until the auto unduck after a jump
	CNetworkVar( float, m_flFallVelocity ); // velocity when the player hit the ground
	CNetworkVar( float, m_flStepSize );

	CNetworkQAngle( m_viewPunchAngle ); // auto-decaying view angle adjustment
	CNetworkQAngle( m_aimPunchAngle ); // auto-decaying aim angle adjustment
	CNetworkQAngle( m_aimPunchAngleVel );

	CNetworkVar( bool, m_bDucked ); // fully ducked
	CNetworkVar( bool, m_bDucking ); // in a duck transition
	CNetworkVar( float, m_flLastDuckTime ); // to handle duck spam
	CNetworkVar( bool, m_bInDuckJump );
	CNetworkVar( bool, m_bAllowAutoMovement );
	CNetworkVar( bool, m_bSlowMovement );
};

inline float CPlayerMoveCore::GetStepSize() const { return m_pMoveLocal->m_flStepSize; }
}
