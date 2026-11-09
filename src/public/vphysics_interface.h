#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Public interfaces to vphysics DLL

import ks.tier1;
import std;
#include "vcollide.h"
#include "tier3/tier3.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

// ------------------------------------------------------------------------------------
// UNITS:
// ------------------------------------------------------------------------------------
// NOTE:  Coordinates are in HL units.  1 unit == 1 inch.  X is east (forward), Y is north (left), Z is up (up)
// QAngle are pitch (around y), Yaw (around Z), Roll (around X)
// AngularImpulse are exponetial maps (an axis in HL units scaled by a "twist" angle in degrees)
//		They can be transformed like normals/covectors and added linearly
// mass is kg, volume is in^3, acceleration is in/s^2, velocity is in/s

// density is kg/m^3 (water ~= 998 at room temperature)
// preferably, these would be in kg/in^3, but the range of those numbers makes them not very human readable
// having water be about 1000 is really convenient for data entry.
// Since volume is in in^3 and density is in kg/m^3: 
//	density = (mass / volume) * CUBIC_METERS_PER_CUBIC_INCH
// Force is applied using impulses (kg*in/s)
// Torque is applied using impulses (kg*degrees/s)
// ------------------------------------------------------------------------------------

inline constexpr auto METERS_PER_INCH = (0.0254f);
inline constexpr auto CUBIC_METERS_PER_CUBIC_INCH = (METERS_PER_INCH*METERS_PER_INCH*METERS_PER_INCH);
// 2.2 lbs / kg
inline constexpr auto POUNDS_PER_KG = (2.2f);
inline constexpr auto KG_PER_POUND = (1.0f/POUNDS_PER_KG);

// convert from pounds to kg
template <typename T1> [[gnu::always_inline]] inline auto lbs2kg( T1 x ) { return ((x)*KG_PER_POUND); }
template <typename T1> [[gnu::always_inline]] inline auto kg2lbs( T1 x ) { return ((x)*POUNDS_PER_KG); }

const float VPHYSICS_MIN_MASS = 0.1f;
const float VPHYSICS_MAX_MASS = 5e4f;

class IPhysicsObject;
class IPhysicsEnvironment;
class IPhysicsSurfaceProps;
class IPhysicsConstraint;
class IPhysicsConstraintGroup;
class IPhysicsFluidController;
class IPhysicsSpring;
class IPhysicsVehicleController;
class IConvexInfo;
class IPhysicsObjectPairHash;
class IPhysicsCollisionSet;
class IPhysicsPlayerController;
class IPhysicsFrictionSnapshot;

struct Ray_t;
struct constraint_ragdollparams_t;
struct constraint_hingeparams_t;
struct constraint_fixedparams_t;
struct constraint_ballsocketparams_t;
struct constraint_slidingparams_t;
struct constraint_pulleyparams_t;
struct constraint_lengthparams_t;
struct constraint_groupparams_t;

struct vehicleparams_t;

struct fluidparams_t;
struct springparams_t;
struct objectparams_t;
struct debugcollide_t;
class CGameTrace;
typedef CGameTrace trace_t;
struct physics_stats_t;
struct physics_performanceparams_t;
struct virtualmeshparams_t;

//enum PhysInterfaceId_t;

enum PhysInterfaceId_t 
{
	PIID_UNKNOWN,
	PIID_IPHYSICSOBJECT,
	PIID_IPHYSICSFLUIDCONTROLLER,
	PIID_IPHYSICSSPRING,
	PIID_IPHYSICSCONSTRAINTGROUP,
	PIID_IPHYSICSCONSTRAINT,
	PIID_IPHYSICSSHADOWCONTROLLER,
	PIID_IPHYSICSPLAYERCONTROLLER,
	PIID_IPHYSICSMOTIONCONTROLLER,
	PIID_IPHYSICSVEHICLECONTROLLER,
	PIID_IPHYSICSGAMETRACE,

	PIID_NUM_TYPES
};


inline constexpr const char *VPHYSICS_DEBUG_OVERLAY_INTERFACE_VERSION = "VPhysicsDebugOverlay001";

class IVPhysicsDebugOverlay
{
public:
	void AddEntityTextOverlay(int ent_index, int line_offset, float duration, int r, int g, int b, int a, const char *format, ...);
	void AddBoxOverlay(const Vector& origin, const Vector& mins, const Vector& max, QAngle const& orientation, int r, int g, int b, int a, float duration);
	void AddTriangleOverlay(const Vector& p1, const Vector& p2, const Vector& p3, int r, int g, int b, int a, bool noDepthTest, float duration);
	void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b,bool noDepthTest, float duration);
	void AddTextOverlay(const Vector& origin, float duration, const char *format, ...);
	void AddTextOverlay(const Vector& origin, int line_offset, float duration, const char *format, ...);
	void AddScreenTextOverlay(float flXPos, float flYPos,float flDuration, int r, int g, int b, int a, const char *text);
	void AddSweptBoxOverlay(const Vector& start, const Vector& end, const Vector& mins, const Vector& max, const QAngle & angles, int r, int g, int b, int a, float flDuration);
	void AddTextOverlayRGB(const Vector& origin, int line_offset, float duration, float r, float g, float b, float alpha, const char *format, ...);
};

inline constexpr const char *VPHYSICS_INTERFACE_VERSION = "VPhysics031";

class IPhysics
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	IPhysicsEnvironment		*CreateEnvironment( void );
	void DestroyEnvironment( IPhysicsEnvironment * );
	IPhysicsEnvironment		*GetActiveEnvironmentByIndex( int index );

	// Creates a fast hash of pairs of objects
	// Useful for maintaining a table of object relationships like pairs that do not collide.
	IPhysicsObjectPairHash		*CreateObjectPairHash();
	void						DestroyObjectPairHash( IPhysicsObjectPairHash *pHash );

	// holds a cache of these by id.  So you can get by id to search for the previously created set
	// UNDONE: Sets are currently limited to 32 elements.  More elements will return NULL in create.
	// NOTE: id is not allowed to be zero.
	IPhysicsCollisionSet		*FindOrCreateCollisionSet( std::uintptr_t id, int maxElementCount );
	IPhysicsCollisionSet		*FindCollisionSet( std::uintptr_t id );
	void						DestroyAllCollisionSets();
};


// CPhysConvex is a single convex solid
class CPhysConvex;
// CPhysPolysoup is an abstract triangle soup mesh
class CPhysPolysoup;
class ICollisionQuery;
class IVPhysicsKeyParser;
struct convertconvexparams_t;
class CPackedPhysicsDescription;


// UNDONE: Find a better place for this?  Should be in collisionutils, but it's needs VPHYSICS' solver.
struct truncatedcone_t
{
	Vector	origin;
	Vector	normal;
	float	h;			// height of the cone (hl units)
	float	theta;		// cone angle (degrees)
};


class IPhysicsCollision
{
public:

	// produce a convex element from verts (convex hull around verts)
	CPhysConvex		*ConvexFromVerts( Vector **pVerts, int vertCount );
	// produce a convex element from planes (csg of planes)
	CPhysConvex		*ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance );
	// calculate volume of a convex element
	float			ConvexVolume( CPhysConvex *pConvex );

	float			ConvexSurfaceArea( CPhysConvex *pConvex );
	// store game-specific data in a convex solid
	void			SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData );
	// If not converted, free the convex elements with this call
	void			ConvexFree( CPhysConvex *pConvex );
	CPhysConvex		*BBoxToConvex( const Vector &mins, const Vector &maxs );
	// produce a convex element from a convex polyhedron
	CPhysConvex		*ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron );
	// produce a set of convex triangles from a convex polygon, normal is assumed to be on the side with forward point ordering, which should be clockwise, output will need to be able to hold exactly (iPointCount-2) convexes
	void			ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput );

	// concave objects
	// create a triangle soup
	CPhysPolysoup	*PolysoupCreate( void );
	// destroy the container and memory
	void			PolysoupDestroy( CPhysPolysoup *pSoup );
	// add a triangle to the soup
	void			PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits );
	// convert the convex into a compiled collision model
	CPhysCollide *ConvertPolysoupToCollide( CPhysPolysoup *pSoup, bool useMOPP );
	
	// Convert an array of convex elements to a compiled collision model (this deletes the convex elements)
	CPhysCollide	*ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount );
	CPhysCollide	*ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams );
	// Free a collide that was created with ConvertConvexToCollide()
	void			DestroyCollide( CPhysCollide *pCollide );

	// Get the memory size in bytes of the collision model for serialization
	int				CollideSize( CPhysCollide *pCollide );
	// serialize the collide to a block of memory
	int				CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap = false );
	// unserialize the collide from a block of memory
	CPhysCollide	*UnserializeCollide( char *pBuffer, int size, int index );

	// compute the volume of a collide
	float			CollideVolume( CPhysCollide *pCollide );
	// compute surface area for tools
	float			CollideSurfaceArea( CPhysCollide *pCollide );

	// Get the support map for a collide in the given direction
	Vector			CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction );

	// Get an AABB for an oriented collision model
	void			CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles );

	void			CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter );
	void			CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter );
	// get the approximate cross-sectional area projected orthographically on the bbox of the collide
	// NOTE: These are fractional areas - unitless.  Basically this is the fraction of the OBB on each axis that
	// would be visible if the object were rendered orthographically.
	// NOTE: This has been precomputed when the collide was built or this function will return 1,1,1
	Vector			CollideGetOrthographicAreas( const CPhysCollide *pCollide );
	void			CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas );

	// query the vcollide index in the physics model for the instance
	int				CollideIndex( const CPhysCollide *pCollide );

	// Convert a bbox to a collide
	CPhysCollide	*BBoxToCollide( const Vector &mins, const Vector &maxs );
	int				GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit );


	// Trace an AABB against a collide
	void TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );
	void TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );
	void TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );

	// Trace one collide against another
	void TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );

	// relatively slow test for box vs. truncated cone
	bool			IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone );

	// loads a set of solids into a vcollide_t
	void			VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int size, bool swap = false );
	// destroyts the set of solids created by VCollideLoad
	void			VCollideUnload( vcollide_t *pVCollide );

	// begins parsing a vcollide.  NOTE: This keeps pointers to the text
	// If you free the text and call members of IVPhysicsKeyParser, it will crash
	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( const char *pKeyData );
	IVPhysicsKeyParser *VPhysicsKeyParserCreate( vcollide_t *pVCollide );
	// Free the parser created by VPhysicsKeyParserCreate
	void			VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser );

	// creates a list of verts from a collision mesh
	int				CreateDebugMesh( CPhysCollide const *pCollisionModel, Vector **outVerts );
	// destroy the list of verts created by CreateDebugMesh
	void			DestroyDebugMesh( int vertCount, Vector *outVerts );

	// create a queryable version of the collision model
	ICollisionQuery *CreateQueryModel( CPhysCollide *pCollide );
	// destroy the queryable version
	void			DestroyQueryModel( ICollisionQuery *pQuery );

	IPhysicsCollision *ThreadContextCreate( void );
	void			ThreadContextDestroy( IPhysicsCollision *pThreadContex );

	CPhysCollide	*CreateVirtualMesh( const virtualmeshparams_t &params );
	bool			SupportsVirtualMesh();


	bool			GetBBoxCacheSize( int *pCachedSize, int *pCachedCount );

	
	// extracts a polyhedron that defines a CPhysConvex's shape
	CPolyhedron		*PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron );

	// dumps info about the collide to Msg()
	void			OutputDebugInfo( const CPhysCollide *pCollide );
	unsigned int	ReadStat( int statID );

	// Get an AABB for an oriented collision model
	float			CollideGetRadius( const CPhysCollide *pCollide );

	void			*VCollideAllocUserData( vcollide_t *pVCollide, std::size_t userDataSize );
	void			VCollideFreeUserData( vcollide_t *pVCollide );
	void			VCollideCheck( vcollide_t *pVCollide, const char *pName );
	bool			TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr );
};



// this can be used to post-process a collision model
class ICollisionQuery
{
public:
	// number of convex pieces in the whole solid
	int		ConvexCount( void );
	// triangle count for this convex piece
	int		TriangleCount( int convexIndex );
	// get the stored game data
	unsigned int GetGameData( int convexIndex );
	// Gets the triangle's verts to an array
	void	GetTriangleVerts( int convexIndex, int triangleIndex, Vector *verts );
	
	// UNDONE: This doesn't work!!!
	void	SetTriangleVerts( int convexIndex, int triangleIndex, const Vector *verts );
	
	// returns the 7-bit material index
	int		GetTriangleMaterialIndex( int convexIndex, int triangleIndex );
	// sets a 7-bit material index for this triangle
	void	SetTriangleMaterialIndex( int convexIndex, int triangleIndex, int index7bits );
};

//-----------------------------------------------------------------------------
// Purpose: Ray traces from game engine.
//-----------------------------------------------------------------------------
class IPhysicsGameTrace
{
public:
	virtual void VehicleTraceRay( const Ray_t &ray, void *pVehicle, trace_t *pTrace ) = 0;
	virtual	void VehicleTraceRayWithWater( const Ray_t &ray, void *pVehicle, trace_t *pTrace ) = 0;
	virtual bool VehiclePointInWater( const Vector &vecPoint ) = 0;
};

// The caller should implement this to return contents masks per convex on a collide
class IConvexInfo
{
public:
	virtual unsigned int GetContents( int convexGameData ) = 0;
};

class CPhysicsEventHandler;
class IPhysicsCollisionData
{
public:
	void GetSurfaceNormal( Vector &out );		// normal points toward second object (object index 1)
	void GetContactPoint( Vector &out );		// contact point of collision (in world space)
	void GetContactSpeed( Vector &out );		// speed of surface 1 relative to surface 0 (in world space)
};


struct vcollisionevent_t
{
	IPhysicsObject	*pObjects[2];
	int				surfaceProps[2];
	bool			isCollision;
	bool			isShadowCollision;
	float			deltaCollisionTime;

	float			collisionSpeed;				// only valid at postCollision
	IPhysicsCollisionData *pInternalData;		// may change pre/post collision
};

class IPhysicsCollisionEvent
{
public:
	// returns the two objects that collided, time between last collision of these objects
	// and an opaque data block of collision information
	// NOTE: PreCollision/PostCollision ALWAYS come in matched pairs!!!
	virtual void PreCollision( vcollisionevent_t *pEvent ) = 0;
	virtual void PostCollision( vcollisionevent_t *pEvent ) = 0;

	// This is a scrape event.  The object has scraped across another object consuming the indicated energy
	virtual void Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData ) = 0;

	virtual void StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData ) = 0;
	virtual void EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData ) = 0;

	virtual void FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid ) = 0;
	virtual void FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid ) = 0;

	virtual void PostSimulationFrame() = 0;

	virtual void ObjectEnterTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject ) {}
	virtual void ObjectLeaveTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject ) {}
};


class IPhysicsObjectEvent
{
public:
	// these can be used to optimize out queries on sleeping objects
	// Called when an object is woken after sleeping
	virtual void ObjectWake( IPhysicsObject *pObject ) = 0;
	// called when an object goes to sleep (no longer simulating)
	virtual void ObjectSleep( IPhysicsObject *pObject ) = 0;
};

class IPhysicsConstraintEvent
{
public:
	// the constraint is now inactive, the game code is required to delete it or re-activate it.
	virtual void ConstraintBroken( IPhysicsConstraint * ) = 0;
};

struct hlshadowcontrol_params_t
{
	Vector			targetPosition;
	QAngle			targetRotation;
	float			maxAngular;
	float			maxDampAngular;
	float			maxSpeed;
	float			maxDampSpeed;
	float			dampFactor;
	float			teleportDistance;
};

// UNDONE: At some point allow this to be parameterized using hlshadowcontrol_params_t.
// All of the infrastructure is in place to do that.
class IPhysicsShadowController
{
public:

	void Update( const Vector &position, const QAngle &angles, float timeOffset );
	void MaxSpeed( float maxSpeed, float maxAngularSpeed );
	void StepUp( float height );
	
	// If the teleport distance is non-zero, the object will be teleported to 
	// the target location when the error exceeds this quantity.
	void SetTeleportDistance( float teleportDistance );
	bool AllowsTranslation();
	bool AllowsRotation();

	// There are two classes of shadow objects:
	// 1) Game physics controlled, shadow follows game physics (this is the default)
	// 2) Physically controlled - shadow position is a target, but the game hasn't guaranteed that the space can be occupied by this object
	void SetPhysicallyControlled( bool isPhysicallyControlled );
	bool IsPhysicallyControlled();
	void GetLastImpulse( Vector *pOut );
	void UseShadowMaterial( bool bUseShadowMaterial );
	void ObjectMaterialChanged( int materialIndex );


	//Basically get the last inputs to IPhysicsShadowController::Update(), returns last input to timeOffset in Update()
	float GetTargetPosition( Vector *pPositionOut, QAngle *pAnglesOut );
	
	float GetTeleportDistance( void );
	void GetMaxSpeed( float *pMaxSpeedOut, float *pMaxAngularSpeedOut );
};

class CPhysicsSimObject;
class IPhysicsMotionController;

// Callback for simulation
class IMotionEvent
{
public:
	// These constants instruct the simulator as to how to apply the values copied to linear & angular
	// GLOBAL/LOCAL refer to the coordinate system of the values, whereas acceleration/force determine whether or not
	// mass is divided out (forces must be divided by mass to compute acceleration)
	enum simresult_e { SIM_NOTHING = 0, SIM_LOCAL_ACCELERATION, SIM_LOCAL_FORCE, SIM_GLOBAL_ACCELERATION, SIM_GLOBAL_FORCE };
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular ) = 0;
};



class IPhysicsMotionController
{
public:
	void SetEventHandler( IMotionEvent *handler );
	void AttachObject( IPhysicsObject *pObject, bool checkIfAlreadyAttached );
	void DetachObject( IPhysicsObject *pObject );

	// returns the number of objects currently attached to the controller
	int CountObjects( void );
	// NOTE: pObjectList is an array with at least CountObjects() allocated
	void GetObjects( IPhysicsObject **pObjectList );
	// detaches all attached objects
	void ClearObjects( void );
	// wakes up all attached objects
	void WakeObjects( void );

	enum priority_t
	{
		LOW_PRIORITY = 0,
		MEDIUM_PRIORITY = 1,
		HIGH_PRIORITY = 2,
	};
	void SetPriority( priority_t priority );
};

// -------------------
// Collision filter function.  Return 0 if objects should not be tested for collisions, nonzero otherwise
// Install with IPhysicsEnvironment::SetCollisionFilter()
// -------------------
class IPhysicsCollisionSolver
{
public:
	virtual int ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 ) = 0;
	virtual int ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt ) = 0;
	
	// pObject has already done the max number of collisions this tick, should we freeze it to save CPU?
	virtual bool ShouldFreezeObject( IPhysicsObject *pObject ) = 0;

	// The system has already done too many collision checks, performance will suffer.
	// How many more should it do?
	virtual int AdditionalCollisionChecksThisTick( int currentChecksDone ) = 0;

	// This list of objects is in a connected contact graph that is too large to solve quickly
	// return true to freeze the system, false to solve it
	virtual bool ShouldFreezeContacts( IPhysicsObject **pObjectList, int objectCount ) = 0;
};

enum PhysicsTraceType_t
{
	VPHYSICS_TRACE_EVERYTHING = 0,
	VPHYSICS_TRACE_STATIC_ONLY,
	VPHYSICS_TRACE_MOVING_ONLY,
	VPHYSICS_TRACE_TRIGGERS_ONLY,
	VPHYSICS_TRACE_STATIC_AND_MOVING,
};

class IPhysicsTraceFilter
{
public:
	virtual bool ShouldHitObject( IPhysicsObject *pObject, int contentsMask ) = 0;
	virtual PhysicsTraceType_t	GetTraceType() const = 0;
};

class IPhysicsEnvironment
{
public:

	void SetDebugOverlay( CreateInterfaceFn debugOverlayFactory );
	IVPhysicsDebugOverlay *GetDebugOverlay( void );

	// gravity is a 3-vector in in/s^2
	void			SetGravity( const Vector &gravityVector );
	void			GetGravity( Vector *pGravityVector ) const;

	// air density is in kg / m^3 (water is 1000)
	// This controls drag, air that is more dense has more drag.
	void			SetAirDensity( float density );
	float			GetAirDensity( void ) const;
	
	// object creation
	// create a polygonal object.  pCollisionModel was created by the physics builder DLL in a pre-process.
	IPhysicsObject	*CreatePolyObject( const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams );
	// same as above, but this one cannot move or rotate (infinite mass/inertia)
	IPhysicsObject	*CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams );
	// Create a perfectly spherical object
	IPhysicsObject *CreateSphereObject( float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic );
	// destroy an object created with CreatePolyObject() or CreatePolyObjectStatic()
	void DestroyObject( IPhysicsObject * );

	// Create a polygonal fluid body out of the specified collision model
	// This object will affect any other objects that collide with the collision model
	IPhysicsFluidController	*CreateFluidController( IPhysicsObject *pFluidObject, fluidparams_t *pParams );
	// Destroy an object created with CreateFluidController()
	void DestroyFluidController( IPhysicsFluidController * );

	// Create a simulated spring that connects 2 objects
	IPhysicsSpring	*CreateSpring( IPhysicsObject *pObjectStart, IPhysicsObject *pObjectEnd, springparams_t *pParams );
	void DestroySpring( IPhysicsSpring * );

	// Create a constraint in the space of pReferenceObject which is attached by the constraint to pAttachedObject
	IPhysicsConstraint	*CreateRagdollConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll );
	IPhysicsConstraint	*CreateHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge );
	IPhysicsConstraint	*CreateFixedConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed );
	IPhysicsConstraint	*CreateSlidingConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding );
	IPhysicsConstraint	*CreateBallsocketConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket );
	IPhysicsConstraint *CreatePulleyConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley );
	IPhysicsConstraint *CreateLengthConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length );

	void DestroyConstraint( IPhysicsConstraint * );

	IPhysicsConstraintGroup *CreateConstraintGroup( const constraint_groupparams_t &groupParams );
	void DestroyConstraintGroup( IPhysicsConstraintGroup *pGroup );

	IPhysicsShadowController *CreateShadowController( IPhysicsObject *pObject, bool allowTranslation, bool allowRotation );
	void						DestroyShadowController( IPhysicsShadowController * );

	IPhysicsPlayerController	*CreatePlayerController( IPhysicsObject *pObject );
	void						DestroyPlayerController( IPhysicsPlayerController * );

	IPhysicsMotionController	*CreateMotionController( IMotionEvent *pHandler );
	void						DestroyMotionController( IPhysicsMotionController *pController );

	IPhysicsVehicleController	*CreateVehicleController( IPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace );
	void						DestroyVehicleController( IPhysicsVehicleController * );

	// install a function to filter collisions/penentration
	void			SetCollisionSolver( IPhysicsCollisionSolver *pSolver );

	// run the simulator for deltaTime seconds
	void			Simulate( float deltaTime );
	// true if currently running the simulator (i.e. in a callback during physenv->Simulate())
	bool			IsInSimulation() const;

	// Manage the timestep (period) of the simulator.  The main functions are all integrated with
	// this period as dt.
	float			GetSimulationTimestep() const;
	void			SetSimulationTimestep( float timestep );

	// returns the current simulation clock's value.  This is an absolute time.
	float			GetSimulationTime() const;
	void			ResetSimulationClock();
	// returns the current simulation clock's value at the next frame.  This is an absolute time.
	float			GetNextFrameTime( void ) const;

	// Collision callbacks (game code collision response)
	void			SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents );
	void			SetObjectEventHandler( IPhysicsObjectEvent *pObjectEvents );
	void			SetConstraintEventHandler( IPhysicsConstraintEvent *pConstraintEvents );

	void			SetQuickDelete( bool bQuick );

	int				GetActiveObjectCount() const;
	void			GetActiveObjects( IPhysicsObject **pOutputObjectList ) const;
	const IPhysicsObject **GetObjectList( int *pOutputObjectCount ) const;
	bool			TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment );

	void			CleanupDeleteList( void );
	void			EnableDeleteQueue( bool enable );

	// Debugging:
	bool IsCollisionModelUsed( CPhysCollide *pCollide ) const;
	
	// Physics world version of the enginetrace API:
	void TraceRay( const Ray_t &ray, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace );
	void SweepCollideable( const CPhysCollide *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
		const QAngle &vecAngles, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace );

	// performance tuning
	void GetPerformanceSettings( physics_performanceparams_t *pOutput ) const;
	void SetPerformanceSettings( const physics_performanceparams_t *pSettings );

	// perf/cost statistics
	void ReadStats( physics_stats_t *pOutput );
	void ClearStats();

	unsigned int	GetObjectSerializeSize( IPhysicsObject *pObject ) const;
	void			SerializeObjectToBuffer( IPhysicsObject *pObject, unsigned char *pBuffer, unsigned int bufferSize );
	IPhysicsObject *UnserializeObjectFromBuffer( void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions );


	void EnableConstraintNotify( bool bEnable );
	void DebugCheckContacts(void);

	void			SetAlternateGravity( const Vector &gravityVector );
	void			GetAlternateGravity( Vector *pGravityVector ) const;

	float			GetDeltaFrameTime( int maxTicks ) const;
	void			ForceObjectsToSleep( IPhysicsObject **pList, int listCount );
	
	//Network prediction related functions
	void			SetPredicted( bool bPredicted ); //Interaction with this system and it's objects may not always march forward, sometimes it will get/set data in the past.
	bool			IsPredicted( void );
	void			SetPredictionCommandNum( int iCommandNum ); //what command the client is working on right now
	int				GetPredictionCommandNum( void );
	void			DoneReferencingPreviousCommands( int iCommandNum ); //won't need data from commands before this one any more
	void			RestorePredictedSimulation( void ); //called to restore results from a previous simulation with the same predicted timestamp set

	// destroy a CPhysCollide used in CreatePolyObject()/CreatePolyObjectStatic() when any owning IPhysicsObject is flushed from the queued deletion list.
	void DestroyCollideOnDeadObjectFlush( CPhysCollide * ); //should only be used after calling DestroyObject() on all IPhysicsObjects created with it.
};

enum callbackflags
{
	CALLBACK_GLOBAL_COLLISION	= 0x0001,
	CALLBACK_GLOBAL_FRICTION	= 0x0002,
	CALLBACK_GLOBAL_TOUCH		= 0x0004,
	CALLBACK_GLOBAL_TOUCH_STATIC = 0x0008,
	CALLBACK_SHADOW_COLLISION	= 0x0010,
	CALLBACK_GLOBAL_COLLIDE_STATIC = 0x0020,
	CALLBACK_IS_VEHICLE_WHEEL	= 0x0040,
	CALLBACK_FLUID_TOUCH		= 0x0100,
	CALLBACK_NEVER_DELETED		= 0x0200,	// HACKHACK: This means this object will never be deleted (set on the world)
	CALLBACK_MARKED_FOR_DELETE	= 0x0400,	// This allows vphysics to skip some work for this object since it will be
											// deleted later this frame. (Set automatically by destroy calls)
	CALLBACK_ENABLING_COLLISION = 0x0800,	// This is active during the time an object is enabling collisions
											// allows us to skip collisions between "new" objects and objects marked for delete
	CALLBACK_DO_FLUID_SIMULATION = 0x1000,  // remove this to opt out of fluid simulations
	CALLBACK_IS_PLAYER_CONTROLLER= 0x2000,	// HACKHACK: Set this on players until player cotrollers are unified with shadow controllers
	CALLBACK_CHECK_COLLISION_DISABLE = 0x4000,
	CALLBACK_MARKED_FOR_TEST	= 0x8000,	// debug -- marked object is being debugged
};

enum collisionhints
{
	COLLISION_HINT_DEBRIS		= 0x0001,
	COLLISION_HINT_STATICSOLID	= 0x0002,
	COLLISION_HINT_RAGDOLL      = 0x0004, // lwss add
};

class IPredictedPhysicsObject;

class IPhysicsObject
{
public:

	// returns true if this object is static/unmoveable
	// NOTE: returns false for objects that are not created static, but set EnableMotion(false);
	// Call IsMoveable() to find if the object is static OR has motion disabled
	bool			IsStatic() const;
	bool			IsAsleep() const;
	bool			IsTrigger() const;
	bool			IsFluid() const;		// fluids are special triggers with fluid controllers attached, they return true to IsTrigger() as well!
	bool			IsHinged() const;
	bool			IsCollisionEnabled() const;
	bool			IsGravityEnabled() const;
	bool			IsDragEnabled() const;
	bool			IsMotionEnabled() const;
	bool			IsMoveable() const;	 // legacy: IsMotionEnabled() && !IsStatic()
	bool			IsAttachedToConstraint(bool bExternalOnly) const;

	// Enable / disable collisions for this object
	void			EnableCollisions( bool enable );
	// Enable / disable gravity for this object
	void			EnableGravity( bool enable );
	// Enable / disable air friction / drag for this object
	void			EnableDrag( bool enable );
	// Enable / disable motion (pin / unpin the object)
	void			EnableMotion( bool enable );

	// Game can store data in each object (link back to game object)
	void			SetGameData( void *pGameData );
	void			*GetGameData( void ) const;
	// This flags word can be defined by the game as well
	void			SetGameFlags( unsigned short userFlags );
	unsigned short	GetGameFlags( void ) const;
	void			SetGameIndex( unsigned short gameIndex );
	unsigned short	GetGameIndex( void ) const;

	// setup various callbacks for this object
	void			SetCallbackFlags( unsigned short callbackflags );
	// get the current callback state for this object
	unsigned short	GetCallbackFlags( void ) const;

	// "wakes up" an object
	// NOTE: ALL OBJECTS ARE "Asleep" WHEN CREATED
	void			Wake( void );
	void			Sleep( void );
	// call this when the collision filter conditions change due to this 
	// object's state (e.g. changing solid type or collision group)
	void			RecheckCollisionFilter();
	// NOTE: Contact points aren't updated when collision rules change, call this to force an update
	// UNDONE: Force this in RecheckCollisionFilter() ?
	void			RecheckContactPoints( bool bSearchForNewContacts = false );

	// mass accessors
	void			SetMass( float mass );
	float			GetMass( void ) const;
	// get 1/mass (it's cached)
	float			GetInvMass( void ) const;
	Vector			GetInertia( void ) const;
	Vector			GetInvInertia( void ) const;
	void			SetInertia( const Vector &inertia );

	void			SetDamping( const float *speed, const float *rot );
	void			GetDamping( float *speed, float *rot ) const;

	// coefficients are optional, pass either
	void			SetDragCoefficient( float *pDrag, float *pAngularDrag );
	void			SetBuoyancyRatio( float ratio );			// Override bouyancy

	// material index
	int				GetMaterialIndex() const;
	void			SetMaterialIndex( int materialIndex );

	// contents bits
	unsigned int	GetContents() const;
	void			SetContents( unsigned int contents );

	// Get the radius if this is a sphere object (zero if this is a polygonal mesh)
	float			GetSphereRadius() const;
	// Set the radius on a sphere. May need to force recalculation of contact points
	void			SetSphereRadius(float radius);
	float			GetEnergy() const;
	Vector			GetMassCenterLocalSpace() const;

	// NOTE: This will teleport the object
	void			SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport );
	void			SetPositionMatrix( const matrix3x4_t&matrix, bool isTeleport );

	void			GetPosition( Vector *worldPosition, QAngle *angles ) const;
	void			GetPositionMatrix( matrix3x4_t *positionMatrix ) const;
	// force the velocity to a new value
	// NOTE: velocity is in worldspace, angularVelocity is relative to the object's 
	// local axes (just like pev->velocity, pev->avelocity)
	void			SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity );

	// like the above, but force the change into the simulator immediately
	void			SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity );

	// NOTE: velocity is in worldspace, angularVelocity is relative to the object's 
	// local axes (just like pev->velocity, pev->avelocity)
	void			GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const;

	// NOTE: These are velocities, not forces.  i.e. They will have the same effect regardless of
	// the object's mass or inertia
	void			AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity );
	// gets a velocity in the object's local frame of reference at a specific point
	void			GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const;
	// gets the velocity actually moved by the object in the last simulation update
	void			GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const;
	// NOTE:	These are here for convenience, but you can do them yourself by using the matrix
	//			returned from GetPositionMatrix()
	// convenient coordinate system transformations (params - dest, src)
	void			LocalToWorld( Vector *worldPosition, const Vector &localPosition ) const;
	void			WorldToLocal( Vector *localPosition, const Vector &worldPosition ) const;

	// transforms a vector (no translation) from object-local to world space
	void			LocalToWorldVector( Vector *worldVector, const Vector &localVector ) const;
	// transforms a vector (no translation) from world to object-local space
	void			WorldToLocalVector( Vector *localVector, const Vector &worldVector ) const;
	
	// push on an object
	// force vector is direction & magnitude of impulse kg in / s
	void			ApplyForceCenter( const Vector &forceVector );
	void			ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition );
	// apply torque impulse.  This will change the angular velocity on the object.
	// HL Axes, kg degrees / s
	void			ApplyTorqueCenter( const AngularImpulse &torque );

	// Calculates the force/torque on the center of mass for an offset force impulse (pass output to ApplyForceCenter / ApplyTorqueCenter)
	void			CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const;
	// Calculates the linear/angular velocities on the center of mass for an offset force impulse (pass output to AddVelocity)
	void			CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const;
	// calculate drag scale
	float			CalculateLinearDrag( const Vector &unitDirection ) const;
	float			CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const;

	// returns true if the object is in contact with another object
	// if true, puts a point on the contact surface in contactPoint, and
	// a pointer to the object in contactObject
	// NOTE: You can pass NULL for either to avoid computations
	// BUGBUG: Use CreateFrictionSnapshot instead of this - this is a simple hack
	bool			GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const;

	// refactor this a bit - move some of this to IPhysicsShadowController
	void			SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation );
	void			UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset );
	
	// returns number of ticks since last Update() call
	int				GetShadowPosition( Vector *position, QAngle *angles ) const;
	IPhysicsShadowController *GetShadowController( void ) const;
	void			RemoveShadowController();
	// applies the math of the shadow controller to this object.
	// for use in your own controllers
	// returns the new value of secondsToArrival with dt time elapsed
	float			ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt );


	const CPhysCollide	*GetCollide( void ) const;
	const char			*GetName() const;

	void			BecomeTrigger();
	void			RemoveTrigger();

	// sets the object to be hinged.  Fixed it place, but able to rotate around one axis.
	void			BecomeHinged( int localAxis );
	// resets the object to original state
	void			RemoveHinged();

	// used to iterate the contact points of an object
	IPhysicsFrictionSnapshot *CreateFrictionSnapshot();
	void DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot );

	// dumps info about the object to Msg()
	void			OutputDebugInfo() const;

	// EnableGravity still determines whether to apply gravity
	// This flag determines which gravity constant to use for an alternate gravity effect
	void			SetUseAlternateGravity( bool bSet );
	void			SetCollisionHints( uint32 collisionHints );
	uint32			GetCollisionHints() const;

	inline bool				IsPredicted( void ) const { return GetPredictedInterface() != nullptr; } //true if class has an IPredictedPhysicsObject interface
	IPredictedPhysicsObject *GetPredictedInterface( void ) const;
	void			SyncWith( IPhysicsObject *pOther );
};

class IPredictedPhysicsObject : public IPhysicsObject
{
public:

	void SetErrorDelta_Position( const Vector &vPosition );
	void SetErrorDelta_Velocity( const Vector &vVelocity );
};


class IPhysicsSpring
{
public:
	void			GetEndpoints( Vector *worldPositionStart, Vector *worldPositionEnd );
	void			SetSpringConstant( float flSpringContant);
	void			SetSpringDamping( float flSpringDamping);
	void			SetSpringLength( float flSpringLenght);

	// Get the starting object
	IPhysicsObject *GetStartObject( void );

	// Get the end object
	IPhysicsObject *GetEndObject( void );
};


//-----------------------------------------------------------------------------
// Purpose: These properties are defined per-material.  This is accessible at 
//			each triangle in a collision mesh
//-----------------------------------------------------------------------------
struct surfacephysicsparams_t
{
// vphysics physical properties
	float			friction;
	float			elasticity;				// collision elasticity - used to compute coefficient of restitution
	float			density;				// physical density (in kg / m^3)
	float			thickness;				// material thickness if not solid (sheet materials) in inches
	float			dampening;
};

struct surfaceaudioparams_t
{
// sounds / audio data
	float			reflectivity;		// like elasticity, but how much sound should be reflected by this surface
	float			hardnessFactor;	// like elasticity, but only affects impact sound choices
	float			roughnessFactor;	// like friction, but only affects scrape sound choices

// audio thresholds
	float			roughThreshold;	// surface roughness > this causes "rough" scrapes, < this causes "smooth" scrapes
	float			hardThreshold;	// surface hardness > this causes "hard" impacts, < this causes "soft" impacts
	float			hardVelocityThreshold;	// collision velocity > this causes "hard" impacts, < this causes "soft" impacts
									// NOTE: Hard impacts must meet both hardnessFactor AND velocity thresholds
};

struct surfacesoundnames_t
{
	unsigned short	walkStepLeft;
	unsigned short	walkStepRight;
	unsigned short	runStepLeft;
	unsigned short	runStepRight;

	unsigned short	impactSoft;
	unsigned short	impactHard;

	unsigned short	scrapeSmooth;
	unsigned short	scrapeRough;

	unsigned short	bulletImpact;
	unsigned short	rolling;

	unsigned short	breakSound;
	unsigned short	strainSound;
};

struct surfacesoundhandles_t
{
	HSOUNDSCRIPTHASH	walkStepLeft;
	HSOUNDSCRIPTHASH	walkStepRight;
	HSOUNDSCRIPTHASH	runStepLeft;
	HSOUNDSCRIPTHASH	runStepRight;

	HSOUNDSCRIPTHASH	impactSoft;
	HSOUNDSCRIPTHASH	impactHard;

	HSOUNDSCRIPTHASH	scrapeSmooth;
	HSOUNDSCRIPTHASH	scrapeRough;

	HSOUNDSCRIPTHASH	bulletImpact;
	HSOUNDSCRIPTHASH	rolling;

	HSOUNDSCRIPTHASH	breakSound;
	HSOUNDSCRIPTHASH	strainSound;
};

struct surfacegameprops_t
{
// game movement data
	float			maxSpeedFactor;			// Modulates player max speed when walking on this surface
	float			jumpFactor;				// Indicates how much higher the player should jump when on the surface
// Game-specific data
	float			penetrationModifier;
	float			damageModifier;
	unsigned short	material;
	// Indicates whether or not the player is on a ladder.
	unsigned char	climbable;
	unsigned char	pad;
};

//-----------------------------------------------------------------------------
// Purpose: Each different material has an entry like this
//-----------------------------------------------------------------------------
struct surfacedata_t
{
	surfacephysicsparams_t	physics;	// physics parameters
	surfaceaudioparams_t	audio;		// audio parameters
	surfacesoundnames_t		sounds;		// names of linked sounds
	surfacegameprops_t		game;		// Game data / properties

	surfacesoundhandles_t		soundhandles;
};

class ISaveRestoreOps;

inline constexpr const char *VPHYSICS_SURFACEPROPS_INTERFACE_VERSION = "VPhysicsSurfaceProps001";
class IPhysicsSurfaceProps
{
public:

	// parses a text file containing surface prop keys
	int		ParseSurfaceData( const char *pFilename, const char *pTextfile );
	// current number of entries in the database
	int		SurfacePropCount( void ) const;

	int		GetSurfaceIndex( const char *pSurfacePropName ) const;
	void	GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const;

	surfacedata_t	*GetSurfaceData( int surfaceDataIndex );
	const char		*GetString( unsigned short stringTableIndex ) const;


	const char		*GetPropName( int surfaceDataIndex ) const;

	// sets the global index table for world materials
	// UNDONE: Make this per-CPhysCollide
	void	SetWorldMaterialIndexTable( int *pMapArray, int mapSize );

	// NOTE: Same as GetPhysicsProperties, but maybe more convenient
	void	GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const;

	//lwss - commented this out, not used.
	//virtual ISaveRestoreOps* GetMaterialIndexDataOps() const = 0;
};

class IPhysicsFluidController
{
public:

	void	SetGameData( void *pGameData );
	void	*GetGameData( void ) const;

	void	GetSurfacePlane( Vector *pNormal, float *pDist ) const;
	float	GetDensity() const;
	void	WakeAllSleepingObjects();
	int		GetContents() const;
};


//-----------------------------------------------------------------------------
// Purpose: parameter block for creating fluid dynamic motion
// UNDONE: Expose additional fluid model paramters?
//-----------------------------------------------------------------------------
struct fluidparams_t
{
	Vector4D	surfacePlane;	// x,y,z normal, dist (plane constant) fluid surface
	Vector		currentVelocity; // velocity of the current in inches/second
	float		damping;		// damping factor for buoyancy (tweak)
	float		torqueFactor;
	float		viscosityFactor;
	void		*pGameData;
	bool		useAerodynamics;// true if this controller should calculate surface pressure
	int			contents;

	fluidparams_t() {}
	fluidparams_t( fluidparams_t const& src )
	{
		Vector4DCopy( src.surfacePlane, surfacePlane );
		VectorCopy( src.currentVelocity, currentVelocity );
		damping = src.damping;
		torqueFactor = src.torqueFactor;
		viscosityFactor = src.viscosityFactor;
		contents = src.contents;
	}
};

//-----------------------------------------------------------------------------
// Purpose: parameter block for creating linear springs
// UNDONE: Expose additional spring model paramters?
//-----------------------------------------------------------------------------
struct springparams_t
{
	springparams_t()
	{
		std::memset( this, 0, sizeof(*this) );
	}
	float	constant;		// spring constant
	float	naturalLength;// relaxed length
	float	damping;		// damping factor
	float	relativeDamping;	// relative damping (damping proportional to the change in the relative position of the objects)
	Vector	startPosition;
	Vector	endPosition;
	bool	useLocalPositions;	// start & end Position are in local space to start and end objects if this is true
	bool	onlyStretch;		// only apply forces when the length is greater than the natural length
};

//-----------------------------------------------------------------------------
// Purpose: parameter block for creating polygonal objects
//-----------------------------------------------------------------------------
struct objectparams_t
{
	Vector		*massCenterOverride;
	float		mass;
	float		inertia;
	float		damping;
	float		rotdamping;
	float		rotInertiaLimit;
	const char	*pName;				// used only for debugging
	void		*pGameData;
	float		volume;
	float		dragCoefficient;
	bool		enableCollisions;
};

struct convertconvexparams_t
{
	bool		buildOuterConvexHull;
	bool		buildDragAxisAreas;
	bool		buildOptimizedTraceTables;
	bool		checkOptimalTracing;
	bool		bUseFastApproximateInertiaTensor;
	bool		bBuildAABBTree;
	float		dragAreaEpsilon;
	CPhysConvex *pForcedOuterHull;

	void Defaults()
	{
		dragAreaEpsilon = 0.25f; // 0.5in x 0.5in square
		buildOuterConvexHull = false;
		buildDragAxisAreas = false;
		buildOptimizedTraceTables = false;
		checkOptimalTracing = false;
		bUseFastApproximateInertiaTensor = false;
		bBuildAABBTree = false;
		pForcedOuterHull = nullptr;
	}
};

//-----------------------------------------------------------------------------
// Physics interface IDs
//
// Note that right now the order of the enum also defines the order of save/load


//-------------------------------------

#define DEFINE_PIID( type, enumval ) \
	template <> inline PhysInterfaceId_t GetPhysIID<type>( type ** ) { return enumval; }

template <class PHYSPTR> inline PhysInterfaceId_t GetPhysIID(PHYSPTR **); // will get link error if no match

DEFINE_PIID( IPhysicsObject, 			PIID_IPHYSICSOBJECT );
DEFINE_PIID( IPhysicsFluidController, 	PIID_IPHYSICSFLUIDCONTROLLER );
DEFINE_PIID( IPhysicsSpring, 			PIID_IPHYSICSSPRING );
DEFINE_PIID( IPhysicsConstraintGroup, 	PIID_IPHYSICSCONSTRAINTGROUP );
DEFINE_PIID( IPhysicsConstraint, 		PIID_IPHYSICSCONSTRAINT );
DEFINE_PIID( IPhysicsShadowController, 	PIID_IPHYSICSSHADOWCONTROLLER );
DEFINE_PIID( IPhysicsPlayerController,	PIID_IPHYSICSPLAYERCONTROLLER );
DEFINE_PIID( IPhysicsMotionController,	PIID_IPHYSICSMOTIONCONTROLLER );
DEFINE_PIID( IPhysicsVehicleController,	PIID_IPHYSICSVEHICLECONTROLLER );
DEFINE_PIID( IPhysicsGameTrace,			PIID_IPHYSICSGAMETRACE );

//-----------------------------------------------------------------------------
