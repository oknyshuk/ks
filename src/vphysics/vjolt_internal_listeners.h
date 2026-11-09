// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

import std;

class JoltPhysicsObject;
class JoltPhysicsConstraint;
class JoltPhysicsFluidController;
class JoltPhysicsMotionController;
class JoltPhysicsPlayerController;
class JoltPhysicsShadowController;
class JoltPhysicsVehicleController;
class JoltPhysicsSpring;

// The simulation controllers the environment steps before (and for the vehicle also after) every
// simulation: a closed set, dispatched with std::visit.
//   OnPreSimulate( float flDeltaTime )   Called before the simulation is run
//   OnPostSimulate( float flDeltaTime )  Called after the simulation is run (only where defined)
using JoltPhysicsController = std::variant< JoltPhysicsFluidController *, JoltPhysicsMotionController *, JoltPhysicsPlayerController *,
	JoltPhysicsShadowController *, JoltPhysicsVehicleController * >;

// Everything holding on to a JoltPhysicsObject that needs to hear about the changes it cannot see
// by polling: the object going away, and the object being moved in a way the simulation could not
// have predicted. A closed set, dispatched with std::visit:
//   OnJoltPhysicsObjectDestroyed( JoltPhysicsObject *pObject )   Called whenever a physics object is destroyed
//   OnJoltPhysicsObjectTeleported( JoltPhysicsObject *pObject )  Called when an object is teleported, so that
//       state carried over from the previous frame -- which described a configuration that no longer
//       exists -- can be dropped (only where defined)
using JoltObjectListener = std::variant< JoltPhysicsConstraint *, JoltPhysicsFluidController *, JoltPhysicsMotionController *,
	JoltPhysicsPlayerController *, JoltPhysicsVehicleController *, JoltPhysicsSpring * >;
