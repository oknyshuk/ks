
#pragma once

class JoltPhysicsObject;

abstract_class IJoltPhysicsController
{
public:
	virtual ~IJoltPhysicsController() {}

	// Called before the simulation is run
	virtual void OnPreSimulate( float flDeltaTime ) {};
	// Called after the simulation is run
	virtual void OnPostSimulate( float flDeltaTime ) {};
};

// Implemented by anything holding on to a JoltPhysicsObject that needs to hear about the
// changes it cannot see by polling: the object going away, and the object being moved in a
// way the simulation could not have predicted.
abstract_class IJoltObjectListener
{
public:
	virtual ~IJoltObjectListener() {}

	// Called whenever a physics object is destroyed
	virtual void OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject ) = 0;

	// Called when an object is teleported, so that state carried over from the previous
	// frame -- which described a configuration that no longer exists -- can be dropped.
	virtual void OnJoltPhysicsObjectTeleported( JoltPhysicsObject *pObject ) {}
};
