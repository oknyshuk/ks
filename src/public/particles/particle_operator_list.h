#pragma once
import ks.core.types;
import ks.math.ssemath;
import ks.math.vector4d;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The closed set of particle operators (PLAN.md Decision 2). Every operator class registered with
// DEFINE_PARTICLE_OPERATOR is listed here, once, under the source file (group) that defines it.
// The lists generate ParticleOperatorKind, ParticleOperatorGroup and the switch dispatch of
// ParticleOps (particles/particle_operator_dispatch.inl); a new operator is added to its group's
// list and nowhere else.
//
// Each list is invoked as KS_PARTICLE_OPS_<Group>( X, ... ) and expands to X( class, ... ) per class;
// the trailing arguments are handed through to X.

#define KS_PARTICLE_OPS_Constraints( X, ... ) \
	X( C_OP_ConstrainDistance, __VA_ARGS__ ) \
	X( C_OP_ConstrainDistanceToPath, __VA_ARGS__ ) \
	X( C_OP_PlanarConstraint, __VA_ARGS__ ) \
	X( C_OP_WorldCollideConstraint, __VA_ARGS__ ) \
	X( C_OP_WorldTraceConstraint, __VA_ARGS__ ) \
	X( C_OP_BoxConstraint, __VA_ARGS__ )

#define KS_PARTICLE_OPS_Initializers( X, ... ) \
	X( C_INIT_RingWave, __VA_ARGS__ ) \
	X( C_INIT_CreateInEpitrochoid, __VA_ARGS__ ) \
	X( C_INIT_CreateOnModel, __VA_ARGS__ ) \
	X( C_INIT_SetHitboxToClosest, __VA_ARGS__ ) \
	X( C_INIT_SetHitboxToModel, __VA_ARGS__ ) \
	X( C_INIT_CreateWithinSphere, __VA_ARGS__ ) \
	X( C_INIT_CreateWithinBox, __VA_ARGS__ ) \
	X( C_INIT_PositionOffset, __VA_ARGS__ ) \
	X( C_INIT_PositionPlaceOnGround, __VA_ARGS__ ) \
	X( C_INIT_VelocityRandom, __VA_ARGS__ ) \
	X( C_INIT_InitialVelocityNoise, __VA_ARGS__ ) \
	X( C_INIT_RandomLifeTime, __VA_ARGS__ ) \
	X( C_INIT_RandomScalar, __VA_ARGS__ ) \
	X( C_INIT_RandomVector, __VA_ARGS__ ) \
	X( C_INIT_RandomVectorComponent, __VA_ARGS__ ) \
	X( C_INIT_RandomRadius, __VA_ARGS__ ) \
	X( C_INIT_RandomAlpha, __VA_ARGS__ ) \
	X( C_INIT_RandomRotation, __VA_ARGS__ ) \
	X( C_INIT_RandomRotationSpeed, __VA_ARGS__ ) \
	X( C_INIT_RandomYaw, __VA_ARGS__ ) \
	X( C_INIT_RandomColor, __VA_ARGS__ ) \
	X( C_INIT_ColorLitPerParticle, __VA_ARGS__ ) \
	X( C_INIT_RandomTrailLength, __VA_ARGS__ ) \
	X( C_INIT_RandomSequence, __VA_ARGS__ ) \
	X( C_INIT_SequenceFromCP, __VA_ARGS__ ) \
	X( C_INIT_PositionWarp, __VA_ARGS__ ) \
	X( C_INIT_CreationNoise, __VA_ARGS__ ) \
	X( C_INIT_CreateAlongPath, __VA_ARGS__ ) \
	X( C_INIT_MoveBetweenPoints, __VA_ARGS__ ) \
	X( C_INIT_RemapScalar, __VA_ARGS__ ) \
	X( C_INIT_RemapParticleCountToScalar, __VA_ARGS__ ) \
	X( C_INIT_InheritVelocity, __VA_ARGS__ ) \
	X( C_INIT_VelocityFromCP, __VA_ARGS__ ) \
	X( C_INIT_AgeNoise, __VA_ARGS__ ) \
	X( C_INIT_SequenceLifeTime, __VA_ARGS__ ) \
	X( C_INIT_CreateInHierarchy, __VA_ARGS__ ) \
	X( C_INIT_RemapScalarToVector, __VA_ARGS__ ) \
	X( C_INIT_OffsetVectorToVector, __VA_ARGS__ ) \
	X( C_INIT_CreateSequentialPath, __VA_ARGS__ ) \
	X( C_INIT_InitialRepulsionVelocity, __VA_ARGS__ ) \
	X( C_INIT_RandomYawFlip, __VA_ARGS__ ) \
	X( C_INIT_RandomSecondSequence, __VA_ARGS__ ) \
	X( C_INIT_RemapCPtoScalar, __VA_ARGS__ ) \
	X( C_INIT_RemapCPtoVector, __VA_ARGS__ ) \
	X( C_INIT_ChaoticAttractor, __VA_ARGS__ ) \
	X( C_INIT_CreateFromParentParticles, __VA_ARGS__ ) \
	X( C_INIT_InheritFromParentParticles, __VA_ARGS__ ) \
	X( C_INIT_DistanceToCPInit, __VA_ARGS__ ) \
	X( C_INIT_LifespanFromVelocity, __VA_ARGS__ ) \
	X( C_INIT_CreateFromPlaneCache, __VA_ARGS__ ) \
	X( C_INIT_ModelCull, __VA_ARGS__ ) \
	X( C_INIT_RtEnvCull, __VA_ARGS__ ) \
	X( C_INIT_NormalAlignToCP, __VA_ARGS__ ) \
	X( C_INIT_NormalOffset, __VA_ARGS__ ) \
	X( C_INIT_RemapSpeedToScalar, __VA_ARGS__ ) \
	X( C_INIT_InitFromCPSnapshot, __VA_ARGS__ ) \
	X( C_INIT_InitFromParentKilled, __VA_ARGS__ ) \
	X( C_INIT_RemapInitialDirectionToCPToVector, __VA_ARGS__ ) \
	X( C_INIT_RemapInitialCPDirectionToRotation, __VA_ARGS__ )

#define KS_PARTICLE_OPS_Emitters( X, ... ) \
	X( C_OP_InstantaneousEmitter, __VA_ARGS__ ) \
	X( C_OP_ContinuousEmitter, __VA_ARGS__ ) \
	X( C_OP_NoiseEmitter, __VA_ARGS__ ) \
	X( C_OP_MaintainEmitter, __VA_ARGS__ )

#define KS_PARTICLE_OPS_Forces( X, ... ) \
	X( C_OP_RandomForce, __VA_ARGS__ ) \
	X( C_OP_ParentVortices, __VA_ARGS__ ) \
	X( C_OP_TwistAroundAxis, __VA_ARGS__ ) \
	X( C_OP_AttractToControlPoint, __VA_ARGS__ ) \
	X( C_OP_ForceBasedOnDistanceToPlane, __VA_ARGS__ ) \
	X( C_OP_TimeVaryingForce, __VA_ARGS__ ) \
	X( C_OP_TurbulenceForce, __VA_ARGS__ )

#define KS_PARTICLE_OPS_Operators( X, ... ) \
	X( C_OP_BasicMovement, __VA_ARGS__ ) \
	X( C_OP_FadeAndKill, __VA_ARGS__ ) \
	X( C_OP_FadeAndKillForTracers, __VA_ARGS__ ) \
	X( C_OP_FadeIn, __VA_ARGS__ ) \
	X( C_OP_FadeOut, __VA_ARGS__ ) \
	X( C_OP_FadeInSimple, __VA_ARGS__ ) \
	X( C_OP_FadeOutSimple, __VA_ARGS__ ) \
	X( C_OP_ClampScalar, __VA_ARGS__ ) \
	X( C_OP_ClampVector, __VA_ARGS__ ) \
	X( C_OP_OscillateScalar, __VA_ARGS__ ) \
	X( C_OP_OscillateScalarSimple, __VA_ARGS__ ) \
	X( C_OP_OscillateVector, __VA_ARGS__ ) \
	X( C_OP_OscillateVectorSimple, __VA_ARGS__ ) \
	X( C_OP_DifferencePreviousParticle, __VA_ARGS__ ) \
	X( C_OP_RemapScalar, __VA_ARGS__ ) \
	X( C_OP_LerpScalar, __VA_ARGS__ ) \
	X( C_OP_LerpEndCapScalar, __VA_ARGS__ ) \
	X( C_OP_LerpEndCapVector, __VA_ARGS__ ) \
	X( C_OP_LerpVector, __VA_ARGS__ ) \
	X( C_OP_RemapSpeed, __VA_ARGS__ ) \
	X( C_OP_RemapSpeedtoCP, __VA_ARGS__ ) \
	X( C_OP_RemapModelVolumetoCP, __VA_ARGS__ ) \
	X( C_OP_RemapBoundingVolumetoCP, __VA_ARGS__ ) \
	X( C_OP_RemapAverageScalarValuetoCP, __VA_ARGS__ ) \
	X( C_OP_RampScalarLinear, __VA_ARGS__ ) \
	X( C_OP_RampScalarSpline, __VA_ARGS__ ) \
	X( C_OP_RampScalarLinearSimple, __VA_ARGS__ ) \
	X( C_OP_RampScalarSplineSimple, __VA_ARGS__ ) \
	X( C_OP_Noise, __VA_ARGS__ ) \
	X( C_OP_VectorNoise, __VA_ARGS__ ) \
	X( C_OP_Decay, __VA_ARGS__ ) \
	X( C_OP_VelocityDecay, __VA_ARGS__ ) \
	X( C_OP_AlphaDecay, __VA_ARGS__ ) \
	X( C_OP_RadiusDecay, __VA_ARGS__ ) \
	X( C_OP_DecayMaintainCount, __VA_ARGS__ ) \
	X( C_OP_Cull, __VA_ARGS__ ) \
	X( C_OP_Spin, __VA_ARGS__ ) \
	X( C_OP_SpinUpdate, __VA_ARGS__ ) \
	X( C_OP_SpinYaw, __VA_ARGS__ ) \
	X( C_OP_InterpolateRadius, __VA_ARGS__ ) \
	X( C_OP_ColorInterpolate, __VA_ARGS__ ) \
	X( C_OP_PositionLock, __VA_ARGS__ ) \
	X( C_OP_ControlpointLight, __VA_ARGS__ ) \
	X( C_OP_SetChildControlPoints, __VA_ARGS__ ) \
	X( C_OP_SetControlPointsToParticle, __VA_ARGS__ ) \
	X( C_OP_SetPerChildControlPoint, __VA_ARGS__ ) \
	X( C_OP_SetControlPointPositions, __VA_ARGS__ ) \
	X( C_OP_DampenToCP, __VA_ARGS__ ) \
	X( C_OP_DistanceBetweenCPs, __VA_ARGS__ ) \
	X( C_OP_DistanceBetweenCPsToCP, __VA_ARGS__ ) \
	X( C_OP_PercentageBetweenCPs, __VA_ARGS__ ) \
	X( C_OP_PercentageBetweenCPsVector, __VA_ARGS__ ) \
	X( C_OP_DistanceToCP, __VA_ARGS__ ) \
	X( C_OP_SetControlPointToPlayer, __VA_ARGS__ ) \
	X( C_OP_MoveToHitbox, __VA_ARGS__ ) \
	X( C_OP_LockToBone, __VA_ARGS__ ) \
	X( C_OP_CPOffsetToPercentageBetweenCPs, __VA_ARGS__ ) \
	X( C_OP_PlaneCull, __VA_ARGS__ ) \
	X( C_OP_DistanceCull, __VA_ARGS__ ) \
	X( C_OP_ModelCull, __VA_ARGS__ ) \
	X( C_OP_SetControlPointToCenter, __VA_ARGS__ ) \
	X( C_OP_VelocityMatchingForce, __VA_ARGS__ ) \
	X( C_OP_MovementMaintainOffset, __VA_ARGS__ ) \
	X( C_OP_MovementPlaceOnGround, __VA_ARGS__ ) \
	X( C_OP_InheritFromParentParticles, __VA_ARGS__ ) \
	X( C_OP_OrientTo2dDirection, __VA_ARGS__ ) \
	X( C_OP_RestartAfterDuration, __VA_ARGS__ ) \
	X( C_OP_StopAfterCPDuration, __VA_ARGS__ ) \
	X( C_OP_Orient2DRelToCP, __VA_ARGS__ ) \
	X( C_OP_SetControlPointRotation, __VA_ARGS__ ) \
	X( C_OP_MovementRotateParticleAroundAxis, __VA_ARGS__ ) \
	X( C_OP_RotateVector, __VA_ARGS__ ) \
	X( C_OP_MaxVelocity, __VA_ARGS__ ) \
	X( C_OP_LagCompensation, __VA_ARGS__ ) \
	X( C_OP_MaintainSequentialPath, __VA_ARGS__ ) \
	X( C_OP_LockToSavedSequentialPath, __VA_ARGS__ ) \
	X( C_OP_RemapDotProductToScalar, __VA_ARGS__ ) \
	X( C_OP_RemapCPtoScalar, __VA_ARGS__ ) \
	X( C_OP_NormalLock, __VA_ARGS__ ) \
	X( C_OP_SetControlPointToImpactPoint, __VA_ARGS__ ) \
	X( C_OP_RemapCPtoVector, __VA_ARGS__ ) \
	X( C_OP_RemapVelocityToVector, __VA_ARGS__ ) \
	X( C_OP_RemapCPVelocityToVector, __VA_ARGS__ ) \
	X( C_OP_SetCPOrientationToDirection, __VA_ARGS__ ) \
	X( C_OP_RemapDirectionToCPToVector, __VA_ARGS__ ) \
	X( C_OP_NormalizeVector, __VA_ARGS__ ) \
	X( C_OP_RemapControlPointDirectionToVector, __VA_ARGS__ )

#define KS_PARTICLE_OPS_Renderers( X, ... ) \
	X( C_OP_RenderPoints, __VA_ARGS__ ) \
	X( C_OP_RenderSprites, __VA_ARGS__ ) \
	X( C_OP_RenderSpritesTrail, __VA_ARGS__ ) \
	X( C_OP_RenderRope, __VA_ARGS__ ) \
	X( C_OP_RenderScreenVelocityRotate, __VA_ARGS__ ) \
	X( C_OP_RenderModels, __VA_ARGS__ ) \
	X( C_OP_RenderProjected, __VA_ARGS__ )

#define KS_PARTICLE_OPS( X, ... ) \
	KS_PARTICLE_OPS_Constraints( X, __VA_ARGS__ ) \
	KS_PARTICLE_OPS_Initializers( X, __VA_ARGS__ ) \
	KS_PARTICLE_OPS_Emitters( X, __VA_ARGS__ ) \
	KS_PARTICLE_OPS_Forces( X, __VA_ARGS__ ) \
	KS_PARTICLE_OPS_Operators( X, __VA_ARGS__ ) \
	KS_PARTICLE_OPS_Renderers( X, __VA_ARGS__ )

enum class ParticleOperatorGroup : unsigned char
{
	Constraints,
	Initializers,
	Emitters,
	Forces,
	Operators,
	Renderers,
};

enum class ParticleOperatorKind : unsigned char
{
#define KS_PARTICLE_OP_KIND( cls, ... ) cls,
	KS_PARTICLE_OPS( KS_PARTICLE_OP_KIND, )
#undef KS_PARTICLE_OP_KIND
};

#define KS_PARTICLE_OP_GROUP_OF( cls, group ) case ParticleOperatorKind::cls: return ParticleOperatorGroup::group;

constexpr ParticleOperatorGroup ParticleOperatorGroupOf( ParticleOperatorKind kind )
{
	switch ( kind )
	{
	KS_PARTICLE_OPS_Constraints( KS_PARTICLE_OP_GROUP_OF, Constraints )
	KS_PARTICLE_OPS_Initializers( KS_PARTICLE_OP_GROUP_OF, Initializers )
	KS_PARTICLE_OPS_Emitters( KS_PARTICLE_OP_GROUP_OF, Emitters )
	KS_PARTICLE_OPS_Forces( KS_PARTICLE_OP_GROUP_OF, Forces )
	KS_PARTICLE_OPS_Operators( KS_PARTICLE_OP_GROUP_OF, Operators )
	KS_PARTICLE_OPS_Renderers( KS_PARTICLE_OP_GROUP_OF, Renderers )
	}
	return ParticleOperatorGroup();
}
#undef KS_PARTICLE_OP_GROUP_OF

//-----------------------------------------------------------------------------
// The entry points of an operator (what used to be the virtual functions of CParticleOperatorInstance),
// as E( return type, name, const or nothing, ( parameters ), ( arguments ) ). ParticleOps::<name>
// takes the operator as its first parameter, pOp.
//-----------------------------------------------------------------------------
#define KS_PARTICLE_OP_UNPAREN( ... ) __VA_ARGS__
#define KS_PARTICLE_OP_ARGS( ... ) pOp __VA_OPT__( , ) __VA_ARGS__
#define KS_PARTICLE_OP_SIGNATURE( Const, ... ) Const CParticleOperatorInstance *pOp __VA_OPT__( , ) __VA_ARGS__

#define KS_PARTICLE_OP_ENTRIES( E ) \
	E( void, InitParams, , ( CParticleSystemDefinition *pDef ), ( pDef ) ) \
	E( std::size_t, GetRequiredContextBytes, const, ( ), ( ) ) \
	E( void, InitializeContextData, const, ( CParticleCollection *pParticles, void *pContext ), ( pParticles, pContext ) ) \
	E( uint32, GetWrittenAttributes, const, ( ), ( ) ) \
	E( uint32, GetReadAttributes, const, ( ), ( ) ) \
	E( uint64, GetReadControlPointMask, const, ( ), ( ) ) \
	E( uint32, GetFilter, const, ( ), ( ) ) \
	E( uint64, GetNonPositionalControlPointMask, const, ( ), ( ) ) \
	E( uint32, GetReadInitialAttributes, const, ( ), ( ) ) \
	E( void, Operate, const, ( CParticleCollection *pParticles, float flOpStrength, void *pContext ), ( pParticles, flOpStrength, pContext ) ) \
	E( void, PostSimulate, const, ( CParticleCollection *pParticles, void *pContext ), ( pParticles, pContext ) ) \
	E( void, Render, const, ( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ), \
		( pRenderContext, pParticles, vecDiffuseModulation, pContext, nViewRecursionDepth ) ) \
	E( bool, IsBatchable, const, ( ), ( ) ) \
	E( bool, IsOrderImportant, const, ( ), ( ) ) \
	E( bool, ShouldRun, const, ( bool bApplyingParentKillList ), ( bApplyingParentKillList ) ) \
	E( void, RenderUnsorted, const, ( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ), \
		( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount ) ) \
	E( int, GetParticlesToRender, const, ( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ), \
		( pParticles, pContext, nFirstParticle, nRemainingVertices, nRemainingIndices, pVertsUsed, pIndicesUsed ) ) \
	E( uint32, Emit, const, ( CParticleCollection *pParticles, float flOpCurStrength, void *pContext ), ( pParticles, flOpCurStrength, pContext ) ) \
	E( void, StopEmission, const, ( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ), ( pParticles, pContext, bInfiniteOnly ) ) \
	E( void, StartEmission, const, ( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ), ( pParticles, pContext, bInfiniteOnly ) ) \
	E( void, Restart, , ( CParticleCollection *pParticles, void *pContext ), ( pParticles, pContext ) ) \
	E( void, InitParticleSystem, const, ( CParticleCollection *pParticles, void *pContext ), ( pParticles, pContext ) ) \
	E( void, AddForces, const, ( FourVectors *AccumulatedForces, CParticleCollection *pParticles, int nBlocks, float flCurStrength, void *pContext ), \
		( AccumulatedForces, pParticles, nBlocks, flCurStrength, pContext ) ) \
	E( void, SetupConstraintPerFrameData, const, ( CParticleCollection *pParticles, void *pContext ), ( pParticles, pContext ) ) \
	E( bool, EnforceConstraint, const, ( int nStartBlock, int nNumBlocks, CParticleCollection *pParticles, void *pContext, int nNumValidParticlesInLastChunk ), \
		( nStartBlock, nNumBlocks, pParticles, pContext, nNumValidParticlesInLastChunk ) ) \
	E( bool, IsFinalConstaint, const, ( ), ( ) ) \
	E( bool, InitMultipleOverride, , ( ), ( ) ) \
	E( bool, IsScrubSafe, , ( ), ( ) ) \
	E( void, InitNewParticlesScalar, const, ( CParticleCollection *pParticles, int nFirstParticle, int n_particles, int attribute_write_mask, void *pContext ), \
		( pParticles, nFirstParticle, n_particles, attribute_write_mask, pContext ) ) \
	E( void, InitNewParticlesBlock, const, ( CParticleCollection *pParticles, int start_block, int n_blocks, int attribute_write_mask, void *pContext ), \
		( pParticles, start_block, n_blocks, attribute_write_mask, pContext ) ) \
	E( bool, MayCreateMoreParticles, const, ( CParticleCollection const *pParticles, void *pContext ), ( pParticles, pContext ) ) \
	E( bool, ShouldRunBeforeEmitters, const, ( ), ( ) ) \
	E( void, SkipToTime, const, ( float flTime, CParticleCollection *pParticles, void *pContext ), ( flTime, pParticles, pContext ) ) \
	E( void, RenderDebug, const, ( CParticleCollection *pParticles ), ( pParticles ) ) \
	E( void, Precache, , ( ), ( ) ) \
	E( void, Uncache, , ( ), ( ) )
