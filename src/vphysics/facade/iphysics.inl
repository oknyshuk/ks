// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysics class to JoltPhysicsInterface. Included by the source that defines JoltPhysicsInterface.

bool IPhysics::Connect( CreateInterfaceFn factory )
{ return static_cast<JoltPhysicsInterface*>(this)->Connect( factory ); }
void IPhysics::Disconnect()
{ static_cast<JoltPhysicsInterface*>(this)->Disconnect(); }
void *IPhysics::QueryInterface( const char *pInterfaceName )
{ return static_cast<JoltPhysicsInterface*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IPhysics::Init()
{ return static_cast<JoltPhysicsInterface*>(this)->Init(); }
void IPhysics::Shutdown()
{ static_cast<JoltPhysicsInterface*>(this)->Shutdown(); }

IPhysicsEnvironment * IPhysics::CreateEnvironment()
{ return static_cast<JoltPhysicsInterface*>(this)->CreateEnvironment(); }
void IPhysics::DestroyEnvironment( IPhysicsEnvironment *p0 )
{ static_cast<JoltPhysicsInterface*>(this)->DestroyEnvironment( p0 ); }
IPhysicsEnvironment * IPhysics::GetActiveEnvironmentByIndex( int index )
{ return static_cast<JoltPhysicsInterface*>(this)->GetActiveEnvironmentByIndex( index ); }
IPhysicsObjectPairHash * IPhysics::CreateObjectPairHash()
{ return static_cast<JoltPhysicsInterface*>(this)->CreateObjectPairHash(); }
void IPhysics::DestroyObjectPairHash( IPhysicsObjectPairHash *pHash )
{ static_cast<JoltPhysicsInterface*>(this)->DestroyObjectPairHash( pHash ); }
IPhysicsCollisionSet * IPhysics::FindOrCreateCollisionSet( std::uintptr_t id, int maxElementCount )
{ return static_cast<JoltPhysicsInterface*>(this)->FindOrCreateCollisionSet( id, maxElementCount ); }
IPhysicsCollisionSet * IPhysics::FindCollisionSet( std::uintptr_t id )
{ return static_cast<JoltPhysicsInterface*>(this)->FindCollisionSet( id ); }
void IPhysics::DestroyAllCollisionSets()
{ static_cast<JoltPhysicsInterface*>(this)->DestroyAllCollisionSets(); }
