#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The former virtual methods of CBaseEntity that are dispatched by kind (entity_dispatch.h).
// E( return type, name, const or nothing, ( parameters ), ( argument names ), extra... )
// Every method listed here is declared in CBaseEntity twice: `Name`, the dispatcher (not virtual), and
// `Name_Impl`, CBaseEntity's own version, which classes override by defining a `Name_Impl` of the same
// signature. A method that is still `virtual` in CBaseEntity is not in this list.

#define KS_ENTITY_METHODS( E, ... ) \
	E( void, Spawn, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Precache, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Activate, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, OnRestore, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, UpdateOnRemove, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, CreateVPhysics, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, ObjectCaps, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, DrawDebugTextOverlays, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, UpdateTransmitState, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, ShouldTransmit, , ( const CCheckTransmitInfo *pInfo ), ( pInfo ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, SetTransmit, , ( CCheckTransmitInfo *pInfo, bool bAlways ), ( pInfo, bAlways ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, KeyValue, , ( const char *szKeyName, const char *szValue ), ( szKeyName, szValue ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, KeyValue, , ( const char *szKeyName, float flValue ), ( szKeyName, flValue ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, KeyValue, , ( const char *szKeyName, int nValue ), ( szKeyName, nValue ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, KeyValue, , ( const char *szKeyName, const Vector &vecValue ), ( szKeyName, vecValue ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, GetKeyValue, , ( const char *szKeyName, char *szValue, int iMaxLen ), ( szKeyName, szValue, iMaxLen ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( Class_T, Classify, , ( ), ( ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, TraceAttack, , ( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr ), ( info, vecDir, ptr ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, OnTakeDamage, , ( const CTakeDamageInfo &info ), ( info ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, TakeHealth, , ( float flHealth, int bitsDamageType ), ( flHealth, bitsDamageType ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Event_Killed, , ( const CTakeDamageInfo &info ), ( info ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Think, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Use, , ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ), ( pActivator, pCaller, useType, value ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, StartTouch, , ( CBaseEntity *pOther ), ( pOther ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Touch, , ( CBaseEntity *pOther ), ( pOther ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, EndTouch, , ( CBaseEntity *pOther ), ( pOther ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Blocked, , ( CBaseEntity *pOther ), ( pOther ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, StartBlocked, , ( CBaseEntity *pOther ), ( pOther ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, EndBlocked, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, MoveDone, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, DrawDebugGeometryOverlays, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, SetModel, , ( const char *szModelName ), ( szModelName ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, TestCollision, , ( const Ray_t& ray, unsigned int mask, trace_t& trace ), ( ray, mask, trace ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, TestHitboxes, , ( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr ), ( ray, fContentsMask, tr ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, ComputeWorldSpaceSurroundingBox, , ( Vector *pWorldMins, Vector *pWorldMaxs ), ( pWorldMins, pWorldMaxs ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, ShouldCollide, const, ( int collisionGroup, int contentsMask ), ( collisionGroup, contentsMask ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, VPhysicsCollision, , ( int index, gamevcollisionevent_t *pEvent ), ( index, pEvent ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, VPhysicsUpdate, , ( IPhysicsObject *pPhysics ), ( pPhysics ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, VPhysicsShadowUpdate, , ( IPhysicsObject *pPhysics ), ( pPhysics ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, VPhysicsShadowCollision, , ( int index, gamevcollisionevent_t *pEvent ), ( index, pEvent ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, VPhysicsGetObjectList, , ( IPhysicsObject **pList, int listMax ), ( pList, listMax ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, VPhysicsFriction, , ( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit ), ( pObject, energy, surfaceProps, surfacePropsHit ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, VPhysicsDestroyObject, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, UpdatePhysicsShadowToCurrentPosition, , ( float deltaTime ), ( deltaTime ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, ForceVPhysicsCollide, , ( CBaseEntity *pEntity ), ( pEntity ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( unsigned int, PhysicsSolidMaskForEntity, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, PhysicsSimulate, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, PhysicsTouchTriggers, , ( const Vector *pPrevAbsOrigin ), ( pPrevAbsOrigin ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, PhysicsSplash, , ( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed ), ( centerPoint, normal, rawSpeed, scaledSpeed ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, ResolveFlyCollisionCustom, , ( trace_t &trace, Vector &vecVelocity ), ( trace, vecVelocity ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBasePlayer *, HasPhysicsAttacker, , ( float dt ), ( dt ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, ShouldSavePhysics, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Teleport, , ( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts ), ( newPosition, newAngles, newVelocity, bUseSlowHighAccuracyContacts ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, ModifyOrAppendCriteria, , ( AI_CriteriaSet& set ), ( set ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( ResponseRules::IResponseSystem *, GetResponseSystem, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( const char *, GetTracerType, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseEntity *, GetEnemy, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseEntity *, GetEnemy, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, DispatchResponse, , ( const char *conceptName ), ( conceptName ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, ChangeTeam, , ( int iTeamNum ), ( iTeamNum ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, BloodColor, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseEntity *, Respawn, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, PostConstructor, , ( const char *szClassname ), ( szClassname ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, PassesDamageFilter, , ( const CTakeDamageInfo &info ), ( info ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, NotifySystemEvent, , ( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params ), ( pNotify, eventType, params ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, MakeTracer, , ( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType ), ( vecTracerSrc, tr, iTracerType ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( Vector, GetSmoothedVelocity, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( float, GetDamage, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( Vector, EyePosition, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Event_KilledOther, , ( CBaseEntity *pVictim, const CTakeDamageInfo &info ), ( pVictim, info ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, DoImpactEffect, , ( trace_t &tr, int nDamageType ), ( tr, nDamageType ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( Vector, BodyTarget, , ( const Vector &posSrc, bool bNoisy ), ( posSrc, bNoisy ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, SetParent, , ( CBaseEntity* pNewParent, int iAttachment ), ( pNewParent, iAttachment ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, RequiredEdictIndex, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CStudioHdr*, OnNewModel, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, OnEntityEvent, , ( EntityEvent_t event, void *pEventData ), ( event, pEventData ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, OnControls, , ( CBaseEntity *pControls ), ( pControls ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsViewable, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsTriggered, , ( CBaseEntity *pActivator ), ( pActivator ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsNetClient, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsMoving, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, ImpactTrace, , ( trace_t *pTrace, int iDamageType, const char *pCustomImpactName ), ( pTrace, iDamageType, pCustomImpactName ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, GetGroundVelocityToApply, , ( Vector &vecGroundVel ), ( vecGroundVel ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( float, GetDelay, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, FVisible, , ( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker ), ( pEntity, traceMask, ppBlocker ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, FVisible, , ( const Vector &vecTarget, int traceMask, CBaseEntity **ppBlocker ), ( vecTarget, traceMask, ppBlocker ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, DecalTrace, , ( trace_t *pTrace, char const *decalName ), ( pTrace, decalName ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( const Vector&, WorldSpaceCenter, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, SetOwnerEntity, , ( CBaseEntity* pOwner ), ( pOwner ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, SetHealth, , ( int amt ), ( amt ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, SetDamage, , ( float flDamage ), ( flDamage ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, SUB_AllowedToFade, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, RunVScripts, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, PostClientActive, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseCombatWeapon *, MyCombatWeaponPointer, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseCombatCharacter *, MyCombatCharacterPointer, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( const QAngle &, LocalEyeAngles, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsTemplate, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsPlayer, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsNPC, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsLockedByMaster, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsBaseTrain, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, IsBaseCombatWeapon, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, GetVelocity, , ( Vector *vVelocity, AngularImpulse *vAngVelocity ), ( vVelocity, vAngVelocity ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, GetVectors, const, ( Vector* forward, Vector* right, Vector* up ), ( forward, right, up ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( Vector, GetStepOrigin, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( QAngle, GetStepAngles, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( IServerVehicle*, GetServerVehicle, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( const char *, GetPlayerName, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( int, GetDamageType, const, (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseAnimatingOverlay *, GetBaseAnimatingOverlay, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( CBaseAnimating*, GetBaseAnimating, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( float, GetAutoAimRadius, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( Vector, GetAutoAimCenter, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( const QAngle &, EyeAngles, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( char const *, DamageDecal, , ( int bitsDamageType, int gameMaterial ), ( bitsDamageType, gameMaterial ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, CanStandOn, const, ( CBaseEntity *pSurface ), ( pSurface ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( bool, CanStandOn, const, ( edict_t *ent ), ( ent ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, FireBullets, , ( const FireBulletsInfo_t &info ), ( info ) __VA_OPT__( , ) __VA_ARGS__ ) \
	E( void, Splash, , (  ), (  ) __VA_OPT__( , ) __VA_ARGS__ )
