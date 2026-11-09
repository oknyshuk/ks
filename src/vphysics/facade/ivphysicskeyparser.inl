// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVPhysicsKeyParser class to JoltPhysicsParseKV. Included by the source that defines JoltPhysicsParseKV.

const char * IVPhysicsKeyParser::GetCurrentBlockName()
{ return static_cast<JoltPhysicsParseKV*>(this)->GetCurrentBlockName(); }
bool IVPhysicsKeyParser::Finished()
{ return static_cast<JoltPhysicsParseKV*>(this)->Finished(); }
void IVPhysicsKeyParser::ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseSolid( pSolid, unknownKeyHandler ); }
void IVPhysicsKeyParser::ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseFluid( pFluid, unknownKeyHandler ); }
void IVPhysicsKeyParser::ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseRagdollConstraint( pConstraint, unknownKeyHandler ); }
void IVPhysicsKeyParser::ParseSurfaceTable( int *table, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseSurfaceTable( table, unknownKeyHandler ); }
void IVPhysicsKeyParser::ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseCustom( pCustom, unknownKeyHandler ); }
void IVPhysicsKeyParser::ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseVehicle( pVehicle, unknownKeyHandler ); }
void IVPhysicsKeyParser::SkipBlock()
{ static_cast<JoltPhysicsParseKV*>(this)->SkipBlock(); }
void IVPhysicsKeyParser::ParseCollisionRules( ragdollcollisionrules_t *pRules, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseCollisionRules( pRules, unknownKeyHandler ); }
void IVPhysicsKeyParser::ParseRagdollAnimatedFriction( ragdollanimatedfriction_t *pFriction, IVPhysicsKeyHandler *unknownKeyHandler )
{ static_cast<JoltPhysicsParseKV*>(this)->ParseRagdollAnimatedFriction( pFriction, unknownKeyHandler ); }
