//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Used to fire events based on the orientation of a given entity.
//
//			Looks at its target's anglular velocity every frame and fires outputs
//			as the angular velocity passes a given threshold value.
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "eventqueue.h"
#include "mathlib/mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{
	AVELOCITY_SENSOR_NO_LAST_RESULT = -2
};

ConVar g_debug_angularsensor( "g_debug_angularsensor", "0", FCVAR_CHEAT );

class CPointAngularVelocitySensor : public CPointEntity
{
	DECLARE_CLASS( CPointAngularVelocitySensor, CPointEntity );

public:

	CPointAngularVelocitySensor();
	void Activate(void);
	void Spawn(void);
	void Think(void);

private:

	float SampleAngularVelocity(CBaseEntity *pEntity);
	int CompareToThreshold(CBaseEntity *pEntity, float flThreshold, bool bFireVelocityOutput);
	void FireCompareOutput(int nCompareResult, CBaseEntity *pActivator);
	void DrawDebugLines( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Test", .type = FIELD_VOID } ]] void InputTest( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TestWithInterval", .type = FIELD_VOID } ]] void InputTestWithInterval( inputdata_t &inputdata );

	EHANDLE m_hTargetEntity;				// Entity whose angles are being monitored.
	[[= ks::reflect::Key{ .name = "threshold" } ]] float m_flThreshold;					// The threshold angular velocity that we are looking for.
	int m_nLastCompareResult;				// The comparison result from our last measurement, expressed as -1, 0, or 1
	int m_nLastFireResult;					// The last result for which we fire the output.
	
	float m_flFireTime;
	[[= ks::reflect::Key{ .name = "fireinterval" } ]] float m_flFireInterval;
	float m_flLastAngVelocity;
	
	QAngle m_lastOrientation;

	[[= ks::reflect::Key{ .name = "axis" } ]] Vector m_vecAxis;
	[[= ks::reflect::Key{ .name = "usehelper" } ]] bool m_bUseHelper;

	// Outputs
	[[= ks::reflect::Key{ .name = "AngularVelocity" } ]] COutputFloat m_AngularVelocity;

	// Compare the target's angular velocity to the threshold velocity and fire the appropriate output.
	// These outputs are filtered by m_flFireInterval to ignore excessive oscillations.
	[[= ks::reflect::Key{ .name = "OnLessThan" } ]] COutputEvent m_OnLessThan;
	[[= ks::reflect::Key{ .name = "OnLessThanOrEqualTo" } ]] COutputEvent m_OnLessThanOrEqualTo;		
	[[= ks::reflect::Key{ .name = "OnGreaterThan" } ]] COutputEvent m_OnGreaterThan;			
	[[= ks::reflect::Key{ .name = "OnGreaterThanOrEqualTo" } ]] COutputEvent m_OnGreaterThanOrEqualTo;
	[[= ks::reflect::Key{ .name = "OnEqualTo" } ]] COutputEvent m_OnEqualTo;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(point_angularvelocitysensor, CPointAngularVelocitySensor);


IMPLEMENT_REFLECT_DATAMAP( CPointAngularVelocitySensor )



//-----------------------------------------------------------------------------
// Purpose: constructor provides default values
//-----------------------------------------------------------------------------
CPointAngularVelocitySensor::CPointAngularVelocitySensor()
{
	m_flFireInterval = 0.2f;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning after parsing keyvalues.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::Spawn(void)
{
	m_flThreshold = fabs(m_flThreshold);
	m_nLastFireResult = AVELOCITY_SENSOR_NO_LAST_RESULT;
	m_nLastCompareResult = AVELOCITY_SENSOR_NO_LAST_RESULT;
	// m_flFireInterval = 0.2;
	m_lastOrientation = vec3_angle;
}


//-----------------------------------------------------------------------------
// Purpose: Called after all entities in the map have spawned.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::Activate(void)
{
	BaseClass::Activate();

	m_hTargetEntity = gEntList.FindEntityByName( NULL, m_target );

	if (m_hTargetEntity)
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws magic lines...
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::DrawDebugLines( void )
{
	if ( m_hTargetEntity )
	{
		Vector vForward, vRight, vUp;
		AngleVectors( m_hTargetEntity->GetAbsAngles(), &vForward, &vRight, &vUp );

		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vForward * 64, 255, 0, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vRight * 64, 0, 255, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vUp * 64, 0, 0, 255, false, 0 );
	}

	if ( m_bUseHelper == true )
	{
		QAngle Angles;
		Vector vAxisForward, vAxisRight, vAxisUp;

		Vector vLine = m_vecAxis - GetAbsOrigin();

		VectorNormalize( vLine );

		VectorAngles( vLine, Angles );
		AngleVectors( Angles, &vAxisForward, &vAxisRight, &vAxisUp );

		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vAxisForward * 64, 255, 0, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vAxisRight * 64, 0, 255, 0, false, 0 );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vAxisUp * 64, 0, 0, 255, false, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the magnitude of the entity's angular velocity.
//-----------------------------------------------------------------------------
float CPointAngularVelocitySensor::SampleAngularVelocity(CBaseEntity *pEntity)
{
	if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS)
	{
		IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
		if (pPhys != NULL)
		{
			Vector vecVelocity;
			AngularImpulse vecAngVelocity;
			pPhys->GetVelocity(&vecVelocity, &vecAngVelocity);

			QAngle angles;
			pPhys->GetPosition( NULL, &angles );

			float dt = gpGlobals->curtime - GetLastThink();
			if ( dt == 0 )
				dt = 0.1;

			// HACKHACK: We don't expect a real 'delta' orientation here, just enough of an error estimate to tell if this thing
			// is trying to move, but failing.
			QAngle delta = angles - m_lastOrientation;

			if ( ( delta.Length() / dt )  < ( vecAngVelocity.Length() * 0.01 ) )
			{
				return 0.0f;
			}
			m_lastOrientation = angles;

			if ( m_bUseHelper == false )
			{
				return vecAngVelocity.Length();
			}
			else
			{
				Vector vLine = m_vecAxis - GetAbsOrigin();
				VectorNormalize( vLine );

				Vector vecWorldAngVelocity;
				pPhys->LocalToWorldVector( &vecWorldAngVelocity, vecAngVelocity );
				float flDot = DotProduct( vecWorldAngVelocity, vLine );

				return flDot;
			}
		}
	}
	else
	{
		QAngle vecAngVel = pEntity->GetLocalAngularVelocity();
		float flMax = MAX(fabs(vecAngVel[PITCH]), fabs(vecAngVel[YAW]));

		return MAX(flMax, fabs(vecAngVel[ROLL]));
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Compares the given entity's angular velocity to the threshold velocity.
// Input  : pEntity - Entity whose angular velocity is being measured.
//			flThreshold - 
// Output : Returns -1 if less than, 0 if equal to, or 1 if greater than the threshold.
//-----------------------------------------------------------------------------
int CPointAngularVelocitySensor::CompareToThreshold(CBaseEntity *pEntity, float flThreshold, bool bFireVelocityOutput)
{
	if (pEntity == NULL)
	{
		return 0;
	}

	float flAngVelocity = SampleAngularVelocity(pEntity);

	if ( g_debug_angularsensor.GetBool() )
	{
		DrawDebugLines();
	}

	if (bFireVelocityOutput && (flAngVelocity != m_flLastAngVelocity))
	{
		m_AngularVelocity.Set(flAngVelocity, pEntity, this);
		m_flLastAngVelocity = flAngVelocity;
	}

	if (flAngVelocity > flThreshold)
	{
		return 1;
	}

	if (flAngVelocity == flThreshold)
	{
		return 0;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Called every frame to sense the angular velocity of the target entity.
// Output is filtered by m_flFireInterval to ignore excessive oscillations.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::Think(void)
{
	if (m_hTargetEntity != NULL)
	{
		//
		// Check to see if the measure entity's angular velocity has been within
		// tolerance of the threshold for the given period of time.
		//
		int nCompare = CompareToThreshold(m_hTargetEntity, m_flThreshold, true);
		if (nCompare != m_nLastCompareResult)
		{
			// If we've oscillated back to where we last fired the output, don't
			// fire the same output again.
			if (nCompare == m_nLastFireResult)
			{
				m_flFireTime = 0;
			}
			else if (m_nLastCompareResult != AVELOCITY_SENSOR_NO_LAST_RESULT)
			{
				//
				// The value has changed -- reset the timer. We'll fire the output if
				// it stays at this value until the interval expires.
				//
				m_flFireTime = gpGlobals->curtime + m_flFireInterval;
			}
			
			m_nLastCompareResult = nCompare;
		}
		else if ((m_flFireTime != 0) && (gpGlobals->curtime >= m_flFireTime))
		{
			//
			// The compare result has held steady long enough -- time to
			// fire the output.
			//
			FireCompareOutput(nCompare, this);
			m_nLastFireResult = nCompare;
			m_flFireTime = 0;
		}

		SetNextThink( gpGlobals->curtime );
	}
}


//-----------------------------------------------------------------------------
// Fires the output after the fire interval if the velocity is stable. 
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::InputTestWithInterval( inputdata_t &inputdata )
{
	if (m_hTargetEntity != NULL)
	{
		m_flFireTime = gpGlobals->curtime + m_flFireInterval;
		m_nLastFireResult = AVELOCITY_SENSOR_NO_LAST_RESULT;
		m_nLastCompareResult = CompareToThreshold(m_hTargetEntity, m_flThreshold, true);

		SetNextThink( gpGlobals->curtime );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for forcing an instantaneous test of the condition.
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::InputTest( inputdata_t &inputdata )
{
	int nCompareResult = CompareToThreshold(m_hTargetEntity, m_flThreshold, false);
	FireCompareOutput(nCompareResult, inputdata.pActivator);
}

	
//-----------------------------------------------------------------------------
// Purpose: Fires the appropriate output based on the given comparison result.
// Input  : nCompareResult - 
//			pActivator - 
//-----------------------------------------------------------------------------
void CPointAngularVelocitySensor::FireCompareOutput( int nCompareResult, CBaseEntity *pActivator )
{
	if (nCompareResult == -1)
	{
		m_OnLessThan.FireOutput(pActivator, this);
		m_OnLessThanOrEqualTo.FireOutput(pActivator, this);
	}
	else if (nCompareResult == 1)
	{
		m_OnGreaterThan.FireOutput(pActivator, this);
		m_OnGreaterThanOrEqualTo.FireOutput(pActivator, this);
	}
	else
	{
		m_OnEqualTo.FireOutput(pActivator, this);
		m_OnLessThanOrEqualTo.FireOutput(pActivator, this);
		m_OnGreaterThanOrEqualTo.FireOutput(pActivator, this);
	}
}

// ============================================================================
//
//  Simple velocity sensor
//
// ============================================================================

class CPointVelocitySensor : public CPointEntity
{
	DECLARE_CLASS( CPointVelocitySensor, CPointEntity );

public:

	void Spawn();
	void Activate( void );
	void Think( void );

private:

	void SampleVelocity( void );

	EHANDLE m_hTargetEntity;				// Entity whose angles are being monitored.
	[[= ks::reflect::Key{ .name = "axis" } ]] Vector	m_vecAxis;						// Axis along which to measure the speed.
	[[= ks::reflect::Key{ .name = "enabled" } ]] bool	m_bEnabled;						// Whether we're measuring or not

	// Outputs
	float m_fPrevVelocity; // stores velocity from last frame, so we only write the output if it has changed
	[[= ks::reflect::Key{ .name = "Velocity" } ]] COutputFloat m_Velocity;

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_velocitysensor, CPointVelocitySensor );

IMPLEMENT_REFLECT_DATAMAP( CPointVelocitySensor )


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPointVelocitySensor::Spawn()
{
	Vector vLine = m_vecAxis - GetAbsOrigin();
	VectorNormalize( vLine );
	m_vecAxis = vLine;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointVelocitySensor::Activate( void )
{
	BaseClass::Activate();

	m_hTargetEntity = gEntList.FindEntityByName( NULL, m_target );
	
	if ( m_bEnabled && m_hTargetEntity )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointVelocitySensor::InputEnable( inputdata_t &inputdata )
{
	// Don't interrupt us if we're already enabled
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	
	if ( m_hTargetEntity )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointVelocitySensor::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CPointVelocitySensor::Think( void )
{
	if ( m_hTargetEntity != NULL && m_bEnabled )
	{
		SampleVelocity();
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the magnitude of the entity's angular velocity.
//-----------------------------------------------------------------------------
void CPointVelocitySensor::SampleVelocity( void )
{
	if ( m_hTargetEntity == NULL )
		return;

	Vector vecVelocity;

	if ( m_hTargetEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhys = m_hTargetEntity->VPhysicsGetObject();
		if ( pPhys != NULL )
		{
			pPhys->GetVelocity( &vecVelocity, NULL );
		}
	}
	else
	{
		vecVelocity = m_hTargetEntity->GetAbsVelocity();
	}

	/*
	float flSpeed = VectorNormalize( vecVelocity );
	float flDot = ( m_vecAxis != vec3_origin ) ? DotProduct( vecVelocity, m_vecAxis ) : 1.0f;
	*/
	// We want the component of the velocity vector in the direction of the axis, which since the
	// axis is normalized is simply their dot product (eg V . A = |V|*|A|*cos(theta) )
	m_fPrevVelocity = ( m_vecAxis != vec3_origin ) ? DotProduct( vecVelocity, m_vecAxis ) : 1.0f;

	// if it's changed since the last frame, poke the output 
	if ( m_fPrevVelocity != m_Velocity.Get() )
	{
		m_Velocity.Set( m_fPrevVelocity, NULL, NULL );
	}
}
