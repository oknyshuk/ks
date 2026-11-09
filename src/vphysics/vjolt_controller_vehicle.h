// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include "vjolt_object.h" // IJoltObjectListener
#include "vjolt_environment.h" // IJoltPhysicsController
#include "vphysics/vehicles.h"

struct JoltPhysicsWheel
{
	JoltPhysicsObject* pObject = nullptr;
	bool InWater = false;
	float Depth = 0.0f;
};

struct JoltPhysicsInternalVehicleState
{
	bool  EngineDisabled = false;
	float BoostDelay = 0.0f;
	float BoosterRemainingTime = 0.0f;
	float LargestWheelRadius = 0.0f;
};

class JoltPhysicsVehicleController final : public IPhysicsVehicleController
{
public:
	static constexpr int MaxWheels = VEHICLE_MAX_WHEEL_COUNT;

	JoltPhysicsVehicleController( JoltPhysicsEnvironment *pEnvironment, JPH::PhysicsSystem *pPhysicsSystem, JoltPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace );
	~JoltPhysicsVehicleController();

	void				Update( float dt, vehicle_controlparams_t &controls );
	const vehicle_operatingparams_t & GetOperatingParams();
	const vehicleparams_t & GetVehicleParams();
	vehicleparams_t &	GetVehicleParamsForChange();
	float				UpdateBooster( float dt );
	int					GetWheelCount( void );
	IPhysicsObject *	GetWheel( int index );
	bool				GetWheelContactPoint( int index, Vector *pContactPoint, int *pSurfaceProps );
	void				SetSpringLength( int wheelIndex, float length );
	void				SetWheelFriction( int wheelIndex, float friction );

	void				OnVehicleEnter( void );
	void				OnVehicleExit( void );

	void				SetEngineDisabled( bool bDisable );
	bool				IsEngineDisabled( void );

	void				GetCarSystemDebugData( vehicle_debugcarsystem_t &debugCarSystem );
	void				VehicleDataReload();

public:
	// IJoltObjectListener
	void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject );

	float GetSpeed();

	// IJoltPhysicsController
	void OnPreSimulate( float flDeltaTime );
	void OnPostSimulate( float flDeltaTime );

private:

	void HandleBoostKey();
	void HandleBoostDecay();

	void CreateWheel( JPH::VehicleConstraintSettings &vehicleSettings, matrix3x4_t &bodyMatrix, int axleIdx, int wheelIdx );
	void CreateWheels( JPH::VehicleConstraintSettings& vehicleSettings );

	JPH::WheeledVehicleControllerSettings *CreateVehicleController();
	JPH::WheeledVehicleController *GetWheeledVehicleController();

	matrix3x4_t GetBodyMatrix() const;

	void DetachObject();

	JoltPhysicsEnvironment					*m_pEnvironment = nullptr;
	JPH::PhysicsSystem						*m_pPhysicsSystem = nullptr;
	JoltPhysicsObject						*m_pCarBodyObject = nullptr;
	vehicleparams_t							m_VehicleParams = {};
	unsigned int							m_VehicleType = 0u;

	vehicle_operatingparams_t				m_OperatingParams = {};
	vehicle_controlparams_t					m_ControlParams = {};

	std::vector< JoltPhysicsWheel >			m_Wheels;

	float									m_TotalWheelMass = 0.0f;
	JoltPhysicsInternalVehicleState			m_InternalState;

	JPH::Ref< JPH::VehicleConstraint >		m_VehicleConstraint;
	JPH::Ref< JPH::VehicleCollisionTester >	m_Tester;


};
