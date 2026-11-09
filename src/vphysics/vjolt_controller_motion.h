// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include "vjolt_object.h"
#include "vjolt_environment.h"

class JoltPhysicsMotionController : public IPhysicsMotionController
{
public:
	JoltPhysicsMotionController( IMotionEvent *pHandler );
	~JoltPhysicsMotionController();

	void SetEventHandler( IMotionEvent *handler );
	void AttachObject( IPhysicsObject *pObject, bool checkIfAlreadyAttached );
	void DetachObject( IPhysicsObject *pObject );

	int CountObjects( void );
	void GetObjects( IPhysicsObject **pObjectList );
	void ClearObjects( void );
	void WakeObjects( void );

	void SetPriority( priority_t priority );

public:

	void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject );
	void OnPreSimulate( float flDeltaTime );

private:
	IMotionEvent *m_pMotionEvent;

	std::vector< JoltPhysicsObject * > m_pObjects;
};
