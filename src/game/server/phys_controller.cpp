//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "entitylist.h"
#include "physics.h"
#include "vphysics/constraints.h"
#include "phys_controller.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPhysThruster;

//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CConstantForceController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	void Init( IMotionEvent::simresult_e controlType ) 
	{ 
		m_controlType = controlType;
	}

	void SetConstantForce( const Vector &linear, const AngularImpulse &angular );
	void ScaleConstantForce( float scale );
	
	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	IMotionEvent::simresult_e	m_controlType;
	Vector			m_linear;
	AngularImpulse	m_angular;
	Vector			m_linearSave;
	AngularImpulse	m_angularSave;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CConstantForceController )


void CConstantForceController::SetConstantForce( const Vector &linear, const AngularImpulse &angular )
{
	m_linear = linear;
	m_angular = angular;
	// cache these for scaling later
	m_linearSave = linear;
	m_angularSave = angular;
}

void CConstantForceController::ScaleConstantForce( float scale )
{
	m_linear = m_linearSave * scale;
	m_angular = m_angularSave * scale;
}


IMotionEvent::simresult_e CConstantForceController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear = m_linear;
	angular = m_angular;
	
	return m_controlType;
}

// UNDONE: Make these logical entities
//-----------------------------------------------------------------------------
// Purpose: This is a general entity that has a force/motion controller that 
//			simply integrates a constant linear/angular acceleration
//-----------------------------------------------------------------------------
abstract_class CPhysForce : public CPointEntity
{
public:
	DECLARE_CLASS( CPhysForce, CPointEntity );

	CPhysForce();
	~CPhysForce();

	DECLARE_DATADESC();

	virtual void OnRestore( );
	void Spawn( void );
	void Activate( void );

	void ForceOn( void );
	void ForceOff( void );
	void ActivateForce( void );
	void SetAttachedObject( EHANDLE hObject ) { m_attachedObject = hObject; }

	// Input handlers
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputActivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputDeactivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "scale", .type = FIELD_FLOAT } ]] void InputForceScale( inputdata_t &inputdata );

	void SaveForce( void );
	void ScaleForce( float scale );

	// MUST IMPLEMENT THIS IN DERIVED CLASS
	virtual void SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular ) = 0;

	// optional
	virtual void OnActivate( void ) {}

protected:	
	IPhysicsMotionController	*m_pController;

	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t		m_nameAttach;
	[[= ks::reflect::Key{ .name = "force" } ]] float			m_force;
	[[= ks::reflect::Key{ .name = "forcetime" } ]] float			m_forceTime;
	EHANDLE			m_attachedObject;
	bool			m_wasRestored;

	CConstantForceController m_integrator;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysForce )


CPhysForce::CPhysForce( void )
{
	m_pController = nullptr;
	m_wasRestored = false;
}

CPhysForce::~CPhysForce()
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
	}
}

void CPhysForce::Spawn( void )
{
	if ( m_spawnflags & SF_THRUST_LOCAL_ORIENTATION )
	{
		m_integrator.Init( IMotionEvent::SIM_LOCAL_ACCELERATION );
	}
	else
	{
		m_integrator.Init( IMotionEvent::SIM_GLOBAL_ACCELERATION );
	}
}

void CPhysForce::OnRestore( )
{
	BaseClass::OnRestore();

	if ( m_pController )
	{
		m_pController->SetEventHandler( &m_integrator );
	}
	m_wasRestored = true;
}

void CPhysForce::Activate( void )
{
	BaseClass::Activate();

	if ( m_pController )
	{
		m_pController->WakeObjects();
	}
	if ( m_wasRestored )
		return;

	if ( m_attachedObject == nullptr )
	{
		m_attachedObject = gEntList.FindEntityByName( nullptr, m_nameAttach );
	}
	
	// Let the derived class set up before we throw the switch
	OnActivate();

	if ( m_spawnflags & SF_THRUST_STARTACTIVE )
	{
		ForceOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::InputActivate( inputdata_t &inputdata )
{
	ForceOn();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::InputDeactivate( inputdata_t &inputdata )
{
	ForceOff();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::InputForceScale( inputdata_t &inputdata )
{
	ScaleForce( inputdata.value.Float() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPhysForce::ForceOn( void )
{
	if ( m_pController )
		return;

	ActivateForce();
	if ( m_forceTime )
	{
		SetNextThink( gpGlobals->curtime + m_forceTime );
		SetThink( &CPhysForce::ForceOff );
	}
}


void CPhysForce::ActivateForce( void )
{
	IPhysicsObject *pPhys = nullptr;
	if ( m_attachedObject )
	{
		pPhys = m_attachedObject->VPhysicsGetObject();
	}
	
	if ( !pPhys )
		return;

	Vector linear;
	AngularImpulse angular;

	SetupForces( pPhys, linear, angular );

	m_integrator.SetConstantForce( linear, angular );
	m_pController = physenv->CreateMotionController( &m_integrator );
	m_pController->AttachObject( pPhys, true );
	// Make sure the object is simulated
	pPhys->Wake();
}


void CPhysForce::ForceOff( void )
{
	if ( !m_pController )
		return;

	physenv->DestroyMotionController( m_pController );
	m_pController = nullptr;
	SetThink( nullptr );
	SetNextThink( TICK_NEVER_THINK );
	IPhysicsObject *pPhys = nullptr;
	if ( m_attachedObject )
	{
		pPhys = m_attachedObject->VPhysicsGetObject();
		if ( pPhys )
		{
			pPhys->Wake();
		}
	}
}


void CPhysForce::ScaleForce( float scale )
{
	if ( !m_pController )
		ForceOn();

	m_integrator.ScaleConstantForce( scale );
	m_pController->WakeObjects();
}


//-----------------------------------------------------------------------------
// Purpose: A rocket-engine/thruster based on the force controller above
//			Calculate the force (and optional torque) that the engine would create
//-----------------------------------------------------------------------------
class CPhysThruster : public CPhysForce
{
	DECLARE_CLASS( CPhysThruster, CPhysForce );
public:

	virtual void OnActivate( void );
	virtual void SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular );

private:
	friend CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags );
	
	Vector			m_localOrigin;
};

LINK_ENTITY_TO_CLASS( phys_thruster, CPhysThruster );


//-----------------------------------------------------------------------------
// Purpose: Use this to spawn a keepupright controller via code instead of map-placed
//-----------------------------------------------------------------------------
CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags )
{
	CPhysThruster *pThruster = ( CPhysThruster* )CBaseEntity::Create( "phys_thruster", vecOrigin, vecAngles, pOwner );
	if ( pThruster )
	{
		pThruster->AddSpawnFlags( nFlags );
		pThruster->m_attachedObject = pOwner;
		pThruster->m_force = flForce;
		pThruster->m_forceTime = flForcetime;
		pThruster->Spawn();
		pThruster->Activate();
		if ( bActive )
			pThruster->ForceOn();
	}

	return pThruster;
}

void CPhysThruster::OnActivate( void )
{
	if ( m_attachedObject != nullptr )
	{
		matrix3x4_t worldToAttached, thrusterToAttached;
		MatrixInvert( m_attachedObject->EntityToWorldTransform(), worldToAttached );
		ConcatTransforms( worldToAttached, EntityToWorldTransform(), thrusterToAttached );
		MatrixGetColumn( thrusterToAttached, 3, m_localOrigin );

		if ( HasSpawnFlags( SF_THRUST_LOCAL_ORIENTATION ) )
		{
			QAngle angles;
			MatrixAngles( thrusterToAttached, angles );
			SetLocalAngles( angles );
		}
		// maintain the local relationship with this entity
		// it may move before the thruster is activated
		if ( HasSpawnFlags( SF_THRUST_IGNORE_POS ) )
		{
			m_localOrigin.Init();
		}
	}
}

// utility function to duplicate this call in local space
void CalculateVelocityOffsetLocal( IPhysicsObject *pPhys, const Vector &forceLocal, const Vector &positionLocal, Vector &outVelLocal, AngularImpulse &outAngular )
{
	Vector posWorld, forceWorld;
	pPhys->LocalToWorld( &posWorld, positionLocal );
	pPhys->LocalToWorldVector( &forceWorld, forceLocal );
	Vector velWorld;
	pPhys->CalculateVelocityOffset( forceWorld, posWorld, &velWorld, &outAngular );
	pPhys->WorldToLocalVector( &outVelLocal, velWorld );
}

void CPhysThruster::SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular )
{
	Vector thrustVector;
	AngleVectors( GetLocalAngles(), &thrustVector );
	thrustVector *= m_force;

	// multiply the force by mass (it's actually just an acceleration)
	if ( m_spawnflags & SF_THRUST_MASS_INDEPENDENT )
	{
		thrustVector *= pPhys->GetMass();
	}
	if ( m_spawnflags & SF_THRUST_LOCAL_ORIENTATION )
	{
		CalculateVelocityOffsetLocal( pPhys, thrustVector, m_localOrigin, linear, angular );
	}
	else
	{
		Vector position;
		VectorTransform( m_localOrigin, m_attachedObject->EntityToWorldTransform(), position );
		pPhys->CalculateVelocityOffset( thrustVector, position, &linear, &angular );
	}

	if ( !(m_spawnflags & SF_THRUST_FORCE) )
	{
		// clear out force
		linear.Init();
	}

	if ( !(m_spawnflags & SF_THRUST_TORQUE) )
	{
		// clear out torque
		angular.Init();
	}
}


//-----------------------------------------------------------------------------
// Purpose: A controllable motor - exerts torque
//-----------------------------------------------------------------------------
class CPhysTorque : public CPhysForce
{
	DECLARE_CLASS( CPhysTorque, CPhysForce );
public:
	DECLARE_DATADESC();
	void Spawn( void );
	virtual void SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular );
private:	
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector m_axis;
};

IMPLEMENT_REFLECT_DATAMAP( CPhysTorque )

LINK_ENTITY_TO_CLASS( phys_torque, CPhysTorque );

void CPhysTorque::Spawn( void )
{
	// force spawnflags to agree with implementation of this class
	m_spawnflags |= SF_THRUST_TORQUE | SF_THRUST_MASS_INDEPENDENT;
	m_spawnflags &= ~SF_THRUST_FORCE;

	m_axis -= GetAbsOrigin();
	VectorNormalize(m_axis);
	UTIL_SnapDirectionToAxis( m_axis );
	BaseClass::Spawn();
}

void CPhysTorque::SetupForces( IPhysicsObject *pPhys, Vector &linear, AngularImpulse &angular )
{
	// clear out force
	linear.Init();

	matrix3x4_t matrix;
	pPhys->GetPositionMatrix( &matrix );

	// transform motor axis to local space
	Vector axis_ls;
	VectorIRotate( m_axis, matrix, axis_ls );

	// Set torque to be around selected axis
	angular = axis_ls * m_force;
}



//-----------------------------------------------------------------------------
// Purpose: This class only implements the IMotionEvent-specific behavior
//			It keeps track of the forces so they can be integrated
//-----------------------------------------------------------------------------
class CMotorController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	float		m_speed;
	float		m_maxTorque;
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector		m_axis;
	[[= ks::reflect::Key{ .name = "inertiafactor" } ]] float		m_inertiaFactor;

	float		m_lastSpeed;
	float		m_lastAcceleration;
	float		m_lastForce;
	float		m_restistanceDamping;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CMotorController )


IMotionEvent::simresult_e CMotorController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear = vec3_origin;
	angular = vec3_origin;

	if ( m_speed == 0 )
		return SIM_NOTHING;

	matrix3x4_t matrix;
	pObject->GetPositionMatrix( &matrix );
	AngularImpulse currentRotAxis;
	
	// currentRotAxis is in local space
	pObject->GetVelocity( nullptr, &currentRotAxis );
	// transform motor axis to local space
	Vector motorAxis_ls;
	VectorIRotate( m_axis, matrix, motorAxis_ls );
	float currentSpeed = DotProduct( currentRotAxis, motorAxis_ls );
	
	float inertia = DotProductAbs( pObject->GetInertia(), motorAxis_ls );

	// compute absolute acceleration, don't integrate over the timestep
	float accel = m_speed - currentSpeed;
	float rotForce = accel * inertia * m_inertiaFactor;

	// BUGBUG: This heuristic is a little flaky
	// UNDONE: Make a better heuristic for speed control
	if ( fabsf(m_lastAcceleration) > 0 )
	{
		float deltaSpeed = currentSpeed - m_lastSpeed;
		// make sure they are going the same way
		if ( deltaSpeed * accel > 0 )
		{
			float factor = deltaSpeed / m_lastAcceleration;
			factor = 1 - clamp( factor, 0, 1 );
			rotForce += m_lastForce * factor * m_restistanceDamping;
		}
		else 
		{
			if ( currentSpeed != 0 )
			{
				// have we reached a steady state that isn't our target?
				float increase = deltaSpeed / m_lastAcceleration;
				if ( fabsf(increase) < 0.05 )
				{
					rotForce += m_lastForce * m_restistanceDamping;
				}
			}
		}
	}
	// -------------------------------------------------------


	if ( m_maxTorque != 0 )
	{
		if ( rotForce > m_maxTorque )
		{
			rotForce = m_maxTorque;
		}
		else if ( rotForce < -m_maxTorque )
		{
			rotForce = -m_maxTorque;
		}
	}

	m_lastForce = rotForce;
	m_lastAcceleration = (rotForce / inertia);
	m_lastSpeed = currentSpeed;

	// this is in local space
	angular = motorAxis_ls * rotForce;
	
	return SIM_LOCAL_FORCE;
}

#define SF_MOTOR_START_ON		0x0001				// starts on by default
#define SF_MOTOR_NOCOLLIDE		0x0002				// don't collide with world geometry
#define SF_MOTOR_HINGE			0x0004				// motor also acts as a hinge constraining the object to this axis
// NOTE: THIS DOESN'T WORK YET
#define SF_MOTOR_LOCAL			0x0008				// Maintain local relationship with the attached object

class CPhysMotor : public CLogicalEntity
{
	DECLARE_CLASS( CPhysMotor, CLogicalEntity );
public:
	~CPhysMotor();
	DECLARE_DATADESC();
	void Spawn( void );
	void Activate( void );
	void Think( void );

	void TurnOn( void );
	void TargetSpeedChanged( void );
	void OnRestore();

	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetTargetSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	void CalculateAcceleration();

	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t	m_nameAttach;
	EHANDLE		m_attachedObject;
	[[= ks::reflect::Key{ .name = "spinup" } ]] float		m_spinUp;
	[[= ks::reflect::Key{ .name = "addangaccel" } ]] float		m_additionalAcceleration;
	float		m_angularAcceleration;
	float		m_lastTime;
	// FIXME: can we remove m_flSpeed from CBaseEntity?
	//float m_flSpeed;

	IPhysicsConstraint	*m_pHinge;
	IPhysicsMotionController *m_pController;
	CMotorController m_motor;
};


IMPLEMENT_REFLECT_DATAMAP( CPhysMotor )

LINK_ENTITY_TO_CLASS( phys_motor, CPhysMotor );


void CPhysMotor::CalculateAcceleration()
{
	if ( m_spinUp )
	{
		m_angularAcceleration = fabsf(m_flSpeed / m_spinUp);
	}
	else
	{
		m_angularAcceleration = fabsf(m_flSpeed);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that sets a speed to spin up or down to.
//-----------------------------------------------------------------------------
void CPhysMotor::InputSetTargetSpeed( inputdata_t &inputdata )
{
	if ( m_flSpeed == inputdata.value.Float() )
		return;

	m_flSpeed = inputdata.value.Float();
	TargetSpeedChanged();
	CalculateAcceleration();
}


void CPhysMotor::TargetSpeedChanged( void )
{
	SetNextThink( gpGlobals->curtime );
	m_lastTime = gpGlobals->curtime;
	m_pController->WakeObjects();
}


//------------------------------------------------------------------------------
// Purpose: Input handler that turns the motor on.
//------------------------------------------------------------------------------
void CPhysMotor::InputTurnOn( inputdata_t &inputdata )
{
	TurnOn();
}


//------------------------------------------------------------------------------
// Purpose: Input handler that turns the motor off.
//------------------------------------------------------------------------------
void CPhysMotor::InputTurnOff( inputdata_t &inputdata )
{
	m_motor.m_speed = 0;
	SetNextThink( TICK_NEVER_THINK );
}


CPhysMotor::~CPhysMotor()
{
	if ( m_attachedObject && m_pHinge )
	{
		IPhysicsObject *pPhys = m_attachedObject->VPhysicsGetObject();
		if ( pPhys )
		{
			PhysClearGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP);
		}
	}

	physenv->DestroyConstraint( m_pHinge );
	physenv->DestroyMotionController( m_pController );
}


void CPhysMotor::Spawn( void )
{
	m_motor.m_axis -= GetLocalOrigin();
	float axisLength = VectorNormalize(m_motor.m_axis);
	// double check that the axis is at least a unit long. If not, warn and self-destruct.
	if ( axisLength > 1.0f )
	{
		UTIL_SnapDirectionToAxis( m_motor.m_axis );
	}
	else
	{
		Warning("phys_motor %s does not have a valid axis helper, and self-destructed!\n", GetDebugName());

		m_motor.m_speed = 0;
		SetNextThink( TICK_NEVER_THINK );

		UTIL_Remove(this);
	}
}


void CPhysMotor::TurnOn( void )
{
	CBaseEntity *pAttached = m_attachedObject;
	if ( !pAttached )
		return;

	IPhysicsObject *pPhys = pAttached->VPhysicsGetObject();
	if ( pPhys )
	{
		m_pController->WakeObjects();
		// If the current speed is zero, the objects can run a tick without getting torque'd and go back to sleep
		// so force a think now and have some acceleration happen before the controller gets called.
		m_lastTime = gpGlobals->curtime - TICK_INTERVAL;
		Think();
	}
}


void CPhysMotor::Activate( void )
{
	BaseClass::Activate();
	
	// This gets called after all objects spawn and after all objects restore
	if ( m_attachedObject == nullptr )
	{
		CBaseEntity *pAttach = gEntList.FindEntityByName( nullptr, m_nameAttach );
		if ( pAttach && pAttach->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			m_attachedObject = pAttach;
			IPhysicsObject *pPhys = m_attachedObject->VPhysicsGetObject();
			CalculateAcceleration();
			matrix3x4_t matrix;
			pPhys->GetPositionMatrix( &matrix );
			Vector motorAxis_ls;
			VectorIRotate( m_motor.m_axis, matrix, motorAxis_ls );
			float inertia = DotProductAbs( pPhys->GetInertia(), motorAxis_ls );
			m_motor.m_maxTorque = inertia * m_motor.m_inertiaFactor * (m_angularAcceleration + m_additionalAcceleration);
			m_motor.m_restistanceDamping = 1.0f;
		}
	}

	if ( m_attachedObject )
	{
		IPhysicsObject *pPhys = m_attachedObject->VPhysicsGetObject();

		// create a hinge constraint for this object?
		if ( m_spawnflags & SF_MOTOR_HINGE )
		{
			// UNDONE: Don't do this on restore?
			if ( !m_pHinge )
			{
				constraint_hingeparams_t hingeParams;
				hingeParams.Defaults();
				hingeParams.worldAxisDirection = m_motor.m_axis;
				hingeParams.worldPosition = GetLocalOrigin();

				m_pHinge = physenv->CreateHingeConstraint( g_PhysWorldObject, pPhys, nullptr, hingeParams );
				m_pHinge->SetGameData( (void *)this );
				// can't grab this object
				PhysSetGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP);
			}

			if ( m_spawnflags & SF_MOTOR_NOCOLLIDE )
			{
				PhysDisableEntityCollisions( g_PhysWorldObject, pPhys );
			}
		}
		else
		{
			m_pHinge = nullptr;
		}

		// NOTE: On restore, this path isn't run because m_pController will not be NULL
		if ( !m_pController )
		{
			m_pController = physenv->CreateMotionController( &m_motor );
			m_pController->AttachObject( m_attachedObject->VPhysicsGetObject(), false );

			if ( m_spawnflags & SF_MOTOR_START_ON )
			{
				TurnOn();
			}
		}
	}
}

void CPhysMotor::OnRestore()
{
	BaseClass::OnRestore();
	// Need to do this on restore since there's no good way to save this
	if ( m_pController )
	{
		m_pController->SetEventHandler( &m_motor );
	}
}

void CPhysMotor::Think( void )
{
	// angular acceleration is always positive - it should be treated as a magnitude - the controller 
	// will apply it in the proper direction
	Assert(m_angularAcceleration>=0);

	m_motor.m_speed = UTIL_Approach( m_flSpeed, m_motor.m_speed, m_angularAcceleration*(gpGlobals->curtime-m_lastTime) );
	m_lastTime = gpGlobals->curtime;
	if ( m_motor.m_speed != m_flSpeed )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//======================================================================================
// KEEPUPRIGHT CONTROLLER
//======================================================================================

class CKeepUpright : public CPointEntity, public IMotionEvent
{
	DECLARE_CLASS( CKeepUpright, CPointEntity );
public:
	DECLARE_DATADESC();

	CKeepUpright();
	~CKeepUpright();
	void Spawn();
	void Activate();

	// IMotionEvent
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	// Inputs
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata )
	{
		m_bActive = true;
	}
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata )
	{
		m_bActive = false;
	}

	[[= ks::reflect::Input{ .name = "SetAngularLimit", .type = FIELD_FLOAT } ]] void InputSetAngularLimit( inputdata_t &inputdata )
	{
		m_angularLimit = inputdata.value.Float();
	}

private:	
	friend CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive );

	Vector						m_worldGoalAxis;
	Vector						m_localTestAxis;
	IPhysicsMotionController	*m_pController;
	[[= ks::reflect::Key{ .name = "attach1" } ]] string_t					m_nameAttach;
	EHANDLE						m_attachedObject;
	[[= ks::reflect::Key{ .name = "angularlimit" } ]] float						m_angularLimit;
	bool						m_bActive;
	bool						m_bDampAllRotation;
};

#define SF_KEEPUPRIGHT_START_INACTIVE		0x0001

LINK_ENTITY_TO_CLASS( phys_keepupright, CKeepUpright );

IMPLEMENT_REFLECT_DATAMAP( CKeepUpright )

CKeepUpright::CKeepUpright()
{
	// by default, recover from up to 15 degrees / sec angular velocity
	m_angularLimit = 15;
	m_attachedObject = nullptr;
	m_bDampAllRotation = false;
}

CKeepUpright::~CKeepUpright()
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = nullptr;
	}
}

void CKeepUpright::Spawn()
{
	// align the object's local Z axis
	m_localTestAxis.Init( 0, 0, 1 );
	// Use our Up axis so mapmakers can orient us arbitrarily
	GetVectors( nullptr, nullptr, &m_worldGoalAxis );

	SetMoveType( MOVETYPE_NONE );

	if ( m_spawnflags & SF_KEEPUPRIGHT_START_INACTIVE )
	{
		m_bActive = false;
	}
	else
	{
		m_bActive = true;
	}
}

void CKeepUpright::Activate()
{
	BaseClass::Activate();
	
	if ( !m_pController )
	{
		// This case occurs when spawning
		IPhysicsObject *pPhys;
		if ( m_attachedObject )
		{
			pPhys = m_attachedObject->VPhysicsGetObject();
		}
		else
		{
			pPhys = FindPhysicsObjectByName( STRING(m_nameAttach), this );
		}

		if ( !pPhys )
		{
			UTIL_Remove(this);
			return;
		}
		// HACKHACK: Due to changes in the vehicle simulator the keepupright controller used in coast_01 is unstable
		// force it to have perfect damping to compensate.
		// detect it using the hack of angular limit == 150, attached to a vehicle
		// Fixing it in the code is the simplest course of action presently

		m_pController = physenv->CreateMotionController( (IMotionEvent *)this );
		m_pController->AttachObject( pPhys, false );
	}
	else
	{
		// This case occurs when restoring
		m_pController->SetEventHandler( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use this to spawn a keepupright controller via code instead of map-placed
//-----------------------------------------------------------------------------
CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive )
{
	CKeepUpright *pKeepUpright = (CKeepUpright*)CBaseEntity::Create( "phys_keepupright", vecOrigin, vecAngles, pOwner );
	if ( pKeepUpright )
	{
		pKeepUpright->m_attachedObject = pOwner;
		pKeepUpright->m_angularLimit = flAngularLimit;
		if ( !bActive )
		{
			pKeepUpright->AddSpawnFlags( SF_KEEPUPRIGHT_START_INACTIVE );
		}
		pKeepUpright->Spawn();
		pKeepUpright->Activate();
	}

	return pKeepUpright;
}

IMotionEvent::simresult_e CKeepUpright::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	if ( !m_bActive )
		return SIM_NOTHING;

	linear.Init();

	AngularImpulse angVel;
	pObject->GetVelocity( nullptr, &angVel );

	matrix3x4_t matrix;
	// get the object's local to world transform
	pObject->GetPositionMatrix( &matrix );

	// Get the alignment axis in object space
	Vector currentLocalTargetAxis;
	VectorIRotate( m_worldGoalAxis, matrix, currentLocalTargetAxis );

	float invDeltaTime = (1/deltaTime);

	if ( m_bDampAllRotation )
	{
		angular = ComputeRotSpeedToAlignAxes( m_localTestAxis, currentLocalTargetAxis, angVel, 0, invDeltaTime, m_angularLimit );
		angular -= angVel;
		angular *= invDeltaTime;
		return SIM_LOCAL_ACCELERATION;
	}

	angular = ComputeRotSpeedToAlignAxes( m_localTestAxis, currentLocalTargetAxis, angVel, 1.0, invDeltaTime, m_angularLimit );
	angular *= invDeltaTime;


	return SIM_LOCAL_ACCELERATION;
}


// computes the torque necessary to align testAxis with alignAxis
AngularImpulse ComputeRotSpeedToAlignAxes( const Vector &testAxis, const Vector &alignAxis, const AngularImpulse &currentSpeed, float damping, float scale, float maxSpeed )
{
	Vector rotationAxis = CrossProduct( testAxis, alignAxis );

	// atan2() is well defined, so do a Dot & Cross instead of asin(Cross)
	float cosine = DotProduct( testAxis, alignAxis );
	float sine = VectorNormalize( rotationAxis );
	float angle = atan2( sine, cosine );

	angle = RAD2DEG(angle);
	AngularImpulse angular = rotationAxis * scale * angle;
	angular -= rotationAxis * damping * DotProduct( currentSpeed, rotationAxis );

	float len = VectorNormalize( angular );

	if ( len > maxSpeed )
	{
		len = maxSpeed;
	}

	return angular * len;
}

