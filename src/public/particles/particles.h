#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

// Purpose: particle system definitions

// Suppress GCC warning about SIMD type attributes in templates
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

import std;
import ks.vstdlib;
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"

#include "dmxloader/dmxelement.h"
#include "trace.h"
#include "raytrace.h"
#include "materialsystem/imesh.h"
#include "particles/particle_operator_list.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct DmxElementUnpackStructure_t;
class CParticleSystemDefinition;
class CParticleCollection;
class CParticleOperatorInstance;
struct CParticleOperatorDeleter
{
	void operator()( CParticleOperatorInstance *pOp ) const;
};
typedef std::vector< std::unique_ptr< CParticleOperatorInstance, CParticleOperatorDeleter > > ParticleOperatorList_t;
class CParticleSystemDictionary;
namespace ks
{
	class byte_reader;
	class byte_writer;
}
class IParticleOperatorDefinition;
class CSheet;
class CMeshBuilder;
extern float s_pRandomFloats[];
				

//-----------------------------------------------------------------------------
// Random numbers
//-----------------------------------------------------------------------------
#define MAX_RANDOM_FLOATS 4096
#define RANDOM_FLOAT_MASK ( MAX_RANDOM_FLOATS - 1 )

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Get a list of the files inside the particle manifest file
void GetParticleManifest( std::vector< std::string >& list );
void GetParticleManifest( std::vector< std::string >& list, const char *pFile );

//-----------------------------------------------------------------------------
// Particle attributes
//-----------------------------------------------------------------------------
inline constexpr auto MAX_PARTICLE_ATTRIBUTES = 24;

#define DEFPARTICLE_ATTRIBUTE( name, bit, datatype )			\
	const int PARTICLE_ATTRIBUTE_##name##_MASK = (1 << bit);	\
	const int PARTICLE_ATTRIBUTE_##name = bit;					\
	const EAttributeDataType PARTICLE_ATTRIBUTE_##name##_DATATYPE = datatype;

// required
DEFPARTICLE_ATTRIBUTE( XYZ, 0, ATTRDATATYPE_4V );

// particle lifetime (duration) of particle as a float.
DEFPARTICLE_ATTRIBUTE( LIFE_DURATION, 1, ATTRDATATYPE_FLOAT );

// prev coordinates for verlet integration
DEFPARTICLE_ATTRIBUTE( PREV_XYZ, 2, ATTRDATATYPE_4V );

// radius of particle
DEFPARTICLE_ATTRIBUTE( RADIUS, 3, ATTRDATATYPE_FLOAT );

// rotation angle of particle
DEFPARTICLE_ATTRIBUTE( ROTATION, 4, ATTRDATATYPE_FLOAT );

// rotation speed of particle
DEFPARTICLE_ATTRIBUTE( ROTATION_SPEED, 5, ATTRDATATYPE_FLOAT );

// tint of particle
DEFPARTICLE_ATTRIBUTE( TINT_RGB, 6, ATTRDATATYPE_4V );

// alpha tint of particle
DEFPARTICLE_ATTRIBUTE( ALPHA, 7, ATTRDATATYPE_FLOAT );

// creation time stamp (relative to particle system creation)
DEFPARTICLE_ATTRIBUTE( CREATION_TIME, 8, ATTRDATATYPE_FLOAT );

// sequnece # (which animation sequence number this particle uses )
DEFPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER, 9, ATTRDATATYPE_FLOAT );

// length of the trail 
DEFPARTICLE_ATTRIBUTE( TRAIL_LENGTH, 10, ATTRDATATYPE_FLOAT );

// unique particle identifier
DEFPARTICLE_ATTRIBUTE( PARTICLE_ID, 11, ATTRDATATYPE_INT );

// unique rotation around up vector
DEFPARTICLE_ATTRIBUTE( YAW, 12, ATTRDATATYPE_FLOAT );

// second sequnece # (which animation sequence number this particle uses )
DEFPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER1, 13, ATTRDATATYPE_FLOAT );

// hit box index
DEFPARTICLE_ATTRIBUTE( HITBOX_INDEX, 14, ATTRDATATYPE_INT );

DEFPARTICLE_ATTRIBUTE( HITBOX_RELATIVE_XYZ, 15, ATTRDATATYPE_4V );

DEFPARTICLE_ATTRIBUTE( ALPHA2, 16, ATTRDATATYPE_FLOAT );

// particle trace caching fields
DEFPARTICLE_ATTRIBUTE( SCRATCH_VEC, 17, ATTRDATATYPE_4V );		//scratch field used for storing arbitraty vec data	
DEFPARTICLE_ATTRIBUTE( SCRATCH_FLOAT, 18, ATTRDATATYPE_4V );	//scratch field used for storing arbitraty float data		
DEFPARTICLE_ATTRIBUTE( UNUSED, 19, ATTRDATATYPE_FLOAT );	
DEFPARTICLE_ATTRIBUTE( PITCH, 20, ATTRDATATYPE_4V );	

DEFPARTICLE_ATTRIBUTE( NORMAL, 21, ATTRDATATYPE_4V );			// 0 0 0 if none

DEFPARTICLE_ATTRIBUTE( GLOW_RGB, 22, ATTRDATATYPE_4V );			// glow color
DEFPARTICLE_ATTRIBUTE( GLOW_ALPHA, 23, ATTRDATATYPE_FLOAT );	// glow alpha

inline constexpr auto MAX_PARTICLE_CONTROL_POINTS = 64;

#define ATTRIBUTES_WHICH_ARE_VEC3S_MASK ( PARTICLE_ATTRIBUTE_SCRATCH_VEC_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK | \
                                          PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_TINT_RGB_MASK | \
                                          PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK  | PARTICLE_ATTRIBUTE_NORMAL_MASK | \
	                                      PARTICLE_ATTRIBUTE_GLOW_RGB_MASK )

inline constexpr auto ATTRIBUTES_WHICH_ARE_0_TO_1 = (PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_ALPHA2_MASK);
#define ATTRIBUTES_WHICH_ARE_ANGLES (PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_YAW_MASK | PARTICLE_ATTRIBUTE_PITCH_MASK )
inline constexpr auto ATTRIBUTES_WHICH_ARE_INTS = (PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK | PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK );

// Auto filters
inline constexpr auto ATTRIBUTES_WHICH_ARE_POSITION_AND_VELOCITY = (PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK);
inline constexpr auto ATTRIBUTES_WHICH_ARE_LIFE_DURATION = (PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK);
#define ATTRIBUTES_WHICH_ARE_ROTATION (PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_YAW_MASK | PARTICLE_ATTRIBUTE_ROTATION_SPEED_MASK | PARTICLE_ATTRIBUTE_PITCH_MASK )
inline constexpr auto ATTRIBUTES_WHICH_ARE_SIZE = (PARTICLE_ATTRIBUTE_RADIUS_MASK | PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK);
#define ATTRIBUTES_WHICH_ARE_COLOR_AND_OPACITY (PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_GLOW_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_ALPHA2_MASK | PARTICLE_ATTRIBUTE_GLOW_ALPHA_MASK )
inline constexpr auto ATTRIBUTES_WHICH_ARE_ANIMATION_SEQUENCE = (PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1_MASK);
inline constexpr auto ATTRIBUTES_WHICH_ARE_HITBOX = (PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK | PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK);
inline constexpr auto ATTRIBUTES_WHICH_ARE_NORMAL = (PARTICLE_ATTRIBUTE_NORMAL_MASK);

inline constexpr auto MAX_PARTICLES_IN_A_SYSTEM = 5000;

// Set this to 1 or 0 to enable or disable particle profiling.
// Note that this profiling is expensive on Linux, and some anti-virus
// products can make this very expensive on Windows.

inline constexpr auto MIN_PARTICLE_SPEED = 0.001;

inline constexpr auto MAX_PARTICLE_ORIENTATION_TYPES = 3;

//-----------------------------------------------------------------------------
// Particle function types
//-----------------------------------------------------------------------------
enum ParticleFunctionType_t
{
	FUNCTION_RENDERER = 0,
	FUNCTION_OPERATOR,
	FUNCTION_INITIALIZER,
	FUNCTION_EMITTER,
	FUNCTION_CHILDREN,	// NOTE: This one is a fake function type, only here to help eliminate a ton of duplicated code in the editor
    FUNCTION_FORCEGENERATOR,
    FUNCTION_CONSTRAINT,
	PARTICLE_FUNCTION_COUNT
};

//-----------------------------------------------------------------------------
// Particle filter types
// Used for classifying operators in the interface
// Can only have 32 altogether
//-----------------------------------------------------------------------------
enum ParticleFilterType_t
{
	FILTER_NOT_SPECIAL = 0,
	FILTER_POSITION_AND_VELOCITY,
	FILTER_LIFE_DURATION,
	FILTER_PARAMETER_REMAPPING,
	FILTER_ROTATION,
	FILTER_SIZE,
	FILTER_COLOR_AND_OPACITY,
	FILTER_ANIMATION_SEQUENCE,
	FILTER_HITBOX,
	FILTER_NORMAL,
	FILTER_CONTROL_POINTS,
	FILTER_COUNT
};

enum ParticleFilterMask_t
{
	FILTER_NOT_SPECIAL_MASK				= 0,
	FILTER_POSITION_AND_VELOCITY_MASK	= 1 << 1,
	FILTER_LIFE_DURATION_MASK			= 1 << 2,
	FILTER_PARAMETER_REMAPPING_MASK		= 1 << 3,
	FILTER_ROTATION_MASK				= 1 << 4,
	FILTER_SIZE_MASK					= 1 << 5,
	FILTER_COLOR_AND_OPACITY_MASK		= 1 << 6,
	FILTER_ANIMATION_SEQUENCE_MASK		= 1 << 7,
	FILTER_HITBOX_MASK					= 1 << 8,
	FILTER_NORMAL_MASK					= 1 << 9,
	FILTER_CONTROL_POINTS_MASK			= 1 << 10
};

struct CParticleVisibilityInputs
{
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flAlphaScaleMin;
	float	m_flAlphaScaleMax;
	float	m_flRadiusScaleMin;
	float	m_flRadiusScaleMax;
	float	m_flRadiusScaleFOVBase;
	float	m_flProxyRadius;
	float	m_flDistanceInputMin;
	float	m_flDistanceInputMax;
	float	m_flDotInputMin;
	float	m_flDotInputMax;
	int		m_nCPin;
};

struct ModelHitBoxInfo_t
{
	Vector m_vecBoxMins;
	Vector m_vecBoxMaxes;
	matrix3x4_t m_Transform;
};

class CModelHitBoxesInfo
{
public:
	float m_flLastUpdateTime;
	float m_flPrevLastUpdateTime;
	int m_nNumHitBoxes;
	int m_nNumPrevHitBoxes;
	ModelHitBoxInfo_t *m_pHitBoxes;
	ModelHitBoxInfo_t *m_pPrevBoxes;

	bool CurAndPrevValid( void ) const
	{
		return ( m_nNumHitBoxes && ( m_nNumPrevHitBoxes == m_nNumHitBoxes ) );
	}

	CModelHitBoxesInfo( void )
	{
		m_flLastUpdateTime = -1;
		m_nNumHitBoxes = 0;
		m_nNumPrevHitBoxes = 0;
		m_pHitBoxes = nullptr;
		m_pPrevBoxes = nullptr;

	}

	~CModelHitBoxesInfo( void )
	{
		if ( m_pHitBoxes )
			delete[] m_pHitBoxes;
		if ( m_pPrevBoxes )
			delete[] m_pPrevBoxes;
	}

};


//-----------------------------------------------------------------------------
// Particle kill list
//-----------------------------------------------------------------------------
inline constexpr auto KILL_LIST_INDEX_BITS = 24;
inline constexpr auto KILL_LIST_FLAGS_BITS = ( 32 - KILL_LIST_INDEX_BITS );
inline constexpr auto KILL_LIST_INDEX_MASK = ( ( 1 << KILL_LIST_INDEX_BITS ) - 1 );
inline constexpr auto KILL_LIST_FLAGS_MASK = ( ( 1 << KILL_LIST_FLAGS_BITS ) - 1 );
struct KillListItem_t
{
	unsigned int nIndex : KILL_LIST_INDEX_BITS;
	unsigned int nFlags : KILL_LIST_FLAGS_BITS;
};
enum KillListFlags
{
	// TODO: use this in ApplyKillList (the idea: pass particles to a child system, but dont then kill them)
	KILL_LIST_FLAG_DONT_KILL = ( 1 << 0 )
};


//-----------------------------------------------------------------------------
// Interface to allow the particle system to call back into the client
//-----------------------------------------------------------------------------

// The game side that has something to answer with (the client) hands the manager one of these:
// the particle library lives in the shared image, the game classes live in the side's namespace, so
// the game's answers reach the library as borrowed callables. An IParticleSystemQuery without hooks
// answers with the engine's defaults (what the server and the tools get).
struct ParticleQueryHooks
{
	std::function_ref< void( const Vector &vecOrigin, Color &tint ) > GetLightingAtPoint;
	std::function_ref< void( const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask,
							 const class IHandleEntity *ignore, int collisionGroup, CBaseTrace *ptr ) > TraceLine;
	std::function_ref< bool( const Vector &vecPos, const int nContentsMask ) > IsPointInSolid;
	std::function_ref< bool( CParticleCollection *pParticles, void *pObject, Vector *pPnt ) > MovePointInsideControllingObject;
	std::function_ref< int( const char *pszRtEnvName ) > GetRayTraceEnvironmentFromName;
	std::function_ref< int( const char *pszCollisionGroupName ) > GetCollisionGroupFromName;
	std::function_ref< void( CParticleCollection *pParticles, int nControlPointNumber, int nNumPtsOut, float flBBoxScale,
							 int nNumTrysToGetAPointInsideTheModel, Vector *pPntsOut, Vector vecDirectionBias,
							 Vector *pHitBoxRelativeCoordOut, int *pHitBoxIndexOut, int nDesiredHitbox,
							 const char *pszHitboxSetName ) > GetRandomPointsOnControllingObjectHitBox;
	std::function_ref< void( CParticleCollection *pParticles, int nControlPointNumber, int nNumPtsIn, float flBBoxScale,
							 Vector *pPntsIn, Vector *pHitBoxRelativeCoordOut, int *pHitBoxIndexOut, int nDesiredHitbox,
							 const char *pszHitboxSetName ) > GetClosestControllingObjectHitBox;
	std::function_ref< int( CParticleCollection *pParticles, int nControlPointNumber, int nBufSize,
							ModelHitBoxInfo_t *pHitBoxOutputBuffer, const char *pszHitboxSetName ) > GetControllingObjectHitBoxInfo;
	std::function_ref< void( CParticleCollection *pParticles, int nControlPointNumber, Vector vecMin, Vector vecMax ) > GetControllingObjectOBBox;
	std::function_ref< void( int envnumber, const FourRays &rays, fltx4 TMin, fltx4 TMax,
							 RayTracingResult *rslt_out, int32 skip_id ) > TraceAgainstRayTraceEnv;
	std::function_ref< Vector() > GetLocalPlayerPos;
	std::function_ref< void( Vector *pForward, Vector *pRight, Vector *pUp ) > GetLocalPlayerEyeVectors;
	std::function_ref< Vector() > GetCurrentViewOrigin;
	std::function_ref< int() > GetActivityCount;
	std::function_ref< const char *( int nActivityIndex ) > GetActivityNameFromIndex;
	std::function_ref< int( void *pModel, const char *pszActivityName ) > GetActivityNumber;
	std::function_ref< float( int *pQueryHandle, const Vector &vecOrigin, float flScale ) > GetPixelVisibility;
	std::function_ref< void( const Vector &pos ) > SetUpLightingEnvironment;
	std::function_ref< void() > PreSimulate;
	std::function_ref< void() > PostSimulate;
	std::function_ref< void( const Vector &origin, const Vector &dest, int r, int g, int b, bool noDepthTest, float duration ) > DebugDrawLine;
	std::function_ref< void *( char const *pMdlName ) > GetModel;
	std::function_ref< void( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber,
							 int nBodyPart, int nSubModel, int nSkin, int nAnimationSequence, float flAnimationRate,
							 float r, float g, float b, float a ) > DrawModel;
	std::function_ref< void( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation,
							 float r, float g, float b, float a, void *&pUserVar ) > UpdateProjectedTexture;
};

class IParticleSystemQuery
{
public:
	IParticleSystemQuery() = default;
	explicit IParticleSystemQuery( const ParticleQueryHooks &hooks ) : m_pHooks( &hooks ) {}

	bool IsEditor( ) { return false; }

	void GetLightingAtPoint( const Vector& vecOrigin, Color &tint )
	{
		if ( m_pHooks )
			m_pHooks->GetLightingAtPoint( vecOrigin, tint );
		else
			tint.SetColor( 255, 255, 255, 255 );
	}

	void TraceLine( const Vector& vecAbsStart,
					const Vector& vecAbsEnd, unsigned int mask, 
					const class IHandleEntity *ignore,
					int collisionGroup,
					CBaseTrace *ptr )
	{
		if ( m_pHooks )
			m_pHooks->TraceLine( vecAbsStart, vecAbsEnd, mask, ignore, collisionGroup, ptr );
		else
			ptr->fraction = 1.0;							// no hit
	}

	bool IsPointInSolid( const Vector& vecPos, const int nContentsMask )
	{
		return m_pHooks ? m_pHooks->IsPointInSolid( vecPos, nContentsMask ) : false;
	}

	// given a possible spawn point, tries to movie it to be on or in the source object. returns
	// true if it succeeded
	bool MovePointInsideControllingObject( CParticleCollection *pParticles,
										   void *pObject,
										   Vector *pPnt )
	{
		return m_pHooks ? m_pHooks->MovePointInsideControllingObject( pParticles, pObject, pPnt ) : true;
	}

	bool IsPointInControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, Vector vecPos, bool bBBoxOnly = false )
	{
		return true;
	}

	int GetRayTraceEnvironmentFromName( const char *pszRtEnvName )
	{
		return m_pHooks ? m_pHooks->GetRayTraceEnvironmentFromName( pszRtEnvName ) : 0;	// == PRECIPITATION
	}

	int GetCollisionGroupFromName( const char *pszCollisionGroupName )
	{
		return m_pHooks ? m_pHooks->GetCollisionGroupFromName( pszCollisionGroupName ) : 0;	// == COLLISION_GROUP_NONE
	}
	
	void GetRandomPointsOnControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsOut,
		float flBBoxScale,
		int nNumTrysToGetAPointInsideTheModel,
		Vector *pPntsOut,
		Vector vecDirectionBias,
		Vector *pHitBoxRelativeCoordOut = nullptr,
		int *pHitBoxIndexOut = nullptr, 
		int nDesiredHitbox = -1, 
		const char *pszHitboxSetName = nullptr )
	{
		if ( m_pHooks )
		{
			m_pHooks->GetRandomPointsOnControllingObjectHitBox( pParticles, nControlPointNumber, nNumPtsOut, flBBoxScale,
				nNumTrysToGetAPointInsideTheModel, pPntsOut, vecDirectionBias, pHitBoxRelativeCoordOut, pHitBoxIndexOut,
				nDesiredHitbox, pszHitboxSetName );
			return;
		}
		for ( int i = 0; i < nNumPtsOut; ++i )
		{
			pPntsOut[i].Init();
		}
	}

	void GetClosestControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsIn,
		float flBBoxScale,
		Vector *pPntsIn,
		Vector *pHitBoxRelativeCoordOut = nullptr,
		int *pHitBoxIndexOut = nullptr,
		int nDesiredHitbox = -1, 
		const char *pszHitboxSetName = nullptr )
	{
		if ( m_pHooks )
		{
			m_pHooks->GetClosestControllingObjectHitBox( pParticles, nControlPointNumber, nNumPtsIn, flBBoxScale, pPntsIn,
				pHitBoxRelativeCoordOut, pHitBoxIndexOut, nDesiredHitbox, pszHitboxSetName );
			return;
		}
		for ( int i=0; i < nNumPtsIn; i++ )
		{
			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;

			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}

	int GetControllingObjectHitBoxInfo(
		CParticleCollection *pParticles,
		int nControlPointNumber,
		int nBufSize,										// # of output slots available
		ModelHitBoxInfo_t *pHitBoxOutputBuffer,
		const char *pszHitboxSetName )
	{
		// returns number of hit boxes output
		return m_pHooks ? m_pHooks->GetControllingObjectHitBoxInfo( pParticles, nControlPointNumber, nBufSize, pHitBoxOutputBuffer, pszHitboxSetName ) : 0;
	}

	void GetControllingObjectOBBox( CParticleCollection *pParticles,
		int nControlPointNumber,
		Vector vecMin, Vector vecMax )
	{
		if ( m_pHooks )
			m_pHooks->GetControllingObjectOBBox( pParticles, nControlPointNumber, vecMin, vecMax );
		else
			vecMin = vecMax = vec3_origin;
	}

	// Traces Four Rays against a defined RayTraceEnvironment
	void TraceAgainstRayTraceEnv( 
		int envnumber, 
		const FourRays &rays, fltx4 TMin, fltx4 TMax,
		RayTracingResult *rslt_out, int32 skip_id ) const
	{
		if ( m_pHooks )
		{
			m_pHooks->TraceAgainstRayTraceEnv( envnumber, rays, TMin, TMax, rslt_out, skip_id );
			return;
		}
		rslt_out->HitDistance = Four_Ones;
		rslt_out->surface_normal.DuplicateVector( vec3_origin );
	}

	Vector GetLocalPlayerPos( void )
	{
		return m_pHooks ? m_pHooks->GetLocalPlayerPos() : vec3_origin;
	}

	void GetLocalPlayerEyeVectors( Vector *pForward, Vector *pRight = nullptr, Vector *pUp = nullptr )
	{
		if ( m_pHooks )
		{
			m_pHooks->GetLocalPlayerEyeVectors( pForward, pRight, pUp );
			return;
		}
		*pForward = vec3_origin;
		*pRight = vec3_origin;
		*pUp = vec3_origin;
	}

	Vector GetCurrentViewOrigin()
	{
		return m_pHooks ? m_pHooks->GetCurrentViewOrigin() : vec3_origin;
	}

	int GetActivityCount() { return m_pHooks ? m_pHooks->GetActivityCount() : 0; }

	const char *GetActivityNameFromIndex( int nActivityIndex ) { return m_pHooks ? m_pHooks->GetActivityNameFromIndex( nActivityIndex ) : nullptr; }

	int GetActivityNumber( void *pModel, const char *m_pszActivityName ) { return m_pHooks ? m_pHooks->GetActivityNumber( pModel, m_pszActivityName ) : -1; }

	float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale )
	{
		return m_pHooks ? m_pHooks->GetPixelVisibility( pQueryHandle, vecOrigin, flScale ) : 0.0f;
	}

	void SetUpLightingEnvironment( const Vector& pos )
	{
		if ( m_pHooks )
			m_pHooks->SetUpLightingEnvironment( pos );
	}

	void PreSimulate( )
	{
		if ( m_pHooks )
			m_pHooks->PreSimulate();
	}

	void PostSimulate( )
	{
		if ( m_pHooks )
			m_pHooks->PostSimulate();
	}

	void DebugDrawLine(const Vector& origin, const Vector& dest, int r, int g, int b,bool noDepthTest, float duration)
	{
		if ( m_pHooks )
			m_pHooks->DebugDrawLine( origin, dest, r, g, b, noDepthTest, duration );
	}

	void *GetModel( char const *pMdlName ) { return m_pHooks ? m_pHooks->GetModel( pMdlName ) : nullptr; }

	void DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
					int nSkin, int nAnimationSequence = 0, float flAnimationRate = 30.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f )
	{
		if ( m_pHooks )
			m_pHooks->DrawModel( pModel, DrawMatrix, pParticles, nParticleNumber, nBodyPart, nSubModel, nSkin, nAnimationSequence, flAnimationRate, r, g, b, a );
	}

	void BeginDrawModels( int nNumModels, Vector const &vecCenter, CParticleCollection *pParticles ) {}

	void FinishDrawModels( CParticleCollection *pParticles ) {}

	void UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar )
	{
		if ( m_pHooks )
			m_pHooks->UpdateProjectedTexture( nParticleID, pMaterial, vOrigin, flRadius, flRotation, r, g, b, a, pUserVar );
	}

private:
	const ParticleQueryHooks *m_pHooks = nullptr;
};


//-----------------------------------------------------------------------------
//
// Particle system manager. Using a class because tools need it that way
// so the SFM and PET tools can share managers despite being linked to 
// separate particle system .libs
//
//-----------------------------------------------------------------------------
typedef int ParticleSystemHandle_t;
inline constexpr ParticleSystemHandle_t INVALID_PARTICLE_SYSTEM_HANDLE = 0xFFFF;

class CParticleSystemMgr
{
public:
	// Constructor, destructor
	CParticleSystemMgr();
	~CParticleSystemMgr();

	// Initialize the particle system
	bool Init( IParticleSystemQuery *pQuery, bool bAllowPrecache );
	void Shutdown();

	// methods to add builtin operators. If you don't call these at startup, you won't be able to sim or draw. These are done separately from Init, so that
	// the server can omit the code needed for rendering/simulation, if desired.
	void AddBuiltinSimulationOperators( void );
	void AddBuiltinRenderingOperators( void );

	// Registration of known operators
	void AddParticleOperator( ParticleFunctionType_t nOpType, IParticleOperatorDefinition *pOpFactory );

	// Read a particle config file, add it to the list of particle configs
	bool ReadParticleConfigFile( const char *pFileName, bool bPrecache, bool bDecommitTempMemory = true );
	bool ReadParticleConfigFile( ks::byte_reader &buf, bool bPrecache, bool bDecommitTempMemory = true, const char *pFileName = nullptr );
	void DecommitTempMemory();

	// For recording, write a specific particle system in binary DMX format
	bool WriteParticleConfigFile( const char *pParticleSystemName, ks::byte_writer &buf, bool bPreventNameBasedLookup = false );
	bool WriteParticleConfigFile( const DmObjectId_t& id, ks::byte_writer &buf, bool bPreventNameBasedLookup = false );

	// create a particle system by name. returns null if one of that name does not exist
	CParticleCollection *CreateParticleCollection( const char *pParticleSystemName, float flDelay = 0.0f, int nRandomSeed = 0 );
	CParticleCollection *CreateParticleCollection( ParticleSystemHandle_t particleSystemName, float flDelay = 0.0f, int nRandomSeed = 0 );

	// create a particle system given a particle system id
	CParticleCollection *CreateParticleCollection( const DmObjectId_t &id, float flDelay = 0.0f, int nRandomSeed = 0 );

	// Is a particular particle system defined?
	bool IsParticleSystemDefined( const char *pParticleSystemName );
	bool IsParticleSystemDefined( const DmObjectId_t &id );

	// Returns the index of the specified particle system. 
	ParticleSystemHandle_t GetParticleSystemIndex( const char *pParticleSystemName );
	ParticleSystemHandle_t FindOrAddParticleSystemIndex( const char *pParticleSystemName );

	// Returns the name of the specified particle system.
	const char *GetParticleSystemNameFromIndex( ParticleSystemHandle_t iIndex );

	// Return the number of particle systems in our dictionary
	int GetParticleSystemCount( void );

	// Get the label for a filter
	const char *GetFilterName( ParticleFilterType_t nFilterType ) const;

	// call to get available particle operator definitions
	// NOTE: FUNCTION_CHILDREN will return a faked one, for ease of writing the editor
	std::vector< IParticleOperatorDefinition * > &GetAvailableParticleOperatorList( ParticleFunctionType_t nWhichList );

	void GetParticleSystemsInFile( const char *pFileName, std::vector< std::string > *pOutSystemNameList );
	void GetParticleSystemsInBuffer( ks::byte_reader &buf, std::vector< std::string > *pOutSystemNameList );

	// Returns the unpack structure for a particle system definition
	const DmxElementUnpackStructure_t *GetParticleSystemDefinitionUnpackStructure();

	// Particle sheet management
	void ShouldLoadSheets( bool bLoadSheets );
	CSheet *FindOrLoadSheet( CParticleSystemDefinition *pDef, bool bTryReloading = false );
	void FlushAllSheets( void );

	// Render cache used to render opaque particle collections
	void ResetRenderCache( void );
	void AddToRenderCache( CParticleCollection *pParticles );
	void DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth );

	IParticleSystemQuery *Query( void ) { return m_pQuery; }

	// return the particle field name
	const char* GetParticleFieldName( int nParticleField ) const;

	// WARNING: the pointer returned by this function may be invalidated 
	// *at any time* by the editor, so do not ever cache it.
	CParticleSystemDefinition* FindParticleSystem( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( const DmObjectId_t& id );
	CParticleSystemDefinition* FindParticleSystem( ParticleSystemHandle_t hParticleSystem );
	CParticleSystemDefinition* FindPrecachedParticleSystem( int nPrecacheIndex );


	void CommitProfileInformation( bool bCommit );			// call after simulation, if you want
															// sim time recorded. if oyu pass
															// flase, info will be thrown away and
															// uncomitted time reset.  Having this
															// function lets you only record
															// profile data for slow frames if
															// desired.


	void DumpProfileInformation( void );					// write particle_profile.csv

	void DumpParticleList( const char *pNameSubstring );

	// Cache/uncache materials used by particle systems
	void PrecacheParticleSystem( int nStringNumber, const char *pName );
	void UncacheAllParticleSystems();


	// Sets the last simulation time, used for particle system sleeping logic
	void SetLastSimulationTime( float flTime );
	float GetLastSimulationTime() const;

	// Sets the last simulation duration ( the amount of time we spent simulating particle ) last frame
	// Used to fallback to cheaper particle systems under load
	void SetLastSimulationDuration( float flDuration ); 
	float GetLastSimulationDuration() const; 

	void SetFallbackParameters( float flBase, float flMultiplier, float flSimFallbackBaseMultiplier, float flSimThresholdMs );
	float GetFallbackBase() const;
	float GetFallbackMultiplier() const;
	float GetSimFallbackThresholdMs() const;
	float GetSimFallbackBaseMultiplier() const;

	void SetSystemLevel( int nCPULevel, int nGPULevel );
	int GetParticleCPULevel() const;
	int GetParticleGPULevel() const;

	void LevelShutdown( void );								// called at level unload time


	void FrameUpdate( void );								// call this once per frame on main thread

	// Particle attribute query funcs
	int GetParticleAttributeByName( const char *pAttribute ) const;		// SLOW! returns -1 on error
	const char *GetParticleAttributeName( int nAttribute ) const;		// returns 'unknown' on error
	EAttributeDataType GetParticleAttributeDataType( int nAttribute ) const;

private:
	struct RenderCache_t
	{
		IMaterial *m_pMaterial;
		std::vector< CParticleCollection * > m_ParticleCollections;
	};

	struct BatchStep_t
	{
		CParticleCollection *m_pParticles;
		CParticleOperatorInstance *m_pRenderer;
		void *m_pContext;
		int m_nFirstParticle;
		int m_nParticleCount;
		int m_nVertCount;
	};

	struct Batch_t
	{
		int m_nVertCount;
		int m_nIndexCount;
		std::vector< BatchStep_t > m_BatchStep; 
	};

	struct ParticleAttribute_t
	{
		EAttributeDataType nDataType;
		const char *pName;
	};

	// Unserialization-related methods
	bool ReadParticleDefinitions( ks::byte_reader &buf, const char *pFileName, bool bPrecache, bool bDecommitTempMemory );
	void AddParticleSystem( CDmxElement *pParticleSystem );

	// Serialization-related methods
	CDmxElement *CreateParticleDmxElement( const DmObjectId_t &id );
	CDmxElement *CreateParticleDmxElement( const char *pParticleSystemName );

	bool WriteParticleConfigFile( CDmxElement *pParticleSystem, ks::byte_writer &buf, bool bPreventNameBasedLookup );

	// Builds a list of batches to render
	void BuildBatchList( int iRenderCache, IMatRenderContext *pRenderContext, std::vector< Batch_t >& batches );

	// Known operators
	std::vector< IParticleOperatorDefinition * > m_ParticleOperators[PARTICLE_FUNCTION_COUNT];

	// Particle system dictionary
	CParticleSystemDictionary *m_pParticleSystemDictionary;
	
	// Sheets by texture name, in first-load order: indices are the sheet symbols cached by definitions.
	std::vector< std::unique_ptr< CSheet > > m_SheetList;
	ks::idict< int > m_SheetIndex;

	// attaching and dtaching killlists. when simulating, a particle system gets a kill list. after
	// simulating, the memory for that will be used for the next particle system.  This matters for
	// threaded particles, because we don't want to share the same kill list between simultaneously
	// simulating particle systems.
	void AttachKillList( CParticleCollection *pParticles);
	void DetachKillList( CParticleCollection *pParticles);

	// Set up s_AttributeTable
	void InitAttributeTable( void );

	// For visualization (currently can only visualize one operator at a time)
	CParticleCollection *m_pVisualizedParticles;
	DmObjectId_t m_VisualizedOperatorId;
	IParticleSystemQuery *m_pQuery;
	std::vector< RenderCache_t > m_RenderCache;
	IMaterial *m_pShadowDepthMaterial;
	float m_flLastSimulationTime;
	float m_flLastSimulationDuration;

	std::vector< ParticleSystemHandle_t > m_PrecacheLookup;
	std::vector< ParticleSystemHandle_t > m_ClientPrecacheLookup;

	bool m_bDidInit;
	bool m_bUsingDefaultQuery;
	bool m_bShouldLoadSheets;
	bool m_bAllowPrecache;

	int m_nNumFramesMeasured;

	float m_flFallbackBase;
	float m_flFallbackMultiplier;
	float m_flSimFallbackBaseMultiplier;
	float m_flSimThresholdMs;

	int m_nCPULevel;
	int m_nGPULevel;

	static ParticleAttribute_t s_AttributeTable[MAX_PARTICLE_ATTRIBUTES];

	friend class CParticleSystemDefinition;
	friend class CParticleCollection;
};

extern CParticleSystemMgr *g_pParticleSystemMgr;


//-----------------------------------------------------------------------------
// A particle system can only have 1 operator using a particular ID
//-----------------------------------------------------------------------------
enum ParticleOperatorId_t
{
	// Generic IDs
	OPERATOR_GENERIC = -2,		// Can have as many of these as you want
	OPERATOR_SINGLETON = -1,	// Can only have 1 operator with the same name as this one

	// Renderer operator IDs

	// Operator IDs

	// Initializer operator IDs
	OPERATOR_PI_POSITION,		// Particle initializer: position (can only have 1 position setter)
	OPERATOR_PI_RADIUS,
	OPERATOR_PI_ALPHA,
	OPERATOR_PI_TINT_RGB,
	OPERATOR_PI_ROTATION,
	OPERATOR_PI_YAW,

	// Emitter IDs

	OPERATOR_ID_COUNT,
};


//-----------------------------------------------------------------------------
// Class factory for particle operators
//-----------------------------------------------------------------------------
class IParticleOperatorDefinition
{
public:
	// the definition of a built-in operator class
	IParticleOperatorDefinition( ParticleOperatorKind kind, const char *pFactoryName, ParticleOperatorId_t id, bool bIsObsolete,
								 DmxElementUnpackStructure_t *const *ppUnpackParams )
		: m_Kind( kind ), m_Group( ParticleOperatorGroupOf( kind ) ), m_bCreatable( true ),
		  m_pFactoryName( pFactoryName ), m_Id( id ), m_bIsObsolete( bIsObsolete ), m_ppUnpackParams( ppUnpackParams )
	{
	}

	// the faked definition of the editor's child entry: it only has an unpack structure
	explicit IParticleOperatorDefinition( DmxElementUnpackStructure_t *const *ppUnpackParams )
		: m_Kind( ParticleOperatorKind() ), m_Group( ParticleOperatorGroup() ), m_bCreatable( false ),
		  m_pFactoryName( nullptr ), m_Id( OPERATOR_GENERIC ), m_bIsObsolete( false ), m_ppUnpackParams( ppUnpackParams )
	{
	}

	const char *GetName() const { return m_pFactoryName; }
	CParticleOperatorInstance *CreateInstance( const DmObjectId_t &id ) const;
	const DmxElementUnpackStructure_t* GetUnpackStructure() const { return *m_ppUnpackParams; }
	ParticleOperatorId_t GetId() const { return m_Id; }
	// Editor won't display obsolete operators
	bool IsObsolete() const { return m_bIsObsolete; }
	ParticleOperatorKind GetKind() const { return m_Kind; }
	ParticleOperatorGroup GetGroup() const { return m_Group; }

	[[gnu::always_inline]] inline void RecordExecutionTime( float flETime )
	{
	}

private:
	ParticleOperatorKind m_Kind;
	ParticleOperatorGroup m_Group;
	bool m_bCreatable;
	const char *m_pFactoryName;
	ParticleOperatorId_t m_Id;
	bool m_bIsObsolete;
	DmxElementUnpackStructure_t *const *m_ppUnpackParams;
};


//-----------------------------------------------------------------------------
// Particle operators
//-----------------------------------------------------------------------------
class CParticleOperatorInstance
{
public:
	// custom allocators so we can be simd aligned
	void *operator new( std::size_t nSize );
	void* operator new( std::size_t size, int nBlockUse, const char *pFileName, int nLine );
	void operator delete( void *pData );
	void operator delete( void* p, int nBlockUse, const char *pFileName, int nLine );

	// The entry points. CParticleOperatorInstance is not polymorphic: each of these runs the operator's
	// own member of that name, selected by the operator's kind (ParticleOps, below), so calling one
	// through a CParticleOperatorInstance pointer is always right. The defaults an operator class gets when
	// it does not define an entry are the <entry>_Default members further down; they are protected.
#define KS_PARTICLE_OP_DECLARE_DISPATCHER( Ret, Name, Const, Params, Args ) \
	Ret Name Params Const;
	KS_PARTICLE_OP_ENTRIES( KS_PARTICLE_OP_DECLARE_DISPATCHER )
#undef KS_PARTICLE_OP_DECLARE_DISPATCHER
	// (GetWrittenAttributes() and GetReadAttributes() have no default: every operator defines them)

	// splits particle initialization up into scalar and block sections, callingt he right code
	void InitNewParticles( CParticleCollection *pParticles, int nFirstParticle, int n_particles, int attribute_write_mask , void *pContext) const;

	// Returns the operator definition that spawned this operator
	const IParticleOperatorDefinition *GetDefinition()
	{
		return m_pDef;
	}

	// Returns a unique ID for this definition
	const DmObjectId_t& GetId() { return m_Id; }

	// Used as a debugging mechanism to prevent bogus calls to RandomInt or RandomFloat inside operators
	// Use CParticleCollection::RandomInt/RandomFloat instead
	int RandomInt( int nMin, int nMax )
	{
		// NOTE: Use CParticleCollection::RandomInt! 
		contract_assert( 0 );
		return 0;
	}

	float RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f )
	{
		// NOTE: Use CParticleCollection::RandomFloat! 
		contract_assert( 0 );
		return 0.0f;
	}

	float RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f )
	{
		// NOTE: Use CParticleCollection::RandomFloatExp! 
		contract_assert( 0 );
		return 0.0f;
	}

	float m_flOpStartFadeInTime;
	float m_flOpEndFadeInTime;
	float m_flOpStartFadeOutTime;
	float m_flOpEndFadeOutTime;
	float m_flOpFadeOscillatePeriod;

	float m_flOpTimeOffsetMin;
	float m_flOpTimeOffsetMax;
	int m_nOpTimeOffsetSeed;

	int m_nOpStrengthScaleSeed;
	float m_flOpStrengthMinScale;
	float m_flOpStrengthMaxScale;

    int m_nOpTimeScaleSeed;
    float m_flOpTimeScaleMin;
    float m_flOpTimeScaleMax;

	bool m_bStrengthFastPath;								// set for operators which just always have strengh = 0
	
	int m_nOpEndCapState;



protected:
	// What an operator class inherits for an entry point it does not define (the dispatch picks it; see
	// ParticleOps). They are named apart from the entry points on purpose.

	// unpack structure will be applied by creator. add extra initialization needed here
	void InitParams_Default( CParticleSystemDefinition *pDef )
	{
	}

	std::size_t GetRequiredContextBytes_Default( ) const 
	{
		return 0;
	}

	void InitializeContextData_Default( CParticleCollection *pParticles, void *pContext ) const
	{
	}

	uint64 GetReadControlPointMask_Default() const
	{
		return 0;
	}

	uint32 GetFilter_Default( void ) const;

	// these control points are NOT positions or matrices (ie don't try to transform them)
	uint64 GetNonPositionalControlPointMask_Default() const
	{
		return 0;
	}

	//  Used when an operator needs to read the attributes of a particle at spawn time
	uint32 GetReadInitialAttributes_Default( void ) const
	{
		return 0;
	}

	// a particle simulator does this
	void Operate_Default( CParticleCollection *pParticles, float flOpStrength, void *pContext ) const
	{
	}

	void PostSimulate_Default( CParticleCollection *pParticles, void *pContext ) const
	{
	}

	// a renderer overrides this
	void Render_Default( IMatRenderContext *pRenderContext, 
						 CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
	{
	}

	bool IsBatchable_Default() const
	{
		return true;
	}

	bool IsOrderImportant_Default() const
	{
		return false;
	}

	bool ShouldRun_Default( bool bApplyingParentKillList ) const
	{
		return !bApplyingParentKillList;
	}

	void RenderUnsorted_Default( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
	{
	}

	// Returns the number of verts + indices to render
	int GetParticlesToRender_Default( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const
	{
		*pVertsUsed = 0;
		*pIndicesUsed = 0;
		return 0;
	}

	// emitters over-ride this. Return a mask of what fields you initted
	uint32 Emit_Default( CParticleCollection *pParticles, float flOpCurStrength,
						 void *pContext ) const
	{
		return 0;
	}

	// emitters over-ride this. 
	void StopEmission_Default( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
	}

	void StartEmission_Default( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
	}

	void Restart_Default( CParticleCollection *pParticles, void *pContext ) {}

	// initters over-ride this
	void InitParticleSystem_Default( CParticleCollection *pParticles, void *pContext ) const
	{
	}

	// a force generator does this. It accumulates in the force array
	void AddForces_Default( FourVectors *AccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flCurStrength,
							void *pContext ) const
	{
	}

	// this is called for each constarint every frame. It can set up data like nearby world traces,
	// etc
	void SetupConstraintPerFrameData_Default( CParticleCollection *pParticles,
											  void *pContext ) const
	{
	}

	// a constraint overrides this. It shold return a true if it did anything
	bool EnforceConstraint_Default( int nStartBlock,
									int nNumBlocks,
									CParticleCollection *pParticles,
									void *pContext, 
									int nNumValidParticlesInLastChunk ) const
	{
		return false;
	}

	// should the constraint be run only once after all other constraints?
	bool IsFinalConstaint_Default( void ) const
	{
		return false;
	}

	// determines if a mask needs to be initialized multiple times. 
	bool InitMultipleOverride_Default()
	{
		return false;
	}

	// Indicates if this initializer is scrub-safe (initializers don't use random numbers, for example)
	bool IsScrubSafe_Default()
	{
		return false;
	}

	// particle-initters over-ride this
	void InitNewParticlesScalar_Default( CParticleCollection *pParticles, int nFirstParticle, int n_particles, int attribute_write_mask, void *pContext ) const
	{
	}

	// init new particles in blocks of 4. initters that have sse smarts should over ride this. the scalar particle initter will still be cllaed for head/tail.
	void InitNewParticlesBlock_Default( CParticleCollection *pParticles, int start_block, int n_blocks, int attribute_write_mask, void *pContext ) const;

	// this function is queried to determine if a particle system is over and doen with. A particle
	// system is done with when it has noparticles and no operators intend to create any more
	bool MayCreateMoreParticles_Default( CParticleCollection const *pParticles, void *pContext ) const
	{
		return false;
	}

	bool ShouldRunBeforeEmitters_Default( void ) const
	{
		return false;
	}

	// Called when the SFM wants to skip forward in time
	void SkipToTime_Default( float flTime, CParticleCollection *pParticles, void *pContext ) const {}

	// Used for editing + debugging to visualize the operator in 3D
	void RenderDebug_Default( CParticleCollection *pParticles ) const {}

	void Precache_Default( void )
	{
	}

	void Uncache_Default( void )
	{
	}

protected:
	// utility function for initting a scalar attribute to a random range in an sse fashion
	void InitScalarAttributeRandomRangeExpBlock( int nAttributeId, float fMinValue, float fMaxValue, float fExp,
		CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert = false ) const;
	void AddScalarAttributeRandomRangeExpBlock( int nAttributeId, float fMinValue, float fMaxValue, float fExp,
		CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert = false ) const;

	void InitScalarAttributeRandomRangeExpScalar( int nAttributeId, float fMinValue, float fMaxValue, float fExp,
												  CParticleCollection *pParticles, int nStartParticle, int nParticleCount ) const;

	void CheckForFastPath( void );							// call at operator init time

	// utility funcs to access CParticleCollection data:
	bool HasAttribute( CParticleCollection *pParticles, int nAttribute ) const;
	KillListItem_t *GetParentKillList( CParticleCollection *pParticles, int &nNumParticlesToKill ) const;

private:
	friend class CParticleCollection;
	friend class CParticleSystemDefinition;
	friend class CParticleSystemMgr;
	friend class IParticleOperatorDefinition;
	friend struct ParticleOps;

	const IParticleOperatorDefinition *m_pDef;
	// which of the closed set of operator classes this is: what ParticleOps dispatches on
	ParticleOperatorKind m_Kind;
	ParticleOperatorGroup m_Group;
	void SetDefinition( const IParticleOperatorDefinition * pDef, const DmObjectId_t &id )
	{
		m_pDef = pDef;
		m_Kind = pDef->GetKind();
		m_Group = pDef->GetGroup();
		CopyUniqueId( id, &m_Id );
	}

	DmObjectId_t m_Id;

	template <typename T> friend class CParticleOperatorDefinition;
};

class CParticleInitializerOperatorInstance : public CParticleOperatorInstance
{
public:

	bool ShouldRun( bool bApplyingParentKillList ) const
	{
		return ( !bApplyingParentKillList ) || m_bRunForParentApplyKillList;
	}

	bool m_bRunForParentApplyKillList;
};

class CParticleRenderOperatorInstance : public CParticleOperatorInstance
{
public:

	CParticleVisibilityInputs VisibilityInputs;
};

//-----------------------------------------------------------------------------
// Helper macro for creating particle operator factories
//-----------------------------------------------------------------------------
// What the definition of an operator class adds is the unpack structure the class' own
// BEGIN/END_PARTICLE_OPERATOR_UNPACK fills in.
template < class T >
class CParticleOperatorDefinition : public IParticleOperatorDefinition
{
public:
	CParticleOperatorDefinition( ParticleOperatorKind kind, const char *pFactoryName, ParticleOperatorId_t id, bool bIsObsolete )
		: IParticleOperatorDefinition( kind, pFactoryName, id, bIsObsolete, &m_pUnpackParams )
	{
	}

private:
	static DmxElementUnpackStructure_t *m_pUnpackParams;
};

//-----------------------------------------------------------------------------
// Dispatch over the closed set of particle operators
//
// Every operator class is a ParticleOperatorKind (particle_operator_list.h). The operators are not
// virtual: the entry points of CParticleOperatorInstance (Operate, Emit, Render, ...) switch on the
// group (the source file that defines the class) and then on the kind, and call the class' member of
// that name. A class that does not define an entry gets the CParticleOperatorInstance::<entry>_Default
// instead (detected by the member name resolving to the base's own dispatcher); the defaults are
// protected and named apart, so no call through a base pointer can reach one by accident. The group
// switch exists so that each group's classes are only visible in the one translation unit that defines
// them (particle_operator_dispatch.inl is included at the end of each).
//
// ParticleOps is the implementation of that: only CParticleOperatorInstance calls into it (plus the
// create/destroy functions, which are public).
//-----------------------------------------------------------------------------
struct ParticleOps
{
	// construction and destruction of an instance of a kind
	static CParticleOperatorInstance *Create( ParticleOperatorKind kind );
	static void Destroy( CParticleOperatorInstance *pOp );

private:
	friend class CParticleOperatorInstance;

#define KS_PARTICLE_OP_DECLARE_ENTRY( Ret, Name, Const, Params, Args ) \
	static Ret Name( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	static Ret Name##_Constraints( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	static Ret Name##_Initializers( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	static Ret Name##_Emitters( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	static Ret Name##_Forces( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	static Ret Name##_Operators( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	static Ret Name##_Renderers( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ); \
	/* the call of the entry on an operator class: its own member, or else the base's default */ \
	template < class Op > static Ret Name##_Call( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) );
	KS_PARTICLE_OP_ENTRIES( KS_PARTICLE_OP_DECLARE_ENTRY )
#undef KS_PARTICLE_OP_DECLARE_ENTRY

	static CParticleOperatorInstance *Create_Constraints( ParticleOperatorKind kind );
	static CParticleOperatorInstance *Create_Initializers( ParticleOperatorKind kind );
	static CParticleOperatorInstance *Create_Emitters( ParticleOperatorKind kind );
	static CParticleOperatorInstance *Create_Forces( ParticleOperatorKind kind );
	static CParticleOperatorInstance *Create_Operators( ParticleOperatorKind kind );
	static CParticleOperatorInstance *Create_Renderers( ParticleOperatorKind kind );
	static void Destroy_Constraints( CParticleOperatorInstance *pOp );
	static void Destroy_Initializers( CParticleOperatorInstance *pOp );
	static void Destroy_Emitters( CParticleOperatorInstance *pOp );
	static void Destroy_Forces( CParticleOperatorInstance *pOp );
	static void Destroy_Operators( CParticleOperatorInstance *pOp );
	static void Destroy_Renderers( CParticleOperatorInstance *pOp );
};

// The entry points of CParticleOperatorInstance
#define KS_PARTICLE_OP_THIS_ARGS( ... ) this __VA_OPT__( , ) __VA_ARGS__
#define KS_PARTICLE_OP_DEFINE_DISPATCHER( Ret, Name, Const, Params, Args ) \
	inline Ret CParticleOperatorInstance::Name Params Const \
	{ \
		return ParticleOps::Name( KS_PARTICLE_OP_THIS_ARGS Args ); \
	}
KS_PARTICLE_OP_ENTRIES( KS_PARTICLE_OP_DEFINE_DISPATCHER )
#undef KS_PARTICLE_OP_DEFINE_DISPATCHER
#undef KS_PARTICLE_OP_THIS_ARGS

// The call of an entry on operator class Op: the member Op defines, or else (when the name only finds
// the dispatcher of CParticleOperatorInstance) the default. An entry without a default (see the list)
// that an operator does not define does not compile.
#define KS_PARTICLE_OP_DEFINE_CALL( Ret, Name, Const, Params, Args ) \
	template < class Op > Ret ParticleOps::Name##_Call( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ) \
	{ \
		Const Op *pThis = static_cast< Const Op * >( pOp ); \
		if constexpr ( std::is_same_v< decltype( &Op::Name ), decltype( &CParticleOperatorInstance::Name ) > ) \
			return pThis->Name##_Default Args; \
		else \
			return pThis->Name Args; \
	}
KS_PARTICLE_OP_ENTRIES( KS_PARTICLE_OP_DEFINE_CALL )
#undef KS_PARTICLE_OP_DEFINE_CALL

inline uint32 CParticleOperatorInstance::GetFilter_Default( void ) const
{
	uint32 filter = 0;
	uint32 wrAttrib = GetWrittenAttributes();

	if (wrAttrib & ATTRIBUTES_WHICH_ARE_POSITION_AND_VELOCITY)
	{
		filter = filter | FILTER_POSITION_AND_VELOCITY_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_LIFE_DURATION)
	{
		filter = filter | FILTER_LIFE_DURATION_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_ROTATION)
	{
		filter = filter | FILTER_ROTATION_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_SIZE)
	{
		filter = filter | FILTER_SIZE_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_COLOR_AND_OPACITY)
	{
		filter = filter | FILTER_COLOR_AND_OPACITY_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_ANIMATION_SEQUENCE)
	{
		filter = filter | FILTER_ANIMATION_SEQUENCE_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_HITBOX)
	{
		filter = filter | FILTER_HITBOX_MASK;
	}
	if (wrAttrib & ATTRIBUTES_WHICH_ARE_NORMAL)
	{
		filter = filter | FILTER_NORMAL_MASK;
	}

	return filter;
}

inline void CParticleOperatorInstance::InitNewParticlesBlock_Default( CParticleCollection *pParticles, int start_block, int n_blocks, int attribute_write_mask, void *pContext ) const
{
	// default behaviour is to call the scalar one 4x times
	InitNewParticlesScalar( pParticles, 4*start_block, 4*n_blocks, attribute_write_mask, pContext );
}

inline void CParticleOperatorDeleter::operator()( CParticleOperatorInstance *pOp ) const
{
	ParticleOps::Destroy( pOp );
}

#define DECLARE_PARTICLE_OPERATOR( _className )				\
	DECLARE_DMXELEMENT_UNPACK()								\
	friend class CParticleOperatorDefinition<_className >;	\
	friend struct ParticleOps

#define DEFINE_PARTICLE_OPERATOR( _className, _operatorName, _id )	\
	static CParticleOperatorDefinition<_className> s_##_className##Factory( ParticleOperatorKind::_className, _operatorName, _id, false )

#define DEFINE_PARTICLE_OPERATOR_OBSOLETE( _className, _operatorName, _id )	\
	static CParticleOperatorDefinition<_className> s_##_className##Factory( ParticleOperatorKind::_className, _operatorName, _id, true )

#define BEGIN_PARTICLE_OPERATOR_UNPACK( _className )										\
	BEGIN_DMXELEMENT_UNPACK( _className )													\
	DMXELEMENT_UNPACK_FIELD( "operator start fadein","0", float, m_flOpStartFadeInTime )	\
	DMXELEMENT_UNPACK_FIELD( "operator end fadein","0", float, m_flOpEndFadeInTime )		\
	DMXELEMENT_UNPACK_FIELD( "operator start fadeout","0", float, m_flOpStartFadeOutTime )	\
	DMXELEMENT_UNPACK_FIELD( "operator end fadeout","0", float, m_flOpEndFadeOutTime ) \
    DMXELEMENT_UNPACK_FIELD( "operator fade oscillate","0", float, m_flOpFadeOscillatePeriod ) \
    DMXELEMENT_UNPACK_FIELD( "operator time offset seed","0", int, m_nOpTimeOffsetSeed ) \
    DMXELEMENT_UNPACK_FIELD( "operator time offset min","0", float, m_flOpTimeOffsetMin ) \
    DMXELEMENT_UNPACK_FIELD( "operator time offset max","0", float, m_flOpTimeOffsetMax ) \
    DMXELEMENT_UNPACK_FIELD( "operator time scale seed","0", int, m_nOpTimeScaleSeed ) \
    DMXELEMENT_UNPACK_FIELD( "operator time scale min","1", float, m_flOpTimeScaleMin ) \
    DMXELEMENT_UNPACK_FIELD( "operator time scale max","1", float, m_flOpTimeScaleMax ) \
    DMXELEMENT_UNPACK_FIELD( "operator time strength random scale max", "1", float, m_flOpStrengthMaxScale ) \
    DMXELEMENT_UNPACK_FIELD( "operator strength scale seed","0", int, m_nOpStrengthScaleSeed ) \
    DMXELEMENT_UNPACK_FIELD( "operator strength random scale min", "1", float, m_flOpStrengthMinScale ) \
    DMXELEMENT_UNPACK_FIELD( "operator strength random scale max", "1", float, m_flOpStrengthMaxScale ) \
    DMXELEMENT_UNPACK_FIELD( "operator end cap state", "-1", int, m_nOpEndCapState ) 
#define END_PARTICLE_OPERATOR_UNPACK( _className )		\
	END_DMXELEMENT_UNPACK_TEMPLATE( _className, CParticleOperatorDefinition<_className>::m_pUnpackParams )

#define BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( _className )									\
	BEGIN_PARTICLE_OPERATOR_UNPACK( _className )													\
    DMXELEMENT_UNPACK_FIELD( "run for killed parent particles", "1", bool, m_bRunForParentApplyKillList )

#define BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( _className )											\
	BEGIN_PARTICLE_OPERATOR_UNPACK( _className )													\
	DMXELEMENT_UNPACK_FIELD( "Visibility Proxy Input Control Point Number", "-1", int, VisibilityInputs.m_nCPin )	\
	DMXELEMENT_UNPACK_FIELD( "Visibility Proxy Radius", "1.0", float, VisibilityInputs.m_flProxyRadius )				\
	DMXELEMENT_UNPACK_FIELD( "Visibility input minimum","0", float, VisibilityInputs.m_flInputMin )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input maximum","1", float, VisibilityInputs.m_flInputMax )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input dot minimum","0", float, VisibilityInputs.m_flDotInputMin )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input dot maximum","0", float, VisibilityInputs.m_flDotInputMax )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input distance minimum","0", float, VisibilityInputs.m_flDistanceInputMin )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input distance maximum","0", float, VisibilityInputs.m_flDistanceInputMax )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility Alpha Scale minimum","0", float, VisibilityInputs.m_flAlphaScaleMin )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Alpha Scale maximum","1", float, VisibilityInputs.m_flAlphaScaleMax )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Radius Scale minimum","1", float, VisibilityInputs.m_flRadiusScaleMin )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Radius Scale maximum","1", float, VisibilityInputs.m_flRadiusScaleMax )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Radius FOV Scale base","0", float, VisibilityInputs.m_flRadiusScaleFOVBase )		
#define REGISTER_PARTICLE_OPERATOR( _type, _className )	\
	g_pParticleSystemMgr->AddParticleOperator( _type, &s_##_className##Factory )

// need to think about particle constraints in terms of segregating affected particles so as to
// run multi-pass constraints on only a subset


//-----------------------------------------------------------------------------
// flags for particle systems
//-----------------------------------------------------------------------------
enum
{
	PCFLAGS_FIRST_FRAME = 0x1,
	PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED = 0x2,
};



inline constexpr auto DEBUG_PARTICLE_SORT = 0;

//------------------------------------------------------------------------------
// particle render helpers
//------------------------------------------------------------------------------
struct CParticleVisibilityData
{
	float	m_flAlphaVisibility;
	float	m_flRadiusVisibility;
	bool	m_bUseVisibility;
};
// sorting functionality for rendering. Call GetRenderList( bool bSorted ) to get the list of
// particles to render (sorted or not, including children).
// **do not casually change this structure**. The sorting code treats it interchangably as an SOA
// and accesses it using sse. Any changes to this struct need the sort code updated.**
struct ParticleRenderData_t
{
	float m_flSortKey;					// what we sort by
	int   m_nIndex;						// index or fudged index (for child particles)
	float m_flRadius;					// effective radius, using visibility
	uint8 m_nAlpha;						// effective alpha, combining alpha and alpha2 and vis. 0 - 255
	uint8 m_nAlphaPad[3];				// this will be written to
};

struct ExtendedParticleRenderData_t : ParticleRenderData_t
{
	float m_flX;
	float m_flY;
	float m_flZ;
	float m_flPad;
};


struct alignas( 16 ) _FourInts
{
	int32 m_nValue[4];
};
using FourInts = _FourInts;


struct ParticleBaseRenderData_SIMD_View
{
	fltx4 m_fl4SortKey;
	FourVectors m_fl4XYZ;
	fltx4 m_fl4Alpha;
	fltx4 m_fl4Red;
	fltx4 m_fl4Green;
	fltx4 m_fl4Blue;
	fltx4 m_fl4Radius;
	fltx4 m_fl4AnimationTimeValue;
	fltx4 m_fl4SequenceID;									// 56 bytes per particle
};

struct ParticleFullRenderData_SIMD_View : public ParticleBaseRenderData_SIMD_View
{
	fltx4 m_fl4Rotation;
	fltx4 m_fl4Yaw;

	// no-op operation so templates can compile
	[[gnu::always_inline]] inline void SetARGB2( fltx4 const &fl4Red, fltx4 const &fl4Green, fltx4 const &fl4Blue, fltx4 const &fl4Alpha )
	{
	}

	[[gnu::always_inline]] inline void SetNormal( fltx4 const &fl4NormalX, fltx4 const &fl4NormalY, fltx4 const &fl4NormalZ )
	{
	}
};

struct ParticleRenderDataWithOutlineInformation_SIMD_View : public ParticleFullRenderData_SIMD_View
{
	FourVectors m_v4Color2;
	fltx4 m_fl4Alpha2;

	[[gnu::always_inline]] inline void SetARGB2( fltx4 const &fl4Red, fltx4 const &fl4Green, fltx4 const &fl4Blue, fltx4 const &fl4Alpha )
	{
		m_v4Color2.x = fl4Red;
		m_v4Color2.y = fl4Green;
		m_v4Color2.z = fl4Blue;
		m_fl4Alpha2 = fl4Alpha;
	}
};

struct ParticleRenderDataWithNormal_SIMD_View : public ParticleFullRenderData_SIMD_View
{
	FourVectors m_v4Normal;

	[[gnu::always_inline]] inline void SetNormal( fltx4 const &fl4NormalX, fltx4 const &fl4NormalY, fltx4 const &fl4NormalZ )
	{
		m_v4Normal.x = fl4NormalX;
		m_v4Normal.y = fl4NormalY;
		m_v4Normal.z = fl4NormalZ;
	}

};



// definitions for byte fields ( colors ). endian-ness matters here.
#define BYTE_FIELD(x)							\
    uint8 x;									\
    uint8 x##_Pad[3 + 3 * 4 ];

#define FLOAT_FIELD( x )						\
    float x;									\
    float x##_Pad[3];

struct ParticleBaseRenderData_Scalar_View
{
	int32 m_nSortKey;
	int32 m_nPad00[3];
	FLOAT_FIELD( m_flX );
	FLOAT_FIELD( m_flY );
	FLOAT_FIELD( m_flZ );

	BYTE_FIELD( m_nAlpha );
	BYTE_FIELD( m_nRed );
	BYTE_FIELD( m_nGreen );
	BYTE_FIELD( m_nBlue );

	FLOAT_FIELD( m_flRadius );
	FLOAT_FIELD( m_flAnimationTimeValue );
	uint8 m_nSequenceID;
	uint8 m_nSequenceID1;
	uint8 m_nPadSequence[2 + 3 * 4];
};
struct ParticleFullRenderData_Scalar_View : public ParticleBaseRenderData_Scalar_View
{
	FLOAT_FIELD( m_flRotation );
	FLOAT_FIELD( m_flYaw );
	
	float Red2( void ) const { return 1.0; }
	float Green2( void ) const { return 1.0; }
	float Blue2( void ) const { return 1.0; }
	float Alpha2( void ) const { return 1.0; }
	
	float NormalX( void ) const { return 1.0; }
	float NormalY( void ) const { return 1.0; }
	float NormalZ( void ) const { return 1.0; }
};

struct ParticleRenderDataWithOutlineInformation_Scalar_View : public ParticleFullRenderData_Scalar_View
{
	FLOAT_FIELD( m_flRed2 );
	FLOAT_FIELD( m_flGreen2 );
	FLOAT_FIELD( m_flBlue2 );
	FLOAT_FIELD( m_flAlpha2 );

	float Red2( void ) const { return m_flRed2; }
	float Green2( void ) const { return m_flGreen2; }
	float Blue2( void ) const { return m_flBlue2; }
	float Alpha2( void ) const { return m_flAlpha2; }
};

struct ParticleRenderDataWithNormal_Scalar_View : public ParticleFullRenderData_Scalar_View
{
	FLOAT_FIELD( m_flNormalX );
	FLOAT_FIELD( m_flNormalY );
	FLOAT_FIELD( m_flNormalZ );

	float NormalX( void ) const { return m_flNormalX; }
	float NormalY( void ) const { return m_flNormalY; }
	float NormalZ( void ) const { return m_flNormalZ; }

};



ParticleFullRenderData_Scalar_View **GetExtendedRenderList( CParticleCollection *pParticles,
															IMatRenderContext *pRenderContext,
															bool bSorted, int *pNparticles,
															CParticleVisibilityData *pVisibilityData);


ParticleRenderDataWithOutlineInformation_Scalar_View **GetExtendedRenderListWithPerParticleGlow(
	CParticleCollection *pParticles,
	IMatRenderContext *pRenderContext,
	bool bSorted, int *pNparticles,
	CParticleVisibilityData *pVisibilityData );


ParticleRenderDataWithNormal_Scalar_View **GetExtendedRenderListWithNormals(
	CParticleCollection *pParticles,
	IMatRenderContext *pRenderContext,
	bool bSorted, int *pNparticles,
	CParticleVisibilityData *pVisibilityData );

// returns # of particles
int GenerateExtendedSortedIndexList( Vector vecCamera, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, 
									 CParticleCollection *pParticles, bool bSorted, void *pOutBuf, 
									 ParticleFullRenderData_Scalar_View **pParticlePtrs );


//------------------------------------------------------------------------------
// CParticleSnapshot wraps a CSOAContainer, so that a particle system
// can write to it or read from it (e.g. this can be attached to a control point).
//------------------------------------------------------------------------------
class CParticleSnapshot
{
	DECLARE_DMXELEMENT_UNPACK();

public:
	CParticleSnapshot()  { Purge(); }
	~CParticleSnapshot() { Purge(); }

	struct AttributeMap
	{
		AttributeMap( int nContainerAttribute, int nParticleAttribute ) : m_nContainerAttribute( nContainerAttribute ), m_nParticleAttribute( nParticleAttribute ) {}
		int m_nContainerAttribute, m_nParticleAttribute;
	};
	typedef std::vector< AttributeMap > AttributeMapVector;

	// Has the CParticleSnapshot been fully initialized?
	bool IsValid( void ) { return !!m_pContainer; }

	// Initialize from a .psf DMX file:
	bool Unserialize( const char *pFullPath );
	// Serialize to a .psf DMX file (NOTE: external containers will serialize out fine, but won't be external when read back in)
	bool Serialize( const char *pFullPath, bool bTextMode = true ); // TODO: once this stabilizes, switch to binary mode

	// Initialize by creating a new container, with a specified attribute mapping (will clear out old data if it exists)
	//  - each map entry specifies container attribute and associated particle attribute
	//  - the mapping should be one-to-one (container attributes are 'labeled' with particle attributes),
	//    so each particle attribute and container field should be used *AT MOST* once
	//  - NOTE: many-to-one and one-to-many mappings may be implemented by particle read/write operators
	bool Init( int nX, int nY, int nZ, const AttributeMapVector &attributeMaps );
	// Same as the other Init, except the attribute mapping are specified with varargs instead of a vector
	// (pairs of int params specify container attribute and associated particle attribute, terminated by -1)
	bool Init( int nX, int nY, int nZ, ... );

	// Initialize by wrapping a pre-existing container, with a specified attribute mapping
	//  - same conditions/parameters as above
	//  - existing container attributes are expected to match the particle attribute data types
	//  - if a new attribute is added to the external container, call InitExternal again to update the snapshot
	bool InitExternal( CSOAContainer *pContainer, const AttributeMapVector &attributeMaps );
	bool InitExternal( CSOAContainer *pContainer, ... );

	// Clear the snapshot (and container) back to its initial state
	void Purge( void );

	// This provides READ-ONLY access to the snapshot's container (all write accessors should have wrappers here, to ensure
	// that the container's set of attributes is not modified, which would invalidate the snapshot attribute mapping table)
	const CSOAContainer *GetContainer( void ) { return m_pContainer; }


	// Does the snapshot have data (of the appropriate type) for the given particle attribute?
	bool HasAttribute( int nParticleAttribute, EAttributeDataType nDataType ) const
	{
		contract_assert( ( nParticleAttribute >= 0 ) && ( nParticleAttribute < MAX_PARTICLE_ATTRIBUTES ) );
		int nContainerIndex = m_ParticleAttributeToContainerAttribute[ nParticleAttribute ];
		return ( ( nContainerIndex != -1 ) && ( m_pContainer->GetAttributeType( nContainerIndex ) == nDataType ) );
	}

	// ---------- Wrappers for CSOAContainer members ---------- 
	int NumCols( void ) { return m_pContainer->NumCols(); }
	int NumRows( void ) { return m_pContainer->NumRows(); }
	int NumSlices( void ) { return m_pContainer->NumSlices(); }
	// Read data from the container for the given particle attribute, at index (nIndex,0,0)
	template<class T> T *ElementPointer( int nParticleAttribute, int nX = 0, int nY = 0, int nZ = 0 ) const
	{
		contract_assert( ( nParticleAttribute >= 0 ) && ( nParticleAttribute < MAX_PARTICLE_ATTRIBUTES ) );
		contract_assert( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] != -1 );
		return m_pContainer->ElementPointer<T>( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ], nX, nY, nZ );
	}
	FourVectors *ElementPointer4V( int nParticleAttribute, int nX = 0, int nY = 0, int nZ = 0 ) const
	{
		contract_assert( ( nParticleAttribute >= 0 ) && ( nParticleAttribute < MAX_PARTICLE_ATTRIBUTES ) );
		contract_assert( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] != -1 );
		return m_pContainer->ElementPointer4V( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ], nX, nY, nZ );
	}
	// ---------- Wrappers for CSOAContainer members ---------- 


private:

	enum ParticleSnapshotDmxVersion_t { PARTICLE_SNAPSHOT_DMX_VERSION = 1 };

	// Whether we're using an external container (as opposed to our embedded one)
	bool UsingExternalContainer( void ) { return ( m_pContainer && ( m_pContainer != &m_Container ) ); }

	// Utility function used by the Init methods to add+validate an attribute mapping pair:
	bool AddAttributeMapping(      int nFieldNumber, int nParticleAttribute, const char *pFunc );
	// Utility function used by the Init methods to validate data types for an attribute mapping pair:
	bool ValidateAttributeMapping( int nFieldNumber, int nParticleAttribute, const char *pFunc );

	// Utility function to validate the embedded container after it is unserialized
	bool EmbeddedContainerIsValid( void );

	// Check whether the particle system's defined attributes have been changed (this won't compile if they have), so we can update the serialization code if need be
	void CheckParticleAttributesForChanges( void );


	CSOAContainer	m_Container;	// Embedded container
	CSOAContainer *	m_pContainer;	// Pointer either to the embedded container or an external one

	// For each particle attribute, this contains the index of the corresponding container attribute (-1 means 'none')
	int m_ParticleAttributeToContainerAttribute[ MAX_PARTICLE_ATTRIBUTES ];
	int m_ContainerAttributeToParticleAttribute[ MAX_SOA_FIELDS ]; // Reverse mapping (used for error-checking)
};


//------------------------------------------------------------------------------
// structure describing the parameter block used by operators which use the path between two points to
// control particles.
//------------------------------------------------------------------------------
struct CPathParameters
{
	int m_nStartControlPointNumber;
	int m_nEndControlPointNumber;
	int m_nBulgeControl;
	float m_flBulge;
	float m_flMidPoint;

	void ClampControlPointIndices( void )
	{
		m_nStartControlPointNumber = std::max(0, std::min( MAX_PARTICLE_CONTROL_POINTS-1, m_nStartControlPointNumber ) );
		m_nEndControlPointNumber = std::max(0, std::min( MAX_PARTICLE_CONTROL_POINTS-1, m_nEndControlPointNumber ) );
	}
};


struct CParticleControlPoint
{
	Vector m_Position;
	Vector m_PrevPosition;

	// orientation
	Vector m_ForwardVector;
	Vector m_UpVector;
	Vector m_RightVector;

	// reference to entity or whatever this control point comes from
	void *m_pObject;

	// parent for hierarchies
	int m_nParent;

	// CParticleSnapshot which particles can read data from or write data to:
	CParticleSnapshot *m_pSnapshot;
};

struct CParticleCPInfo
{
	CParticleControlPoint m_ControlPoint;
	CModelHitBoxesInfo m_CPHitBox;
};


// struct for simd xform to transform a point from an identitiy coordinate system to that of the control point
struct CParticleSIMDTransformation
{
	FourVectors m_v4Origin;
	FourVectors m_v4Fwd;
	FourVectors m_v4Up;
	FourVectors m_v4Right;


	[[gnu::always_inline]] inline void VectorRotate( FourVectors &InPnt )
	{
		fltx4 fl4OutX = SubSIMD( AddSIMD( MulSIMD( InPnt.x, m_v4Fwd.x ), MulSIMD( InPnt.z, m_v4Up.x ) ), MulSIMD( InPnt.y, m_v4Right.x ) );
		fltx4 fl4OutY = SubSIMD( AddSIMD( MulSIMD( InPnt.x, m_v4Fwd.y ), MulSIMD( InPnt.z, m_v4Up.y ) ), MulSIMD( InPnt.y, m_v4Right.y ) );
		InPnt.z = SubSIMD( AddSIMD( MulSIMD( InPnt.x, m_v4Fwd.z ), MulSIMD( InPnt.z, m_v4Up.z ) ), MulSIMD( InPnt.y, m_v4Right.z ) );
		InPnt.x = fl4OutX;
		InPnt.y = fl4OutY;
	}

	[[gnu::always_inline]] inline void VectorTransform( FourVectors &InPnt )
	{
		VectorRotate( InPnt );
		InPnt.x = AddSIMD( InPnt.x, m_v4Origin.x );
		InPnt.y = AddSIMD( InPnt.y, m_v4Origin.y );
		InPnt.z = AddSIMD( InPnt.z, m_v4Origin.z );
	}
};

inline constexpr auto NUM_COLLISION_CACHE_MODES = 4;

//-----------------------------------------------------------------------------
//
// CParticleCollection
//
//-----------------------------------------------------------------------------

enum EParticleRestartMode_t
{
	RESTART_NORMAL,											// just reset emitters
	RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN,				// reset emitters. If another restart has already happened, emit particles right now to handle multiple resets per frame.

};

struct CParticleAttributeAddressTable
{
	float *m_pAttributes[MAX_PARTICLE_ATTRIBUTES];
	std::size_t m_nFloatStrides[MAX_PARTICLE_ATTRIBUTES];

	[[gnu::always_inline]] inline std::size_t Stride( int nAttr ) const
	{
		return m_nFloatStrides[nAttr];
	}

	[[gnu::always_inline]] inline float *Address( int nAttr ) const
	{
		return m_pAttributes[nAttr];
	}

	[[gnu::always_inline]] inline uint8 *ByteAddress( int nAttr ) const
	{
		return ( uint8 * ) m_pAttributes[nAttr];
	}

	[[gnu::always_inline]] inline float *FloatAttributePtr( int nAttribute, int nParticleNumber ) const
	{
		int block_ofs = nParticleNumber / 4;
		return m_pAttributes[ nAttribute ] + 
			m_nFloatStrides[ nAttribute ] * block_ofs +
			( nParticleNumber & 3 );
	}

	void CopyParticleAttributes( int nSrcIndex, int nDestIndex ) const;
};

//-----------------------------------------------------------------------------
// CCachedParticleBatches
//
// Caches up to MAX_CACHED_PARTICLE_BATCHES particle batches.  The 
// ICachedPerFrameMeshData comes from the dynamic mesh created when rendering
// the particles the first time.  This cache is only valid for a single frame.
//
// Only certain particle systems work with caching.  The particle sytem must
// not sort, must not alter vertices on the CPU based upon view data, and must
// not be rendering in mat_queue_mode 0.  SpriteTrail particle system that use
// the spritecard shader cache their particle batches for reuse throughout the
// frame.
//-----------------------------------------------------------------------------

inline constexpr auto MAX_CACHED_PARTICLE_BATCHES = 8;
class CCachedParticleBatches
{
public:
	uint32 m_nLastValidParticleCacheFrame;
	int m_nCachedRenderListCount;
	ICachedPerFrameMeshData *m_pCachedBatches[ MAX_CACHED_PARTICLE_BATCHES ];

	CCachedParticleBatches() : m_nLastValidParticleCacheFrame( (uint32)-1 ), m_nCachedRenderListCount( 0 )
	{
		V_memset( m_pCachedBatches, 0, sizeof( ICachedPerFrameMeshData* ) * MAX_CACHED_PARTICLE_BATCHES );
	}
	~CCachedParticleBatches()
	{
		ClearBatches();
	}

	[[gnu::always_inline]] inline void ClearBatches()
	{
		m_nCachedRenderListCount = 0;
		for ( int i=0; i<MAX_CACHED_PARTICLE_BATCHES; ++i )
		{
			if ( m_pCachedBatches[ i ] ) 
				 m_pCachedBatches[ i ]->Free();
			m_pCachedBatches[ i ] = nullptr;
		}
	}

	[[gnu::always_inline]] inline void SetCachedBatch( int nBatch, ICachedPerFrameMeshData* pBatch )
	{
		if ( nBatch >= MAX_CACHED_PARTICLE_BATCHES )
			return;

		m_pCachedBatches[ nBatch ] = pBatch;
	}

	[[gnu::always_inline]] inline ICachedPerFrameMeshData *GetCachedBatch( int nBatch )
	{
		if ( nBatch >= MAX_CACHED_PARTICLE_BATCHES )
			return nullptr;
		
		return m_pCachedBatches[ nBatch ];
	}

	[[gnu::always_inline]] inline void SetCachedRenderListCount( int nParticleCount )
	{
		m_nCachedRenderListCount = nParticleCount;
	}

	[[gnu::always_inline]] inline int GetCachedRenderListCount()
	{
		return m_nCachedRenderListCount;
	}
};

class CParticleCollection
{
public:
	~CParticleCollection( void );

	// Restarts the particle collection, stopping all non-continuous emitters
	void Restart( EParticleRestartMode_t eMode = RESTART_NORMAL );

	// compute bounds from particle list
	void RecomputeBounds( void );


	void SetControlPoint( int nWhichPoint, const Vector &v );
	void SetControlPointObject( int nWhichPoint, void *pObject );

	void SetControlPointOrientation( int nWhichPoint, const Vector &forward,
									 const Vector &right, const Vector &up );
	void SetControlPointForwardVector( int nWhichPoint, const Vector &v );
	void SetControlPointUpVector( int nWhichPoint, const Vector &v );
	void SetControlPointRightVector( int nWhichPoint, const Vector &v );
	void SetControlPointParent( int nWhichPoint, int n );
	void SetControlPointSnapshot( int nWhichPoint, CParticleSnapshot *pSnapshot );

	void SetControlPointOrientation( int nWhichPoint, const Quaternion &q );

	// get the pointer to an attribute for a given particle.  
	// !!speed!! if you find yourself calling this anywhere that matters, 
	// you're not handling the simd-ness of the particle system well
	// and will have bad perf.
	const float *GetFloatAttributePtr( int nAttribute, int nParticleNumber ) const;
	const int *GetIntAttributePtr( int nAttribute, int nParticleNumber ) const;
	const fltx4 *GetM128AttributePtr( int nAttribute, std::size_t *pStrideOut ) const;
	const FourVectors *Get4VAttributePtr( int nAttribute, std::size_t *pStrideOut ) const;
	const FourInts *Get4IAttributePtr( int nAttribute, std::size_t *pStrideOut ) const;
	const int *GetIntAttributePtr( int nAttribute, std::size_t *pStrideOut ) const;

	Vector GetVectorAttributeValue( int nAttribute, int nParticleNumber ) const;
	float GetFloatAttributeValue( int nAttribute, int nParticleNumber ) const;

	int *GetIntAttributePtrForWrite( int nAttribute, int nParticleNumber );

	float *GetFloatAttributePtrForWrite( int nAttribute, int nParticleNumber );
	fltx4 *GetM128AttributePtrForWrite( int nAttribute, std::size_t *pStrideOut );
	FourVectors *Get4VAttributePtrForWrite( int nAttribute, std::size_t *pStrideOut );

	const float *GetInitialFloatAttributePtr( int nAttribute, int nParticleNumber ) const;
	const fltx4 *GetInitialM128AttributePtr( int nAttribute, std::size_t *pStrideOut ) const;
	const FourVectors *GetInitial4VAttributePtr( int nAttribute, std::size_t *pStrideOut ) const;
	float *GetInitialFloatAttributePtrForWrite( int nAttribute, int nParticleNumber );
	fltx4 *GetInitialM128AttributePtrForWrite( int nAttribute, std::size_t *pStrideOut );

	void Simulate( float dt );
	void SkipToTime( float t );

	// the camera objetc may be compared for equality against control point objects
	void Render( int nViewRecursionLevel, IMatRenderContext *pRenderContext, const Vector4D &vecDiffuseModulation, bool bTranslucentOnly = false, void *pCameraObject = nullptr );

	bool IsValid( void ) const { return m_pDef != nullptr; }

	// this system and all children are valid
	bool IsFullyValid( void ) const;

	const char *GetName() const;

	bool DependsOnSystem( const char *pName ) const;

	// IsFinished returns true when a system has no particles and won't be creating any more
	bool IsFinished( void ) const;

	// Used to make sure we're accessing valid memory
	bool IsValidAttributePtr( int nAttribute, const void *pPtr ) const;

	void SwapPosAndPrevPos( void );

	void SetNActiveParticles( int nCount );
	void KillParticle(int nPidx, unsigned int nFlags = 0);

	void StopEmission( bool bInfiniteOnly = false, bool bRemoveAllParticles = false, bool bWakeOnStop = false, bool bPlayEndCap = false );
	void StartEmission( bool bInfiniteOnly = false );
	void SetDormant( bool bDormant );
	bool IsEmitting() const;

	const Vector& GetControlPointAtCurrentTime( int nControlPoint ) const;
	void GetControlPointOrientationAtCurrentTime( int nControlPoint, Vector *pForward, Vector *pRight, Vector *pUp ) const;
	void GetControlPointTransformAtCurrentTime( int nControlPoint, matrix3x4_t *pMat );
	void GetControlPointTransformAtCurrentTime( int nControlPoint, VMatrix *pMat );
	int GetControlPointParent( int nControlPoint ) const;
	CParticleSnapshot *GetControlPointSnapshot( int nWhichPoint ) const;

	// Used to retrieve the position of a control point
	// somewhere between m_fCurTime and m_fCurTime - m_fPreviousDT
	void GetControlPointAtTime( int nControlPoint, float flTime, Vector *pControlPoint );
	void GetControlPointAtPrevTime( int nControlPoint, Vector *pControlPoint );
	void GetControlPointOrientationAtTime( int nControlPoint, float flTime, Vector *pForward, Vector *pRight, Vector *pUp );
	void GetControlPointTransformAtTime( int nControlPoint, float flTime, matrix3x4_t *pMat );
	void GetControlPointTransformAtTime( int nControlPoint, float flTime, VMatrix *pMat );
	void GetControlPointTransformAtTime( int nControlPoint, float flTime, CParticleSIMDTransformation *pXForm );
	int GetHighestControlPoint( void ) const;

	// Control point accessed:
	// NOTE: Unlike the definition's version of these methods,
	// these OR-in the masks of their children.
	bool ReadsControlPoint( int nPoint ) const;
	bool IsNonPositionalControlPoint( int nPoint ) const;

	// Used by particle systems to generate random numbers. Do not call these methods - use sse
	// code
	int RandomInt( int nMin, int nMax );
	float RandomFloat( float flMin, float flMax );
	float RandomFloatExp( float flMin, float flMax, float flExponent );
	void RandomVector( float flMin, float flMax, Vector *pVector );
	void RandomVector( const Vector &vecMin, const Vector &vecMax, Vector *pVector );
	float RandomVectorInUnitSphere( Vector *pVector );	// Returns the length sqr of the vector

	// NOTE: These versions will produce the *same random numbers* if you give it the same random
	// sample id. do not use these methods.
	int RandomInt( int nRandomSampleId, int nMin, int nMax );
	float RandomFloat( int nRandomSampleId, float flMin, float flMax );
	float RandomFloatExp( int nRandomSampleId, float flMin, float flMax, float flExponent );
	void RandomVector( int nRandomSampleId, float flMin, float flMax, Vector *pVector );
	void RandomVector( int nRandomSampleId, const Vector &vecMin, const Vector &vecMax, Vector *pVector );
	float RandomVectorInUnitSphere( int nRandomSampleId, Vector *pVector );	// Returns the length sqr of the vector

	fltx4 RandomFloat( const FourInts &ParticleID, int nRandomSampleOffset );


	// Random number offset (for use in getting Random #s in operators)
	int OperatorRandomSampleOffset() const;

	// Returns the render bounds
	void GetBounds( Vector *pMin, Vector *pMax );

	// Visualize operators (for editing/debugging)
	void VisualizeOperator( const DmObjectId_t *pOpId = nullptr );

	// Does the particle system use the power of two frame buffer texture (refraction?)
	bool UsesPowerOfTwoFrameBufferTexture( bool bThisFrame ) const;

	// Does the particle system use the full frame buffer texture (soft particles)
	bool UsesFullFrameBufferTexture( bool bThisFrame ) const;

	// Is the particle system translucent?
	bool IsTranslucent() const;

	// Is the particle system two-pass?
	bool IsTwoPass() const;

	// Is the particle system batchable?
	bool IsBatchable() const;

	// Is the order of the particles important
	bool IsOrderImportant() const;

	// Should this system be run want to read its parent's kill list inside ApplyKillList?
	bool ShouldRunForParentApplyKillList( void ) const;

	// Renderer iteration
	int GetRendererCount() const;
	CParticleOperatorInstance *GetRenderer( int i );
	void *GetRendererContext( int i );

	bool CheckIfOperatorShouldRun( CParticleOperatorInstance const * op, float *pflCurStrength, bool bApplyingParentKillList = false );

	Vector TransformAxis( const Vector &SrcAxis, bool bLocalSpace, int nControlPointNumber = 0);

	// return backwards-sorted particle list. use --addressing
	const ParticleRenderData_t *GetRenderList( IMatRenderContext *pRenderContext, bool bSorted, int *pNparticles, CParticleVisibilityData *pVisibilityData );

	// calculate the points of a curve for a path
	void CalculatePathValues( CPathParameters const &PathIn,
							  float flTimeStamp,
							  Vector *pStartPnt,
							  Vector *pMidPnt,
							  Vector *pEndPnt
							  );

	int GetGroupID() const;

	void InitializeNewParticles( int nFirstParticle, int nParticleCount, uint32 nInittedMask, bool bApplyingParentKillList = false );
	
	// update hit boxes for control point if not updated yet for this sim step
	void UpdateHitBoxInfo( int nControlPointNumber, const char *pszHitboxSetName );

	// Used by particle system definitions to manage particle collection lists
	void UnlinkFromDefList( );

	[[gnu::always_inline]] inline uint8 const *GetPrevAttributeMemory( void ) const
	{
		return m_pPreviousAttributeMemory;
	}

	[[gnu::always_inline]] inline uint8 const *GetAttributeMemory( void ) const
	{
		return m_pParticleMemory;
	}
	
	[[gnu::always_inline]] inline bool IsUsingInterpolatedRendering( void ) const
	{
		return (
			( m_flTargetDrawTime < m_flCurTime ) &&
			( m_flTargetDrawTime >= m_flPrevSimTime ) &&
			( GetPrevAttributeMemory() ) &&
			( ! m_bFrozen ) );
	}

	void ResetParticleCache();
	CCachedParticleBatches *GetCachedParticleBatches();

	// render helpers
	int GenerateCulledSortedIndexList( ParticleRenderData_t *pOut, Vector vecCamera, Vector vecFwd, CParticleVisibilityData *pVisibilityData, bool bSorted );
	int GenerateSortedIndexList( ParticleRenderData_t *pOut, Vector vecCameraPos, CParticleVisibilityData *pVisibilityData, bool bSorted );

	CParticleCollection *GetNextCollectionUsingSameDef() { return m_pNextDef; }

	ks::tracked_ref< CSheet > m_Sheet;
	bool m_bTriedLoadingSheet;



protected:
	CParticleCollection( );

	// Used by client code
	bool Init( const char *pParticleSystemName );
	bool Init( CParticleSystemDefinition *pDef );

	// Bloat the bounding box by bounds around the control point
	void BloatBoundsUsingControlPoint();

	// to run emitters on restart, out of main sim.
	void RunRestartedEmitters( void );

	void	SetRenderable( void *pRenderable );


private:



	void Init( CParticleSystemDefinition *pDef, float flDelay, int nRandomSeed );
	void InitStorage( CParticleSystemDefinition *pDef );
	void InitParticleCreationTime( int nFirstParticle, int nNumToInit );
	void CopyInitialAttributeValues( int nStartParticle, int nNumParticles );
	void ApplyKillList( void );
	void SetAttributeToConstant( int nAttribute, float fValue );
	void SetAttributeToConstant( int nAttribute, float fValueX, float fValueY, float fValueZ );
	void InitParticleAttributes( int nStartParticle, int nNumParticles, int nAttrsLeftToInit );

	// call emitter and initializer operators on the specified system
	// NOTE: this may be called from ApplyKillList, so the child can access about-to-be-killed particles
	static void EmitAndInit( CParticleCollection *pCollection, bool bApplyingParentKillList = false );

	// initialize this attribute for all active particles
	void FillAttributeWithConstant( int nAttribute, float fValue );

	// Updates the previous control points
	void UpdatePrevControlPoints( float dt );

	// Returns the memory for a particular constant attribute
	float *GetConstantAttributeMemory( int nAttribute );

	// Swaps two particles in the particle list
	void SwapAdjacentParticles( int hParticle );

	// Unlinks a particle from the list
	void UnlinkParticle( int hParticle );

	// Inserts a particle before another particle in the list
	void InsertParticleBefore( int hParticle, int hBefore );

	// Move a particle from one index to another
	void MoveParticle( int nInitialIndex, int nNewIndex );

	// Computes the sq distance to a particle position
	float ComputeSqrDistanceToParticle( int hParticle, const Vector &vecPosition ) const;

	// Grows the dist sq range for all particles
	void GrowDistSqrBounds( float flDistSqr );

	// Simulates the first frame
	void SimulateFirstFrame( );

	bool SystemContainsParticlesWithBoolSet( bool CParticleCollection::*pField ) const;
	// Does the particle collection contain opaque particle systems
	bool ContainsOpaqueCollections();
	bool ComputeUsesPowerOfTwoFrameBufferTexture();
	bool ComputeUsesFullFrameBufferTexture();
	bool ComputeIsTranslucent();
	bool ComputeIsTwoPass();
	bool ComputeIsBatchable();
	bool ComputeIsOrderImportant();
	bool ComputeRunForParentApplyKillList();

	void LabelTextureUsage( void );

	void LinkIntoDefList( );

	// Return the number of particle systems sharing the same definition
	int GetCurrentParticleDefCount( CParticleSystemDefinition* pDef );

	void CopyParticleAttributesToPreviousAttributes( void ) const;

public:
	fltx4 m_fl4CurTime;										// accumulated time

	int m_nPaddedActiveParticles;	// # of groups of 4 particles
	float m_flCurTime;				// accumulated time
	
	// support for simulating particles at < the frame rate and interpolating.
	// for a system simulating at lower frame rate, flDrawTime will be < m_flCurTime and >=m_flPrevSimTime
	float m_flPrevSimTime;									// the time of the previous sim
	float m_flTargetDrawTime;										// the timestamp for drawing

	int m_nActiveParticles;			// # of active particles
	float m_flDt;
	float m_flPreviousDt;
	float m_flNextSleepTime;								// time to go to sleep if not drawn


	ks::tracked_ref< CParticleSystemDefinition > m_pDef;
	int m_nAllocatedParticles;
	int m_nMaxAllowedParticles;
	bool m_bDormant;
	bool m_bEmissionStopped;
	bool m_bPendingRestart;
	bool m_bQueuedStartEmission;
	bool m_bFrozen;
	bool m_bInEndCap;

	int m_LocalLightingCP;
	Color m_LocalLighting;

	// control point data.  Don't set these directly, or they won't propagate down to children
	// particle control points can act as emitter centers, repulsions points, etc.  what they are
	// used for depends on what operators and parameters your system has.
	int m_nNumControlPointsAllocated;
	CParticleCPInfo *m_pCPInfo;

	[[gnu::always_inline]] inline CParticleControlPoint &ControlPoint( int nIdx ) const;

	[[gnu::always_inline]] inline CModelHitBoxesInfo &ControlPointHitBox( int nIdx ) const;


	// public so people can call methods
	uint8 *m_pOperatorContextData;
	CParticleCollection *m_pNext;							// for linking children together
	CParticleCollection *m_pPrev;							// for linking children together

	struct CWorldCollideContextData *m_pCollisionCacheData[NUM_COLLISION_CACHE_MODES]; // children can share collision caches w/ parent

	CParticleCollection *m_pParent;
	
	ks::intrusive_dlist<CParticleCollection>  m_Children;	// list for all child particle systems, owned
	Vector m_Center;										// average of particle centers
	void *m_pRenderable;									// for use by client


	void *operator new(std::size_t nSize);
	void *operator new( std::size_t size, int nBlockUse, const char *pFileName, int nLine );
	void operator delete(void *pData);
	void operator delete( void* p, int nBlockUse, const char *pFileName, int nLine );


protected:
	// current bounds for the particle system
	bool m_bBoundsValid;
	Vector m_MinBounds;
	Vector m_MaxBounds;
	int m_nHighestCP;  //Highest CP set externally.  Needs to assert if a system calls to an unassigned CP.

private:


	int m_nAttributeMemorySize;
	unsigned char *m_pParticleMemory;						// fixed size at initialization. Must be aligned for SSE
	unsigned char *m_pParticleInitialMemory;				// fixed size at initialization. Must be aligned for SSE
	unsigned char *m_pConstantMemory;
	uint8 *m_pPreviousAttributeMemory;						// for simulating at less than the display rate


	int m_nPerParticleInitializedAttributeMask;
	int m_nPerParticleUpdatedAttributeMask;
	int m_nPerParticleReadInitialAttributeMask;				// What fields do operators want to see initial attribute values for?

	CParticleAttributeAddressTable m_ParticleAttributes;
	CParticleAttributeAddressTable m_ParticleInitialAttributes;
	CParticleAttributeAddressTable m_PreviousFrameAttributes;

	float *m_pConstantAttributes;

	uint64 m_nControlPointReadMask;							// Mask indicating which control points have been accessed
	uint64 m_nControlPointNonPositionalMask;				// Mask indicating which control points are non-positional (ie shouldn't be transformed)
	int m_nParticleFlags;									// PCFLAGS_xxx
	bool m_bIsScrubbable : 1;
	bool m_bIsRunningInitializers : 1;
	bool m_bIsRunningOperators : 1;
	bool m_bIsTranslucent : 1;
	bool m_bIsTwoPass : 1;
	bool m_bAnyUsesPowerOfTwoFrameBufferTexture : 1;		// whether or not we or any children use this
	bool m_bAnyUsesFullFrameBufferTexture : 1;
	bool m_bIsBatchable : 1;
	bool m_bIsOrderImportant : 1;							// is order important when deleting
	bool m_bRunForParentApplyKillList : 1;					// see ShouldRunForParentApplyKillList()

	bool m_bUsesPowerOfTwoFrameBufferTexture;				// whether or not we use this, _not_ our children
	bool m_bUsesFullFrameBufferTexture;
	
	// How many frames have we drawn?
	int m_nDrawnFrames;


	// Used to assign unique ids to each particle
	int m_nUniqueParticleId;

	// Used to generate random numbers
	int m_nRandomQueryCount;
	int m_nRandomSeed;
	int m_nOperatorRandomSampleOffset;

	float m_flMinDistSqr;
	float m_flMaxDistSqr;
	float m_flOOMaxDistSqr;
	Vector m_vecLastCameraPos;
	float m_flLastMinDistSqr;
	float m_flLastMaxDistSqr;

	// Particle collection kill list. set up by particle system mgr
	int m_nNumParticlesToKill;
	KillListItem_t *m_pParticleKillList;

	// Used to build a list of all particle collections that have the same particle def 
	CParticleCollection *m_pNextDef;
	CParticleCollection *m_pPrevDef;

	void LoanKillListTo( CParticleCollection *pBorrower ) const;
	bool HasAttachedKillList( void ) const;

	CCachedParticleBatches *m_pCachedParticleBatches;

	// For debugging
	CParticleOperatorInstance *m_pRenderOp;
	friend class CParticleSystemMgr;
	friend class CParticleOperatorInstance;
	friend class CParticleSystemDefinition;
	friend class C4VInterpolatedAttributeIterator;
	friend class CM128InterpolatedAttributeIterator;
};



class CM128InitialAttributeIterator : public CStridedConstPtr<fltx4>
{
public:
	[[gnu::always_inline]] inline CM128InitialAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetInitialM128AttributePtr( nAttribute, &m_nStride );
	}
};


class CM128AttributeIterator : public CStridedConstPtr<fltx4>
{
public:
	[[gnu::always_inline]] inline void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetM128AttributePtr( nAttribute, &m_nStride );
	}
	[[gnu::always_inline]] inline CM128AttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}
	CM128AttributeIterator( void )
	{
	}

	[[gnu::always_inline]] inline fltx4 operator()( fltx4 fl4T ) const
	{
		return *( m_pData );
	}
};

class C4IAttributeIterator : public CStridedConstPtr<FourInts>
{
public:
	[[gnu::always_inline]] inline void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->Get4IAttributePtr( nAttribute, &m_nStride );
	}
	[[gnu::always_inline]] inline C4IAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}
	[[gnu::always_inline]] inline C4IAttributeIterator( void )
	{
	}
};

class CM128AttributeWriteIterator : public CStridedPtr<fltx4>
{
public:
	[[gnu::always_inline]] inline CM128AttributeWriteIterator( void )
	{
	}
	[[gnu::always_inline]] inline void Init ( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetM128AttributePtrForWrite( nAttribute, &m_nStride );
	}
	[[gnu::always_inline]] inline CM128AttributeWriteIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}
};

class C4VAttributeIterator : public CStridedConstPtr<FourVectors>
{
public:
	[[gnu::always_inline]] inline void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->Get4VAttributePtr( nAttribute, &m_nStride );
	}
	[[gnu::always_inline]] inline C4VAttributeIterator( void )
	{
	}

	[[gnu::always_inline]] inline C4VAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}

	[[gnu::always_inline]] inline fltx4 X( fltx4 T )
	{
		return m_pData->x;
	}

	[[gnu::always_inline]] inline fltx4 Y( fltx4 T )
	{
		return m_pData->y;
	}

	[[gnu::always_inline]] inline fltx4 Z( fltx4 T )
	{
		return m_pData->z;
	}

};

class CM128InterpolatedAttributeIterator : public CM128AttributeIterator
{
protected:
	intp m_nOldDataOffset;

	[[gnu::always_inline]] inline fltx4 const *PreviousData( void ) const
	{
		return ( fltx4 const * ) ( ( ( uint8 const * ) m_pData ) + m_nOldDataOffset );
	}

public:
	[[gnu::always_inline]] inline void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetM128AttributePtr( nAttribute, &m_nStride );
		contract_assert( pParticles->GetPrevAttributeMemory() );
		if ( m_nStride )
		{
			m_nOldDataOffset = 
				pParticles->m_PreviousFrameAttributes.ByteAddress( nAttribute ) - pParticles->m_ParticleAttributes.ByteAddress( nAttribute );
		}
		else
		{
			m_nOldDataOffset = 0;
		}

	}
	[[gnu::always_inline]] inline CM128InterpolatedAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}

	CM128InterpolatedAttributeIterator( void )
	{
	}

	[[gnu::always_inline]] inline fltx4 operator()( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = *( PreviousData() );
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( *m_pData, fl4Ret ) ) );
	}
};

class C4VInterpolatedAttributeIterator : public C4VAttributeIterator
{
protected:
	std::ptrdiff_t m_nOldDataOffset;

	[[gnu::always_inline]] inline FourVectors const *PreviousData( void ) const
	{
		return ( FourVectors const * ) ( ( ( uint8 const * ) m_pData ) + m_nOldDataOffset );
	}

public:
	void Init( int nAttribute, CParticleCollection *pParticles );


	[[gnu::always_inline]] inline C4VInterpolatedAttributeIterator( void )
	{
	}

	[[gnu::always_inline]] inline C4VInterpolatedAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}

	[[gnu::always_inline]] inline fltx4 X( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = PreviousData()->x;
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( m_pData->x, fl4Ret ) ) );
	}

	[[gnu::always_inline]] inline fltx4 Y( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = PreviousData()->y;
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( m_pData->y, fl4Ret ) ) );
	}

	[[gnu::always_inline]] inline fltx4 Z( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = PreviousData()->z;
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( m_pData->z, fl4Ret ) ) );
	}

};

class C4VInitialAttributeIterator : public CStridedConstPtr<FourVectors>
{
public:
	[[gnu::always_inline]] inline C4VInitialAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetInitial4VAttributePtr( nAttribute, &m_nStride );
	}
};

class C4VAttributeWriteIterator : public CStridedPtr<FourVectors>
{
public:
	[[gnu::always_inline]] inline C4VAttributeWriteIterator( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->Get4VAttributePtrForWrite( nAttribute, &m_nStride );
	}
};





//-----------------------------------------------------------------------------
// Inline methods of CParticleCollection
//-----------------------------------------------------------------------------

inline bool CParticleCollection::HasAttachedKillList( void ) const
{
	return m_pParticleKillList != nullptr;
}

inline bool CParticleCollection::ReadsControlPoint( int nPoint ) const
{
	return ( m_nControlPointReadMask & ( 1ULL << nPoint ) ) != 0;
}

inline bool CParticleCollection::IsNonPositionalControlPoint( int nPoint ) const
{
	return ( m_nControlPointNonPositionalMask & ( 1ULL << nPoint ) ) != 0;
}

inline void CParticleCollection::SetNActiveParticles( int nCount )
{
	contract_assert( nCount >= 0 && nCount <= m_nMaxAllowedParticles );
	m_nActiveParticles = nCount;
	m_nPaddedActiveParticles = ( nCount+3 )/4;
}

inline void CParticleCollection::SwapPosAndPrevPos( void )
{
	// strides better be the same!
	contract_assert( m_ParticleAttributes.Stride( PARTICLE_ATTRIBUTE_XYZ ) == m_ParticleAttributes.Stride( PARTICLE_ATTRIBUTE_PREV_XYZ  ) );
	V_swap( m_ParticleAttributes.m_pAttributes[ PARTICLE_ATTRIBUTE_XYZ ], m_ParticleAttributes.m_pAttributes[ PARTICLE_ATTRIBUTE_PREV_XYZ ] );
}

[[gnu::always_inline]] inline CParticleControlPoint &CParticleCollection::ControlPoint( int nIdx ) const
{
	contract_assert( nIdx >= 0 && nIdx < m_nNumControlPointsAllocated );
	return m_pCPInfo[ std::max(0, std::min( nIdx, m_nNumControlPointsAllocated - 1 )) ].m_ControlPoint;
}

[[gnu::always_inline]] inline CModelHitBoxesInfo &CParticleCollection::ControlPointHitBox( int nIdx ) const
{
	return m_pCPInfo[ std::max(0, std::min( nIdx, m_nNumControlPointsAllocated - 1 )) ].m_CPHitBox;
}

inline void CParticleCollection::LoanKillListTo( CParticleCollection *pBorrower ) const
{
	contract_assert( ! pBorrower->m_pParticleKillList );
	pBorrower->m_nNumParticlesToKill = 0;
	pBorrower->m_pParticleKillList = m_pParticleKillList;
}

inline void CParticleCollection::SetAttributeToConstant( int nAttribute, float fValue )
{
	float *fconst = m_pConstantAttributes + 4*3*nAttribute;
	fconst[0] = fconst[1] = fconst[2] = fconst[3] = fValue;
}

inline void CParticleCollection::SetAttributeToConstant( int nAttribute, float fValueX, float fValueY, float fValueZ )
{
	float *fconst = m_pConstantAttributes + 4*3*nAttribute;
	fconst[0] = fconst[1] = fconst[2] = fconst[3] = fValueX;
	fconst[4] = fconst[5] = fconst[6] = fconst[7] = fValueY;
	fconst[8] = fconst[9] = fconst[10] = fconst[11] = fValueZ;
}

inline void CParticleCollection::SetControlPoint( int nWhichPoint, const Vector &v )
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_Position = v;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPoint( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointObject( int nWhichPoint, void *pObject )
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_pObject = pObject;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointObject( nWhichPoint, pObject );
	}
}

inline void CParticleCollection::SetControlPointOrientation( int nWhichPoint, const Vector &forward,
															 const Vector &right, const Vector &up )
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );

	// check perpendicular
	contract_assert( fabs( DotProduct( forward, up ) ) <= 0.1f );
	contract_assert( fabs( DotProduct( forward, right ) ) <= 0.1f );
	contract_assert( fabs( DotProduct( right, up ) ) <= 0.1f );

	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_ForwardVector = forward;
		ControlPoint( nWhichPoint ).m_UpVector = up;
		ControlPoint( nWhichPoint ).m_RightVector = right;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	// make sure all children are finished
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointOrientation( nWhichPoint, forward, right, up );
	}
}

inline Vector CParticleCollection::TransformAxis( const Vector &SrcAxis, bool bLocalSpace,
												  int nControlPointNumber)
{
	if ( bLocalSpace )
	{
		return												// mxmul
			( SrcAxis.x * ControlPoint( nControlPointNumber ).m_RightVector )+
			( SrcAxis.y * ControlPoint( nControlPointNumber ).m_ForwardVector )+
			( SrcAxis.z * ControlPoint( nControlPointNumber ).m_UpVector );
	}
	else
		return SrcAxis;
}


inline void CParticleCollection::SetControlPointOrientation( int nWhichPoint, const Quaternion &q )
{
	matrix3x4_t mat;
	Vector vecForward, vecUp, vecRight;
	QuaternionMatrix( q, mat );
	MatrixVectors( mat, &vecForward, &vecRight, &vecUp );
	SetControlPointOrientation( nWhichPoint, vecForward, vecRight, vecUp );
}

inline void CParticleCollection::SetControlPointForwardVector( int nWhichPoint, const Vector &v )
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_ForwardVector = v;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointForwardVector( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointUpVector( int nWhichPoint, const Vector &v )
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_UpVector = v;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointUpVector( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointRightVector( int nWhichPoint, const Vector &v)
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_RightVector = v;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointRightVector( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointParent( int nWhichPoint, int n )
{
	contract_assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_nParent = n;
		m_nHighestCP = std::max( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointParent( nWhichPoint, n );
	}
}

inline void CParticleCollection::SetControlPointSnapshot( int nWhichPoint, CParticleSnapshot *pSnapshot )
{
	contract_assert( ( nWhichPoint >= 0 ) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_pSnapshot = pSnapshot;
	}
	for( CParticleCollection *i = m_Children.head(); i; i=i->m_pNext )
	{
		i->SetControlPointSnapshot( nWhichPoint, pSnapshot );
	}
}


// Returns the memory for a particular constant attribute
inline float *CParticleCollection::GetConstantAttributeMemory( int nAttribute )
{
	return m_pConstantAttributes + 3 * 4 * nAttribute;
}

// Random number offset (for use in getting Random #s in operators)
inline int CParticleCollection::OperatorRandomSampleOffset() const
{
	return m_nOperatorRandomSampleOffset;
}

// Used by particle systems to generate random numbers
inline int CParticleCollection::RandomInt( int nRandomSampleId, int nMin, int nMax )
{
	// do not call
	float flRand = s_pRandomFloats[ ( m_nRandomSeed + nRandomSampleId ) & RANDOM_FLOAT_MASK ];
	flRand *= ( nMax + 1 - nMin );
	int nRand = (int)flRand + nMin;
	return nRand;
}

inline float CParticleCollection::RandomFloat( int nRandomSampleId, float flMin, float flMax )
{
	// do not call
	float flRand = s_pRandomFloats[ ( m_nRandomSeed + nRandomSampleId ) & RANDOM_FLOAT_MASK ];
	flRand *= ( flMax - flMin );
	flRand += flMin;
	return flRand;
}

inline fltx4 CParticleCollection::RandomFloat( const FourInts &ParticleID, int nRandomSampleOffset )
{
	fltx4 Retval;
	int nOfs=m_nRandomSeed+nRandomSampleOffset;
	SubFloat( Retval, 0 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[0] ) & RANDOM_FLOAT_MASK ];
	SubFloat( Retval, 1 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[1] ) & RANDOM_FLOAT_MASK ];
	SubFloat( Retval, 2 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[2] ) & RANDOM_FLOAT_MASK ];
	SubFloat( Retval, 3 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[3] ) & RANDOM_FLOAT_MASK ];
	return Retval;
}


inline float CParticleCollection::RandomFloatExp( int nRandomSampleId, float flMin, float flMax, float flExponent )
{
	// do not call
	float flRand = s_pRandomFloats[ ( m_nRandomSeed + nRandomSampleId ) & RANDOM_FLOAT_MASK ];
	flRand = std::powf( flRand, flExponent );
	flRand *= ( flMax - flMin );
	flRand += flMin;
	return flRand;
}

inline void CParticleCollection::RandomVector( int nRandomSampleId, float flMin, float flMax, Vector *pVector )
{
	// do not call
	float flDelta = flMax - flMin;
	int nBaseId = m_nRandomSeed + nRandomSampleId;

	pVector->x = s_pRandomFloats[ nBaseId & RANDOM_FLOAT_MASK ];
	pVector->x *= flDelta;
	pVector->x += flMin;

	pVector->y = s_pRandomFloats[ ( nBaseId + 1 ) & RANDOM_FLOAT_MASK ];
	pVector->y *= flDelta;
	pVector->y += flMin;

	pVector->z = s_pRandomFloats[ ( nBaseId + 2 ) & RANDOM_FLOAT_MASK ];
	pVector->z *= flDelta;
	pVector->z += flMin;
}

inline void CParticleCollection::RandomVector( int nRandomSampleId, const Vector &vecMin, const Vector &vecMax, Vector *pVector )
{
	// do not call
	int nBaseId = m_nRandomSeed + nRandomSampleId;
	pVector->x = RandomFloat( nBaseId,     vecMin.x, vecMax.x );
	pVector->y = RandomFloat( nBaseId + 1, vecMin.y, vecMax.y );
	pVector->z = RandomFloat( nBaseId + 2, vecMin.z, vecMax.z );
}

// Used by particle systems to generate random numbers
inline int CParticleCollection::RandomInt( int nMin, int nMax )
{
	// do not call
	return RandomInt( m_nRandomQueryCount++, nMin, nMax );
}

inline float CParticleCollection::RandomFloat( float flMin, float flMax )
{
	// do not call
	return RandomFloat( m_nRandomQueryCount++, flMin, flMax );
}

inline float CParticleCollection::RandomFloatExp( float flMin, float flMax, float flExponent )
{
	// do not call
	return RandomFloatExp( m_nRandomQueryCount++, flMin, flMax, flExponent );
}

inline void CParticleCollection::RandomVector( float flMin, float flMax, Vector *pVector )
{
	// do not call
	RandomVector( m_nRandomQueryCount, flMin, flMax, pVector );
	m_nRandomQueryCount +=3;
}

inline void CParticleCollection::RandomVector( const Vector &vecMin, const Vector &vecMax, Vector *pVector )
{
	// do not call
	RandomVector( m_nRandomQueryCount, vecMin, vecMax, pVector );
	m_nRandomQueryCount +=3;
}

inline float CParticleCollection::RandomVectorInUnitSphere( Vector *pVector )
{
	// do not call
	float flUnitSphere = RandomVectorInUnitSphere( m_nRandomQueryCount, pVector );
	m_nRandomQueryCount +=3;
	return flUnitSphere;
}


// get the pointer to an attribute for a given particle.  !!speed!! if you find yourself
// calling this anywhere that matters, you're not handling the simd-ness of the particle system
// well and will have bad perf.
inline const float *CParticleCollection::GetFloatAttributePtr( int nAttribute, int nParticleNumber ) const
{
	contract_assert( nParticleNumber < m_nAllocatedParticles );
	return m_ParticleAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline int *CParticleCollection::GetIntAttributePtrForWrite( int nAttribute, int nParticleNumber )
{
	return reinterpret_cast< int* >( GetFloatAttributePtrForWrite( nAttribute, nParticleNumber ) );
}

inline const int *CParticleCollection::GetIntAttributePtr( int nAttribute, int nParticleNumber ) const
{
	return (int*)GetFloatAttributePtr( nAttribute, nParticleNumber );
}

inline const fltx4 *CParticleCollection::GetM128AttributePtr( int nAttribute, std::size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const FourInts *CParticleCollection::Get4IAttributePtr( int nAttribute, std::size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<FourInts *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const int32 *CParticleCollection::GetIntAttributePtr( int nAttribute, std::size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute );
	return reinterpret_cast<int32 *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const FourVectors *CParticleCollection::Get4VAttributePtr( int nAttribute, std::size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute ) / 12;
	return reinterpret_cast<const FourVectors *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline FourVectors *CParticleCollection::Get4VAttributePtrForWrite( int nAttribute, std::size_t *pStrideOut ) 
{
	*( pStrideOut ) = m_ParticleAttributes.Stride( nAttribute ) / 12;
	return reinterpret_cast<FourVectors *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const FourVectors *CParticleCollection::GetInitial4VAttributePtr( int nAttribute, std::size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleInitialAttributes.Stride( nAttribute )/12;
	return reinterpret_cast<FourVectors *>( m_ParticleInitialAttributes.Address( nAttribute ) );
}

inline Vector CParticleCollection::GetVectorAttributeValue( int nAttribute, int nParticleNumber ) const
{
	contract_assert( nParticleNumber < m_nAllocatedParticles );
	std::size_t nStride;
	float const *pData = ( float const * ) Get4VAttributePtr( nAttribute, &nStride );
	int nOfs = nParticleNumber / 4;
	int nRemainder = nParticleNumber & 3;
	pData += 12 * nOfs + nRemainder;
	Vector vecRet( pData[0], pData[4], pData[8] );
	return vecRet;
}

inline float CParticleCollection::GetFloatAttributeValue( int nAttribute, int nParticleNumber ) const
{
	contract_assert( nParticleNumber < m_nAllocatedParticles );
	float const *pData = GetFloatAttributePtr( nAttribute, nParticleNumber );
	return *pData;
}


inline float *CParticleCollection::GetFloatAttributePtrForWrite( int nAttribute, int nParticleNumber )
{
	// NOTE: If you hit this assertion, it means your particle operator isn't returning
	// the appropriate fields in the RequiredAttributesMask call
	contract_assert( !m_bIsRunningInitializers || ( m_nPerParticleInitializedAttributeMask & (1 << nAttribute) ) );
	contract_assert( !m_bIsRunningOperators || ( m_nPerParticleUpdatedAttributeMask & (1 << nAttribute) ) );

	contract_assert( m_ParticleAttributes.Stride( nAttribute ) != 0 );

	contract_assert( nParticleNumber < m_nAllocatedParticles );
	return m_ParticleAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline fltx4 *CParticleCollection::GetM128AttributePtrForWrite( int nAttribute, std::size_t *pStrideOut )
{
	// NOTE: If you hit this assertion, it means your particle operator isn't returning
	// the appropriate fields in the RequiredAttributesMask call
	contract_assert( !m_bIsRunningInitializers || ( m_nPerParticleInitializedAttributeMask & (1 << nAttribute) ) );
	contract_assert( !m_bIsRunningOperators || ( m_nPerParticleUpdatedAttributeMask & (1 << nAttribute) ) );
	contract_assert( m_ParticleAttributes.Stride( nAttribute ) != 0 );

	*( pStrideOut ) = m_ParticleAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const float *CParticleCollection::GetInitialFloatAttributePtr( int nAttribute, int nParticleNumber ) const
{
	contract_assert( nParticleNumber < m_nAllocatedParticles );
	return m_ParticleInitialAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline const fltx4 *CParticleCollection::GetInitialM128AttributePtr( int nAttribute, std::size_t *pStrideOut ) const
{
	*( pStrideOut ) = m_ParticleInitialAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleInitialAttributes.Address( nAttribute ) );
}

inline float *CParticleCollection::GetInitialFloatAttributePtrForWrite( int nAttribute, int nParticleNumber )
{
	contract_assert( nParticleNumber < m_nAllocatedParticles );
	contract_assert( m_nPerParticleReadInitialAttributeMask & ( 1 << nAttribute ) );
	return m_ParticleInitialAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline fltx4 *CParticleCollection::GetInitialM128AttributePtrForWrite( int nAttribute, std::size_t *pStrideOut )
{
	contract_assert( m_nPerParticleReadInitialAttributeMask & ( 1 << nAttribute ) );
	*( pStrideOut ) = m_ParticleInitialAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleInitialAttributes.Address( nAttribute ) );
}

// Used to make sure we're accessing valid memory
inline bool CParticleCollection::IsValidAttributePtr( int nAttribute, const void *pPtr ) const
{
	if ( pPtr < m_ParticleAttributes.Address( nAttribute ) )
		return false;

	std::size_t nArraySize = m_ParticleAttributes.Stride( nAttribute ) * m_nAllocatedParticles / 4;
	void *pMaxPtr = m_ParticleAttributes.Address( nAttribute ) + nArraySize;
	return ( pPtr <= pMaxPtr );
}


[[gnu::always_inline]] inline void CParticleCollection::KillParticle( int nPidx, unsigned int nKillFlags )
{
	// add a particle to the sorted kill list. entries must be added in sorted order.
	// within a particle operator, this is safe to call. Outside of one, you have to call
	// the ApplyKillList() method yourself. The storage for the kill list is global between
	// all particle systems, so you can't kill a particle in 2 different CParticleCollections
	// w/o calling ApplyKillList
	contract_assert( !m_nNumParticlesToKill || ( nPidx > (int)m_pParticleKillList[ m_nNumParticlesToKill - 1 ].nIndex ) );

	// note that it is permissible to kill particles with indices>the number of active
	// particles, in order to faciliate easy sse coding (that said, we only expect the
	// particle index to be at most more than 3 larger than the particle count)
	contract_assert( nPidx < m_nActiveParticles + 4 );

	static_assert( ( sizeof( KillListItem_t ) == 4 ) && ( MAX_PARTICLES_IN_A_SYSTEM < ( 1 << KILL_LIST_INDEX_BITS ) ) );
	contract_assert( !( nPidx & ~KILL_LIST_INDEX_MASK ) && !( nKillFlags & ~KILL_LIST_FLAGS_MASK ) );
	KillListItem_t killItem = { (unsigned int)nPidx, nKillFlags };

	contract_assert( m_nNumParticlesToKill < MAX_PARTICLES_IN_A_SYSTEM );
	m_pParticleKillList[ m_nNumParticlesToKill++ ] = killItem;
}

// initialize this attribute for all active particles
inline void CParticleCollection::FillAttributeWithConstant( int nAttribute, float fValue )
{
	std::size_t stride;
	fltx4 *pAttr = GetM128AttributePtrForWrite( nAttribute, &stride );
	fltx4 fill=ReplicateX4( fValue );
	for( int i = 0; i < m_nPaddedActiveParticles; i++ )
	{
		*(pAttr) = fill;
		pAttr += stride;
	}
}


//-----------------------------------------------------------------------------
// Helper to set vector attribute values
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline void SetVectorAttribute( float *pAttribute, float x, float y, float z )
{
	pAttribute[0] = x;
	pAttribute[4] = y;
	pAttribute[8] = z;
}

[[gnu::always_inline]] inline void SetVectorAttribute( float *pAttribute, const Vector &v )
{
	pAttribute[0] = v.x;
	pAttribute[4] = v.y;
	pAttribute[8] = v.z;
}

[[gnu::always_inline]] inline void SetVectorFromAttribute( Vector &v, const float *pAttribute )
{
	v.x = pAttribute[0];
	v.y = pAttribute[4];
	v.z = pAttribute[8];
}


//-----------------------------------------------------------------------------
// Computes the sq distance to a particle position
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline float CParticleCollection::ComputeSqrDistanceToParticle( int hParticle, const Vector &vecPosition ) const
{
	const float *xyz = GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, hParticle );
	Vector vecParticlePosition( xyz[0], xyz[4], xyz[8] );
	return vecParticlePosition.DistToSqr( vecPosition );
}


//-----------------------------------------------------------------------------
// Grows the dist sq range for all particles
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline void CParticleCollection::GrowDistSqrBounds( float flDistSqr )
{
	if ( m_flLastMinDistSqr > flDistSqr )
	{
		m_flLastMinDistSqr = flDistSqr;
	}
	else if ( m_flLastMaxDistSqr < flDistSqr )
	{
		m_flLastMaxDistSqr = flDistSqr;
	}
}




//-----------------------------------------------------------------------------
// Data associated with children particle systems
//-----------------------------------------------------------------------------
struct ParticleChildrenInfo_t
{
	DmObjectId_t m_Id;
	ParticleSystemHandle_t m_Name;
	bool m_bUseNameBasedLookup;
	float m_flDelay;		// How much to delay this system after the parent starts
	bool m_bEndCap;			// This child only plays when an effect is stopped with endcap effects.
};


//-----------------------------------------------------------------------------
// A template describing how a particle system will function
//-----------------------------------------------------------------------------
class CParticleSystemDefinition
{
	DECLARE_DMXELEMENT_UNPACK();
	private:
		ks::tracked_ref_list< CParticleSystemDefinition > m_References;
		template < class T > friend class ks::tracked_ref;;

	
public:
	CParticleSystemDefinition( void );
	~CParticleSystemDefinition( void );

	// Serialization, unserialization
	void Read( CDmxElement *pElement );
	CDmxElement *Write();

	const char *MaterialName() const;
	IMaterial *GetMaterial() const;
	const char *GetName() const;
    const DmObjectId_t& GetId() const;

	// Does the particle system use the power of two frame buffer texture (refraction?)
	bool UsesPowerOfTwoFrameBufferTexture();

	// Does the particle system use the full frame buffer texture (soft particles)
	bool UsesFullFrameBufferTexture();

	// Should we always precache this?
	bool ShouldAlwaysPrecache() const;

	// Should we batch particle collections using this definition up?
	bool ShouldBatch() const;

	// Is the particle system rendered on the viewmodel?
	bool IsViewModelEffect() const;

	bool IsScreenSpaceEffect() const;

	void SetDrawThroughLeafSystem( bool bDraw ) { m_bDrawThroughLeafSystem = bDraw; }
	bool IsDrawnThroughLeafSystem( void ) const { return m_bDrawThroughLeafSystem; }

	// Used to iterate over all particle collections using the same def
	CParticleCollection *FirstCollection();

	// What's the effective cull size + fill cost?
	// Used for early retirement
	float GetCullRadius() const;
	float GetCullFillCost() const;
	int GetCullControlPoint() const;
	const char *GetCullReplacementDefinition() const;

	int GetMaxRecursionDepth() const;

	// Retirement
	bool HasRetirementBeenChecked( int nFrame ) const;
	void MarkRetirementCheck( int nFrame );

	bool HasFallback() const;
	CParticleSystemDefinition *GetFallbackReplacementDefinition() const;

	int GetMinCPULevel() const;
	int GetMinGPULevel() const;

	// Control point read
	void MarkReadsControlPoint( int nPoint );
	bool ReadsControlPoint( int nPoint ) const;
	bool IsNonPositionalControlPoint( int nPoint ) const;

	float GetMaxTailLength() const;
	void  SetMaxTailLength( float flMaxTailLength );
	// Sheet symbols (used to avoid string->symbol conversions when effects are created)
	void InvalidateSheetIndex();
	void CacheSheetIndex( int nSheetIndex );	// index into CParticleSystemMgr::m_SheetList, -1 for none
	bool IsSheetIndexCached() const;
	int GetSheetIndex() const;


private:
	void Precache();
	void Uncache();
	bool IsPrecached() const;

	void UnlinkAllCollections();

	void SetupContextData( );
	void ParseChildren( CDmxElement *pElement );
	void ParseOperators( const char *pszName, ParticleFunctionType_t nFunctionType,
		CDmxElement *pElement, ParticleOperatorList_t &out_list );
	void WriteChildren( CDmxElement *pElement );
	void WriteOperators( CDmxElement *pElement, const char *pOpKeyName,
		const ParticleOperatorList_t &inList );
	ParticleOperatorList_t *GetOperatorList( ParticleFunctionType_t type );
	CParticleOperatorInstance *FindOperatorById( ParticleFunctionType_t type, const DmObjectId_t &id );
	CParticleOperatorInstance *FindOperatorByName( const char *pOperatorName ); // SLOW!

private:
	int m_nInitialParticles;
	int m_nPerParticleUpdatedAttributeMask;
	int m_nPerParticleInitializedAttributeMask;
	int m_nInitialAttributeReadMask;
	int m_nAttributeReadMask;
	uint64 m_nControlPointReadMask;
	uint64 m_nControlPointNonPositionalMask;
	Vector m_BoundingBoxMin;
	Vector m_BoundingBoxMax;
	std::string m_MaterialName;
	CMaterialReference m_Material;
	Vector4D m_vecMaterialModulation;
	CParticleCollection *m_pFirstCollection;
	std::string m_CullReplacementName;
	float m_flCullRadius;
	float m_flCullFillCost;
	int m_nCullControlPoint;
	int m_nRetireCheckFrame;
	int m_nMaxRecursionDepth;
	float m_flMaxTailLength;

	// Fallbacks for exceeding maximum number of the same type of system at once.
	std::string m_FallbackReplacementName;
	int m_nFallbackMaxCount;
	int m_nFallbackCurrentCount;
	ks::tracked_ref< CParticleSystemDefinition > m_pFallback;

	// Default attribute values
	Color m_ConstantColor;
	Vector m_ConstantNormal;
	float m_flConstantRadius;
	float m_flConstantRotation;
	float m_flConstantRotationSpeed;
	int m_nConstantSequenceNumber;
	int m_nConstantSequenceNumber1;
	int m_nGroupID;
	float m_flMaximumTimeStep;
	float m_flMaximumSimTime;					 // maximum time to sim before drawing first frame.
	float m_flMinimumSimTime; // minimum time to sim before drawing first frame - prevents all
							  // capped particles from drawing at 0 time.
	float m_flMinimumTimeStep;				  // for simulating at < frame rate

	int m_nMinimumFrames;					  // number of frames to apply max/min simulation times

	int m_nMinCPULevel;						// minimum CPU/GPU levels for a 
	int m_nMinGPULevel;						// particle system to be allowed to spawn



	// Is the particle system rendered on the viewmodel?
	bool m_bViewModelEffect;
	bool m_bScreenSpaceEffect;
	bool m_bDrawThroughLeafSystem;
	bool m_bSheetIndexCached;

	int m_nSheetIndex;

	std::size_t m_nContextDataSize;
	DmObjectId_t m_Id;

public:
	float m_flMaxDrawDistance;								// distance at which to not draw. 
	float m_flNoDrawTimeToGoToSleep;						// after not beeing seen for this long, the system will sleep

	int m_nMaxParticles;
	int m_nSkipRenderControlPoint;							// if the camera is attached to the
															// object associated with this control
															// point, don't render the system

	int m_nAllowRenderControlPoint;							// if the camera is attached to the
															// object associated with this control
															// point, render the system, otherwise, don't


	int m_nAggregationMinAvailableParticles;				// only aggregate if their are this many free particles
	float m_flAggregateRadius;								// aggregate particles if this system is within radius n
	float m_flStopSimulationAfterTime;						// stop ( freeze ) simulation after this time

	std::string m_Name;

	ParticleOperatorList_t m_Operators;
	ParticleOperatorList_t m_Renderers;
	ParticleOperatorList_t m_Initializers;
	ParticleOperatorList_t m_Emitters;
	ParticleOperatorList_t m_ForceGenerators;
	ParticleOperatorList_t m_Constraints;
	std::vector< ParticleChildrenInfo_t > m_Children;

	std::vector< std::size_t > m_nOperatorsCtxOffsets;
	std::vector< std::size_t > m_nRenderersCtxOffsets;
	std::vector< std::size_t > m_nInitializersCtxOffsets;
	std::vector< std::size_t > m_nEmittersCtxOffsets;
	std::vector< std::size_t > m_nForceGeneratorsCtxOffsets;
	std::vector< std::size_t > m_nConstraintsCtxOffsets;


	uint m_nPerParticleOutlineMaterialVarToken;

	std::atomic<int> m_nNumIntersectionTests;	 // the number of particle intersectio queries done
	std::atomic<int> m_nNumActualRayTraces;			 // the total number of ray intersections done.

	int m_nMaximumActiveParticles;
	bool m_bShouldSort;
	bool m_bShouldBatch;
	bool m_bIsPrecached : 1;
	bool m_bAlwaysPrecache : 1;

	friend class CParticleCollection;
	friend class CParticleSystemMgr;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CParticleSystemDefinition::CParticleSystemDefinition( void )
{
	m_vecMaterialModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
	m_nSheetIndex = -1;
	m_bSheetIndexCached = false;
	m_nControlPointReadMask = 0;
	m_nControlPointNonPositionalMask = 0;
	m_nInitialAttributeReadMask = 0;
	m_nPerParticleInitializedAttributeMask = 0;
	m_nPerParticleUpdatedAttributeMask = 0;
	m_nAttributeReadMask = 0;
	m_nNumIntersectionTests = 0;
	m_nNumActualRayTraces = 0;
	m_nMaximumActiveParticles = 0;
	m_bIsPrecached = false;
	m_bAlwaysPrecache = false;
	m_bShouldBatch = false;
	m_bShouldSort = true;
	m_pFirstCollection = nullptr;
	m_flCullRadius = 0.0f;
	m_flCullFillCost = 1.0f;
	m_nRetireCheckFrame = 0;
	m_nMaxRecursionDepth = 8;	
	m_nFallbackCurrentCount = 0;
	m_bDrawThroughLeafSystem = true; 
	m_flMaxTailLength = 0.0f;
	m_flMinimumTimeStep = 0;
	m_nPerParticleOutlineMaterialVarToken = 0;
}

inline CParticleSystemDefinition::~CParticleSystemDefinition( void )
{
	// Ensure material ref is released before destruction
	// (CMaterialReference destructor also calls Shutdown, but this ensures
	// the ref is released even if called during early init when materials global might be NULL)
	m_Material.Shutdown();

	UnlinkAllCollections();
}

// Used to iterate over all particle collections using the same def
inline CParticleCollection *CParticleSystemDefinition::FirstCollection()
{ 
	return m_pFirstCollection; 
}

inline float CParticleSystemDefinition::GetCullRadius() const 
{ 
	return m_flCullRadius;
}

inline float CParticleSystemDefinition::GetCullFillCost() const
{
	return m_flCullFillCost;
}

inline const char *CParticleSystemDefinition::GetCullReplacementDefinition() const
{
	return m_CullReplacementName.c_str();
}

inline int CParticleSystemDefinition::GetCullControlPoint() const
{
	return m_nCullControlPoint;
}

inline int CParticleSystemDefinition::GetMaxRecursionDepth() const 
{ 
	return m_nMaxRecursionDepth;
}

inline bool CParticleSystemDefinition::HasFallback() const
{
	return ( m_nFallbackMaxCount > 0 );
}

inline int CParticleSystemDefinition::GetMinCPULevel() const
{
	return m_nMinCPULevel;
}

inline int CParticleSystemDefinition::GetMinGPULevel() const
{
	return m_nMinGPULevel;
}

inline void CParticleSystemDefinition::MarkReadsControlPoint( int nPoint ) 
{ 
	m_nControlPointReadMask |= ( 1ULL << nPoint );
}

inline bool CParticleSystemDefinition::IsNonPositionalControlPoint( int nPoint ) const
{
	return ( m_nControlPointNonPositionalMask & ( 1ULL << nPoint ) ) != 0;
}

inline bool CParticleSystemDefinition::ReadsControlPoint( int nPoint ) const 
{ 
	return ( m_nControlPointReadMask & ( 1ULL << nPoint ) ) != 0;
}

// Retirement
inline bool CParticleSystemDefinition::HasRetirementBeenChecked( int nFrame ) const
{
	return m_nRetireCheckFrame == nFrame;
}

inline void CParticleSystemDefinition::MarkRetirementCheck( int nFrame )
{
	m_nRetireCheckFrame = nFrame;
}

inline bool CParticleSystemDefinition::ShouldBatch() const
{
	return m_bShouldBatch;
}

inline bool CParticleSystemDefinition::IsViewModelEffect() const
{
	return m_bViewModelEffect;
}

inline bool CParticleSystemDefinition::IsScreenSpaceEffect() const
{
	return m_bScreenSpaceEffect;
}


inline float CParticleSystemDefinition::GetMaxTailLength() const
{
	return m_flMaxTailLength;
}

inline void CParticleSystemDefinition::SetMaxTailLength( float flMaxTailLength )
{
	m_flMaxTailLength = flMaxTailLength;
}

inline const char *CParticleSystemDefinition::MaterialName() const
{
	return m_MaterialName.c_str();
}

inline const DmObjectId_t& CParticleSystemDefinition::GetId() const
{
	return m_Id;
}

inline int CParticleCollection::GetGroupID( void ) const
{
	return m_pDef->m_nGroupID;
}

[[gnu::always_inline]] inline const Vector& CParticleCollection::GetControlPointAtCurrentTime( int nControlPoint ) const
{
	contract_assert( !m_pDef || m_pDef->ReadsControlPoint( nControlPoint ) );
	return ControlPoint( nControlPoint ).m_Position;
}

[[gnu::always_inline]] inline void CParticleCollection::GetControlPointOrientationAtCurrentTime( int nControlPoint, Vector *pForward, Vector *pRight, Vector *pUp ) const
{
	contract_assert( nControlPoint <= GetHighestControlPoint() );
	contract_assert( !m_pDef || m_pDef->ReadsControlPoint( nControlPoint ) );

	// FIXME: Use quaternion lerp to get control point transform at time
	*pForward = ControlPoint( nControlPoint).m_ForwardVector;
	*pRight = ControlPoint( nControlPoint ).m_RightVector;
	*pUp = ControlPoint( nControlPoint ).m_UpVector;
}

[[gnu::always_inline]] inline int CParticleCollection::GetControlPointParent( int nControlPoint ) const
{
	contract_assert( nControlPoint <= GetHighestControlPoint() );
	contract_assert( !m_pDef || m_pDef->ReadsControlPoint( nControlPoint ) );
	return ControlPoint( nControlPoint ).m_nParent;
}

[[gnu::always_inline]] inline CParticleSnapshot *CParticleCollection::GetControlPointSnapshot( int nControlPoint ) const
{
	contract_assert( nControlPoint <= GetHighestControlPoint() );
	if ( nControlPoint == -1 )
		return nullptr;
	return ControlPoint( nControlPoint ).m_pSnapshot;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
