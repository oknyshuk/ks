// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsCollision class to JoltPhysicsCollision. Included by the source that defines JoltPhysicsCollision.

CPhysConvex * IPhysicsCollision::ConvexFromVerts( Vector **pVerts, int vertCount )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvexFromVerts( pVerts, vertCount ); }
CPhysConvex * IPhysicsCollision::ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvexFromPlanes( pPlanes, planeCount, mergeDistance ); }
float IPhysicsCollision::ConvexVolume( CPhysConvex *pConvex )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvexVolume( pConvex ); }
float IPhysicsCollision::ConvexSurfaceArea( CPhysConvex *pConvex )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvexSurfaceArea( pConvex ); }
void IPhysicsCollision::SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData )
{ static_cast<JoltPhysicsCollision*>(this)->SetConvexGameData( pConvex, gameData ); }
void IPhysicsCollision::ConvexFree( CPhysConvex *pConvex )
{ static_cast<JoltPhysicsCollision*>(this)->ConvexFree( pConvex ); }
CPhysConvex * IPhysicsCollision::BBoxToConvex( const Vector &mins, const Vector &maxs )
{ return static_cast<JoltPhysicsCollision*>(this)->BBoxToConvex( mins, maxs ); }
CPhysConvex * IPhysicsCollision::ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvexFromConvexPolyhedron( ConvexPolyhedron ); }
void IPhysicsCollision::ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput )
{ static_cast<JoltPhysicsCollision*>(this)->ConvexesFromConvexPolygon( vPolyNormal, pPoints, iPointCount, pOutput ); }
CPhysPolysoup * IPhysicsCollision::PolysoupCreate()
{ return static_cast<JoltPhysicsCollision*>(this)->PolysoupCreate(); }
void IPhysicsCollision::PolysoupDestroy( CPhysPolysoup *pSoup )
{ static_cast<JoltPhysicsCollision*>(this)->PolysoupDestroy( pSoup ); }
void IPhysicsCollision::PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits )
{ static_cast<JoltPhysicsCollision*>(this)->PolysoupAddTriangle( pSoup, a, b, c, materialIndex7bits ); }
CPhysCollide * IPhysicsCollision::ConvertPolysoupToCollide( CPhysPolysoup *pSoup, bool useMOPP )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvertPolysoupToCollide( pSoup, useMOPP ); }
CPhysCollide * IPhysicsCollision::ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvertConvexToCollide( pConvex, convexCount ); }
CPhysCollide * IPhysicsCollision::ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams )
{ return static_cast<JoltPhysicsCollision*>(this)->ConvertConvexToCollideParams( pConvex, convexCount, convertParams ); }
void IPhysicsCollision::DestroyCollide( CPhysCollide *pCollide )
{ static_cast<JoltPhysicsCollision*>(this)->DestroyCollide( pCollide ); }
int IPhysicsCollision::CollideSize( CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideSize( pCollide ); }
int IPhysicsCollision::CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideWrite( pDest, pCollide, bSwap ); }
CPhysCollide * IPhysicsCollision::UnserializeCollide( char *pBuffer, int size, int index )
{ return static_cast<JoltPhysicsCollision*>(this)->UnserializeCollide( pBuffer, size, index ); }
float IPhysicsCollision::CollideVolume( CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideVolume( pCollide ); }
float IPhysicsCollision::CollideSurfaceArea( CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideSurfaceArea( pCollide ); }
Vector IPhysicsCollision::CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideGetExtent( pCollide, collideOrigin, collideAngles, direction ); }
void IPhysicsCollision::CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles )
{ static_cast<JoltPhysicsCollision*>(this)->CollideGetAABB( pMins, pMaxs, pCollide, collideOrigin, collideAngles ); }
void IPhysicsCollision::CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter )
{ static_cast<JoltPhysicsCollision*>(this)->CollideGetMassCenter( pCollide, pOutMassCenter ); }
void IPhysicsCollision::CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter )
{ static_cast<JoltPhysicsCollision*>(this)->CollideSetMassCenter( pCollide, massCenter ); }
Vector IPhysicsCollision::CollideGetOrthographicAreas( const CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideGetOrthographicAreas( pCollide ); }
void IPhysicsCollision::CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas )
{ static_cast<JoltPhysicsCollision*>(this)->CollideSetOrthographicAreas( pCollide, areas ); }
int IPhysicsCollision::CollideIndex( const CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideIndex( pCollide ); }
CPhysCollide * IPhysicsCollision::BBoxToCollide( const Vector &mins, const Vector &maxs )
{ return static_cast<JoltPhysicsCollision*>(this)->BBoxToCollide( mins, maxs ); }
int IPhysicsCollision::GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit )
{ return static_cast<JoltPhysicsCollision*>(this)->GetConvexesUsedInCollideable( pCollideable, pOutputArray, iOutputArrayLimit ); }
void IPhysicsCollision::TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{ static_cast<JoltPhysicsCollision*>(this)->TraceBox( start, end, mins, maxs, pCollide, collideOrigin, collideAngles, ptr ); }
void IPhysicsCollision::TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{ static_cast<JoltPhysicsCollision*>(this)->TraceBox( ray, pCollide, collideOrigin, collideAngles, ptr ); }
void IPhysicsCollision::TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{ static_cast<JoltPhysicsCollision*>(this)->TraceBox( ray, contentsMask, pConvexInfo, pCollide, collideOrigin, collideAngles, ptr ); }
void IPhysicsCollision::TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{ static_cast<JoltPhysicsCollision*>(this)->TraceCollide( start, end, pSweepCollide, sweepAngles, pCollide, collideOrigin, collideAngles, ptr ); }
bool IPhysicsCollision::IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone )
{ return static_cast<JoltPhysicsCollision*>(this)->IsBoxIntersectingCone( boxAbsMins, boxAbsMaxs, cone ); }
void IPhysicsCollision::VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int size, bool swap )
{ static_cast<JoltPhysicsCollision*>(this)->VCollideLoad( pOutput, solidCount, pBuffer, size, swap ); }
void IPhysicsCollision::VCollideUnload( vcollide_t *pVCollide )
{ static_cast<JoltPhysicsCollision*>(this)->VCollideUnload( pVCollide ); }
IVPhysicsKeyParser * IPhysicsCollision::VPhysicsKeyParserCreate( const char *pKeyData )
{ return static_cast<JoltPhysicsCollision*>(this)->VPhysicsKeyParserCreate( pKeyData ); }
IVPhysicsKeyParser * IPhysicsCollision::VPhysicsKeyParserCreate( vcollide_t *pVCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->VPhysicsKeyParserCreate( pVCollide ); }
void IPhysicsCollision::VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser )
{ static_cast<JoltPhysicsCollision*>(this)->VPhysicsKeyParserDestroy( pParser ); }
int IPhysicsCollision::CreateDebugMesh( CPhysCollide const *pCollisionModel, Vector **outVerts )
{ return static_cast<JoltPhysicsCollision*>(this)->CreateDebugMesh( pCollisionModel, outVerts ); }
void IPhysicsCollision::DestroyDebugMesh( int vertCount, Vector *outVerts )
{ static_cast<JoltPhysicsCollision*>(this)->DestroyDebugMesh( vertCount, outVerts ); }
ICollisionQuery * IPhysicsCollision::CreateQueryModel( CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CreateQueryModel( pCollide ); }
void IPhysicsCollision::DestroyQueryModel( ICollisionQuery *pQuery )
{ static_cast<JoltPhysicsCollision*>(this)->DestroyQueryModel( pQuery ); }
IPhysicsCollision * IPhysicsCollision::ThreadContextCreate()
{ return static_cast<JoltPhysicsCollision*>(this)->ThreadContextCreate(); }
void IPhysicsCollision::ThreadContextDestroy( IPhysicsCollision *pThreadContex )
{ static_cast<JoltPhysicsCollision*>(this)->ThreadContextDestroy( pThreadContex ); }
CPhysCollide * IPhysicsCollision::CreateVirtualMesh( const virtualmeshparams_t &params )
{ return static_cast<JoltPhysicsCollision*>(this)->CreateVirtualMesh( params ); }
bool IPhysicsCollision::SupportsVirtualMesh()
{ return static_cast<JoltPhysicsCollision*>(this)->SupportsVirtualMesh(); }
bool IPhysicsCollision::GetBBoxCacheSize( int *pCachedSize, int *pCachedCount )
{ return static_cast<JoltPhysicsCollision*>(this)->GetBBoxCacheSize( pCachedSize, pCachedCount ); }
CPolyhedron * IPhysicsCollision::PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron )
{ return static_cast<JoltPhysicsCollision*>(this)->PolyhedronFromConvex( pConvex, bUseTempPolyhedron ); }
void IPhysicsCollision::OutputDebugInfo( const CPhysCollide *pCollide )
{ static_cast<JoltPhysicsCollision*>(this)->OutputDebugInfo( pCollide ); }
unsigned int IPhysicsCollision::ReadStat( int statID )
{ return static_cast<JoltPhysicsCollision*>(this)->ReadStat( statID ); }
float IPhysicsCollision::CollideGetRadius( const CPhysCollide *pCollide )
{ return static_cast<JoltPhysicsCollision*>(this)->CollideGetRadius( pCollide ); }
void * IPhysicsCollision::VCollideAllocUserData( vcollide_t *pVCollide, std::size_t userDataSize )
{ return static_cast<JoltPhysicsCollision*>(this)->VCollideAllocUserData( pVCollide, userDataSize ); }
void IPhysicsCollision::VCollideFreeUserData( vcollide_t *pVCollide )
{ static_cast<JoltPhysicsCollision*>(this)->VCollideFreeUserData( pVCollide ); }
void IPhysicsCollision::VCollideCheck( vcollide_t *pVCollide, const char *pName )
{ static_cast<JoltPhysicsCollision*>(this)->VCollideCheck( pVCollide, pName ); }
bool IPhysicsCollision::TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr )
{ return static_cast<JoltPhysicsCollision*>(this)->TraceBoxAA( ray, pCollide, ptr ); }
