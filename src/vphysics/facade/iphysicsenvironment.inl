// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsEnvironment class to JoltPhysicsEnvironment. Included by the source that defines JoltPhysicsEnvironment.

void IPhysicsEnvironment::SetDebugOverlay( CreateInterfaceFn debugOverlayFactory )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetDebugOverlay( debugOverlayFactory ); }
IVPhysicsDebugOverlay * IPhysicsEnvironment::GetDebugOverlay()
{ return static_cast<JoltPhysicsEnvironment*>(this)->GetDebugOverlay(); }
void IPhysicsEnvironment::SetGravity( const Vector &gravityVector )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetGravity( gravityVector ); }
void IPhysicsEnvironment::GetGravity( Vector *pGravityVector ) const
{ static_cast<const JoltPhysicsEnvironment*>(this)->GetGravity( pGravityVector ); }
void IPhysicsEnvironment::SetAirDensity( float density )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetAirDensity( density ); }
float IPhysicsEnvironment::GetAirDensity() const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetAirDensity(); }
IPhysicsObject * IPhysicsEnvironment::CreatePolyObject( const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreatePolyObject( pCollisionModel, materialIndex, position, angles, pParams ); }
IPhysicsObject * IPhysicsEnvironment::CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreatePolyObjectStatic( pCollisionModel, materialIndex, position, angles, pParams ); }
IPhysicsObject * IPhysicsEnvironment::CreateSphereObject( float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateSphereObject( radius, materialIndex, position, angles, pParams, isStatic ); }
void IPhysicsEnvironment::DestroyObject( IPhysicsObject *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyObject( p0 ); }
IPhysicsFluidController * IPhysicsEnvironment::CreateFluidController( IPhysicsObject *pFluidObject, fluidparams_t *pParams )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateFluidController( pFluidObject, pParams ); }
void IPhysicsEnvironment::DestroyFluidController( IPhysicsFluidController *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyFluidController( p0 ); }
IPhysicsSpring * IPhysicsEnvironment::CreateSpring( IPhysicsObject *pObjectStart, IPhysicsObject *pObjectEnd, springparams_t *pParams )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateSpring( pObjectStart, pObjectEnd, pParams ); }
void IPhysicsEnvironment::DestroySpring( IPhysicsSpring *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroySpring( p0 ); }
IPhysicsConstraint * IPhysicsEnvironment::CreateRagdollConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateRagdollConstraint( pReferenceObject, pAttachedObject, pGroup, ragdoll ); }
IPhysicsConstraint * IPhysicsEnvironment::CreateHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateHingeConstraint( pReferenceObject, pAttachedObject, pGroup, hinge ); }
IPhysicsConstraint * IPhysicsEnvironment::CreateFixedConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateFixedConstraint( pReferenceObject, pAttachedObject, pGroup, fixed ); }
IPhysicsConstraint * IPhysicsEnvironment::CreateSlidingConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateSlidingConstraint( pReferenceObject, pAttachedObject, pGroup, sliding ); }
IPhysicsConstraint * IPhysicsEnvironment::CreateBallsocketConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateBallsocketConstraint( pReferenceObject, pAttachedObject, pGroup, ballsocket ); }
IPhysicsConstraint * IPhysicsEnvironment::CreatePulleyConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreatePulleyConstraint( pReferenceObject, pAttachedObject, pGroup, pulley ); }
IPhysicsConstraint * IPhysicsEnvironment::CreateLengthConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateLengthConstraint( pReferenceObject, pAttachedObject, pGroup, length ); }
void IPhysicsEnvironment::DestroyConstraint( IPhysicsConstraint *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyConstraint( p0 ); }
IPhysicsConstraintGroup * IPhysicsEnvironment::CreateConstraintGroup( const constraint_groupparams_t &groupParams )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateConstraintGroup( groupParams ); }
void IPhysicsEnvironment::DestroyConstraintGroup( IPhysicsConstraintGroup *pGroup )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyConstraintGroup( pGroup ); }
IPhysicsShadowController * IPhysicsEnvironment::CreateShadowController( IPhysicsObject *pObject, bool allowTranslation, bool allowRotation )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateShadowController( pObject, allowTranslation, allowRotation ); }
void IPhysicsEnvironment::DestroyShadowController( IPhysicsShadowController *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyShadowController( p0 ); }
IPhysicsPlayerController * IPhysicsEnvironment::CreatePlayerController( IPhysicsObject *pObject )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreatePlayerController( pObject ); }
void IPhysicsEnvironment::DestroyPlayerController( IPhysicsPlayerController *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyPlayerController( p0 ); }
IPhysicsMotionController * IPhysicsEnvironment::CreateMotionController( IMotionEvent *pHandler )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateMotionController( pHandler ); }
void IPhysicsEnvironment::DestroyMotionController( IPhysicsMotionController *pController )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyMotionController( pController ); }
IPhysicsVehicleController * IPhysicsEnvironment::CreateVehicleController( IPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace )
{ return static_cast<JoltPhysicsEnvironment*>(this)->CreateVehicleController( pVehicleBodyObject, params, nVehicleType, pGameTrace ); }
void IPhysicsEnvironment::DestroyVehicleController( IPhysicsVehicleController *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyVehicleController( p0 ); }
void IPhysicsEnvironment::SetCollisionSolver( IPhysicsCollisionSolver *pSolver )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetCollisionSolver( pSolver ); }
void IPhysicsEnvironment::Simulate( float deltaTime )
{ static_cast<JoltPhysicsEnvironment*>(this)->Simulate( deltaTime ); }
bool IPhysicsEnvironment::IsInSimulation() const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->IsInSimulation(); }
float IPhysicsEnvironment::GetSimulationTimestep() const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetSimulationTimestep(); }
void IPhysicsEnvironment::SetSimulationTimestep( float timestep )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetSimulationTimestep( timestep ); }
float IPhysicsEnvironment::GetSimulationTime() const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetSimulationTime(); }
void IPhysicsEnvironment::ResetSimulationClock()
{ static_cast<JoltPhysicsEnvironment*>(this)->ResetSimulationClock(); }
float IPhysicsEnvironment::GetNextFrameTime() const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetNextFrameTime(); }
void IPhysicsEnvironment::SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetCollisionEventHandler( pCollisionEvents ); }
void IPhysicsEnvironment::SetObjectEventHandler( IPhysicsObjectEvent *pObjectEvents )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetObjectEventHandler( pObjectEvents ); }
void IPhysicsEnvironment::SetConstraintEventHandler( IPhysicsConstraintEvent *pConstraintEvents )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetConstraintEventHandler( pConstraintEvents ); }
void IPhysicsEnvironment::SetQuickDelete( bool bQuick )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetQuickDelete( bQuick ); }
int IPhysicsEnvironment::GetActiveObjectCount() const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetActiveObjectCount(); }
void IPhysicsEnvironment::GetActiveObjects( IPhysicsObject **pOutputObjectList ) const
{ static_cast<const JoltPhysicsEnvironment*>(this)->GetActiveObjects( pOutputObjectList ); }
const IPhysicsObject ** IPhysicsEnvironment::GetObjectList( int *pOutputObjectCount ) const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetObjectList( pOutputObjectCount ); }
bool IPhysicsEnvironment::TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment )
{ return static_cast<JoltPhysicsEnvironment*>(this)->TransferObject( pObject, pDestinationEnvironment ); }
void IPhysicsEnvironment::CleanupDeleteList()
{ static_cast<JoltPhysicsEnvironment*>(this)->CleanupDeleteList(); }
void IPhysicsEnvironment::EnableDeleteQueue( bool enable )
{ static_cast<JoltPhysicsEnvironment*>(this)->EnableDeleteQueue( enable ); }
bool IPhysicsEnvironment::IsCollisionModelUsed( CPhysCollide *pCollide ) const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->IsCollisionModelUsed( pCollide ); }
void IPhysicsEnvironment::TraceRay( const Ray_t &ray, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace )
{ static_cast<JoltPhysicsEnvironment*>(this)->TraceRay( ray, fMask, pTraceFilter, pTrace ); }
void IPhysicsEnvironment::SweepCollideable( const CPhysCollide *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, const QAngle &vecAngles, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace )
{ static_cast<JoltPhysicsEnvironment*>(this)->SweepCollideable( pCollide, vecAbsStart, vecAbsEnd, vecAngles, fMask, pTraceFilter, pTrace ); }
void IPhysicsEnvironment::GetPerformanceSettings( physics_performanceparams_t *pOutput ) const
{ static_cast<const JoltPhysicsEnvironment*>(this)->GetPerformanceSettings( pOutput ); }
void IPhysicsEnvironment::SetPerformanceSettings( const physics_performanceparams_t *pSettings )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetPerformanceSettings( pSettings ); }
void IPhysicsEnvironment::ReadStats( physics_stats_t *pOutput )
{ static_cast<JoltPhysicsEnvironment*>(this)->ReadStats( pOutput ); }
void IPhysicsEnvironment::ClearStats()
{ static_cast<JoltPhysicsEnvironment*>(this)->ClearStats(); }
unsigned int IPhysicsEnvironment::GetObjectSerializeSize( IPhysicsObject *pObject ) const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetObjectSerializeSize( pObject ); }
void IPhysicsEnvironment::SerializeObjectToBuffer( IPhysicsObject *pObject, unsigned char *pBuffer, unsigned int bufferSize )
{ static_cast<JoltPhysicsEnvironment*>(this)->SerializeObjectToBuffer( pObject, pBuffer, bufferSize ); }
IPhysicsObject * IPhysicsEnvironment::UnserializeObjectFromBuffer( void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions )
{ return static_cast<JoltPhysicsEnvironment*>(this)->UnserializeObjectFromBuffer( pGameData, pBuffer, bufferSize, enableCollisions ); }
void IPhysicsEnvironment::EnableConstraintNotify( bool bEnable )
{ static_cast<JoltPhysicsEnvironment*>(this)->EnableConstraintNotify( bEnable ); }
void IPhysicsEnvironment::DebugCheckContacts()
{ static_cast<JoltPhysicsEnvironment*>(this)->DebugCheckContacts(); }
void IPhysicsEnvironment::SetAlternateGravity( const Vector &gravityVector )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetAlternateGravity( gravityVector ); }
void IPhysicsEnvironment::GetAlternateGravity( Vector *pGravityVector ) const
{ static_cast<const JoltPhysicsEnvironment*>(this)->GetAlternateGravity( pGravityVector ); }
float IPhysicsEnvironment::GetDeltaFrameTime( int maxTicks ) const
{ return static_cast<const JoltPhysicsEnvironment*>(this)->GetDeltaFrameTime( maxTicks ); }
void IPhysicsEnvironment::ForceObjectsToSleep( IPhysicsObject **pList, int listCount )
{ static_cast<JoltPhysicsEnvironment*>(this)->ForceObjectsToSleep( pList, listCount ); }
void IPhysicsEnvironment::SetPredicted( bool bPredicted )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetPredicted( bPredicted ); }
bool IPhysicsEnvironment::IsPredicted()
{ return static_cast<JoltPhysicsEnvironment*>(this)->IsPredicted(); }
void IPhysicsEnvironment::SetPredictionCommandNum( int iCommandNum )
{ static_cast<JoltPhysicsEnvironment*>(this)->SetPredictionCommandNum( iCommandNum ); }
int IPhysicsEnvironment::GetPredictionCommandNum()
{ return static_cast<JoltPhysicsEnvironment*>(this)->GetPredictionCommandNum(); }
void IPhysicsEnvironment::DoneReferencingPreviousCommands( int iCommandNum )
{ static_cast<JoltPhysicsEnvironment*>(this)->DoneReferencingPreviousCommands( iCommandNum ); }
void IPhysicsEnvironment::RestorePredictedSimulation()
{ static_cast<JoltPhysicsEnvironment*>(this)->RestorePredictedSimulation(); }
void IPhysicsEnvironment::DestroyCollideOnDeadObjectFlush( CPhysCollide *p0 )
{ static_cast<JoltPhysicsEnvironment*>(this)->DestroyCollideOnDeadObjectFlush( p0 ); }
