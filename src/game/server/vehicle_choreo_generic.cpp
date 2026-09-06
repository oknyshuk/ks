//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "npcevent.h"
#include "vehicle_base.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"
#include "soundenvelope.h"
#include "soundent.h"
#include "vphysics/constraints.h"
#include "vcollide_parse.h"
#include "ndebugoverlay.h"
#include "player.h"
#include "props.h"
#include "vehicle_choreo_generic_shared.h"
#include "ai_utils.h"

#if defined ( PORTAL2 )
#include "portal_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	VEHICLE_HITBOX_DRIVER		1

#define CHOREO_VEHICLE_VIEW_FOV		90
#define CHOREO_VEHICLE_VIEW_YAW_MIN	-60
#define CHOREO_VEHICLE_VIEW_YAW_MAX	60
#define CHOREO_VEHICLE_VIEW_PITCH_MIN	-90
#define CHOREO_VEHICLE_VIEW_PITCH_MAX	38	


//
// Anim events.
//
enum
{
	AE_CHOREO_VEHICLE_OPEN = 1,	
	AE_CHOREO_VEHICLE_CLOSE = 2,
};


extern ConVar g_debug_vehicledriver;


class CPropVehicleChoreoGeneric;

static const char *pChoreoGenericFollowerBoneNames[] =
{
	"base",
};


//-----------------------------------------------------------------------------
// Purpose: A KeyValues parse for vehicle sound blocks
//-----------------------------------------------------------------------------
class CVehicleChoreoViewParser : public IVPhysicsKeyHandler
{
public:
	CVehicleChoreoViewParser( void );

private:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue );
	virtual void SetDefaults( void *pData );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CChoreoGenericServerVehicle : public CBaseServerVehicle
{
	DECLARE_SIMPLE_DATADESC();
	typedef CBaseServerVehicle BaseClass;

// IServerVehicle
public:
	void GetVehicleViewPosition( int nRole, Vector *pAbsOrigin, QAngle *pAbsAngles, float *pFOV = NULL );
	virtual void ItemPostFrame( CBasePlayer *pPlayer );
	virtual bool IsPassengerUsingStandardWeapons( int nRole = VEHICLE_ROLE_DRIVER ) { return m_bPlayerCanShoot; }

	virtual void SetPlayerCanShoot( bool bCanShoot, int nRole = VEHICLE_ROLE_DRIVER );

protected:

	bool m_bPlayerCanShoot;
	CPropVehicleChoreoGeneric *GetVehicle( void );
};

void CChoreoGenericServerVehicle::SetPlayerCanShoot( bool bCanShoot, int nRole /*= VEHICLE_ROLE_DRIVER*/ )
{
	if ( bCanShoot != m_bPlayerCanShoot )
	{
		SetPassengerWeapon( bCanShoot, GetPassenger( nRole ) );
		m_bPlayerCanShoot = bCanShoot;
	}
}

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CChoreoGenericServerVehicle )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_PropVehicleChoreoGeneric" } ]]
      [[= ks::reflect::From<"m_vehicleView.bClampEyeAngles", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flPitchCurveZero", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flPitchCurveLinear", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flRollCurveZero", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flRollCurveLinear", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flFOV", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flYawMin", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flYawMax", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flPitchMin", ks::reflect::Net{ .bits = 32 }>{} ]]
      [[= ks::reflect::From<"m_vehicleView.flPitchMax", ks::reflect::Net{ .bits = 32 }>{} ]]
      CPropVehicleChoreoGeneric : public CDynamicProp, public IDrivableVehicle
{
	DECLARE_CLASS( CPropVehicleChoreoGeneric, CDynamicProp );

public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPropVehicleChoreoGeneric( void )
	{
		m_ServerVehicle.SetVehicle( this );
		m_bIgnoreMoveParent = false;
		m_bForcePlayerEyePoint = false;
	}

	~CPropVehicleChoreoGeneric( void )
	{
	}

	// CBaseEntity
	virtual void	Precache( void );
	void			Spawn( void );
	void			Think(void);
	virtual int		ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; };
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	DrawDebugGeometryOverlays( void );

	virtual Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );

	void			PlayerControlInit( CBasePlayer *pPlayer );
	void			PlayerControlShutdown( void );
	void			ResetUseKey( CBasePlayer *pPlayer );

	virtual bool OverridePropdata() { return true; }

	bool			ParseViewParams( const char *pScriptName );

	void			GetVectors(Vector* pForward, Vector* pRight, Vector* pUp) const;

	bool CreateVPhysics()
	{
		SetSolid(SOLID_VPHYSICS);
		SetMoveType(MOVETYPE_NONE);
		return true;
	}
	bool ShouldForceExit() { return m_bForcedExit; }
	void ClearForcedExit() { m_bForcedExit = false; }

	// CBaseAnimating
	void HandleAnimEvent( animevent_t *pEvent );

	// Inputs
	[[= ks::reflect::Input{ .name = "EnterVehicleImmediate", .type = FIELD_VOID } ]] void InputEnterVehicleImmediate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnterVehicle", .type = FIELD_VOID } ]] void InputEnterVehicle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ExitVehicle", .type = FIELD_VOID } ]] void InputExitVehicle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Lock", .type = FIELD_VOID } ]] void InputLock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] void InputOpen( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] void InputClose( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Viewlock", .type = FIELD_BOOLEAN } ]] void InputViewlock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetCanShoot", .type = FIELD_BOOLEAN } ]] void InputSetCanShoot( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UseAttachmentEyes", .type = FIELD_BOOLEAN } ]] void InputUseAttachmentEyes( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxPitch", .type = FIELD_FLOAT } ]] void InputSetMaxPitch( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMinPitch", .type = FIELD_FLOAT } ]] void InputSetMinPitch( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxYaw", .type = FIELD_FLOAT } ]] void InputSetMaxYaw( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMinYaw", .type = FIELD_FLOAT } ]] void InputSetMinYaw( inputdata_t &inputdata );

	bool ShouldIgnoreParent( void ) { return m_bIgnoreMoveParent; }

	// Tuned to match HL2s definition, but this should probably return false in all cases
	virtual bool	PassengerShouldReceiveDamage( CTakeDamageInfo &info ) { return (info.GetDamageType() & (DMG_BLAST|DMG_RADIATION)) == 0; }

	CNetworkHandle( CBasePlayer, m_hPlayer, [[= ks::reflect::Net{} ]] );

	CNetworkVarEmbedded( vehicleview_t, m_vehicleView );
private:
	vehicleview_t m_savedVehicleView; // gets saved out for viewlock/unlock input

// IDrivableVehicle
public:

	virtual CBaseEntity *GetDriver( void );
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData ) { return; }
	virtual void FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move ) { return; }
	virtual bool CanEnterVehicle( CBaseEntity *pEntity );
	virtual bool CanExitVehicle( CBaseEntity *pEntity );
	virtual void SetVehicleEntryAnim( bool bOn );
	virtual void SetVehicleExitAnim( bool bOn, Vector vecEyeExitEndpoint ) { m_bExitAnimOn = bOn; if ( bOn ) m_vecEyeExitEndpoint = vecEyeExitEndpoint; }
	virtual void EnterVehicle( CBaseCombatCharacter *pPassenger );

	virtual bool AllowBlockedExit( CBaseCombatCharacter *pPassenger, int nRole ) { return true; }
	virtual bool AllowMidairExit( CBaseCombatCharacter *pPassenger, int nRole ) { return true; }
	virtual void PreExitVehicle( CBaseCombatCharacter *pPassenger, int nRole ) {}
	virtual void ExitVehicle( int nRole );

	virtual void ItemPostFrame( CBasePlayer *pPlayer ) {}
	virtual void SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move ) {}
	virtual string_t GetVehicleScriptName() { return m_vehicleScript; }

	// If this is a vehicle, returns the vehicle interface
	virtual IServerVehicle *GetServerVehicle() { return &m_ServerVehicle; }

	bool ShouldCollide( int collisionGroup, int contentsMask ) const;

	[[= ks::reflect::Key{ .name = "useplayereyes" } ]] bool				m_bForcePlayerEyePoint;			// Uses player's eyepoint instead of 'vehicle_driver_eyes' attachment

protected:

	// Contained IServerVehicle
	CChoreoGenericServerVehicle m_ServerVehicle;

private:

	// Entering / Exiting
	[[= ks::reflect::Key{ .name = "vehiclelocked" } ]] bool				m_bLocked;
	CNetworkVar( bool,	m_bEnterAnimOn, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool,	m_bExitAnimOn, [[= ks::reflect::Net{} ]] );
	CNetworkVector(		m_vecEyeExitEndpoint, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD } ]] );
	bool				m_bForcedExit;
	[[= ks::reflect::Key{ .name = "ignoremoveparent" } ]] bool				m_bIgnoreMoveParent;
	[[= ks::reflect::Key{ .name = "ignoreplayer" } ]] bool				m_bIgnorePlayerCollisions;

	[[= ks::reflect::Key{ .name = "playercanshoot" } ]] bool				m_bPlayerCanShoot;
	CNetworkVar( bool, m_bForceEyesToAttachment, [[= ks::reflect::Key{ .name = "useattachmenteyes" } ]] [[= ks::reflect::Net{} ]] );

	// Vehicle script filename
	[[= ks::reflect::Key{ .name = "vehiclescript" } ]] string_t			m_vehicleScript;

	[[= ks::reflect::Key{ .name = "PlayerOn" } ]] COutputEvent		m_playerOn;
	[[= ks::reflect::Key{ .name = "PlayerOff" } ]] COutputEvent		m_playerOff;
	[[= ks::reflect::Key{ .name = "OnOpen" } ]] COutputEvent		m_OnOpen;
	[[= ks::reflect::Key{ .name = "OnClose" } ]] COutputEvent		m_OnClose;
};

LINK_ENTITY_TO_CLASS( prop_vehicle_choreo_generic, CPropVehicleChoreoGeneric );

IMPLEMENT_REFLECT_DATAMAP( CPropVehicleChoreoGeneric )

IMPLEMENT_REFLECT_SERVERCLASS( CPropVehicleChoreoGeneric, DT_PropVehicleChoreoGeneric )




bool ShouldVehicleIgnoreEntity( CBaseEntity *pVehicle, CBaseEntity *pCollide )
{
	if ( pCollide->GetParent() == pVehicle )
		return true;

	CPropVehicleChoreoGeneric *pChoreoVehicle = dynamic_cast <CPropVehicleChoreoGeneric *>( pVehicle );

	if ( pChoreoVehicle == NULL )
		return false;

	if ( pCollide == NULL )
		return false;

	if ( pChoreoVehicle->ShouldIgnoreParent() == false )
		return false;

	if ( pChoreoVehicle->GetMoveParent() == pCollide )
		return true;
		
	return false;
}


//------------------------------------------------
// Precache
//------------------------------------------------
void CPropVehicleChoreoGeneric::Precache( void )
{
	BaseClass::Precache();

	m_ServerVehicle.Initialize( STRING(m_vehicleScript) );
	m_ServerVehicle.UseLegacyExitChecks( true );
}


//------------------------------------------------
// Spawn
//------------------------------------------------
void CPropVehicleChoreoGeneric::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );
	SetCollisionGroup( COLLISION_GROUP_VEHICLE );

	if ( GetSolid() != SOLID_NONE )
	{
		BaseClass::Spawn();
	}

	m_ServerVehicle.SetPlayerCanShoot( m_bPlayerCanShoot );

	m_takedamage = DAMAGE_EVENTS_ONLY;

	SetNextThink( gpGlobals->curtime );

	ParseViewParams( STRING(m_vehicleScript) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( ptr->hitbox == VEHICLE_HITBOX_DRIVER )
	{
		if ( m_hPlayer != NULL )
		{
			m_hPlayer->TakeDamage( info );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPropVehicleChoreoGeneric::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;
	info.ScaleDamage( 25 );

	// reset the damage
	info.SetDamage( inputInfo.GetDamage() );

	// Check to do damage to prisoner
	if ( m_hPlayer != NULL )
	{
		// Take no damage from physics damages
		if ( info.GetDamageType() & DMG_CRUSH )
			return 0;

		// Take the damage
		m_hPlayer->TakeDamage( info );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CPropVehicleChoreoGeneric::BodyTarget( const Vector &posSrc, bool bNoisy )
{
	Vector	shotPos;

	int eyeAttachmentIndex = LookupAttachment("vehicle_driver_eyes");
	GetAttachment( eyeAttachmentIndex, shotPos );

	if ( bNoisy )
	{
		shotPos[0] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[1] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[2] += random->RandomFloat( -8.0f, 8.0f );
	}

	return shotPos;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::Think(void)
{
	SetNextThink( gpGlobals->curtime + 0.1 );

	if ( GetDriver() )
	{
		BaseClass::Think();
		
		// If the enter or exit animation has finished, tell the server vehicle
		if ( IsSequenceFinished() && (m_bExitAnimOn || m_bEnterAnimOn) )
		{
			GetServerVehicle()->HandleEntryExitFinish( m_bExitAnimOn, true );
		}

#if defined ( PORTAL2 )
		// Hack (possibily temp?) for finale... Make sure the player's body follows somewhat closely to the eye point animation
		// so their shoot position doesn't come from across the world.
		// This could be cleaned up and made an option on a keyvalue if we dont end up solving it a different way.. checking in dirty because we want to this very soon.
		CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating *>(GetServerVehicle()->GetVehicleEnt());
		if ( pAnimating )
		{
			Vector vSeatOrigin;
			QAngle qSeatAngles;
			if ( GetDriver() && pAnimating->GetAttachment( "vehicle_feet_passenger0", vSeatOrigin, qSeatAngles ) )
			{
				// Set us to that position
				CBaseEntity* pDriver = GetDriver();
				pDriver->SetAbsOrigin( vSeatOrigin );
				pDriver->SetAbsAngles( qSeatAngles );
			}

		}
#endif
	}

	StudioFrameAdvance();
	DispatchAnimEvents( this );
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputOpen( inputdata_t &inputdata )
{
	int nSequence = LookupSequence( "open" );

	// Set to the desired anim, or default anim if the desired is not present
	if ( nSequence > ACTIVITY_NOT_AVAILABLE )
	{
		SetCycle( 0 );
		m_flAnimTime = gpGlobals->curtime;
		ResetSequence( nSequence );
		ResetClientsideFrame();
	}
	else
	{
		// Not available try to get default anim
		Msg( "Choreo Generic Vehicle %s: missing open sequence\n", GetDebugName() );
		SetSequence( 0 );
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputClose( inputdata_t &inputdata )
{
	if ( m_bLocked || m_bEnterAnimOn )
		return;

	int nSequence = LookupSequence( "close" );

	// Set to the desired anim, or default anim if the desired is not present
	if ( nSequence > ACTIVITY_NOT_AVAILABLE )
	{
		SetCycle( 0 );
		m_flAnimTime = gpGlobals->curtime;
		ResetSequence( nSequence );
		ResetClientsideFrame();
	}
	else
	{
		// Not available try to get default anim
		Msg( "Choreo Generic Vehicle %s: missing close sequence\n", GetDebugName() );
		SetSequence( 0 );
	}
}



//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputViewlock( inputdata_t &inputdata )
{
	if (inputdata.value.Bool()) // lock
	{
		if (m_savedVehicleView.flFOV == 0) // not already locked
		{
			m_savedVehicleView = m_vehicleView;
			m_vehicleView.flYawMax = m_vehicleView.flYawMin =  m_vehicleView.flPitchMin = m_vehicleView.flPitchMax = 0.0f;
		}
	}
	else
	{	//unlock
		Assert(m_savedVehicleView.flFOV); // is nonzero if something is saved, is zero if nothing was saved.
		if (m_savedVehicleView.flFOV)
		{
			// m_vehicleView = m_savedVehicleView;
			m_savedVehicleView.flFOV = 0;


			m_vehicleView.flYawMax.Set(  m_savedVehicleView.flYawMax);
			m_vehicleView.flYawMin.Set(  m_savedVehicleView.flYawMin);
			m_vehicleView.flPitchMin.Set(m_savedVehicleView.flPitchMin);
			m_vehicleView.flPitchMax.Set(m_savedVehicleView.flPitchMax);

			/* // note: the straight assignments, as in the lower two lines below, do not call the = overload and thus are never transmitted!
			m_vehicleView.flYawMax = 50;  // m_savedVehicleView.flYawMax;
			m_vehicleView.flYawMin = -50; // m_savedVehicleView.flYawMin;
			m_vehicleView.flPitchMin = m_savedVehicleView.flPitchMin;
			m_vehicleView.flPitchMax = m_savedVehicleView.flPitchMax;
			*/
		}
	}
}




//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_CHOREO_VEHICLE_OPEN )
	{
		m_OnOpen.FireOutput( this, this );
		m_bLocked = false;
	}
	else if ( nEvent == AE_CHOREO_VEHICLE_CLOSE )
	{
		m_OnClose.FireOutput( this, this );
		m_bLocked = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( !pPlayer )
		return;

	ResetUseKey( pPlayer );

	GetServerVehicle()->HandlePassengerEntry( pPlayer, (value > 0) );
}


//-----------------------------------------------------------------------------
// Purpose: Return true of the player's allowed to enter / exit the vehicle
//-----------------------------------------------------------------------------
bool CPropVehicleChoreoGeneric::CanEnterVehicle( CBaseEntity *pEntity )
{
	// Prevent entering if the vehicle's being driven by an NPC
	if ( GetDriver() && GetDriver() != pEntity )
		return false;

	// Prevent entering if the vehicle's locked
	return !m_bLocked;
}


//-----------------------------------------------------------------------------
// Purpose: Return true of the player is allowed to exit the vehicle.
//-----------------------------------------------------------------------------
bool CPropVehicleChoreoGeneric::CanExitVehicle( CBaseEntity *pEntity )
{
	// Prevent exiting if the vehicle's locked, rotating, or playing an entry/exit anim.
	return ( !m_bLocked && (GetLocalAngularVelocity() == vec3_angle) && !m_bEnterAnimOn && !m_bExitAnimOn );
}


//-----------------------------------------------------------------------------
// Purpose: Override base class to add display 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::DrawDebugGeometryOverlays(void) 
{
	// Draw if BBOX is on
	if ( m_debugOverlays & OVERLAY_BBOX_BIT )
	{
	}

	BaseClass::DrawDebugGeometryOverlays();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::EnterVehicle( CBaseCombatCharacter *pPassenger )
{
	if ( pPassenger == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pPassenger );
	if ( pPlayer != NULL )
	{
		// Remove any player who may be in the vehicle at the moment
		if ( m_hPlayer )
		{
			ExitVehicle( VEHICLE_ROLE_DRIVER );
		}

		m_hPlayer = pPlayer;
		m_playerOn.FireOutput( pPlayer, this, 0 );

#if defined ( PORTAL2 )
		CPortal_Player *pPortalPlayer = ToPortalPlayer( pPlayer );
		if ( pPortalPlayer && pPortalPlayer->IsZoomed() )
		{
			pPortalPlayer->ZoomOut();
		}
#endif

		m_ServerVehicle.SoundStart();
	}
	else
	{
		// NPCs not supported yet - jdw
		Assert( 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::SetVehicleEntryAnim( bool bOn )
{
	m_bEnterAnimOn = bOn;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::ExitVehicle( int nRole )
{
	CBasePlayer *pPlayer = m_hPlayer;
	if ( !pPlayer )
		return;

	m_hPlayer = NULL;
	ResetUseKey( pPlayer );

	m_playerOff.FireOutput( pPlayer, this, 0 );
	m_bEnterAnimOn = false;

	m_ServerVehicle.SoundShutdown( 1.0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::ResetUseKey( CBasePlayer *pPlayer )
{
	pPlayer->m_afButtonPressed &= ~IN_USE;
}


//-----------------------------------------------------------------------------
// Purpose: Vehicles are permanently oriented off angle for vphysics.
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::GetVectors(Vector* pForward, Vector* pRight, Vector* pUp) const
{
	// This call is necessary to cause m_rgflCoordinateFrame to be recomputed
	const matrix3x4_t &entityToWorld = EntityToWorldTransform();

	if (pForward != NULL)
	{
		MatrixGetColumn( entityToWorld, 1, *pForward ); 
	}

	if (pRight != NULL)
	{
		MatrixGetColumn( entityToWorld, 0, *pRight ); 
	}

	if (pUp != NULL)
	{
		MatrixGetColumn( entityToWorld, 2, *pUp ); 
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CPropVehicleChoreoGeneric::GetDriver( void ) 
{ 
	return m_hPlayer; 
}

//-----------------------------------------------------------------------------
// Purpose: Prevent the player from entering / exiting the vehicle
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputLock( inputdata_t &inputdata )
{
	m_bLocked = true;
}


//-----------------------------------------------------------------------------
// Purpose: Allow the player to enter / exit the vehicle
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputUnlock( inputdata_t &inputdata )
{
	m_bLocked = false;
}


//-----------------------------------------------------------------------------
// Purpose: Force the player to enter the vehicle.
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputEnterVehicle( inputdata_t &inputdata )
{
	if ( m_bEnterAnimOn )
		return;

	// Try the activator first & use them if they are a player.
	CBasePlayer *pPlayer = ToBasePlayer( inputdata.pActivator );
	if ( pPlayer == NULL )
	{
		// Activator was not a player, just grab the single-player player.
		pPlayer = AI_GetSinglePlayer();
		if ( pPlayer == NULL )
			return;
	}

	// Force us to drop anything we're holding
	pPlayer->ForceDropOfCarriedPhysObjects();

	// FIXME: I hate code like this. I should really add a parameter to HandlePassengerEntry
	//		  to allow entry into locked vehicles
	bool bWasLocked = m_bLocked;
	m_bLocked = false;
	GetServerVehicle()->HandlePassengerEntry( pPlayer, true );
	m_bLocked = bWasLocked;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputEnterVehicleImmediate( inputdata_t &inputdata )
{
	if ( m_bEnterAnimOn )
		return;

	// Try the activator first & use them if they are a player.
	CBasePlayer *pPlayer = ToBasePlayer( inputdata.pActivator );
	if ( pPlayer == NULL )
	{
		// Activator was not a player, just grab the singleplayer player.
		pPlayer = AI_GetSinglePlayer();
		if ( pPlayer == NULL )
			return;
	}

	if ( pPlayer->IsInAVehicle() )
	{
		// Force the player out of whatever vehicle they are in.
		pPlayer->LeaveVehicle();
	}
	
	// Force us to drop anything we're holding
	pPlayer->ForceDropOfCarriedPhysObjects();

	pPlayer->GetInVehicle( GetServerVehicle(), VEHICLE_ROLE_DRIVER );
}

//-----------------------------------------------------------------------------
// Purpose: Force the player to exit the vehicle.
//-----------------------------------------------------------------------------
void CPropVehicleChoreoGeneric::InputExitVehicle( inputdata_t &inputdata )
{
	m_bForcedExit = true;
}

//-----------------------------------------------------------------------------
// Purpose: Parses the vehicle's script for the vehicle view parameters
//-----------------------------------------------------------------------------
bool CPropVehicleChoreoGeneric::ParseViewParams( const char *pScriptName )
{
	byte *pFile = UTIL_LoadFileForMe( pScriptName, NULL );
	if ( !pFile )
		return false;

	IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( (char *)pFile );
	CVehicleChoreoViewParser viewParser;
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "vehicle_view" ) )
		{
			pParse->ParseCustom( &m_vehicleView, &viewParser );
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	physcollision->VPhysicsKeyParserDestroy( pParse );
	UTIL_FreeFile( pFile );

	Precache();

	return true;
}

//========================================================================================================================================
// CRANE VEHICLE SERVER VEHICLE
//========================================================================================================================================
CPropVehicleChoreoGeneric *CChoreoGenericServerVehicle::GetVehicle( void )
{
	return (CPropVehicleChoreoGeneric *)GetDrivableVehicle();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pPlayer - 
//-----------------------------------------------------------------------------
void CChoreoGenericServerVehicle::ItemPostFrame( CBasePlayer *player )
{
	Assert( player == GetDriver() );

	GetDrivableVehicle()->ItemPostFrame( player );

	if (( player->m_afButtonPressed & IN_USE ) || GetVehicle()->ShouldForceExit() )
	{
		GetVehicle()->ClearForcedExit();
		if ( GetDrivableVehicle()->CanExitVehicle(player) )
		{
			// Let the vehicle try to play the exit animation
			if ( !HandlePassengerExit( player ) && ( player != NULL ) )
			{
				player->PlayUseDenySound();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoGenericServerVehicle::GetVehicleViewPosition( int nRole, Vector *pAbsOrigin, QAngle *pAbsAngles, float *pFOV /*= NULL*/ )
{
	// FIXME: This needs to be reconciled with the other versions of this function!
	Assert( nRole == VEHICLE_ROLE_DRIVER );
	CBasePlayer *pPlayer = ToBasePlayer( GetDrivableVehicle()->GetDriver() );
	Assert( pPlayer );

	// Use the player's eyes instead of the attachment point
	if ( GetVehicle()->m_bForcePlayerEyePoint )
	{
		// Call to BaseClass because CBasePlayer::EyePosition calls this function.
		*pAbsOrigin = pPlayer->CBasePlayer::BaseClass::EyePosition();
		*pAbsAngles = pPlayer->CBasePlayer::BaseClass::EyeAngles();
		return;
	}

	*pAbsAngles = pPlayer->EyeAngles(); // yuck. this is an in/out parameter.

	float flPitchFactor = 1.0;
	matrix3x4_t vehicleEyePosToWorld;
	Vector vehicleEyeOrigin;
	QAngle vehicleEyeAngles;
	GetVehicle()->GetAttachment( "vehicle_driver_eyes", vehicleEyeOrigin, vehicleEyeAngles );
	AngleMatrix( vehicleEyeAngles, vehicleEyePosToWorld );

	// Compute the relative rotation between the unperterbed eye attachment + the eye angles
	matrix3x4_t cameraToWorld;
	AngleMatrix( *pAbsAngles, cameraToWorld );

	matrix3x4_t worldToEyePos;
	MatrixInvert( vehicleEyePosToWorld, worldToEyePos );

	matrix3x4_t vehicleCameraToEyePos;
	ConcatTransforms( worldToEyePos, cameraToWorld, vehicleCameraToEyePos );

	// Now perterb the attachment point
	vehicleEyeAngles.x = RemapAngleRange( PITCH_CURVE_ZERO * flPitchFactor, PITCH_CURVE_LINEAR, vehicleEyeAngles.x );
	vehicleEyeAngles.z = RemapAngleRange( ROLL_CURVE_ZERO * flPitchFactor, ROLL_CURVE_LINEAR, vehicleEyeAngles.z );

	AngleMatrix( vehicleEyeAngles, vehicleEyeOrigin, vehicleEyePosToWorld );

	// Now treat the relative eye angles as being relative to this new, perterbed view position...
	matrix3x4_t newCameraToWorld;
	ConcatTransforms( vehicleEyePosToWorld, vehicleCameraToEyePos, newCameraToWorld );

	// output new view abs angles
	MatrixAngles( newCameraToWorld, *pAbsAngles );

	// UNDONE: *pOrigin would already be correct in single player if the HandleView() on the server ran after vphysics
	MatrixGetColumn( newCameraToWorld, 3, *pAbsOrigin );
}

bool CPropVehicleChoreoGeneric::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( m_bIgnorePlayerCollisions == true )
	{
		if ( collisionGroup == COLLISION_GROUP_PLAYER || collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
			return false;
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

void CPropVehicleChoreoGeneric::InputSetCanShoot( inputdata_t &inputdata )
{
	m_bPlayerCanShoot = inputdata.value.Bool();
	m_ServerVehicle.SetPlayerCanShoot( m_bPlayerCanShoot );
}

void CPropVehicleChoreoGeneric::InputUseAttachmentEyes( inputdata_t &inputdata )
{
	m_bForceEyesToAttachment = inputdata.value.Bool();
}

void CPropVehicleChoreoGeneric::InputSetMaxPitch( inputdata_t &inputdata )
{
	m_vehicleView.flPitchMax = inputdata.value.Float();
}

void CPropVehicleChoreoGeneric::InputSetMinPitch( inputdata_t &inputdata )
{
	m_vehicleView.flPitchMin = inputdata.value.Float();
}

void CPropVehicleChoreoGeneric::InputSetMaxYaw( inputdata_t &inputdata )
{
	m_vehicleView.flYawMax = inputdata.value.Float();
}

void CPropVehicleChoreoGeneric::InputSetMinYaw( inputdata_t &inputdata )
{
	m_vehicleView.flYawMin = inputdata.value.Float();
}

CVehicleChoreoViewParser::CVehicleChoreoViewParser( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleChoreoViewParser::ParseKeyValue( void *pData, const char *pKey, const char *pValue )
{
	vehicleview_t *pView = (vehicleview_t *)pData;
	// New gear?
	if ( !strcmpi( pKey, "clamp" ) )
	{
		pView->bClampEyeAngles = !!atoi( pValue );
	}
	else if ( !strcmpi( pKey, "pitchcurvezero" ) )
	{
		pView->flPitchCurveZero = atof( pValue );
	}
	else if ( !strcmpi( pKey, "pitchcurvelinear" ) )
	{
		pView->flPitchCurveLinear = atof( pValue );
	}
	else if ( !strcmpi( pKey, "rollcurvezero" ) )
	{
		pView->flRollCurveZero = atof( pValue );
	}
	else if ( !strcmpi( pKey, "rollcurvelinear" ) )
	{
		pView->flRollCurveLinear = atof( pValue );
	}
	else if ( !strcmpi( pKey, "yawmin" ) )
	{
		pView->flYawMin = atof( pValue );
	}
	else if ( !strcmpi( pKey, "yawmax" ) )
	{
		pView->flYawMax = atof( pValue );
	}
	else if ( !strcmpi( pKey, "pitchmin" ) )
	{
		pView->flPitchMin = atof( pValue );
	}
	else if ( !strcmpi( pKey, "pitchmax" ) )
	{
		pView->flPitchMax = atof( pValue );
	}
	else if ( !strcmpi( pKey, "fov" ) )
	{
		pView->flFOV = atof( pValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleChoreoViewParser::SetDefaults( void *pData ) 
{
	vehicleview_t *pView = (vehicleview_t *)pData;

	pView->bClampEyeAngles = true;

	pView->flPitchCurveZero = PITCH_CURVE_ZERO;
	pView->flPitchCurveLinear = PITCH_CURVE_LINEAR;
	pView->flRollCurveZero = ROLL_CURVE_ZERO;
	pView->flRollCurveLinear = ROLL_CURVE_LINEAR;
	pView->flFOV = CHOREO_VEHICLE_VIEW_FOV;
	pView->flYawMin = CHOREO_VEHICLE_VIEW_YAW_MIN;
	pView->flYawMax = CHOREO_VEHICLE_VIEW_YAW_MAX;
	pView->flPitchMin = CHOREO_VEHICLE_VIEW_PITCH_MIN;
	pView->flPitchMax = CHOREO_VEHICLE_VIEW_PITCH_MAX;

}
