#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Shared entity code (game/shared) is compiled for both sides, but only the server's CBaseEntity dispatches its
// former virtual methods by kind (server/entity_dispatch.h) so far. There, the override of such a method is
// spelled `Name_Impl` and is not virtual; the client's C_BaseEntity still has the virtual `Name`. These macros let
// shared code say it once: `KS_VIRTUAL void Spawn_Impl();`, `BaseClass::Spawn_Impl()`.
// (Remove together with the last client virtual of the list.)
#ifdef GAME_DLL
#define KS_VIRTUAL
#define KS_OVERRIDE
#else
#define KS_VIRTUAL virtual
#define KS_OVERRIDE override
#define Spawn_Impl Spawn
#define Precache_Impl Precache
#define Activate_Impl Activate
#define OnRestore_Impl OnRestore
#define UpdateOnRemove_Impl UpdateOnRemove
#define CreateVPhysics_Impl CreateVPhysics
#define ObjectCaps_Impl ObjectCaps
#define DrawDebugTextOverlays_Impl DrawDebugTextOverlays
#define UpdateTransmitState_Impl UpdateTransmitState
#define ShouldTransmit_Impl ShouldTransmit
#define SetTransmit_Impl SetTransmit
#define KeyValue_Impl KeyValue
#define GetKeyValue_Impl GetKeyValue
#define Classify_Impl Classify
#define TraceAttack_Impl TraceAttack
#define OnTakeDamage_Impl OnTakeDamage
#define TakeHealth_Impl TakeHealth
#define Event_Killed_Impl Event_Killed
#define Think_Impl Think
#define Use_Impl Use
#define StartTouch_Impl StartTouch
#define Touch_Impl Touch
#define EndTouch_Impl EndTouch
#define Blocked_Impl Blocked
#define StartBlocked_Impl StartBlocked
#define EndBlocked_Impl EndBlocked
#define MoveDone_Impl MoveDone
#define DrawDebugGeometryOverlays_Impl DrawDebugGeometryOverlays
#define SetModel_Impl SetModel
#define TestCollision_Impl TestCollision
#define TestHitboxes_Impl TestHitboxes
#define ComputeWorldSpaceSurroundingBox_Impl ComputeWorldSpaceSurroundingBox
#define ShouldCollide_Impl ShouldCollide
#define VPhysicsCollision_Impl VPhysicsCollision
#define VPhysicsUpdate_Impl VPhysicsUpdate
#define VPhysicsShadowUpdate_Impl VPhysicsShadowUpdate
#define VPhysicsShadowCollision_Impl VPhysicsShadowCollision
#define VPhysicsGetObjectList_Impl VPhysicsGetObjectList
#define VPhysicsFriction_Impl VPhysicsFriction
#define VPhysicsDestroyObject_Impl VPhysicsDestroyObject
#define UpdatePhysicsShadowToCurrentPosition_Impl UpdatePhysicsShadowToCurrentPosition
#define ForceVPhysicsCollide_Impl ForceVPhysicsCollide
#define PhysicsSolidMaskForEntity_Impl PhysicsSolidMaskForEntity
#define PhysicsSimulate_Impl PhysicsSimulate
#define PhysicsTouchTriggers_Impl PhysicsTouchTriggers
#define PhysicsSplash_Impl PhysicsSplash
#define ResolveFlyCollisionCustom_Impl ResolveFlyCollisionCustom
#define HasPhysicsAttacker_Impl HasPhysicsAttacker
#define ShouldSavePhysics_Impl ShouldSavePhysics
#define Teleport_Impl Teleport
#define ModifyOrAppendCriteria_Impl ModifyOrAppendCriteria
#define GetResponseSystem_Impl GetResponseSystem
#define GetTracerType_Impl GetTracerType
#define GetEnemy_Impl GetEnemy
#define DispatchResponse_Impl DispatchResponse
#define ChangeTeam_Impl ChangeTeam
#define BloodColor_Impl BloodColor
#define Respawn_Impl Respawn
#define PostConstructor_Impl PostConstructor
#define PassesDamageFilter_Impl PassesDamageFilter
#define NotifySystemEvent_Impl NotifySystemEvent
#define MakeTracer_Impl MakeTracer
#define GetSmoothedVelocity_Impl GetSmoothedVelocity
#define GetDamage_Impl GetDamage
#define EyePosition_Impl EyePosition
#define Event_KilledOther_Impl Event_KilledOther
#define DoImpactEffect_Impl DoImpactEffect
#define BodyTarget_Impl BodyTarget
#define SetParent_Impl SetParent
#define RequiredEdictIndex_Impl RequiredEdictIndex
#define OnNewModel_Impl OnNewModel
#define OnEntityEvent_Impl OnEntityEvent
#define OnControls_Impl OnControls
#define IsViewable_Impl IsViewable
#define IsTriggered_Impl IsTriggered
#define IsNetClient_Impl IsNetClient
#define IsMoving_Impl IsMoving
#define ImpactTrace_Impl ImpactTrace
#define GetGroundVelocityToApply_Impl GetGroundVelocityToApply
#define GetDelay_Impl GetDelay
#define FVisible_Impl FVisible
#define DecalTrace_Impl DecalTrace
#define WorldSpaceCenter_Impl WorldSpaceCenter
#define SetOwnerEntity_Impl SetOwnerEntity
#define SetHealth_Impl SetHealth
#define SetDamage_Impl SetDamage
#define SUB_AllowedToFade_Impl SUB_AllowedToFade
#define RunVScripts_Impl RunVScripts
#define PostClientActive_Impl PostClientActive
#define MyCombatWeaponPointer_Impl MyCombatWeaponPointer
#define MyCombatCharacterPointer_Impl MyCombatCharacterPointer
#define LocalEyeAngles_Impl LocalEyeAngles
#define IsTemplate_Impl IsTemplate
#define IsPlayer_Impl IsPlayer
#define IsNPC_Impl IsNPC
#define IsLockedByMaster_Impl IsLockedByMaster
#define IsBaseTrain_Impl IsBaseTrain
#define IsBaseCombatWeapon_Impl IsBaseCombatWeapon
#define GetVelocity_Impl GetVelocity
#define GetVectors_Impl GetVectors
#define GetStepOrigin_Impl GetStepOrigin
#define GetStepAngles_Impl GetStepAngles
#define GetServerVehicle_Impl GetServerVehicle
#define GetPlayerName_Impl GetPlayerName
#define GetDamageType_Impl GetDamageType
#define GetBaseAnimatingOverlay_Impl GetBaseAnimatingOverlay
#define GetBaseAnimating_Impl GetBaseAnimating
#define GetAutoAimRadius_Impl GetAutoAimRadius
#define GetAutoAimCenter_Impl GetAutoAimCenter
#define EyeAngles_Impl EyeAngles
#define DamageDecal_Impl DamageDecal
#define CanStandOn_Impl CanStandOn
#define FireBullets_Impl FireBullets
#define Splash_Impl Splash
#endif
