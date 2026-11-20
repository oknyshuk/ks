// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <time.h>
#include "declare_class.h"
#include "eiface.h"
#include "shareddefs.h"
#include "cmodel.h"
#include "igameevents.h"
#include "networkvar.h"
#include "reflect_annotations.h"
#include "server_class.h"
#include "vphysics_interface.h"
#include "bspfile.h"
#include "string_t.h"
#include "edict.h"
#include "ihandleentity.h"
#include "ispatialpartition.h"
#include "responserules/response_types.h"
#include "baseentity.h"
#include "filesystem.h"
#include "vcollide_parse.h"
#include "vphysics/object_hash.h"
#include "vphysics/constraints.h"
#include "iservernetworkable.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "const.h"
#include "irecipientfilter.h"
#include "rope_shared.h"
#include "videocfg/videocfg.h"
#include "vscript/ivscript.h"
module ks.server:nav_4;

import std;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.reflect_wirevar;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
import :baseentity;
import :fish;
import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.types;
import ks.math.mathlib;
import ks.tier1.memstack;
import :entityutil;
import ks.game.shareddefs;
import ks.core.random;
import ks.core.vprof;
import ks.math.vector2d;
import ks.tier1.convar;
import ks.tier1.exprevaluator;
import ks.tier1.generichash;
import ks.tier1.vector_ops;
import :vscript;
import ks.core.threadtools;
import :sharedvar;
import ks.game.ai_activity;
import ks.core.wchartypes;
import ks.tier1.createinterface;
import ks.tier1.strtools;
import ks.tier1.bitbuf;
import ks.tier1.icvar;
import ks.tier1.interfaces;
import ks.tier1.playerid;
import ks.tier1.registry;
import :entity_2;
import ks.math.pfns;
import :player;
import ks.vstdlib.callqueue;

class CBaseEntity;
class CBasePlayer;
class CNavArea;

using namespace ::ks::reflect;

// Navigation ladders
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//--------------------------------------------------------------------------------------------------------------
/**
 * The NavLadder represents ladders in the Navigation Mesh, and their connections to adjacent NavAreas
 * @todo Deal with ladders that allow jumping off to areas in the middle
 */
class CNavLadder
{
public:
	CNavLadder( void );

	~CNavLadder();

	void OnRoundRestart( void );			///< invoked when a game round restarts

	void Save( ks::byte_writer &fileBuffer, unsigned int version ) const;
	void Load( ks::byte_reader &fileBuffer, unsigned int version );

	unsigned int GetID( void ) const	{ return m_id; }		///< return this ladder's unique ID
	static void CompressIDs( void );							///<re-orders ladder ID's so they are continuous

	enum LadderDirectionType
	{
		LADDER_UP = 0,
		LADDER_DOWN,

		NUM_LADDER_DIRECTIONS
	};

	Vector m_top;									///< world coords of the top of the ladder
	Vector m_bottom;								///< world coords of the top of the ladder
	float m_length;									///< the length of the ladder
	float m_width;

	Vector GetPosAtHeight( float height ) const;	///< Compute x,y coordinate of the ladder at a given height

	CNavArea *m_topForwardArea;						///< the area at the top of the ladder
	CNavArea *m_topLeftArea;
	CNavArea *m_topRightArea;
	CNavArea *m_topBehindArea;						///< area at top of ladder "behind" it - only useful for descending
	CNavArea *m_bottomArea;							///< the area at the bottom of the ladder

	bool IsConnected( const CNavArea *area, LadderDirectionType dir ) const;	///< returns true if given area is connected in given direction

	void ConnectGeneratedLadder( float maxHeightAboveTopArea );		///< Connect a generated ladder to nav areas at the end of nav generation

	void ConnectTo( CNavArea *area );				///< connect this ladder to given area
	void Disconnect( CNavArea *area );				///< disconnect this ladder from given area

	void OnSplit( CNavArea *original, CNavArea *alpha, CNavArea *beta );	///< when original is split into alpha and beta, update our connections
	void OnDestroyNotify( CNavArea *dead );			///< invoked when given area is going away

	void DrawLadder( void ) const;					///< Draws ladder and connections
	void DrawConnectedAreas( void );				///< Draws connected areas

	void UpdateDangling( void );					///< Checks if the ladder is dangling (bots cannot go up)

	bool IsInUse( const CBasePlayer *ignore = nullptr ) const;	///< return true if someone is on this ladder (other than 'ignore')

	void SetDir( NavDirType dir );
	NavDirType GetDir( void ) const;
	const Vector &GetNormal( void ) const;

	void Shift( const Vector &shift );							///< shift the nav ladder

	bool IsUsableByTeam( int teamNumber ) const;
	CBaseEntity *GetLadderEntity( void ) const;

private:
	void FindLadderEntity( void );

	EHANDLE m_ladderEntity;

	NavDirType m_dir;								///< which way the ladder faces (ie: surface normal of climbable side)
	Vector m_normal;								///< surface normal of the ladder surface (or Vector-ized m_dir, if the traceline fails)

	enum LadderConnectionType						///< Ladder connection directions, to facilitate iterating over connections
	{
		LADDER_TOP_FORWARD = 0,
		LADDER_TOP_LEFT,
		LADDER_TOP_RIGHT,
		LADDER_TOP_BEHIND,
		LADDER_BOTTOM,

		NUM_LADDER_CONNECTIONS
	};

	CNavArea ** GetConnection( LadderConnectionType dir );

	static unsigned int m_nextID;					///< used to allocate unique IDs
	unsigned int m_id;								///< unique area ID
};
typedef std::vector< CNavLadder * > NavLadderVector;

class CNavArea;
class HidingSpot;

class CBaseCombatCharacter;
class CBaseEntity;
class CNavLadder;
class CNavMesh;
class CNavNode;
struct Extent;

using namespace ::ks::reflect;

// nav_area.h
// Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

// BOTPORT: Clean up relationship between team index and danger storage in nav areas
enum { MAX_NAV_TEAMS = 2 };

class CFuncElevator;

class CNavVectorNoEditAllocator
{
public:
	CNavVectorNoEditAllocator();

	static void Reset();
	static void *Alloc( std::size_t nSize );
	static void *Realloc( void *pMem, std::size_t nSize );
	static void Free( void *pMem );
	static std::size_t GetSize( void *pMem );

private:
	static CMemoryStack m_memory;
	static void *m_pCurrent;
	static int m_nBytesCurrent;
};

//-------------------------------------------------------------------------------------------------------------------
/**
 * The NavConnect union is used to refer to connections to areas
 */
struct NavConnect
{
	NavConnect();

	union
	{
		unsigned int id;
		CNavArea *area;
	};

	mutable float length;

	bool operator==( const NavConnect &other ) const
	{
		return (area == other.area) ? true : false;
	}
};

typedef std::vector< NavConnect > NavConnectVector;

//-------------------------------------------------------------------------------------------------------------------
/**
 * The NavLadderConnect union is used to refer to connections to ladders
 */
union NavLadderConnect
{
	unsigned int id;
	CNavLadder *ladder;

	bool operator==( const NavLadderConnect &other ) const
	{
		return (ladder == other.ladder) ? true : false;
	}
};
typedef std::vector< NavLadderConnect > NavLadderConnectVector;

//--------------------------------------------------------------------------------------------------------------
/**
 * A HidingSpot is a good place for a bot to crouch and wait for enemies
 */
class HidingSpot
{
public:

	enum 
	{ 
		IN_COVER			= 0x01,							// in a corner with good hard cover nearby
		GOOD_SNIPER_SPOT	= 0x02,							// had at least one decent sniping corridor
		IDEAL_SNIPER_SPOT	= 0x04,							// can see either very far, or a large area, or both
		EXPOSED				= 0x08							// spot in the open, usually on a ledge or cliff
	};

	bool HasGoodCover( void ) const			{ return (m_flags & IN_COVER) ? true : false; }	// return true if hiding spot in in cover
	bool IsGoodSniperSpot( void ) const		{ return (m_flags & GOOD_SNIPER_SPOT) ? true : false; }
	bool IsIdealSniperSpot( void ) const	{ return (m_flags & IDEAL_SNIPER_SPOT) ? true : false; }
	bool IsExposed( void ) const			{ return (m_flags & EXPOSED) ? true : false; }	

	int GetFlags( void ) const		{ return m_flags; }

	void Save( ks::byte_writer &fileBuffer, unsigned int version ) const;
	void Load( ks::byte_reader &fileBuffer, unsigned int version );
	NavErrorType PostLoad( void );

	const Vector &GetPosition( void ) const		{ return m_pos; }	// get the position of the hiding spot
	unsigned int GetID( void ) const			{ return m_id; }
	const CNavArea *GetArea( void ) const		{ return m_area; }	// return nav area this hiding spot is within
	void SetArea( CNavArea *pArea ) { m_area = pArea; }

	void Mark( void )							{ m_marker = m_masterMarker; }
	bool IsMarked( void ) const					{ return (m_marker == m_masterMarker) ? true : false; }
	static void ChangeMasterMarker( void )		{ ++m_masterMarker; }

	void SetSaved( bool bSaved ) { m_bIsSaved = bSaved; }
	bool IsSaved( void ) const { return m_bIsSaved; }

public:
	void SetFlags( int flags )				{ m_flags |= flags; }	// FOR INTERNAL USE ONLY
	void SetPosition( const Vector &pos )	{ m_pos = pos; }		// FOR INTERNAL USE ONLY

private:
	friend class CNavMesh;
	friend void ClassifySniperSpot( HidingSpot *spot );

	HidingSpot( void );										// must use factory to create

	Vector m_pos;											// world coordinates of the spot
	unsigned int m_id;										// this spot's unique ID
	unsigned int m_marker;									// this spot's unique marker
	CNavArea *m_area;										// the nav area containing this hiding spot

	unsigned char m_flags;									// bit flags
	bool m_bIsSaved;										// this hiding spot saves to the nav file (spots specified by entities do not save)

	static unsigned int m_nextID;							// used when allocating spot ID's
	static unsigned int m_masterMarker;						// used to mark spots
};
typedef std::vector< HidingSpot * > HidingSpotVector;
extern std::vector< std::unique_ptr< HidingSpot > > TheHidingSpots;	// owns every spot; areas only observe
bool DestroyHidingSpot( HidingSpot *spot );	// the one way to delete a spot

extern HidingSpot *GetHidingSpotByID( unsigned int id );

//--------------------------------------------------------------------------------------------------------------
/**
 * Stores a pointer to an interesting "spot", and a parametric distance along a path
 */
struct SpotOrder
{
	float t;						// parametric distance along ray where this spot first has LOS to our path
	union
	{
		HidingSpot *spot;			// the spot to look at
		unsigned int id;			// spot ID for save/load
	};
};
typedef std::vector< SpotOrder > SpotOrderVector;

/**
 * This struct stores possible path segments thru a CNavArea, and the dangerous spots
 * to look at as we traverse that path segment.
 */
struct SpotEncounter
{
	NavConnect from;
	NavDirType fromDir;
	NavConnect to;
	NavDirType toDir;
	Ray path;									// the path segment
	SpotOrderVector spots;						// list of spots to look at, in order of occurrence
};
typedef std::vector< std::unique_ptr< SpotEncounter > > SpotEncounterVector;

//-------------------------------------------------------------------------------------------------------------------
/**
 * A CNavArea is a rectangular region defining a walkable area in the environment
 */

class CNavAreaCriticalData
{
protected:
	// --- Begin critical data, which is heavily hit during pathing operations and carefully arranged for cache performance [7/24/2008 tom] ---

	/* 0  */	Vector m_nwCorner;											// north-west corner position (2D mins)
	/* 12 */	Vector m_seCorner;											// south-east corner position (2D maxs)
	/* 24 */	float m_invDxCorners;
	/* 28 */	float m_invDyCorners;
	/* 32 */	float m_neZ;												// height of the implicit corner defined by (m_seCorner.x, m_nwCorner.y, m_neZ)
	/* 36 */	float m_swZ;												// height of the implicit corner defined by (m_nwCorner.x, m_seCorner.y, m_neZ)
	/* 40 */	float		m_danger[ MAX_NAV_TEAMS ];						// danger of this area, allowing bots to avoid areas where they died in the past - zero is no danger
	/* 48 */	float		m_dangerTimestamp;								// time when danger value was set - used for decaying
	/* 52 */	int			m_damagingTickCount;

	/* 56 */	unsigned char m_playerCount[ MAX_NAV_TEAMS ];				// the number of players currently in this area

	/* 58 */	unsigned char m_isBlocked[ MAX_NAV_TEAMS ];					// if true, some part of the world is preventing movement through this nav area

	/* 60 */	float m_totalCost;											// the distance so far plus an estimate of the distance left
	/* 64 */	float m_costSoFar;											// distance travelled so far

	/* 68 */	CNavArea *m_nextOpen, *m_prevOpen;							// only valid if m_openMarker == m_masterMarker
	/* 76 */	uint32 m_openMarker;									    // if this equals the current marker value, we are on the open list
	/* 80 */	uint32 m_marker;										    // used to flag the area as visited

	/* 84 */	int	m_attributeFlags;										// set of attribute bit flags (see NavAttributeType)

	//- connections to adjacent areas -------------------------------------------------------------------
	/* 88 */	NavConnectVector m_connect[ NUM_DIRECTIONS ];				// a list of adjacent areas for each direction
	/* 104*/	NavLadderConnectVector m_ladder[ CNavLadder::NUM_LADDER_DIRECTIONS ];	// list of ladders leading up and down from this area

	/* 112 */	uint32 m_nearNavSearchMarker;								// used in GetNearestNavArea()

	/* 116 */	uint16 m_isUnderwater : 15;									// true if the center of the area is underwater
	/* 116.1 */ uint16 m_isInheritedFrom : 1;								// latch used during visibility inheritance computation
	/* 118 */	uint16 /* NavTraverseType */ m_parentHow;							// how we get from parent to us
	
	/* 120*/	CNavArea *m_parent;											// the area just prior to this on in the search path
	/* 124*/	float m_pathLengthSoFar;									// length of path so far, needed for limiting pathfind max path length
	
	/* *************** 360 cache line *************** */

	// --- End critical data --- 
};

class CNavArea : public CNavAreaCriticalData
{
public:
	DECLARE_CLASS_NOBASE( CNavArea )

	CNavArea( void );
	~CNavArea();
	
	void OnServerActivate( void );						// invoked when map is initially loaded
	void OnRoundRestart( void );						// invoked for each area when the round restarts
	void OnRoundRestartPreEntity( void ) { }			// invoked for each area when the round restarts, but before entities are deleted and recreated
	void OnEnter( CBaseCombatCharacter *who, CNavArea *areaJustLeft ) { }	// invoked when player enters this area 
	void OnExit( CBaseCombatCharacter *who, CNavArea *areaJustEntered ) { }	// invoked when player exits this area 

	void OnDestroyNotify( CNavArea *dead );				// invoked when given area is going away
	void OnDestroyNotify( CNavLadder *dead );			// invoked when given ladder is going away

	void OnEditCreateNotify( CNavArea *newArea ) { }		// invoked when given area has just been added to the mesh in edit mode
	void OnEditDestroyNotify( CNavArea *deadArea ) { }		// invoked when given area has just been deleted from the mesh in edit mode
	void OnEditDestroyNotify( CNavLadder *deadLadder ) { }	// invoked when given ladder has just been deleted from the mesh in edit mode

	void Save( ks::byte_writer &fileBuffer, unsigned int version ) const;
	NavErrorType Load( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion );
	NavErrorType PostLoad( void );								// invoked after all areas have been loaded - for pointer binding, etc

	void SaveBase( ks::byte_writer &fileBuffer, unsigned int version ) const;	// the base area data; Save()/Load()/PostLoad() add the Counter-Strike data
	NavErrorType LoadBase( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion );
	NavErrorType PostLoadBase( void );

	void SaveToSelectedSet( KeyValues *areaKey ) const;		// saves attributes for the area to a KeyValues
	void RestoreFromSelectedSet( KeyValues *areaKey );		// restores attributes from a KeyValues

	// for interactively building or generating nav areas
	void Build( CNavNode *nwNode, CNavNode *neNode, CNavNode *seNode, CNavNode *swNode );
	void Build( const Vector &corner, const Vector &otherCorner );
	void Build( const Vector &nwCorner, const Vector &neCorner, const Vector &seCorner, const Vector &swCorner );

	void ConnectTo( CNavArea *area, NavDirType dir );			// connect this area to given area in given direction
	void Disconnect( CNavArea *area );							// disconnect this area from given area

	void ConnectTo( CNavLadder *ladder );						// connect this area to given ladder
	void Disconnect( CNavLadder *ladder );						// disconnect this area from given ladder

	unsigned int GetID( void ) const	{ return m_id; }		// return this area's unique ID
	static void CompressIDs( void );							// re-orders area ID's so they are continuous
	unsigned int GetDebugID( void ) const { return 0; }		// not used, need the space for cache optimization

	void SetAttributes( int bits )			{ m_attributeFlags = bits; }
	int GetAttributes( void ) const			{ return m_attributeFlags; }
	bool HasAttributes( int bits ) const	{ return ( m_attributeFlags & bits ) ? true : false; }
	void RemoveAttributes( int bits )		{ m_attributeFlags &= ( ~bits ); }

	void SetPlace( Place place )		{ m_place = place; }	// set place descriptor
	Place GetPlace( void ) const		{ return m_place; }		// get place descriptor

	void MarkAsBlocked( int teamID, CBaseEntity *blocker, bool bGenerateEvent = true );	// An entity can force a nav area to be blocked
	void MarkAsUnblocked( int teamID, bool bGenerateEvent = true );	// An entity can force a nav area to be blocked
	// Use nav blockers in coop mode. There is a bug with these functions causing bots to lose
	// their path at the start of rounds that is undiagnosed at the time of this comment. Coop needs nav blockers
	// and doesn't (seem) to have any issues with blocked nav so let's leave it on for them
	void UpdateBlocked( bool force = false, int teamID = TEAM_ANY );		// Updates the (un)blocked status of the nav area (throttled)
	bool IsBlocked( int teamID, bool ignoreNavBlockers = false ) const;
	void UpdateBlockedBase( bool force, int teamID );
	bool IsBlockedBase( int teamID, bool ignoreNavBlockers ) const;

	void CheckFloor( CBaseEntity *ignore );						// Checks if there is a floor under the nav area, in case a breakable floor is gone

	void MarkObstacleToAvoid( float obstructionHeight );
	void UpdateAvoidanceObstacles( void );
	bool HasAvoidanceObstacle( float maxObstructionHeight = StepHeight ) const; // is there a large, immobile object obstructing this area
	float GetAvoidanceObstacleHeight( void ) const; // returns the maximum height of the obstruction above the ground

	void CheckWaterLevel( void );
	bool IsUnderwater( void ) const		{ return !!m_isUnderwater; }

	bool IsOverlapping( const Vector &pos, float tolerance = 0.0f ) const;	// return true if 'pos' is within 2D extents of area.
	bool IsOverlapping( const CNavArea *area ) const;			// return true if 'area' overlaps our 2D extents
	bool IsOverlapping( const Extent &extent ) const;			// return true if 'extent' overlaps our 2D extents
	bool IsOverlappingX( const CNavArea *area ) const;			// return true if 'area' overlaps our X extent
	bool IsOverlappingY( const CNavArea *area ) const;			// return true if 'area' overlaps our Y extent
	inline float GetZ( const Vector * __restrict__ pPos ) const __restrict__;			// return Z of area at (x,y) of 'pos'
	inline float GetZ( const Vector &pos ) const __restrict__;						// return Z of area at (x,y) of 'pos'
	float GetZ( float x, float y ) const __restrict__;				// return Z of area at (x,y) of 'pos'
	bool Contains( const Vector &pos ) const;					// return true if given point is on or above this area, but no others
	bool Contains( const CNavArea *area ) const;	
	bool IsCoplanar( const CNavArea *area ) const;				// return true if this area and given area are approximately co-planar
	void GetClosestPointOnArea( const Vector * __restrict__ pPos, Vector *close ) const __restrict__;	// return closest point to 'pos' on this area - returned point in 'close'
	void GetClosestPointOnArea( const Vector &pos, Vector *close ) const { return GetClosestPointOnArea( &pos, close ); }
	float GetDistanceSquaredToPoint( const Vector &pos ) const;	// return shortest distance between point and this area
	bool IsDegenerate( void ) const;							// return true if this area is badly formed
	bool IsRoughlySquare( void ) const;							// return true if this area is approximately square
	bool IsFlat( void ) const;									// return true if this area is approximately flat
	bool HasNodes( void ) const;
	void GetNodes( NavDirType dir, std::vector< CNavNode * > *nodes ) const;	// build a vector of nodes along the given direction
	CNavNode *FindClosestNode( const Vector &pos, NavDirType dir ) const;	// returns the closest node along the given edge to the given point

	bool IsContiguous( const CNavArea *other ) const;			// return true if the given area and 'other' share a colinear edge (ie: no drop-down or step/jump/climb)
	float ComputeAdjacentConnectionHeightChange( const CNavArea *destinationArea ) const;			// return height change between edges of adjacent nav areas (not actual underlying ground)

	bool IsEdge( NavDirType dir ) const;						// return true if there are no bi-directional links on the given side

	bool IsDamaging( void ) const;								// Return true if continuous damage (ie: fire) is in this area
	void MarkAsDamaging( float duration );						// Mark this area is damaging for the next 'duration' seconds

	bool IsVisible( const Vector &eye, Vector *visSpot = nullptr ) const;	// return true if area is visible from the given eyepoint, return visible spot

	int GetAdjacentCount( NavDirType dir ) const	{ return std::ssize( m_connect[ dir ] ); }	// return number of connected areas in given direction
	CNavArea *GetAdjacentArea( NavDirType dir, int i ) const;	// return the i'th adjacent area in the given direction
	CNavArea *GetRandomAdjacentArea( NavDirType dir ) const;

	const NavConnectVector *GetAdjacentAreas( NavDirType dir ) const	{ return &m_connect[dir]; }
	bool IsConnected( const CNavArea *area, NavDirType dir ) const;	// return true if given area is connected in given direction
	bool IsConnected( const CNavLadder *ladder, CNavLadder::LadderDirectionType dir ) const;	// return true if given ladder is connected in given direction
	float ComputeGroundHeightChange( const CNavArea *area );			// compute change in actual ground height from this area to given area

	const NavConnectVector *GetIncomingConnections( NavDirType dir ) const	{ return &m_incomingConnect[dir]; }	// get areas connected TO this area by a ONE-WAY link (ie: we have no connection back to them)
	void AddIncomingConnection( CNavArea *source, NavDirType incomingEdgeDir );

	const NavLadderConnectVector *GetLadders( CNavLadder::LadderDirectionType dir ) const	{ return &m_ladder[dir]; }

	void ComputePortal( const CNavArea *to, NavDirType dir, Vector *center, float *halfWidth ) const;		// compute portal to adjacent area
	NavDirType ComputeLargestPortal( const CNavArea *to, Vector *center, float *halfWidth ) const;		// compute largest portal to adjacent area, returning direction
	void ComputeClosestPointInPortal( const CNavArea *to, NavDirType dir, const Vector &fromPos, Vector *closePos ) const; // compute closest point within the "portal" between to adjacent areas
	NavDirType ComputeDirection( Vector *point ) const;			// return direction from this area to the given point

	//- for hunting algorithm ---------------------------------------------------------------------------
	void SetClearedTimestamp( int teamID );						// set this area's "clear" timestamp to now
	float GetClearedTimestamp( int teamID ) const;				// get time this area was marked "clear"

	//- hiding spots ------------------------------------------------------------------------------------
	const HidingSpotVector *GetHidingSpots( void ) const	{ return &m_hidingSpots; }
	void AddHidingSpot( HidingSpot * pSpot );
	void RemoveHidingSpot( HidingSpot * pSpot );
	unsigned char GetSavedHidingSpotCount( void ) const;

	SpotEncounter *GetSpotEncounter( const CNavArea *from, const CNavArea *to );	// given the areas we are moving between, return the spots we will encounter
	int GetSpotEncounterCount( void ) const				{ return std::ssize( m_spotEncounters ); }

	//- "danger" ----------------------------------------------------------------------------------------
	void IncreaseDanger( int teamID, float amount );			// increase the danger of this area for the given team
	float GetDanger( int teamID );								// return the danger of this area (decays over time)
	float GetDangerDecayRate( void ) const;				// return danger decay rate per second

	//- extents -----------------------------------------------------------------------------------------
	float GetSizeX( void ) const			{ return m_seCorner.x - m_nwCorner.x; }
	float GetSizeY( void ) const			{ return m_seCorner.y - m_nwCorner.y; }
	void GetExtent( Extent *extent ) const;						// return a computed extent (XY is in m_nwCorner and m_seCorner, Z is computed)

	Vector GetCenter( void ) const	
	{ 
		Vector center;

		center.x = (m_nwCorner.x + m_seCorner.x)/2.0f;
		center.y = (m_nwCorner.y + m_seCorner.y)/2.0f;
		center.z = (m_nwCorner.z + m_seCorner.z)/2.0f;

		return center; 
	}

	Vector GetRandomPoint( void ) const;
	Vector GetCorner( NavCornerType corner ) const;
	void SetCorner( NavCornerType corner, const Vector& newPosition );
	void ComputeNormal( Vector *normal, bool alternate = false ) const;	// Computes the area's normal based on m_nwCorner.  If 'alternate' is specified, m_seCorner is used instead.
	void RemoveOrthogonalConnections( NavDirType dir );

	//- occupy time ------------------------------------------------------------------------------------
	float GetEarliestOccupyTimeBase( int teamID ) const;
	float GetEarliestOccupyTime( int teamID ) const;			// returns the minimum time for someone of the given team to reach this spot from their spawn

	//- player counting --------------------------------------------------------------------------------
	void IncrementPlayerCount( int teamID, int entIndex );		// add one player to this area's count
	void DecrementPlayerCount( int teamID, int entIndex );		// subtract one player from this area's count
	unsigned char GetPlayerCount( int teamID = 0 ) const;		// return number of players of given team currently within this area (team of zero means any/all)

	//- lighting ----------------------------------------------------------------------------------------
	float GetLightIntensity( const Vector &pos ) const;			// returns a 0..1 light intensity for the given point
	float GetLightIntensity( float x, float y ) const;			// returns a 0..1 light intensity for the given point
	float GetLightIntensity( void ) const;						// returns a 0..1 light intensity averaged over the whole area

	//- A* pathfinding algorithm ------------------------------------------------------------------------
	static void MakeNewMarker( void )	{ ++m_masterMarker; if (m_masterMarker == 0) m_masterMarker = 1; }
	void Mark( void )					{ m_marker = m_masterMarker; }
	BOOL IsMarked( void ) const			{ return (m_marker == m_masterMarker) ? true : false; }
	
	void SetParent( CNavArea *parent, NavTraverseType how = NUM_TRAVERSE_TYPES )	{ m_parent = parent; m_parentHow = how; }
	CNavArea *GetParent( void ) const	{ return m_parent; }
	NavTraverseType GetParentHow( void ) const	{ return (NavTraverseType)m_parentHow; }

	bool IsOpen( void ) const;									// true if on "open list"
	void AddToOpenList( void );									// add to open list in decreasing value order
	void AddToOpenListTail( void );								// add to tail of the open list
	void UpdateOnOpenList( void );								// a smaller value has been found, update this area on the open list
	void RemoveFromOpenList( void );
	static bool IsOpenListEmpty( void );
	static CNavArea *PopOpenList( void );						// remove and return the first element of the open list													

	bool IsClosed( void ) const;								// true if on "closed list"
	void AddToClosedList( void );								// add to the closed list
	void RemoveFromClosedList( void );

	static void ClearSearchLists( void );						// clears the open and closed lists for a new search

	void SetTotalCost( float value )	{ contract_assert( value >= 0.0 && !IS_NAN(value) ); m_totalCost = value; }
	float GetTotalCost( void ) const	{ return m_totalCost; }

	void SetCostSoFar( float value )	{ contract_assert( value >= 0.0 && !IS_NAN(value) ); m_costSoFar = value; }
	float GetCostSoFar( void ) const	{ return m_costSoFar; }

	void SetPathLengthSoFar( float value )	{ contract_assert( value >= 0.0 && !IS_NAN(value) ); m_pathLengthSoFar = value; }
	float GetPathLengthSoFar( void ) const	{ return m_pathLengthSoFar; }

	//- editing -----------------------------------------------------------------------------------------
	void Draw( void ) const;							// draw area for debugging & editing
	void DrawFilled( int r, int g, int b, int a, float deltaT = 0.1f, bool noDepthTest = true, float margin = 5.0f ) const;	// draw area as a filled rect of the given color
	void DrawSelectedSet( const Vector &shift ) const;	// draw this area as part of a selected set
	void DrawDragSelectionSet( Color &dragSelectionSetColor ) const;
	void DrawConnectedAreas( void ) const;
	void DrawHidingSpots( void ) const;
	bool SplitEdit( bool splitAlongX, float splitEdge, CNavArea **outAlpha = nullptr, CNavArea **outBeta = nullptr );	// split this area into two areas at the given edge
	bool MergeEdit( CNavArea *adj );							// merge this area and given adjacent area 
	bool SpliceEdit( CNavArea *other );							// create a new area between this area and given area 
	void RaiseCorner( NavCornerType corner, int amount, bool raiseAdjacentCorners = true );	// raise/lower a corner (or all corners if corner == NUM_CORNERS)
	void PlaceOnGround( NavCornerType corner, float inset = 0.0f );	// places a corner (or all corners if corner == NUM_CORNERS) on the ground
	NavCornerType GetCornerUnderCursor( void ) const;
	bool GetCornerHotspot( NavCornerType corner, Vector hotspot[NUM_CORNERS] ) const;	// returns true if the corner is under the cursor
	void Shift( const Vector &shift );							// shift the nav area

	//- ladders -----------------------------------------------------------------------------------------
	void AddLadderUp( CNavLadder *ladder );
	void AddLadderDown( CNavLadder *ladder );

	//- generation and analysis -------------------------------------------------------------------------
	void ComputeHidingSpots( void );					// analyze local area neighborhood to find "hiding spots" in this area - for map learning
	void ComputeSniperSpots( void );					// analyze local area neighborhood to find "sniper spots" in this area - for map learning
	void ComputeSpotEncounters( void );					// compute spot encounter data - for map learning
	void ComputeEarliestOccupyTimes( void );
	void CustomAnalysis( bool isIncremental = false );		// for game-specific analysis
	bool ComputeLighting( void );						// compute 0..1 light intensity at corners and center (requires client via listenserver)
	bool TestStairs( void );									// Test an area for being on stairs
	bool IsAbleToMergeWith( CNavArea *other ) const;

	void InheritAttributes( CNavArea *first, CNavArea *second = nullptr );

	//- visibility -------------------------------------------------------------------------------------
	enum VisibilityType
	{
		NOT_VISIBLE				= 0x00,
		POTENTIALLY_VISIBLE		= 0x01,
		COMPLETELY_VISIBLE		= 0x02,
	};

	VisibilityType ComputeVisibility( const CNavArea *area, bool isPVSValid, bool bCheckPVS = true, bool *pOutsidePVS = nullptr ) const;	// do actual line-of-sight traces to determine if any part of given area is visible from this area
	void SetupPVS( void ) const;
	bool IsInPVS( void ) const;					// return true if this area is within the current PVS

	struct AreaBindInfo							// for pointer loading and binding
	{
		union
		{
			CNavArea *area;
			unsigned int id;
		};

		unsigned char attributes;				// VisibilityType

		bool operator==( const AreaBindInfo &other ) const
		{
			return ( area == other.area );
		}
	};

	bool IsEntirelyVisible( const Vector &eye, CBaseEntity *ignore = nullptr ) const;				// return true if entire area is visible from given eyepoint (CPU intensive)
	bool IsPartiallyVisible( const Vector &eye, CBaseEntity *ignore = nullptr ) const;				// return true if any portion of the area is visible from given eyepoint (CPU intensive)

	bool IsPotentiallyVisible( const CNavArea *area ) const;		// return true if given area is potentially visible from somewhere in this area (very fast)
	bool IsPotentiallyVisibleToTeam( int team ) const;				// return true if any portion of this area is visible to anyone on the given team (very fast)

	bool IsCompletelyVisible( const CNavArea *area ) const;			// return true if given area is completely visible from somewhere in this area (very fast)
	bool IsCompletelyVisibleToTeam( int team ) const;				// return true if given area is completely visible from somewhere in this area by someone on the team (very fast)

	//-------------------------------------------------------------------------------------
	/**
	 * Apply the functor to all navigation areas that are potentially
	 * visible from this area.
	 */
	template < typename Functor >
	bool ForAllPotentiallyVisibleAreas( Functor &func )
	{
		int i;

		++s_nCurrVisTestCounter;

		for ( i=0; i<std::ssize( m_potentiallyVisibleAreas ); ++i )
		{
			CNavArea *area = m_potentiallyVisibleAreas[i].area;
			if ( !area )
				continue;

			// If this assertion triggers, an area is in here twice!
			contract_assert( area->m_nVisTestCounter != s_nCurrVisTestCounter );
			area->m_nVisTestCounter = s_nCurrVisTestCounter;

			if ( m_potentiallyVisibleAreas[i].attributes == NOT_VISIBLE )
				continue;
			
			if ( func( area ) == false )
				return false;
		}

		// for each inherited area
		if ( !m_inheritVisibilityFrom.area )
			return true;

		CAreaBindInfoArray &inherited = m_inheritVisibilityFrom.area->m_potentiallyVisibleAreas;

		for ( i=0; i<std::ssize( inherited ); ++i )
		{
			if ( !inherited[i].area )
				continue;

			// We may have visited this from m_potentiallyVisibleAreas
			if ( inherited[i].area->m_nVisTestCounter == s_nCurrVisTestCounter )
				continue;

			// Theoretically, this shouldn't matter. But, just in case!
			inherited[i].area->m_nVisTestCounter = s_nCurrVisTestCounter;

			if ( inherited[i].attributes == NOT_VISIBLE )
				continue;

			if ( func( inherited[i].area ) == false )
				return false;
		}

		return true;
	}

	//-------------------------------------------------------------------------------------
	/**
	 * Apply the functor to all navigation areas that are
	 * completely visible from somewhere in this area.
	 */
	template < typename Functor >
	bool ForAllCompletelyVisibleAreas( Functor &func )
	{
		int i;

		++s_nCurrVisTestCounter;

		for ( i=0; i<std::ssize( m_potentiallyVisibleAreas ); ++i )
		{
			CNavArea *area = m_potentiallyVisibleAreas[i].area;
			if ( !area )
				continue;

			// If this assertion triggers, an area is in here twice!
			contract_assert( area->m_nVisTestCounter != s_nCurrVisTestCounter );
			area->m_nVisTestCounter = s_nCurrVisTestCounter;

			if ( ( m_potentiallyVisibleAreas[i].attributes & COMPLETELY_VISIBLE ) == 0 )
				continue;

			if ( func( area ) == false )
				return false;
		}

		if ( !m_inheritVisibilityFrom.area )
			return true;

		// for each inherited area
		CAreaBindInfoArray &inherited = m_inheritVisibilityFrom.area->m_potentiallyVisibleAreas;

		for ( i=0; i<std::ssize( inherited ); ++i )
		{
			if ( !inherited[i].area )
				continue;
			
			// We may have visited this from m_potentiallyVisibleAreas
			if ( inherited[i].area->m_nVisTestCounter == s_nCurrVisTestCounter )
				continue;

			// Theoretically, this shouldn't matter. But, just in case!
			inherited[i].area->m_nVisTestCounter = s_nCurrVisTestCounter;

			if ( ( inherited[i].attributes & COMPLETELY_VISIBLE ) == 0 )
				continue;

			if ( func( inherited[i].area ) == false )
				return false;
		}

		return true;
	}

	//- approach areas (Counter-Strike) -----------------------------------------------------------------
	struct ApproachInfo
	{
		NavConnect here;										///< the approach area
		NavConnect prev;										///< the area just before the approach area on the path
		NavConnect next;										///< the area just after the approach area on the path
		uint16 prevToHereHow;                                   // NavTraverseType
		uint16 hereToNextHow;
	};
	const ApproachInfo *GetApproachInfo( int i ) const	{ return &m_approach[i]; }
	int GetApproachInfoCount( void ) const				{ return m_approachCount; }
	void ComputeApproachAreas( void );							///< determine the set of "approach areas" - for map learning

	//- player counting (Counter-Strike) ----------------------------------------------------------------
	void ClearPlayerCount( void );								///< set the player count to zero

protected:
	void UnblockArea( void );
	NavErrorType LoadLegacy( ks::byte_reader &fileBuffer, unsigned int version, unsigned int subVersion );

private:
	friend class CNavMesh;
	friend class CNavLadder;

	static bool m_isReset;										// if true, don't bother cleaning up in destructor since everything is going away

	/*
	m_nwCorner
		nw           ne
		 +-----------+
		 | +-->x     |
		 | |         |
		 | v         |
		 | y         |
		 |           |
		 +-----------+
		sw           se
					m_seCorner
	*/

	static unsigned int m_nextID;								// used to allocate unique IDs
	unsigned int m_id;											// unique area ID

	Place m_place;												// place descriptor

	CountdownTimer m_blockedTimer;								// Throttle checks on our blocked state while blocked
	void UpdateBlockedFromNavBlockers( void );					// checks if nav blockers are still blocking the area

	float m_avoidanceObstacleHeight;							// if nonzero, a prop is obstructing movement through this nav area
	CountdownTimer m_avoidanceObstacleTimer;					// Throttle checks on our obstructed state while obstructed

	//- for hunting -------------------------------------------------------------------------------------
	float m_clearedTimestamp[ MAX_NAV_TEAMS ];					// time this area was last "cleared" of enemies

	//- "danger" ----------------------------------------------------------------------------------------
	void DecayDanger( void );

	//- hiding spots ------------------------------------------------------------------------------------
	HidingSpotVector m_hidingSpots;
	bool IsHidingSpotCollision( const Vector &pos ) const;		// returns true if an existing hiding spot is too close to given position

	//- encounter spots ---------------------------------------------------------------------------------
	SpotEncounterVector m_spotEncounters;						// list of possible ways to move thru this area, and the spots to look at as we do
	void AddSpotEncounters( const CNavArea *from, NavDirType fromDir, const CNavArea *to, NavDirType toDir );	// add spot encounter data when moving from area to area

	float m_earliestOccupyTime[ MAX_NAV_TEAMS ];				// min time to reach this spot from spawn

	//- lighting ----------------------------------------------------------------------------------------
	float m_lightIntensity[ NUM_CORNERS ];						// 0..1 light intensity at corners

	//- A* pathfinding algorithm ------------------------------------------------------------------------
	static uint32 m_masterMarker;

	static CNavArea *m_openList;
	static CNavArea *m_openListTail;

	//- connections to adjacent areas -------------------------------------------------------------------
	NavConnectVector m_incomingConnect[ NUM_DIRECTIONS ];		// a list of adjacent areas for each direction that connect TO us, but we have no connection back to them

	//---------------------------------------------------------------------------------------------------
	CNavNode *m_node[ NUM_CORNERS ];							// nav nodes at each corner of the area

	void ResetNodes( void );									// nodes are going away as part of an incremental nav generation
	void Strip( void );											// remove "analyzed" data from nav area

	void FinishMerge( CNavArea *adjArea );						// recompute internal data once nodes have been adjusted during merge
	void MergeAdjacentConnections( CNavArea *adjArea );			// for merging with "adjArea" - pick up all of "adjArea"s connections
	void AssignNodes( CNavArea *area );							// assign internal nodes to the given area

	void FinishSplitEdit( CNavArea *newArea, NavDirType ignoreEdge );	// given the portion of the original area, update its internal data

	void CalcDebugID();

	CNavArea *m_prevHash, *m_nextHash;							// for hash table in CNavMesh

	void ConnectElevators( void );								// find elevator connections between areas

	//- visibility --------------------------------------------------------------------------------------
	void ComputeVisibilityToMesh( void );						// compute visibility to surrounding mesh
	void ResetPotentiallyVisibleAreas();
	static void ComputeVisToArea( CNavArea *&pOtherArea );

	using CAreaBindInfoArray = std::vector<AreaBindInfo>;

	AreaBindInfo m_inheritVisibilityFrom;						// if non-NULL, m_potentiallyVisibleAreas becomes a list of additions and deletions (NOT_VISIBLE) to the list of this area

	// This does not appear to be used in CS:GO, so take it out of the instances for now
	CAreaBindInfoArray m_potentiallyVisibleAreas;				// list of areas potentially visible from inside this area (after PostLoad(), use area portion of union)

	const CAreaBindInfoArray &ComputeVisibilityDelta( const CNavArea *other ) const;	// return a list of the delta between our visibility list and the given adjacent area

	uint32 m_nVisTestCounter;
	static uint32 s_nCurrVisTestCounter;

	//- approach areas ----------------------------------------------------------------------------------
	enum { MAX_APPROACH_AREAS = 16 };
	ApproachInfo m_approach[ MAX_APPROACH_AREAS ];
	unsigned char m_approachCount;
};

typedef std::vector< CNavArea * > NavAreaVector;
extern NavAreaVector TheNavAreas;

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
//
// Inlines
//

//--------------------------------------------------------------------------------------------------------------
/**
* Return Z of area at (x,y) of 'pos'
* Trilinear interpolation of Z values at quad edges.
* NOTE: pos->z is not used.
*/
float CNavArea::GetZ( const Vector * __restrict__ pos ) const __restrict__
{
	return GetZ( pos->x, pos->y );
}

float CNavArea::GetZ( const Vector & pos ) const __restrict__
{
	return GetZ( pos.x, pos.y );
}

//--------------------------------------------------------------------------------------------------------------
/**
* Return the coordinates of the area's corner.
*/

class CNavMesh;

class CBaseCombatCharacter;
class CBaseEntity;
class CNavArea;
class CNavLadder;
class CNavNode;
struct Extent;
class HidingSpot;

using namespace ::ks::reflect;

// nav_mesh.h
// The Navigation Mesh interface
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The Navigation code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//

extern ConVar nav_edit;
extern ConVar nav_quicksave;
extern ConVar nav_show_approach_points;
extern ConVar nav_show_danger;

//--------------------------------------------------------------------------------------------------------
class NavAreaCollector
{
	bool m_checkForDuplicates;
public:
	NavAreaCollector( bool checkForDuplicates = false )
	{
		m_checkForDuplicates = checkForDuplicates;
	}

	bool operator() ( CNavArea *area )
	{
		if ( m_checkForDuplicates && ks::contains( m_area, area ) )
			return true;

		m_area.push_back( area );
		return true;
	}
	std::vector< CNavArea * > m_area;
};

//--------------------------------------------------------------------------------------------------------
class EditDestroyNotification
{
	CNavArea *m_deadArea;

public:
	EditDestroyNotification( CNavArea *deadArea )
	{
		m_deadArea = deadArea;
	}

	bool operator()( CBaseCombatCharacter *actor )
	{
		actor->OnNavAreaRemoved( m_deadArea );
		return true;
	}
};

//--------------------------------------------------------------------------------------------------------
class NavAttributeClearer
{
public:
	NavAttributeClearer( NavAttributeType attribute )
	{
		m_attribute = attribute;
	}

	bool operator() ( CNavArea *area )
	{
		area->SetAttributes( area->GetAttributes() & (~m_attribute) );

		return true;
	}

	NavAttributeType m_attribute;
};

//--------------------------------------------------------------------------------------------------------
class NavAttributeSetter
{
public:
	NavAttributeSetter( NavAttributeType attribute )
	{
		m_attribute = attribute;
	}

	bool operator() ( CNavArea *area )
	{
		area->SetAttributes( area->GetAttributes() | m_attribute );

		return true;
	}

	NavAttributeType m_attribute;
};

//--------------------------------------------------------------------------------------------------------
class NavAttributeToggler
{
public:
	NavAttributeToggler( NavAttributeType attribute )
	{
		m_attribute = attribute;
	}

	bool operator() ( CNavArea *area );

	NavAttributeType m_attribute;
};

//--------------------------------------------------------------------------------------------------------
struct NavAttributeLookup
{
	const char *name;
	NavAttributeType attribute;	
};

extern NavAttributeLookup TheNavAttributeTable[];

//--------------------------------------------------------------------------------------------------------
class SelectOverlappingAreas
{
public:
	bool operator()( CNavArea *area );
};

//--------------------------------------------------------------------------------------------------------
/**
 * Mixin for entities that can obstruct nav areas (CPhysicsProp, CFuncNavObstruction).
 * Each method forwards to the concrete class's method of the same name (see nav_entities.cpp).
 */
class INavAvoidanceObstacle
{
public:
	enum class Kind
	{
		PhysicsProp,			///< CPhysicsProp
		FuncNavObstruction,		///< CFuncNavObstruction
	};

	bool IsPotentiallyAbleToObstructNavAreas( void ) const;	// could we at some future time obstruct nav?
	float GetNavObstructionHeight( void ) const;			// height at which to obstruct nav areas
	bool CanObstructNavAreas( void ) const;					// can we obstruct nav right this instant?
	CBaseEntity *GetObstructingEntity( void );
	void OnNavMeshLoaded( void );

protected:
	explicit INavAvoidanceObstacle( Kind kind ) : m_obstacleKind( kind ) { }

private:
	Kind m_obstacleKind;
};

//--------------------------------------------------------------------------------------------------------
enum GetNavAreaFlags_t
{
	GETNAVAREA_CHECK_LOS			= 0x1,
	GETNAVAREA_ALLOW_BLOCKED_AREAS	= 0x2,
	GETNAVAREA_CHECK_GROUND			= 0x4,
};

//--------------------------------------------------------------------------------------------------------
// for nav mesh visibilty computation
struct NavVisPair_t
{
	void SetPair( CNavArea *pArea1, CNavArea *pArea2 );

	bool operator==( const NavVisPair_t & ) const = default;

	CNavArea *pAreas[2];
};

// for nav mesh visibilty computation
struct CVisPairHashFuncs
{
	std::size_t operator()( const NavVisPair_t &item ) const
	{
		static_assert( sizeof(CNavArea *) == sizeof( intp ) );
		intp key[2] = { (intp)item.pAreas[0] + item.pAreas[1]->GetID(), (intp)item.pAreas[1] + item.pAreas[0]->GetID() };
		if ( sizeof( key ) >= 16 )
		{
			return Hash16( key );
		}
		else
		{
			return Hash8( key );
		}
	}
};

using NavVisPairSet = std::unordered_set< NavVisPair_t, CVisPairHashFuncs >;

//--------------------------------------------------------------------------------------------------------------
//
// The 'place directory' is used to save and load places from
// nav files in a size-efficient manner that also allows for the 
// order of the place ID's to change without invalidating the
// nav files.
//
// The place directory is stored in the nav file as a list of 
// place name strings.  Each nav area then contains an index
// into that directory, or zero if no place has been assigned to 
// that area.
//
class PlaceDirectory
{
public:
	typedef unsigned short IndexType;	// Loaded/Saved as UnsignedShort.  Change this and you'll have to version.

	PlaceDirectory( void );
	void Reset( void );
	bool IsKnown( Place place ) const;						/// return true if this place is already in the directory
	IndexType GetIndex( Place place ) const;				/// return the directory index corresponding to this Place (0 = no entry)
	void AddPlace( Place place );							/// add the place to the directory if not already known
	Place IndexToPlace( IndexType entry ) const;			/// given an index, return the Place
	void Save( ks::byte_writer &fileBuffer );					/// store the directory
	void Load( ks::byte_reader &fileBuffer, int version );		/// load the directory
	const std::vector< Place > *GetPlaces( void ) const
	{
		return &m_directory;
	}

	bool HasUnnamedPlaces( void ) const 
	{
		return m_hasUnnamedAreas;
	}

private:
	std::vector< Place > m_directory;
	bool m_hasUnnamedAreas;
};

extern PlaceDirectory placeDirectory;

//--------------------------------------------------------------------------------------------------------
/**
 * The CNavMesh is the global interface to the Navigation Mesh.
 */
class CNavMesh : public CGameEventListener
{
public:
	CNavMesh( void );
	~CNavMesh();
	
	void PreLoadAreas( int nAreas ) {}
	CNavArea *CreateArea( void ) const;							// CNavArea factory
	void DestroyArea( CNavArea * ) const;
	HidingSpot *CreateHidingSpot( void ) const;					// Hiding Spot factory

	void Reset( void );											// destroy Navigation Mesh data and revert to initial state
	void ResetBase( void );
	void Update( void );										// invoked on each game frame

	void FireGameEvent( IGameEvent *event );					// incoming event processing

	NavErrorType Load( void );									// load navigation data from a file
	NavErrorType PostLoad( unsigned int version );				// invoked after all areas have been loaded - for pointer binding, etc
	NavErrorType PostLoadBase( unsigned int version );
	bool IsLoaded( void ) const		{ return m_isLoaded; }				// return true if a Navigation Mesh has been loaded
	bool IsAnalyzed( void ) const	{ return m_isAnalyzed; }			// return true if a Navigation Mesh has been analyzed

	/**
	 * Return true if nav mesh can be trusted for all climbing/jumping decisions because game environment is fairly simple.
	 * Authoritative meshes mean path followers can skip CPU intensive realtime scanning of unpredictable geometry.
	 */
	bool IsAuthoritative( void ) const { return false; }		

	const std::vector< Place > *GetPlacesFromNavFile( bool *hasUnnamedPlaces );	// Reads the used place names from the nav file (can be used to selectively precache before the nav is loaded)

	bool Save( void ) const;									// store Navigation Mesh to a file
	bool IsOutOfDate( void ) const	{ return m_isOutOfDate; }			// return true if the Navigation Mesh is older than the current map version

	unsigned int GetSubVersionNumber( void ) const;										// returns sub-version number of the game-specific data format
	void SaveCustomData( ks::byte_writer &fileBuffer ) const { }								// store game-specific mesh data
	void LoadCustomData( ks::byte_reader &fileBuffer, unsigned int subVersion ) { }			// load game-specific mesh data
	void SaveCustomDataPreArea( ks::byte_writer &fileBuffer ) const { }						// store game-specific mesh data that needs to be loaded before areas are read in
	void LoadCustomDataPreArea( ks::byte_reader &fileBuffer, unsigned int subVersion ) { }	// load game-specific mesh data that needs to be loaded before areas are read in

	// events
	void OnServerActivate( void );								// invoked when server loads a new map
	void OnRoundRestart( void );								// invoked when a game round restarts
	void OnRoundRestartPreEntity( void );						// invoked when a game round restarts, but before entities are deleted and recreated
	void OnBreakableCreated( CBaseEntity *breakable ) { }		// invoked when a breakable is created
	void OnBreakableBroken( CBaseEntity *broken ) { }			// invoked when a breakable is broken
	void OnAreaBlocked( CNavArea *area );						// invoked when the area becomes blocked
	void OnAreaUnblocked( CNavArea *area );						// invoked when the area becomes un-blocked
	void OnAvoidanceObstacleEnteredArea( CNavArea *area );					// invoked when the area becomes obstructed
	void OnAvoidanceObstacleLeftArea( CNavArea *area );					// invoked when the area becomes un-obstructed

	void OnEditCreateNotify( CNavArea *newArea );				// invoked when given area has just been added to the mesh in edit mode
	void OnEditDestroyNotify( CNavArea *deadArea );				// invoked when given area has just been deleted from the mesh in edit mode
	void OnEditDestroyNotify( CNavLadder *deadLadder );			// invoked when given ladder has just been deleted from the mesh in edit mode
	void OnNodeAdded( CNavNode *node ) {};						

	// Obstructions
	void RegisterAvoidanceObstacle( INavAvoidanceObstacle *obstruction );
	void UnregisterAvoidanceObstacle( INavAvoidanceObstacle *obstruction );
	const std::vector< INavAvoidanceObstacle * > &GetObstructions( void ) const { return m_avoidanceObstacles; }

	unsigned int GetNavAreaCount( void ) const	{ return m_areaCount; }	// return total number of nav areas

	// See GetNavAreaFlags_t for flags
	CNavArea *GetNavArea( const Vector &pos, float beneathLimt = 120.0f, bool checkLOS = false ) const;	// given a position, return the nav area that IsOverlapping and is *immediately* beneath it
	CNavArea *GetNavArea( CBaseEntity *pEntity, int nGetNavAreaFlags, float flBeneathLimit = 120.0f ) const;
	CNavArea *GetNavAreaByID( unsigned int id ) const;
	CNavArea *GetNearestNavArea( const Vector &pos, bool anyZ = false, float maxDist = 10000.0f, bool checkLOS = false, bool checkGround = true ) const;
	CNavArea *GetNearestNavArea( CBaseEntity *pEntity, int nGetNavAreaFlags = GETNAVAREA_CHECK_GROUND, float maxDist = 10000.0f ) const;

	Place GetPlace( const Vector &pos ) const;							// return Place at given coordinate
	const char *PlaceToName( Place place ) const;						// given a place, return its name
	Place NameToPlace( const char *name ) const;						// given a place name, return a place ID or zero if no place is defined
	Place PartialNameToPlace( const char *name ) const;					// given the first part of a place name, return a place ID or zero if no place is defined, or the partial match is ambiguous
	void PrintAllPlaces( void ) const;									// output a list of names to the console
	int PlaceNameAutocomplete( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );	// Given a partial place name, fill in possible place names for ConCommand autocomplete

	bool GetGroundHeight( const Vector &pos, float *height, Vector *normal = nullptr ) const;		// get the Z coordinate of the topmost ground level below the given point
	bool GetSimpleGroundHeight( const Vector &pos, float *height, Vector *normal = nullptr ) const;// get the Z coordinate of the ground level directly below the given point

	/// increase "danger" weights in the given nav area and nearby ones
	void IncreaseDangerNearby( int teamID, float amount, CNavArea *area, const Vector &pos, float maxRadius, float dangerLimit = -1.0f );
	void DrawDanger( void ) const;										// draw the current danger levels

	void DrawPlayerCounts( void ) const;								// draw the current player counts for each area

	//-------------------------------------------------------------------------------------
	// Auto-generation
	//
	void BeginGeneration( bool incremental = false );					// initiate the generation process
	void BeginAnalysis( bool quitWhenFinished = false );						// re-analyze an existing Mesh.  Determine Hiding Spots, Encounter Spots, etc.

	bool IsGenerating( void ) const		{ return m_generationMode != GENERATE_NONE; }	// return true while a Navigation Mesh is being generated
	const char *GetPlayerSpawnName( void ) const;						// return name of player spawn entity
	void SetPlayerSpawnName( const char *name );						// define the name of player spawn entities
	void AddWalkableSeed( const Vector &pos, const Vector &normal );	// add given walkable position to list of seed positions for map sampling
	void AddWalkableSeeds( void );								// adds walkable positions for any/all positions a mod specifies
	void ClearWalkableSeeds( void )		{ m_walkableSeeds.clear(); }	// erase all walkable seed positions
	void MarkStairAreas( void );

	unsigned int GetGenerationTraceMask( void ) const;			// return the mask used by traces when generating the mesh

	//-------------------------------------------------------------------------------------
	// Edit mode
	//
	unsigned int GetNavPlace( void ) const			{ return m_navPlace; }
	void SetNavPlace( unsigned int place )			{ m_navPlace = place; }

	// Edit callbacks from ConCommands
	void CommandNavDelete( void );										// delete current area
	void CommandNavDeleteMarked( void );								// delete current marked area

	void CommandNavFloodSelect( const CCommand &args );			// select current area and all connected areas, recursively
	void CommandNavToggleSelectedSet( void );							// toggles all areas into/out of the selected set
	void CommandNavStoreSelectedSet( void );							// stores the current selected set for later
	void CommandNavRecallSelectedSet( void );							// restores an older selected set
	void CommandNavAddToSelectedSet( void );							// add current area to selected set
	void CommandNavAddToSelectedSetByID(  const CCommand &args );		// add specified area id to selected set
	void CommandNavRemoveFromSelectedSet( void );						// remove current area from selected set
	void CommandNavToggleInSelectedSet( void );							// add/remove current area from selected set
	void CommandNavClearSelectedSet( void );							// clear the selected set to empty
	void CommandNavBeginSelecting( void );								// start continuously selecting areas into the selected set
	void CommandNavEndSelecting( void );								// stop continuously selecting areas into the selected set
	void CommandNavBeginDragSelecting( void );							// start dragging a selection area
	void CommandNavEndDragSelecting( void );							// stop dragging a selection area
	void CommandNavBeginDragDeselecting( void );						// start dragging a deselection area
	void CommandNavEndDragDeselecting( void );							// stop dragging a deselection area
	void CommandNavRaiseDragVolumeMax( void );							// raise the top of the drag volume
	void CommandNavLowerDragVolumeMax( void );							// lower the top of the drag volume
	void CommandNavRaiseDragVolumeMin( void );							// raise the bottom of the drag volume
	void CommandNavLowerDragVolumeMin( void );							// lower the bottom of the drag volume
	void CommandNavToggleSelecting( bool playSound = true );			// start/stop continuously selecting areas into the selected set
	void CommandNavBeginDeselecting( void );							// start continuously de-selecting areas from the selected set
	void CommandNavEndDeselecting( void );								// stop continuously de-selecting areas from the selected set
	void CommandNavToggleDeselecting( bool playSound = true );			// start/stop continuously de-selecting areas from the selected set
	void CommandNavSelectInvalidAreas( void );							// adds invalid areas to the selected set
	void CommandNavSelectBlockedAreas( void );							// adds blocked areas to the selected set
	void CommandNavSelectObstructedAreas( void );						// adds obstructed areas to the selected set
	void CommandNavSelectDamagingAreas( void );							// adds damaging areas to the selected set
	void CommandNavSelectHalfSpace( const CCommand &args );				// selects all areas that intersect the half-space
	void CommandNavSelectStairs( void );								// adds stairs areas to the selected set

	void CommandNavSplit( void );										// split current area
	void CommandNavMerge( void );										// merge adjacent areas
	void CommandNavMark( const CCommand &args );						// mark an area for further operations
	void CommandNavUnmark( void );										// removes the mark

	void CommandNavBeginArea( void );									// begin creating a new nav area
	void CommandNavEndArea( void );										// end creation of the new nav area

	void CommandNavBeginShiftXY( void );								// begin shifting selected set in the XY plane
	void CommandNavEndShiftXY( void );									// end shifting selected set in the XY plane

	void CommandNavConnect( void );										// connect marked area to selected area
	void CommandNavDisconnect( void );									// disconnect marked area from selected area
	void CommandNavSplice( void );										// create new area in between marked and selected areas
	void CommandNavCrouch( void );										// toggle crouch attribute on current area
	void CommandNavTogglePlaceMode( void );								// switch between normal and place editing
	void CommandNavSetPlaceMode( void );								// switch between normal and place editing
	void CommandNavPlaceFloodFill( void );								// floodfill areas out from current area
	void CommandNavPlaceSet( void );									// sets the Place for the selected set
	void CommandNavPlacePick( void );									// "pick up" the place at the current area
	void CommandNavTogglePlacePainting( void );							// switch between "painting" places onto areas
	void CommandNavMarkUnnamed( void );									// mark an unnamed area for further operations
	void CommandNavCornerSelect( void );								// select a corner on the current area
	void CommandNavCornerRaise( const CCommand &args );					// raise a corner on the current area
	void CommandNavCornerLower( const CCommand &args );					// lower a corner on the current area
	void CommandNavCornerPlaceOnGround( const CCommand &args );			// position a corner on the current area at ground height
	void CommandNavWarpToMark( void );									// warp a spectating local player to the selected mark
	void CommandNavLadderFlip( void );									// Flips the direction a ladder faces
	void CommandNavToggleAttribute( NavAttributeType attribute );		// toggle an attribute on current area
	void CommandNavMakeSniperSpots( void );								// cuts up the marked area into individual areas suitable for sniper spots
	void CommandNavBuildLadder( void );									// builds a nav ladder on the climbable surface under the cursor
	void CommandNavRemoveJumpAreas( void );								// removes jump areas, replacing them with connections
	void CommandNavSubdivide( const CCommand &args );					// subdivide each nav area in X and Y to create 4 new areas - limit min size
	void CommandNavSaveSelected( const CCommand &args );				// Save selected set to disk
	void CommandNavMergeMesh( const CCommand &args );					// Merge a saved selected set into the current mesh
	void CommandNavMarkWalkable( void );

	void AddToDragSelectionSet( CNavArea *pArea );
	void RemoveFromDragSelectionSet( CNavArea *pArea );
	void ClearDragSelectionSet( void );

	CNavArea *GetMarkedArea( void ) const;										// return area marked by user in edit mode
	CNavLadder *GetMarkedLadder( void ) const	{ return m_markedLadder; }		// return ladder marked by user in edit mode

	CNavArea *GetSelectedArea( void ) const		{ return m_selectedArea; }		// return area user is pointing at in edit mode
	CNavLadder *GetSelectedLadder( void ) const	{ return m_selectedLadder; }	// return ladder user is pointing at in edit mode
	void SetMarkedLadder( CNavLadder *ladder );							// mark ladder for further edit operations
	void SetMarkedArea( CNavArea *area );								// mark area for further edit operations

	bool IsContinuouslySelecting( void ) const
	{
		return m_isContinuouslySelecting;
	}

	bool IsContinuouslyDeselecting( void ) const
	{
		return m_isContinuouslyDeselecting;
	}

	void CreateLadder( const Vector &mins, const Vector &maxs, float maxHeightAboveTopArea );
	void CreateLadder( const Vector &top, const Vector &bottom, float width, const Vector2D &ladderDir, float maxHeightAboveTopArea );

	float SnapToGrid( float x, bool forceGrid = false ) const;					// snap given coordinate to generation grid boundary
	Vector SnapToGrid( const Vector& in, bool snapX = true, bool snapY = true, bool forceGrid = false ) const;	// snap given vector's X & Y coordinates to generation grid boundary

	const Vector &GetEditCursorPosition( void ) const	{ return m_editCursorPos; }	// return position of edit cursor
	void StripNavigationAreas( void );
	const char *GetFilename( void ) const;								// return the filename for this map's "nav" file

	/// @todo Remove old select code and make all commands use this selected set
	void AddToSelectedSet( CNavArea *area );							// add area to the currently selected set
	void RemoveFromSelectedSet( CNavArea *area );						// remove area from the currently selected set
	void ClearSelectedSet( void );										// clear the currently selected set to empty
	bool IsSelectedSetEmpty( void ) const;								// return true if the selected set is empty
	bool IsInSelectedSet( const CNavArea *area ) const;					// return true if the given area is in the selected set
	int GetSelecteSetSize( void ) const;
	const NavAreaVector &GetSelectedSet( void ) const;					// return the selected set

	/**
	 * Apply the functor to all navigation areas in the Selected Set,
	 * or the current selected area.
	 * If functor returns false, stop processing and return false.
	 */
	template < typename Functor >
	bool ForAllSelectedAreas( Functor &func )
	{
		if (IsSelectedSetEmpty())
		{
			CNavArea *area = GetSelectedArea();
			
			if (area)
			{
				if (func( area ) == false)
					return false;
			}
		}
		else
		{
			for ( CNavArea *area : m_selectedSet )
			{
				if (func( area ) == false)
					return false;
			}
		}
		
		return true;
	}

	//-------------------------------------------------------------------------------------
	/**
	 * Apply the functor to all navigation areas.
	 * If functor returns false, stop processing and return false.
	 */
	template < typename Functor >
	bool ForAllAreas( Functor &func )
	{
		for ( CNavArea *area : TheNavAreas )
		{
			if (func( area ) == false)
				return false;
		}

		return true;
	}

	// const version of the above
	template < typename Functor >
	bool ForAllAreas( Functor &func ) const
	{
		for ( const CNavArea *area : TheNavAreas )
		{
			if (func( area ) == false)
				return false;
		}

		return true;
	}

	//-------------------------------------------------------------------------------------
	/**
	 * Apply the functor to all navigation areas that overlap the given extent.
	 * If functor returns false, stop processing and return false.
	 */
	template < typename Functor >
	bool ForAllAreasOverlappingExtent( Functor &func, const Extent &extent )
	{
		CVProfScope _( "CNavMesh::ForAllAreasOverlappingExtent", 0, "NextBot", false, BUDGETFLAG_OTHER );

		if ( m_grid.empty() )
		{
			return true;
		}
		static unsigned int searchMarker = RandomInt(0, 1024*1024 );
		if ( ++searchMarker == 0 )
		{
			++searchMarker;
		}

		Extent areaExtent;

		// get list in cell that contains position
		int startX = WorldToGridX( extent.lo.x );
		int endX = WorldToGridX( extent.hi.x );
		int startY = WorldToGridY( extent.lo.y );
		int endY = WorldToGridY( extent.hi.y );

		for( int x = startX; x <= endX; ++x )
		{
			for( int y = startY; y <= endY; ++y )
			{
				int iGrid = x + y*m_gridSizeX;
				if ( iGrid >= std::ssize( m_grid ) )
				{
					ExecuteNTimes( 10, [&]{ Warning( "** Walked off of the CNavMesh::m_grid in ForAllAreasOverlappingExtent()\n" ); } );
					return true;
				}

				NavAreaVector *areaVector = &m_grid[ iGrid ];

				// find closest area in this cell
				for ( CNavArea *area : *areaVector )
				{
					// skip if we've already visited this area
					if ( area->m_nearNavSearchMarker == searchMarker )
						continue;

					// mark as visited
					area->m_nearNavSearchMarker = searchMarker;
					area->GetExtent( &areaExtent );

					if ( extent.IsOverlapping( areaExtent ) )
					{
						if ( func( area ) == false )
							return false;
					}
				}
			}
		}
		return true;
	}

	template < typename Functor >
	bool ForAllAreasInRadius( Functor &func, const Vector &pos, float radius )
	{
		// use a unique marker for this method, so it can be used within a SearchSurroundingArea() call
		static unsigned int searchMarker = RandomInt(0, 1024*1024 );

		++searchMarker;

		if ( searchMarker == 0 )
		{
			++searchMarker;
		}

		// get list in cell that contains position
		int originX = WorldToGridX( pos.x );
		int originY = WorldToGridY( pos.y );
		int shiftLimit = std::ceil( radius / m_gridCellSize );
		float radiusSq = radius * radius;
		if ( radius == 0.0f )
		{
			shiftLimit = std::max( m_gridSizeX, m_gridSizeY );	// range 0 means all areas
		}

		for( int x = originX - shiftLimit; x <= originX + shiftLimit; ++x )
		{
			if ( x < 0 || x >= m_gridSizeX )
				continue;

			for( int y = originY - shiftLimit; y <= originY + shiftLimit; ++y )
			{
				if ( y < 0 || y >= m_gridSizeY )
					continue;

				NavAreaVector *areaVector = &m_grid[ x + y*m_gridSizeX ];

				// find closest area in this cell
				for ( CNavArea *area : *areaVector )
				{
					// skip if we've already visited this area
					if ( area->m_nearNavSearchMarker == searchMarker )
						continue;

					// mark as visited
					area->m_nearNavSearchMarker = searchMarker;

					float distSq = ( area->GetCenter() - pos ).LengthSqr();

					if ( ( distSq <= radiusSq ) || ( radiusSq == 0 ) )
					{
						if ( func( area ) == false )
							return false;
					}
				}
			}
		}
		return true;
	}

	//-------------------------------------------------------------------------------------
	/**
	 * Apply the functor to all navigation ladders.
	 * If functor returns false, stop processing and return false.
	 */
	template < typename Functor >
	bool ForAllLadders( Functor &func )
	{
		for ( int i=0; i<std::ssize( m_ladders ); ++i )
		{
			CNavLadder *ladder = m_ladders[i];

			if (func( ladder ) == false)
				return false;
		}

		return true;
	}

	//-------------------------------------------------------------------------------------
	/**
	 * Apply the functor to all navigation ladders.
	 * If functor returns false, stop processing and return false.
	 */
	template < typename Functor >
	bool ForAllLadders( Functor &func ) const
	{
		for ( int i=0; i<std::ssize( m_ladders ); ++i )
		{
			const CNavLadder *ladder = m_ladders[i];

			if (func( ladder ) == false)
				return false;
		}

		return true;
	}

	//-------------------------------------------------------------------------------------
	/**
	 * tests a new area for connections to adjacent pre-existing areas
	 */
	template < typename Functor > void StitchAreaIntoMesh( CNavArea *area, NavDirType dir, Functor &func );

	//-------------------------------------------------------------------------------------
	/**
	 * Use the functor to test if an area is needing stitching into the existing nav mesh.
	 * The functor is different from how we normally use functors - it does no processing,
	 * and it's return value is true if the area is in the new set to be stiched, and false
	 * if it's a pre-existing area.
	 */
	template < typename Functor >
		bool StitchMesh( Functor &func )
	{
		for ( CNavArea *area : TheNavAreas )
		{
			if ( func( area ) )
			{
				StitchAreaIntoMesh( area, NORTH, func );
				StitchAreaIntoMesh( area, SOUTH, func );
				StitchAreaIntoMesh( area, EAST, func );
				StitchAreaIntoMesh( area, WEST, func );
			}
		}

		return true;
	}

	NavLadderVector& GetLadders( void ) { return m_ladders; }	// Returns the list of ladders
	CNavLadder *GetLadderByID( unsigned int id ) const;

	std::vector< CNavArea * >& GetTransientAreas( void ) { return m_transientAreas; }

	enum EditModeType
	{
		NORMAL,				// normal mesh editing
		PLACE_PAINTING,		// in place painting mode
		CREATING_AREA,		// creating a new nav area
		CREATING_LADDER,	// creating a nav ladder
		DRAG_SELECTING,		// drag selecting a set of areas
		SHIFTING_XY,		// shifting selected set in XY plane
		SHIFTING_Z,			// shifting selected set in Z plane
	};
	EditModeType GetEditMode( void ) const;						// return the current edit mode
	void SetEditMode( EditModeType mode );						// change the edit mode
	bool IsEditMode( EditModeType mode ) const;					// return true if current mode matches given mode

	bool FindNavAreaOrLadderAlongRay( const Vector &start, const Vector &end, CNavArea **area, CNavLadder **ladder, CNavArea *ignore = nullptr );

	void PostProcessCliffAreas();
	void SimplifySelectedAreas( void );	// Simplifies the selected set by reducing to 1x1 areas and re-merging them up with loosened tolerances

protected:
	void PostCustomAnalysis( void ) { }					// invoked when custom analysis step is complete
	bool FindActiveNavArea( void );								// Finds the area or ladder the local player is currently pointing at.  Returns true if a surface was hit by the traceline.
	void RemoveNavArea( CNavArea *area );				// remove an area from the grid
	bool FindGroundForNode( Vector *pos, Vector *normal );
	void GenerateNodes( const Extent &bounds );
	void RemoveNodes( void );

	bool IsMeshVisibilityGenerated( void ) const	{ return false; }	// mesh visibility is not generated (costly to compute and store)

private:
	friend class CNavArea;
	friend class CNavNode;
	friend class CNavUIBasePanel;

	mutable std::vector< NavAreaVector > m_grid;
	float m_gridCellSize;										// the width/height of a grid cell for spatially partitioning nav areas for fast access
	int m_gridSizeX;
	int m_gridSizeY;
	float m_minX;
	float m_minY;
	unsigned int m_areaCount;									// total number of nav areas

	bool m_isLoaded;											// true if a Navigation Mesh has been loaded
	bool m_isOutOfDate;											// true if the Navigation Mesh is older than the actual BSP
	bool m_isAnalyzed;											// true if the Navigation Mesh needs analysis

	enum { HASH_TABLE_SIZE = 256 };
	CNavArea *m_hashTable[ HASH_TABLE_SIZE ];					// hash table to optimize lookup by ID
	int ComputeHashKey( unsigned int id ) const;				// returns a hash key for the given nav area ID

	int WorldToGridX( float wx ) const;							// given X component, return grid index
	int WorldToGridY( float wy ) const;							// given Y component, return grid index
	void AllocateGrid( float minX, float maxX, float minY, float maxY );	// clear and reset the grid to the given extents
	void GridToWorld( int gridX, int gridY, Vector *pos ) const;

	void AddNavArea( CNavArea *area );							// add an area to the grid

	void DestroyNavigationMesh( bool incremental = false );		// free all resources of the mesh and reset it to empty state
	void DestroyHidingSpots( void );

	void ComputeBattlefrontAreas( void );						// determine areas where rushing teams will first meet

	//----------------------------------------------------------------------------------
	// Place directory
	//
	char **m_placeName;											// master directory of place names (ie: "places")
	unsigned int m_placeCount;									// number of "places" defined in placeName[]
	void LoadPlaceDatabase( void );								// load the place names from a file

	//----------------------------------------------------------------------------------
	// Edit mode
	//
	EditModeType m_editMode;									// the current edit mode
	bool m_isEditing;											// true if in edit mode

	unsigned int m_navPlace;									// current navigation place for editing
	void OnEditModeStart( void );								// called when edit mode has just been enabled
	void DrawEditMode( void );									// draw navigation areas
	void OnEditModeEnd( void );									// called when edit mode has just been disabled
	void UpdateDragSelectionSet( void );							// update which areas are overlapping the drag selected bounds
	Vector m_editCursorPos;										// current position of the cursor
	CNavArea *m_markedArea;										// currently marked area for edit operations
	CNavArea *m_selectedArea;									// area that is selected this frame
	CNavArea *m_lastSelectedArea;								// area that was selected last frame
	NavCornerType m_markedCorner;								// currently marked corner for edit operations
	Vector m_anchor;											// first corner of an area being created
	bool m_isPlacePainting;										// if true, we set an area's place by pointing at it
	bool m_splitAlongX;											// direction the selected nav area would be split
	float m_splitEdge;											// location of the possible split

	bool m_climbableSurface;									// if true, the cursor is pointing at a climable surface
	Vector m_surfaceNormal;										// Normal of the surface the cursor is pointing at
	Vector m_ladderAnchor;										// first corner of a ladder being created
	Vector m_ladderNormal;										// Normal of the surface of the ladder being created
	CNavLadder *m_selectedLadder;								// ladder that is selected this frame
	CNavLadder *m_lastSelectedLadder;							// ladder that was selected last frame
	CNavLadder *m_markedLadder;									// currently marked ladder for edit operations

	bool FindLadderCorners( Vector *c1, Vector *c2, Vector *c3 );	// computes the other corners of a ladder given m_ladderAnchor, m_editCursorPos, and m_ladderNormal

	void GetEditVectors( Vector *pos, Vector *forward );		// Gets the eye position and view direction of the editing player

	CountdownTimer m_showAreaInfoTimer;							// Timer that controls how long area info is displayed
	
	NavAreaVector m_selectedSet;								// all currently selected areas
	NavAreaVector m_dragSelectionSet;							// all areas in the current drag selection
	bool m_isContinuouslySelecting;								// if true, we are continuously adding to the selected set
	bool m_isContinuouslyDeselecting;							// if true, we are continuously removing from the selected set

	bool m_bIsDragDeselecting;
	int m_nDragSelectionVolumeZMax;
	int m_nDragSelectionVolumeZMin;

	void DoToggleAttribute( CNavArea *area, NavAttributeType attribute );		// toggle an attribute on given area

	//----------------------------------------------------------------------------------
	// Auto-generation
	//
	bool UpdateGeneration( float maxTime = 0.25f );				// process the auto-generation for 'maxTime' seconds. return false if generation is complete.

	void BeginCustomAnalysis( bool bIncremental ) {}
	void EndCustomAnalysis() {}

	CNavNode *m_currentNode;									// the current node we are sampling from
	NavDirType m_generationDir;
	CNavNode *AddNode( const Vector &destPos, const Vector &destNormal, NavDirType dir, CNavNode *source, bool isOnDisplacement, float obstacleHeight, float flObstacleStartDist, float flObstacleEndDist );		// add a nav node and connect it, update current node

	NavLadderVector m_ladders;									// list of ladder navigation representations
	void BuildLadders( void );
	void DestroyLadders( void );

	bool SampleStep( void );									// sample the walkable areas of the map
	void CreateNavAreasFromNodes( void );						// cover all of the sampled nodes with nav areas

	bool TestArea( CNavNode *node, int width, int height );		// check if an area of size (width, height) can fit, starting from node as upper left corner
	int BuildArea( CNavNode *node, int width, int height );		// create a CNavArea of size (width, height) starting fom node at upper left corner
	bool CheckObstacles( CNavNode *node, int width, int height, int x, int y );

	void MarkPlayerClipAreas( void );
	void MarkJumpAreas( void );
	void StichAndRemoveJumpAreas( void );
	void RemoveJumpAreas( void );	
	void SquareUpAreas( void );
	void MergeGeneratedAreas( void );
	void ConnectGeneratedAreas( void );
	void FixUpGeneratedAreas( void );
	void FixCornerOnCornerAreas( void );
	void FixConnections( void );
	void SplitAreasUnderOverhangs( void );
	void ValidateNavAreaConnections( void );
	void StitchGeneratedAreas( void );							// Stitches incrementally-generated areas into the existing mesh
	void StitchAreaSet( std::vector< CNavArea * > *areas );		// Stitches an arbitrary set of areas into the existing mesh
	void HandleObstacleTopAreas( void );						// Handles fixing/generating areas on top of slim obstacles such as fences and railings
	void RaiseAreasWithInternalObstacles();
	void CreateObstacleTopAreas();
	bool CreateObstacleTopAreaIfNecessary( CNavArea *area, CNavArea *areaOther, NavDirType dir, bool bMultiNode );
	void RemoveOverlappingObstacleTopAreas();	

	enum GenerationStateType
	{
		SAMPLE_WALKABLE_SPACE,
		CREATE_AREAS_FROM_SAMPLES,
		FIND_HIDING_SPOTS,
		FIND_ENCOUNTER_SPOTS,
		FIND_SNIPER_SPOTS,
		FIND_EARLIEST_OCCUPY_TIMES,
		FIND_LIGHT_INTENSITY,
		COMPUTE_MESH_VISIBILITY,
		CUSTOM,													// mod-specific generation step
		SAVE_NAV_MESH,

		NUM_GENERATION_STATES
	}
	m_generationState;											// the state of the generation process
	enum GenerationModeType
	{
		GENERATE_NONE,
		GENERATE_FULL,
		GENERATE_INCREMENTAL,
		GENERATE_SIMPLIFY,
		GENERATE_ANALYSIS_ONLY,
	}
	m_generationMode;											// true while a Navigation Mesh is being generated
	int m_generationIndex;										// used for iterating nav areas during generation process
	int m_sampleTick;											// counter for displaying pseudo-progress while sampling walkable space
	bool m_bQuitWhenFinished;
	float m_generationStartTime;
	Extent m_simplifyGenerationExtent;

	char *m_spawnName;											// name of player spawn entity, used to initiate sampling

	struct WalkableSeedSpot
	{
		Vector pos;
		Vector normal;
	};
	std::vector< WalkableSeedSpot > m_walkableSeeds;				// list of walkable seed spots for sampling

	CNavNode *GetNextWalkableSeedNode( void );					// return the next walkable seed as a node
	int m_seedIdx;
	int m_hostThreadModeRestoreValue;							// stores the value of host_threadmode before we changed it

	void BuildTransientAreaList( void );
	std::vector< CNavArea * > m_transientAreas;

	void UpdateAvoidanceObstacleAreas( void );
	std::vector< CNavArea * > m_avoidanceObstacleAreas;
	std::vector< INavAvoidanceObstacle * > m_avoidanceObstacles;

	void UpdateBlockedAreas( void );
	std::vector< CNavArea * > m_blockedAreas;

	std::vector< int > m_storedSelectedSet;						// "Stored" selected set, so we can do some editing and then restore the old selected set.  Done by ID, so we don't have to worry about split/delete/etc.

	void BeginVisibilityComputations( void );
	void EndVisibilityComputations( void );

	void TestAllAreasForBlockedStatus( void );					// Used to update blocked areas after a round restart. Need to delay so the map logic has all fired.
	CountdownTimer m_updateBlockedAreasTimer;			

	//- Counter-Strike ----------------------------------------------------------------------------------
public:
	void ClearPlayerCounts( void );										///< zero player counts in all areas

	void ResetDMSpawns( void );

private:
	void MaintainChickenPopulation( void );
	int m_desiredChickenCount;
	CountdownTimer m_refreshChickenTimer;
	std::vector< CHandle< CBaseEntity > > m_chickenVector;

	void MaintainDMSpawnPopulation( void );
	int m_desiredDMSpawns;
	int m_consecutiveFailedAttempts;
	CountdownTimer m_refreshDMSpawnTimer;
	std::vector< CHandle< CBaseEntity > > m_DMSpawnVector;

	bool IsSpawnBlockedByTrigger( Vector pos );

	int AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask );
};

// the global singleton interface
extern CNavMesh *TheNavMesh;

// factory for creating the Navigation Mesh
extern CNavMesh *NavMeshFactory( void );

// for debugging the A* algorithm, if nonzero, show debug display and decrement for each pathfind
extern int g_DebugPathfindCounter;

//--------------------------------------------------------------------------------------------------------------
//
// Function prototypes
//

extern void ApproachAreaAnalysisPrep( void );
extern void CleanupApproachAreaAnalysisPrep( void );
extern bool IsHeightDifferenceValid( float test, float other1, float other2, float other3 );

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct animevent_t;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


//=============================================================================================================
// PROP TYPES
//=============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SendProxy_UnmodifiedQAngles( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

class CBaseProp : public CBaseAnimating
{
	DECLARE_CLASS( CBaseProp, CBaseAnimating );
public:

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void Activate_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void CalculateBlockLOS( void );
	int  ParsePropData( void );
	
	void DrawDebugGeometryOverlays_Impl( void );

	// Don't treat as a live target
	virtual bool IsAlive( void ) { return false; }
	virtual bool OverridePropdata() { return true; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBreakableProp : public CBaseProp, public IBreakableWithPropData, public CDefaultPlayerPickupVPhysics
{
	[[= ks::reflect::NetTable{ .name = "DT_BreakableProp" } ]]
	static constexpr bool ks_class = true;

public:
	CBreakableProp();

	DECLARE_CLASS( CBreakableProp, CBaseProp );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void Spawn_Impl();
	void Precache_Impl();
	float GetAutoAimRadius_Impl() { return 24.0f; }
	void UpdateOnRemove_Impl();

	void BreakablePropTouch( CBaseEntity *pOther );

	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Event_Killed_Impl( const CTakeDamageInfo &info );
	void Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info );
	void BreakThink( void );
	void AnimateThink( void );

	virtual void PlayPuntSound();

	[[= ks::reflect::Input{ .name = "Break", .type = FIELD_VOID } ]] void InputBreak( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddHealth", .type = FIELD_INTEGER } ]] void InputAddHealth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RemoveHealth", .type = FIELD_INTEGER } ]] void InputRemoveHealth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHealth", .type = FIELD_INTEGER } ]] void InputSetHealth( inputdata_t &inputdata );

	int	 GetNumBreakableChunks( void ) { return m_iNumBreakableChunks; }

	virtual bool OverridePropdata() { return false; }
	virtual IPhysicsObject *GetRootPhysicsObjectForBreak();

	bool PropDataOverrodeBlockLOS( void ) { return m_bBlockLOSSetByPropData; }
	bool PropDataOverrodeAIWalkable( void ) { return m_bIsWalkableSetByPropData; }

	virtual bool   HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );

	virtual QAngle PreferredCarryAngles( void ) { return m_preferredCarryAngles; }

	virtual void Ignite( float flFlameLifetime, bool bNPCOnly, float flSize = 0.0f, bool bCalledByLevelDesigner = false );

	// Specific interactions
	void	HandleFirstCollisionInteractions( int index, gamevcollisionevent_t *pEvent );
	void	HandleInteractionStick( int index, gamevcollisionevent_t *pEvent );
	void	StickAtPosition( const Vector &stickPosition, const Vector &savePosition, const QAngle &saveAngles );
	
	// Disable auto fading under dx7 or when level fades are specified
	void	DisableAutoFade();

public:
	[[= ks::reflect::Key{ .name = "OnBreak" } ]] COutputEvent	m_OnBreak;
	[[= ks::reflect::Key{ .name = "OnHealthChanged" } ]] COutputFloat	m_OnHealthChanged;
	[[= ks::reflect::Key{ .name = "OnTakeDamage" } ]] COutputEvent	m_OnTakeDamage;

	[[= ks::reflect::Key{ .name = "physdamagescale", .input = true } ]] float			m_impactEnergyScale;

	[[= ks::reflect::Key{ .name = "minhealthdmg" } ]] int				m_iMinHealthDmg;

	QAngle			m_preferredCarryAngles;

public:
// IBreakableWithPropData
	void			SetDmgModBullet( float flDmgMod ) { m_flDmgModBullet = flDmgMod; }
	void			SetDmgModClub( float flDmgMod ) { m_flDmgModClub = flDmgMod; }
	void			SetDmgModExplosive( float flDmgMod ) { m_flDmgModExplosive = flDmgMod; }
	void			SetDmgModFire( float flDmgMod ) { m_flDmgModFire = flDmgMod; }
	float			GetDmgModBullet( void ) { return m_flDmgModBullet; }
	float			GetDmgModClub( void ) { return m_flDmgModClub; }
	float			GetDmgModExplosive( void ) { return m_flDmgModExplosive; }
	float			GetDmgModFire( void ) { return m_flDmgModFire; }
	void			SetExplosiveRadius( float flRadius ) { m_explodeRadius = flRadius; }
	void			SetExplosiveDamage( float flDamage ) { m_explodeDamage = flDamage; }
	float			GetExplosiveRadius( void ) { return m_explodeRadius; }
	float			GetExplosiveDamage( void ) { return m_explodeDamage; }
	void			SetPhysicsDamageTable( string_t iszTableName ) { m_iszPhysicsDamageTableName = iszTableName; }
	string_t		GetPhysicsDamageTable( void ) { return m_iszPhysicsDamageTableName; }
	void			SetBreakableModel( string_t iszModel ) { m_iszBreakableModel = iszModel; }
	string_t 		GetBreakableModel( void ) { return m_iszBreakableModel; }
	void			SetBreakableSkin( int iSkin ) { m_iBreakableSkin = iSkin; }
	int				GetBreakableSkin( void ) { return m_iBreakableSkin; }
	void			SetBreakableCount( int iCount ) { m_iBreakableCount = iCount; }
	int				GetBreakableCount( void ) { return m_iBreakableCount; }
	void			SetMaxBreakableSize( int iSize ) { m_iMaxBreakableSize = iSize; }
	int				GetMaxBreakableSize( void ) { return m_iMaxBreakableSize; }
	void			SetPropDataBlocksLOS( bool bBlocksLOS ) { m_bBlockLOSSetByPropData = true; SetBlocksLOS( bBlocksLOS ); }
	void			SetPropDataIsAIWalkable( bool b ) { m_bIsWalkableSetByPropData = true; SetAIWalkable( b ); }
	void			SetBasePropData( string_t iszBase ) { m_iszBasePropData = iszBase; }
	string_t		GetBasePropData( void ) { return m_iszBasePropData; }
	void			SetInteraction( propdata_interactions_t Interaction ) { m_iInteractions |= (1 << Interaction); }
	void			RemoveInteraction( propdata_interactions_t Interaction ) { m_iInteractions &= ~(1 << Interaction); }
	bool			HasInteraction( propdata_interactions_t Interaction ) { return ( m_iInteractions & (1 << Interaction) ) != 0; }
	void			SetMultiplayerBreakMode( mp_break_t mode ) { m_mpBreakMode = mode; }
	mp_break_t		GetMultiplayerBreakMode( void ) const { return m_mpBreakMode; }

// derived by multiplayer phys props:
	// Base prop_physics can be client-side etc
	virtual void	SetPhysicsMode(int iMode) { m_iPhysicsMode = iMode; }
	virtual int		GetPhysicsMode() { return m_iPhysicsMode; }

	// Copy fade from another breakable prop
	void CopyFadeFrom( CBreakableProp *pSource );

protected:

	bool			UpdateHealth( int iNewHealth, CBaseEntity *pActivator );
	virtual void	OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker ) {}

protected:

	//Base prop_physics can be client-side etc
	int				m_iPhysicsMode;

	unsigned int	m_createTick;
	[[= ks::reflect::Key{ .name = "PressureDelay" } ]] float			m_flPressureDelay;
	EHANDLE			m_hBreaker;

	[[= ks::reflect::Key{ .name = "PerformanceMode" } ]] PerformanceMode_t m_PerformanceMode;

	// Prop data storage
	float			m_flDmgModBullet;
	float			m_flDmgModClub;
	float			m_flDmgModExplosive;
	float			m_flDmgModFire;
	string_t		m_iszPhysicsDamageTableName;
	string_t		m_iszBreakableModel;
	int				m_iBreakableSkin;
	int				m_iBreakableCount;
	int				m_iMaxBreakableSize;
	string_t		m_iszBasePropData;	
	int				m_iInteractions;
	[[= ks::reflect::Key{ .name = "ExplodeDamage" } ]] float			m_explodeDamage;
	[[= ks::reflect::Key{ .name = "ExplodeRadius" } ]] float			m_explodeRadius;

	// Count of how many pieces we'll break into, custom or generic
	int				m_iNumBreakableChunks;

	void			SetEnableMotionPosition( const Vector &position, const QAngle &angles );
	bool			GetEnableMotionPosition( Vector *pPosition, QAngle *pAngles );
	void			ClearEnableMotionPosition();
private:
	CBaseEntity		*FindEnableMotionFixup();

public:
	virtual bool OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	virtual AngularImpulse	PhysGunLaunchAngularImpulse();
	CBasePlayer *HasPhysicsAttacker_Impl( float dt );
	void SetPhysicsAttacker( CBasePlayer *pEntity, float flTime );

protected:
	void CheckRemoveRagdolls();
	
private:
	[[= ks::reflect::Input{ .name = "EnablePhyscannonPickup", .type = FIELD_VOID } ]] void InputEnablePhyscannonPickup( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisablePhyscannonPickup", .type = FIELD_VOID } ]] void InputDisablePhyscannonPickup( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "EnablePuntSound", .type = FIELD_VOID } ]] void InputEnablePuntSound( inputdata_t &inputdata ) { m_bUsePuntSound = true; }
	[[= ks::reflect::Input{ .name = "DisablePuntSound", .type = FIELD_VOID } ]] void InputDisablePuntSound( inputdata_t &inputdata ) { m_bUsePuntSound = false; }

	// Prevents fade scale from happening
	void ForceFadeScaleToAlwaysVisible();
	void RampToDefaultFadeScale();

private:
	enum PhysgunState_t
	{
		PHYSGUN_MUST_BE_DETACHED = 0,
		PHYSGUN_IS_DETACHING,
		PHYSGUN_CAN_BE_GRABBED,					
		PHYSGUN_ANIMATE_ON_PULL,
		PHYSGUN_ANIMATE_IS_ANIMATING,
		PHYSGUN_ANIMATE_FINISHED,
		PHYSGUN_ANIMATE_IS_PRE_ANIMATING,
		PHYSGUN_ANIMATE_IS_POST_ANIMATING,
	};

	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;
	bool					m_bBlockLOSSetByPropData;
	bool					m_bIsWalkableSetByPropData;
	bool					m_bOriginalBlockLOS;	// BlockLOS state before physgun pickup
	char					m_nPhysgunState;		// Ripped-off state
	[[= ks::reflect::Key{ .name = "OnPhysCannonDetach" } ]] COutputEvent			m_OnPhysCannonDetach;	// We've ripped it off!
	[[= ks::reflect::Key{ .name = "OnPhysCannonAnimatePreStarted" } ]] COutputEvent			m_OnPhysCannonAnimatePreStarted;	// Started playing the pre-pull animation
	[[= ks::reflect::Key{ .name = "OnPhysCannonAnimatePullStarted" } ]] COutputEvent			m_OnPhysCannonAnimatePullStarted;	// Player started the pull anim
	[[= ks::reflect::Key{ .name = "OnPhysCannonAnimatePostStarted" } ]] COutputEvent			m_OnPhysCannonAnimatePostStarted;	// Started playing the post-pull animation
	[[= ks::reflect::Key{ .name = "OnPhysCannonPullAnimFinished" } ]] COutputEvent			m_OnPhysCannonPullAnimFinished; // We've had our pull anim finished, or the post-pull has finished if there is one
	float					m_flDefaultFadeScale;	// Things may temporarily change the fade scale, but this is its steady-state condition

	mp_break_t m_mpBreakMode;

	EHANDLE					m_hLastAttacker;		// Last attacker that harmed me.
	EHANDLE					m_hFlareEnt;
	[[= ks::reflect::Key{ .name = "puntsound" } ]] string_t				m_iszPuntSound;
	CNetworkVar( bool, m_noGhostCollision );
	bool					m_bUsePuntSound;
protected:
	CNetworkQAngle( m_qPreferredPlayerCarryAngles, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::QAngles } ]] [[= ks::reflect::Proxy<SendProxy_UnmodifiedQAngles, ks::reflect::WireSide::Send>{} ]] );
	CNetworkVar( bool, m_bClientPhysics, [[= ks::reflect::Net{} ]] );
};

// Spawnflags
inline constexpr auto SF_DYNAMICPROP_USEHITBOX_FOR_RENDERBOX = 64;
inline constexpr auto SF_DYNAMICPROP_NO_VPHYSICS = 128;
inline constexpr auto SF_DYNAMICPROP_DISABLE_COLLISION = 256;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDynamicProp : public CBreakableProp, public IPositionWatcher
{
	[[= ks::reflect::NetTable{ .name = "DT_DynamicProp" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CDynamicProp, CBreakableProp );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CDynamicProp();

	void	Spawn_Impl( void );
	bool	CreateVPhysics_Impl( void );
	void	CreateBoneFollowers();
	void	UpdateOnRemove_Impl( void );
	void	AnimThink( void );
	void	PropSetSequence( int nSequence );
	void	OnRestore_Impl( void );
	bool	OverridePropdata( void );
	virtual void	HandleAnimEvent( animevent_t *pEvent );
	int		ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );

	// baseentity - watch dynamic hierarchy updates
	void	SetParent_Impl( CBaseEntity* pNewParent, int iAttachment = -1 );
	bool			TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );

	// breakable prop
	virtual IPhysicsObject *GetRootPhysicsObjectForBreak();

	// IPositionWatcher
	virtual void NotifyPositionChanged( CBaseEntity *pEntity );

	inline void SetGlowColor( int r, int g, int b );
	inline const color24 GetGlowColor() const;

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetAnimation", .type = FIELD_STRING } ]] void InputSetAnimation( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetAnimationNoReset", .type = FIELD_STRING } ]] void InputSetAnimationNoReset( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetDefaultAnimation", .type = FIELD_STRING } ]] void InputSetDefaultAnimation( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]]
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]]
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableCollision", .type = FIELD_VOID } ]] void InputDisableCollision( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableCollision", .type = FIELD_VOID } ]] void InputEnableCollision( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetPlaybackRate", .type = FIELD_FLOAT } ]] void InputSetPlaybackRate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BecomeRagdoll", .type = FIELD_VOID } ]] void InputBecomeRagdoll( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeAndKill", .type = FIELD_VOID } ]] void InputFadeAndKill( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetGlowEnabled", .type = FIELD_VOID } ]] void InputSetGlowEnabled( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetGlowDisabled", .type = FIELD_VOID } ]] void InputSetGlowDisabled( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetGlowColor", .type = FIELD_COLOR32 } ]] void InputSetGlowColor( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GlowColorRedValue", .type = FIELD_FLOAT } ]] void InputGlowColorRedValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GlowColorGreenValue", .type = FIELD_FLOAT } ]] void InputGlowColorGreenValue( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "GlowColorBlueValue", .type = FIELD_FLOAT } ]] void InputGlowColorBlueValue( inputdata_t &inputdata );

	void UpdateBoneFollowers( void );

	[[= ks::reflect::Key{ .name = "OnAnimationBegun" } ]] COutputEvent		m_pOutputAnimBegun;
	[[= ks::reflect::Key{ .name = "OnAnimationDone" } ]] COutputEvent		m_pOutputAnimOver;

	[[= ks::reflect::Key{ .name = "DefaultAnim" } ]] string_t			m_iszDefaultAnim;

	int					m_iGoalSequence;
	int					m_iTransitionDirection;

	// Random animations
	bool				m_bAnimationDone;
	[[= ks::reflect::Key{ .name = "HoldAnimation" } ]] bool				m_bHoldAnimation;
	[[= ks::reflect::Key{ .name = "RandomAnimation" } ]] bool				m_bRandomAnimator;
	[[= ks::reflect::Key{ .name = "DisableBoneFollowers" } ]] bool				m_bDisableBoneFollowers;
	float				m_flNextRandAnim;
	[[= ks::reflect::Key{ .name = "MinAnimTime" } ]] float				m_flMinRandAnimTime;
	[[= ks::reflect::Key{ .name = "MaxAnimTime" } ]] float				m_flMaxRandAnimTime;
	short				m_nPendingSequence;

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool				m_bStartDisabled;
	[[= ks::reflect::Key{ .name = "AnimateEveryFrame" } ]] bool				m_bAnimateEveryFrame;

	CNetworkVar( bool, m_bUseHitboxesForRenderBox, [[= ks::reflect::Net{} ]] );

	CNetworkVar( float, m_flGlowMaxDist, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "glowdist" } ]] );
	CNetworkVar( bool, m_bShouldGlow, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "glowenabled" } ]] );
	CNetworkColor32( m_clrGlow, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "glowcolor" } ]] );
	CNetworkVar( int, m_nGlowStyle, [[= ks::reflect::Net{ .bits = -1 } ]] [[= ks::reflect::Key{ .name = "glowstyle" } ]] );

protected:
	void FinishSetSequence( int nSequence );
	void PropSetAnim( const char *szAnim );
	bool ShouldSetCreateTime( inputdata_t &inputdata );
	void BoneFollowerHierarchyChanged();

	// Contained Bone Follower manager
	CBoneFollowerManager	m_BoneFollowerManager;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPhysicsProp : public CBreakableProp, public INavAvoidanceObstacle
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysicsProp" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_flPoseParameter" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_flPlaybackRate" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nMuzzleFlashParity" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimatingOverlay", .prop = "overlay_vars" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseFlex", .prop = "m_flexWeight" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseFlex", .prop = "m_blinktoggle" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPhysicsProp, CBreakableProp );
	DECLARE_SERVERCLASS();

public:
	~CPhysicsProp();
	CPhysicsProp( void );

	void Spawn_Impl( void );
	void Precache_Impl();
	bool CreateVPhysics_Impl( void );
	bool OverridePropdata( void );

	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );

	[[= ks::reflect::Input{ .name = "Wake", .type = FIELD_VOID } ]] void InputWake( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Sleep", .type = FIELD_VOID } ]] void InputSleep( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableMotion", .type = FIELD_VOID } ]] void InputEnableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableMotion", .type = FIELD_VOID } ]] void InputDisableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableFloating", .type = FIELD_VOID } ]] void InputDisableFloating( inputdata_t &inputdata );

	void EnableMotion( void );
	bool CanBePickedUpByPhyscannon( void );
	void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );

	bool GetPropDataAngles( const char *pKeyName, QAngle &vecAngles );
	float GetCarryDistanceOffset( void );

	int ObjectCaps_Impl();
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool ShouldDisableMotionOnFreeze( void );

	void GetMassCenter( Vector *pMassCenter );
	float GetMass() const;

	int ExploitableByPlayer() const { return m_iExploitableByPlayer; }

	void ClearFlagsThink( void );

	bool IsPotentiallyAbleToObstructNavAreas( void ) const;	// could we at some future time obstruct nav?
	float GetNavObstructionHeight( void ) const;			// height at which to obstruct nav areas
	bool CanObstructNavAreas( void ) const;					// can we obstruct nav right this instant?
	CBaseEntity *GetObstructingEntity( void ) { return this; }
	void OnNavMeshLoaded( void );
	void NavThink( void );

	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	int DrawDebugTextOverlays_Impl(void);
	bool IsGib();
	DECLARE_DATADESC();

	// Specific interactions
	void	HandleAnyCollisionInteractions( int index, gamevcollisionevent_t *pEvent );

private:
	// Compute impulse to apply to the enabled entity.
	void ComputeEnablingImpulse( int index, gamevcollisionevent_t *pEvent );

	[[= ks::reflect::Key{ .name = "OnMotionEnabled" } ]] COutputEvent m_MotionEnabled;
	[[= ks::reflect::Key{ .name = "OnAwakened" } ]] COutputEvent m_OnAwakened;
	[[= ks::reflect::Key{ .name = "OnPhysGunPickup" } ]] COutputEvent m_OnPhysGunPickup;
	[[= ks::reflect::Key{ .name = "OnPhysGunPunt" } ]] COutputEvent m_OnPhysGunPunt;
	[[= ks::reflect::Key{ .name = "OnPhysGunOnlyPickup" } ]] COutputEvent m_OnPhysGunOnlyPickup;
	[[= ks::reflect::Key{ .name = "OnPhysGunDrop" } ]] COutputEvent m_OnPhysGunDrop;
	[[= ks::reflect::Key{ .name = "OnPlayerUse" } ]] COutputEvent m_OnPlayerUse;
	[[= ks::reflect::Key{ .name = "OnPlayerPickup" } ]] COutputEvent m_OnPlayerPickup;
	[[= ks::reflect::Key{ .name = "OnOutOfWorld" } ]] COutputEvent m_OnOutOfWorld;

	[[= ks::reflect::Key{ .name = "massscale" } ]] float		m_massScale;
	[[= ks::reflect::Key{ .name = "inertiascale" } ]] float		m_inertiaScale;
	[[= ks::reflect::Key{ .name = "Damagetype" } ]] int			m_damageType;
	[[= ks::reflect::Key{ .name = "overridescript" } ]] string_t	m_iszOverrideScript;
	[[= ks::reflect::Key{ .name = "damagetoenablemotion" } ]] int			m_damageToEnableMotion;
	[[= ks::reflect::Key{ .name = "forcetoenablemotion" } ]] float		m_flForceToEnableMotion;

	bool		m_bThrownByPlayer;
	bool		m_bFirstCollisionAfterLaunch;
	[[= ks::reflect::Key{ .name = "ExploitableByPlayer" } ]] int			m_iExploitableByPlayer;
	bool		m_bHasBeenAwakened;
	float		m_fNextCheckDisableMotionContactsTime;

protected:
	CNetworkVar( bool, m_bAwake, [[= ks::reflect::Net{} ]] );

};

//--------------------------------------------------------------------------------------------------------
class CCSPropExplodingBarrel : public CPhysicsProp
{
	DECLARE_CLASS( CCSPropExplodingBarrel, CPhysicsProp );

public:

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void FadeOut( void );
	void StopParticle( void );
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Event_Killed_Impl( const CTakeDamageInfo &info );
	int DrawDebugTextOverlays_Impl( void );
	bool IsBreakable( void ) { return true; }

	EHANDLE m_hBarrelTop;

private:
	bool m_bExploded;

};

// An interface so that objects parented to props can receive collision interaction events.
enum parentCollisionInteraction_t
{
	COLLISIONINTER_PARENT_FIRST_IMPACT = 1,
};

class IParentPropInteraction
{
public:
	virtual void OnParentCollisionInteraction( parentCollisionInteraction_t eType, int index, gamevcollisionevent_t *pEvent ) = 0;
	virtual void OnParentPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason ) = 0;
};

// Used by prop_physics_create and the server benchmark.
// pModelName should not include the "models/" prefix.
CPhysicsProp* CreatePhysicsProp( const char *pModelName, const Vector &vTraceStart, const Vector &vTraceEnd, const IHandleEntity *pTraceIgnore, bool bRequireVCollide, const char *pClassName="physics_prop" );

float GetBreakableDamage( const CTakeDamageInfo &inputInfo, IBreakableWithPropData *pProp = nullptr );
int PropBreakablePrecacheAll( string_t modelName );

extern ConVar func_breakdmg_bullet;
extern ConVar func_breakdmg_club;
extern ConVar func_breakdmg_explosive;

class CBaseEntity;
class CEntityBlocker;
class CNavArea;
class CTakeDamageInfo;
struct Extent;
struct animevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


// Purpose: A base class for model-based doors. The exact movement required to
//			open or close the door is not dictated by this class, only that
//			the door has open, closed, opening, and closing states.
//			Doors must satisfy these requirements:
//			- Derived classes must support being opened by NPCs.
//			- Never autoclose in the face of a player.
//			- Never close into an NPC.

extern ConVar g_debug_doors;

struct opendata_t
{
	Vector vecStandPos;		// Where the NPC should stand.
	Vector vecFaceDir;		// What direction the NPC should face.
	Activity eActivity;		// What activity the NPC should play.
};

class CBasePropDoor : public CDynamicProp
{
	[[= ks::reflect::NetTable{ .name = "DT_BasePropDoor" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_flPoseParameter" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_flPlaybackRate" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimating", .prop = "m_nMuzzleFlashParity" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseAnimatingOverlay", .prop = "overlay_vars" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseFlex", .prop = "m_flexWeight" } ]]
	[[= ks::reflect::Exclude{ .table = "DT_BaseFlex", .prop = "m_blinktoggle" } ]]
	[[= ks::reflect::KeyFrom<"m_ls.sLockedSound", ks::reflect::Key{ .name = "soundlockedoverride", .as = FIELD_SOUNDNAME } >{} ]]
	[[= ks::reflect::KeyFrom<"m_ls.sUnlockedSound", ks::reflect::Key{ .name = "soundunlockedoverride", .as = FIELD_SOUNDNAME } >{} ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CBasePropDoor, CDynamicProp );
	DECLARE_SERVERCLASS();

	CBasePropDoor( void );

	void Spawn_Impl();
	void Precache_Impl();
	void Activate_Impl();
	int	ObjectCaps_Impl();

	virtual bool IsAbleToCloseAreaPortals( void ) const;

	void HandleAnimEvent( animevent_t *pEvent );

	// Base class services.
	// Do not make the functions in this block virtual!!
	// {
	inline bool IsDoorOpen();
	inline bool IsDoorAjar();
	inline bool IsDoorOpening();
	inline bool IsDoorClosed();
	inline bool IsDoorClosing();	
	inline bool IsDoorBlocked() const;
	inline bool IsPlayerOpening();
	inline bool IsOpener(CBaseEntity *pEnt);

	virtual bool IsDoorLocked() { return m_bLocked; }

	bool TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );
	// }

	// Implement these in your leaf class.
	// {
	virtual bool DoorCanClose( bool bAutoClose ) { return true; }
	virtual bool DoorCanOpen( void ) { return true; }

	virtual float GetOpenInterval(void) = 0;

	enum DoorExtent_t
	{
		DOOR_EXTENT_OPEN = 1,
		DOOR_EXTENT_CLOSED = 2,
	};
	virtual void ComputeDoorExtent( Extent *extent, unsigned int extentType ) = 0;	// extent contains the volume encompassing by the door in the specified states
	// }

protected:

	enum DoorState_t
	{
		DOOR_STATE_CLOSED = 0,
		DOOR_STATE_OPENING,
		DOOR_STATE_OPEN,
		DOOR_STATE_CLOSING,
		DOOR_STATE_AJAR,
	};

	// dvs: FIXME: make these private
	void DoorClose();

	CBasePropDoor *GetMaster( void ) { return m_hMaster; }
	bool HasSlaves( void ) { return ( !m_hDoorList.empty() ); }

	inline void SetDoorState( DoorState_t eDoorState );

	virtual void CalcDoorSounds();

	[[= ks::reflect::Key{ .name = "returndelay" } ]] float m_flAutoReturnDelay;	// How many seconds to wait before automatically closing, -1 never closes automatically.
	std::vector< CHandle< CBasePropDoor > >	m_hDoorList;	// List of doors linked to us

	inline CBaseEntity *GetActivator();

	[[= ks::reflect::Key{ .name = "hardware" } ]] int		m_nHardwareType;

	// Called when the door becomes fully closed.
	virtual void OnDoorClosed() {}
private:

	// Implement these in your leaf class.
	// {
	// Called when the door becomes fully open.
	virtual void OnDoorOpened() {}

	// Called to tell the door to start opening.			
	virtual void BeginOpening(CBaseEntity *pOpenAwayFrom) = 0;

	// Called to tell the door to start closing.
	virtual void BeginClosing( void ) = 0;

	// Called when blocked to tell the door to stop moving.
	virtual void DoorStop( void ) = 0;

	// Called when blocked to tell the door to continue moving.
	virtual void DoorResume( void ) = 0;
	
	// Called to send the door instantly to its spawn positions.
	virtual void DoorTeleportToSpawnPosition() = 0;
	// }

protected:
	void UpdateAreaPortals( bool bOpen );
	void DisableAreaPortalThink( void );
	virtual void Lock();
	virtual void Unlock();

private:

	// Main entry points for the door base behaviors.
	// Do not make the functions in this block virtual!!
	// {
	bool DoorActivate();
	void DoorOpen( CBaseEntity *pOpenAwayFrom );
	void OpenIfUnlocked(CBaseEntity *pActivator, CBaseEntity *pOpenAwayFrom);

	void DoorOpenMoveDone();
	void DoorCloseMoveDone();
	void DoorAutoCloseThink();

	public: void Use_Impl(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	private:
	void OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	inline bool WillAutoReturn() { return m_flAutoReturnDelay != -1; }

	public: void StartBlocked_Impl(CBaseEntity *pOther);
	private:
	void OnStartBlocked( CBaseEntity *pOther );
	void MasterStartBlocked( CBaseEntity *pOther );

	public: void Blocked_Impl(CBaseEntity *pOther);
	private:
	public: void EndBlocked_Impl(void);
	private:
	void OnEndBlocked( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] void InputClose(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Lock", .type = FIELD_VOID } ]] void InputLock(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] void InputOpen(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "OpenAwayFrom", .type = FIELD_STRING } ]] void InputOpenAwayFrom(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &inputdata);
	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock(inputdata_t &inputdata);

	void SetDoorBlocker( CBaseEntity *pBlocker );

	void SetMaster( CBasePropDoor *pMaster ) { m_hMaster = pMaster; }
	
	DoorState_t m_eDoorState;	// Holds whether the door is open, closed, opening, or closing.

	locksound_t m_ls;			// The sounds the door plays when being locked, unlocked, etc.
	EHANDLE		m_hActivator;		
	
	EHANDLE	m_hBlocker;				// Entity blocking the door currently
	bool	m_bFirstBlocked;		// Marker for being the first door (in a group) to be blocked (needed for motion control)

protected:
	bool	m_bLocked;				// True if the door is locked.
	[[= ks::reflect::Key{ .name = "forceclosed" } ]] bool m_bForceClosed;			// True if this door must close no matter what.

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "soundmoveoverride" } ]] string_t m_SoundMoving;
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "soundopenoverride" } ]] string_t m_SoundOpen;
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "soundcloseoverride" } ]] string_t m_SoundClose;

	int m_nPhysicsMaterial;

	// dvs: FIXME: can we remove m_flSpeed from CBaseEntity?
	//float m_flSpeed;			// Rotation speed when opening or closing in degrees per second.

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "slavename" } ]] string_t m_SlaveName;

	CHandle< CBasePropDoor > m_hMaster;

	static void RegisterPrivateActivities();

	// Outputs
	[[= ks::reflect::Key{ .name = "OnBlockedClosing" } ]] COutputEvent m_OnBlockedClosing;		// Triggered when the door becomes blocked while closing.
	[[= ks::reflect::Key{ .name = "OnBlockedOpening" } ]] COutputEvent m_OnBlockedOpening;		// Triggered when the door becomes blocked while opening.
	[[= ks::reflect::Key{ .name = "OnUnblockedClosing" } ]] COutputEvent m_OnUnblockedClosing;		// Triggered when the door becomes unblocked while closing.
	[[= ks::reflect::Key{ .name = "OnUnblockedOpening" } ]] COutputEvent m_OnUnblockedOpening;		// Triggered when the door becomes unblocked while opening.
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;			// Triggered when the door reaches the fully closed position.
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;				// Triggered when the door reaches the fully open position.
	[[= ks::reflect::Key{ .name = "OnClose" } ]] COutputEvent m_OnClose;					// Triggered when the door is told to close.
	[[= ks::reflect::Key{ .name = "OnOpen" } ]] COutputEvent m_OnOpen;					// Triggered when the door is told to open.
	[[= ks::reflect::Key{ .name = "OnLockedUse" } ]] COutputEvent m_OnLockedUse;				// Triggered when the user tries to open a locked door.
};

void CBasePropDoor::SetDoorState( DoorState_t eDoorState )
{
	m_eDoorState = eDoorState;
}

bool CBasePropDoor::IsDoorOpen()
{
	return m_eDoorState == DOOR_STATE_OPEN;
}

bool CBasePropDoor::IsDoorAjar()
{
	return ( m_eDoorState == DOOR_STATE_AJAR );
}

bool CBasePropDoor::IsDoorOpening()
{
	return m_eDoorState == DOOR_STATE_OPENING;
}

bool CBasePropDoor::IsDoorClosed()
{
	return m_eDoorState == DOOR_STATE_CLOSED;
}

bool CBasePropDoor::IsDoorClosing()
{
	return m_eDoorState == DOOR_STATE_CLOSING;
}

CBaseEntity *CBasePropDoor::GetActivator()
{
	return m_hActivator;
}

bool CBasePropDoor::IsDoorBlocked() const
{
	return ( m_hBlocker != nullptr );
}



bool CBasePropDoor::IsPlayerOpening()
{
	return ( GetActivator() && GetActivator()->IsPlayer() );
}

bool CBasePropDoor::IsOpener(CBaseEntity *pEnt)
{
	return ( GetActivator() == pEnt );
}

//===============================================
// Rotating prop door
//===============================================
// Check directions for door movement
enum doorCheck_e
{
	DOOR_CHECK_FORWARD,		// Door's forward opening direction
	DOOR_CHECK_BACKWARD,	// Door's backward opening direction
	DOOR_CHECK_FULL,		// Door's complete movement volume
};

enum PropDoorRotatingSpawnPos_t
{
	DOOR_SPAWN_CLOSED = 0,
	DOOR_SPAWN_OPEN_FORWARD,
	DOOR_SPAWN_OPEN_BACK,
	DOOR_SPAWN_AJAR,
};

enum PropDoorRotatingOpenDirection_e
{
	DOOR_ROTATING_OPEN_BOTH_WAYS = 0,
	DOOR_ROTATING_OPEN_FORWARD,
	DOOR_ROTATING_OPEN_BACKWARD,
};
class CPropDoorRotating : public CBasePropDoor
{
	[[= ks::reflect::NetTable{ .name = "DT_PropDoorRotating" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPropDoorRotating, CBasePropDoor );

	public:

	~CPropDoorRotating();

	int		DrawDebugTextOverlays_Impl( void );

	void	Spawn_Impl( void );
	void	MoveDone_Impl( void );
	void	BeginOpening( CBaseEntity *pOpenAwayFrom );
	void	BeginClosing( void );
	void	OnRestore_Impl( void );

	void	DoorTeleportToSpawnPosition();


	void	DoorClose( void );
	bool	DoorCanClose( bool bAutoClose );
	void	DoorOpen( CBaseEntity *pOpenAwayFrom );

	void	OnDoorOpened();
	void	OnDoorClosed();

	void	DoorResume( void );
	void	DoorStop( void );

	float	GetOpenInterval();

	bool	OverridePropdata() { return true; }

	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void	InputSetSpeed( inputdata_t &inputdata );

	virtual void ComputeDoorExtent( Extent *extent, unsigned int extentType );	// extent contains the volume encompassing open + closed states

	int UpdateTransmitState_Impl() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	private:

	bool	IsHingeOnLeft();

	void	AngularMove( const QAngle &vecDestAngle, float flSpeed );
	void	CalculateDoorVolume( QAngle closedAngles, QAngle openAngles, Vector *destMins, Vector *destMaxs );

	bool	CheckDoorClear( doorCheck_e state );

	doorCheck_e	GetOpenState( void );

	[[= ks::reflect::Input{ .name = "SetRotationDistance", .type = FIELD_FLOAT } ]] void	InputSetRotationDistance( inputdata_t &inputdata );			// Set the degree difference between open and closed
	[[= ks::reflect::Input{ .name = "MoveToRotationDistance", .type = FIELD_FLOAT } ]] void	InputMoveToRotationDistance( inputdata_t &inputdata );			// Set the degree difference between open and closed and move to open

	void	CalcOpenAngles( void );		// Subroutine to setup the m_angRotation QAngles based on the m_flDistance variable

	[[= ks::reflect::Key{ .name = "axis" } ]] Vector	m_vecAxis;					// The axis of rotation.
	[[= ks::reflect::Key{ .name = "distance" } ]] float	m_flDistance;				// How many degrees we rotate between open and closed.

	[[= ks::reflect::Key{ .name = "spawnpos" } ]] PropDoorRotatingSpawnPos_t m_eSpawnPosition;
	[[= ks::reflect::Key{ .name = "opendir" } ]] PropDoorRotatingOpenDirection_e m_eOpenDirection;

	[[= ks::reflect::Key{ .name = "ajarangles" } ]] QAngle	m_angRotationAjar;			// Angles to spawn at if we are set to spawn ajar.
	QAngle	m_angRotationClosed;		// Our angles when we are fully closed.
	QAngle	m_angRotationOpenForward;	// Our angles when we are fully open towards our forward vector.
	QAngle	m_angRotationOpenBack;		// Our angles when we are fully open away from our forward vector.

	QAngle	m_angGoal;

	Vector	m_vecForwardBoundsMin;
	Vector	m_vecForwardBoundsMax;
	Vector	m_vecBackBoundsMin;
	Vector	m_vecBackBoundsMax;

	[[= ks::reflect::Key{ .name = "OnRotationDone" } ]] COutputEvent m_OnRotationDone;		// Triggered when we finish rotating.

	CHandle<CEntityBlocker>	m_hDoorBlocker;
};

//--------------------------------------------------------------------------------------------------------
class CPropDoorRotatingBreakable : public CPropDoorRotating
{
	DECLARE_CLASS( CPropDoorRotatingBreakable, CPropDoorRotating );

	public:

	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void UpdateOnRemove_Impl( void );
	void PrecacheBreakables( void );
	int OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void Event_Killed_Impl( const CTakeDamageInfo &info );
	void InputSetRotationDistance( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetUnbreakable", .type = FIELD_VOID } ]] void InputSetUnbreakable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBreakable", .type = FIELD_VOID } ]] void InputSetBreakable( inputdata_t &inputdata );
	virtual bool IsAbleToCloseAreaPortals( void ) const;
	int DrawDebugTextOverlays_Impl( void );
	bool IsBreakable( void ) { return m_bBreakable; }

	virtual void Lock();
	virtual void Unlock();

	virtual void OnDoorOpened( void )
	{
		UnblockNav();
		BaseClass::OnDoorOpened();
	}

	virtual void OnDoorClosed( void )
	{
		BaseClass::OnDoorClosed();
	}

	bool operator()( CNavArea *area );	// functor that blocks areas in our extent

	static bool CalculateBlocked( bool *pResultByTeam, const Vector &vecMins, const Vector &vecMaxs );

	private:

	void UpdateBlocked( bool bBlocked );
	void BlockNav( void );
	void UnblockNav( void );

	// 	void BlockNavArea( bool blocked )
	// 	{
	// 		/**
	// 		 * MSB: I'm commenting this out, because we can't use BLOCKED for this,
	// 		 * since *nothing* can path thru a blocked area - SurvivorBots, population
	// 		 * algorithms, etc.
	// 		 * However, something like "closed door" might be useful to flag here
	// 		 * in the future.
	// 		 *
	// 		 
	// 		if ( blocked )
	// 		{
	// 			CNavArea *area = TheNavMesh->GetNavArea( WorldSpaceCenter() );
	// 			if ( area )
	// 			{
	// 				area->Block();
	// 				m_blockedNavAreaID = area->GetID();
	// 			}
	// 		}
	// 		else
	// 		{
	// 			if ( m_blockedNavAreaID > 0 )
	// 			{
	// 				CNavArea *area = TheNavMesh->GetNavAreaByID( m_blockedNavAreaID );
	// 				if ( area )
	// 				{
	// 					area->UpdateBlocked( true ); // give it a chance to stay blocked by something else
	// 				}
	// 			}
	// 		}
	// 		*/
	// 	}

	int m_blockedNavAreaID;

	bool m_bBreakable;
	bool m_isAbleToCloseAreaPortals;
	int m_currentDamageState;
	int m_blockedTeamNumber;
	bool m_isBlockingNav[MAX_NAV_TEAMS];
	std::vector< string_t > m_damageStates;
};

class CBaseAnimating;
class CTakeDamageInfo;

class C_BaseAnimating;

enum
{
	DEATH_FRAME_HEAD = 1,
	DEATH_FRAME_STOMACH,
	DEATH_FRAME_LEFTARM,
	DEATH_FRAME_RIGHTARM,
	DEATH_FRAME_LEFTLEG,
	DEATH_FRAME_RIGHTLEG,
	MAX_DEATHPOSE_FRAMES  = DEATH_FRAME_RIGHTLEG
};

/// Calculates death pose activity and frame
void SelectDeathPoseActivityAndFrame( CBaseAnimating *entity, const CTakeDamageInfo &info, int hitgroup, Activity& activity, int& frame );

class CBaseEntity;


//=============================================================================
// NDebugOverlay
//=============================================================================
namespace NDebugOverlay
{
	void	Box(const Vector &origin, const Vector &mins, const Vector &maxs, int r, int g, int b, int a, float flDuration);
	void	BoxDirection(const Vector &origin, const Vector &mins, const Vector &maxs, const Vector &forward, int r, int g, int b, int a, float flDuration);
	void	BoxAngles(const Vector &origin, const Vector &mins, const Vector &maxs, const QAngle &angles, int r, int g, int b, int a, float flDuration);
	void	SweptBox(const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, const QAngle & angles, int r, int g, int b, int a, float flDuration);
	void	EntityBounds( const CBaseEntity *pEntity, int r, int g, int b, int a, float flDuration );
	void	Line( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Triangle( const Vector &p1, const Vector &p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float duration );
	void	EntityText( int entityID, int text_offset, const char *text, float flDuration, int r = 255, int g = 255, int b = 255, int a = 255);
	void	EntityTextAtPosition( const Vector &origin, int text_offset, const char *text, float flDuration, int r = 255, int g = 255, int b = 255, int a = 255);
	void	Grid( const Vector &vPosition );
	void	Text( const Vector &origin, const char *text, bool bViewCheck, float flDuration );
	void	ScreenText( float fXpos, float fYpos, const char *text, int r, int g, int b, int a, float flDuration);
	void	Cross3D(const Vector &position, const Vector &mins, const Vector &maxs, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Cross3D(const Vector &position, float size, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Cross3DOriented( const Vector &position, const QAngle &angles, float size, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Cross3DOriented( const matrix3x4_t &m, float size, int c, bool noDepthTest, float flDuration );
	void	DrawOverlayLines(void);
	void	DrawTickMarkedLine(const Vector &startPos, const Vector &endPos, float tickDist, int tickTextDist, int r, int g, int b, bool noDepthTest, float flDuration );
	void	DrawGroundCrossHairOverlay();
	void	HorzArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void	YawArrow( const Vector &startPos, float yaw, float length, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void	VertArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration);
	void	Axis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration );
	void	Sphere( const Vector &center, float radius, int r, int g, int b, bool noDepthTest, float flDuration );
	void	Circle( const Vector &position, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Circle( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Circle( const Vector &position, const Vector &xAxis, const Vector &yAxis, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Sphere( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Cone( const Vector &position, const Vector &axis, float angleRadians, float length, int r, int g, int b, int a, bool bNoDepthTest, float flDuration );
	void	Cross( const Vector &position, float radius, int r, int g, int b, bool bNoDepthTest, float flDuration );
	void	Capsule(const Vector &vStart, const Vector &vEnd, const float &flRadius, int r, int g, int b, int a, float flDuration);
	void	PurgeTextOverlays();
};

// NOTE: If you add a tex type, be sure to modify the s_pImpactEffect
// array in fx_impact.cpp to get an effect when that surface is shot.
inline constexpr auto CHAR_TEX_ANTLION = 'A';
inline constexpr auto CHAR_TEX_BLOODYFLESH = 'B';
inline constexpr auto CHAR_TEX_CONCRETE = 'C';
inline constexpr auto CHAR_TEX_DIRT = 'D';
inline constexpr auto CHAR_TEX_EGGSHELL = 'E'; ///< the egg sacs in the tunnels in ep2.
inline constexpr auto CHAR_TEX_FLESH = 'F';
inline constexpr auto CHAR_TEX_GRATE = 'G';
inline constexpr auto CHAR_TEX_ALIENFLESH = 'H';
inline constexpr auto CHAR_TEX_CLIP = 'I';
inline constexpr auto CHAR_TEX_GRASS = 'J'; // L4D addition
inline constexpr auto CHAR_TEX_SNOW = 'K';
inline constexpr auto CHAR_TEX_PLASTIC = 'L';
inline constexpr auto CHAR_TEX_METAL = 'M';
inline constexpr auto CHAR_TEX_SAND = 'N';
inline constexpr auto CHAR_TEX_FOLIAGE = 'O';
inline constexpr auto CHAR_TEX_COMPUTER = 'P';
inline constexpr auto CHAR_TEX_ASPHALT = 'Q'; // L4D addition
inline constexpr auto CHAR_TEX_BRICK = 'R'; // L4D addition
inline constexpr auto CHAR_TEX_SLOSH = 'S';
inline constexpr auto CHAR_TEX_TILE = 'T';
inline constexpr auto CHAR_TEX_CARDBOARD = 'U'; // L4D addition
inline constexpr auto CHAR_TEX_VENT = 'V';
inline constexpr auto CHAR_TEX_WOOD = 'W';
//#define CHAR_TEX_UNUSED		'X' ///< do not use - "fake" materials use this (ladders, wading, clips, etc)
inline constexpr auto CHAR_TEX_GLASS = 'Y';
inline constexpr auto CHAR_TEX_WARPSHIELD = 'Z'; ///< wierd-looking jello effect for advisor shield.

inline constexpr auto FIRST_CHAR_TEX = CHAR_TEX_ANTLION;
inline constexpr auto LAST_CHAR_TEX = CHAR_TEX_WARPSHIELD;

inline constexpr auto CHAR_TEX_CLAY = 1; // L4D addition
inline constexpr auto CHAR_TEX_PLASTER = 2; // L4D addition
inline constexpr auto CHAR_TEX_ROCK = 3; // L4D addition
inline constexpr auto CHAR_TEX_RUBBER = 4; // L4D addition
inline constexpr auto CHAR_TEX_SHEETROCK = 5; // L4D addition
inline constexpr auto CHAR_TEX_CLOTH = 6; // L4D addition
inline constexpr auto CHAR_TEX_CARPET = 7; // L4D addition
inline constexpr auto CHAR_TEX_PAPER = 8; // L4D addition
inline constexpr auto CHAR_TEX_UPHOLSTERY = 9; // L4D addition
inline constexpr auto CHAR_TEX_PUDDLE = 10; // L4D addition
inline constexpr auto CHAR_TEX_MUD = 11; // L4D addition

inline constexpr auto CHAR_TEX_SANDBARREL = 12; // CSGO addition (no penetration) - sand filled barrel

inline constexpr auto FIRST_L4D_CHAR_TEX = CHAR_TEX_CLAY;
inline constexpr auto LAST_L4D_CHAR_TEX = CHAR_TEX_SANDBARREL;

class IDecalEmitterSystem
{
public:
	virtual int	GetDecalIndexForName( char const *decalname ) = 0;
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial ) = 0;
};

extern IDecalEmitterSystem *decalsystem;

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;


//Since I'm here, might as well explain how these work.  Base.fgd is the file that connects
//flags to entities.  It is full of lines with this number, a label, and a default value.
//Voila, dynamicly generated checkboxes on the Flags tab of Entity Properties.

// doors
inline constexpr auto SF_DOOR_ROTATE_YAW = 0; // yaw by default
inline constexpr auto SF_DOOR_START_OPEN_OBSOLETE = 1;
inline constexpr auto SF_DOOR_ROTATE_BACKWARDS = 2;
inline constexpr auto SF_DOOR_NONSOLID_TO_PLAYER = 4;
inline constexpr auto SF_DOOR_PASSABLE = 8;
inline constexpr auto SF_DOOR_ONEWAY = 16;
inline constexpr auto SF_DOOR_NO_AUTO_RETURN = 32;
inline constexpr auto SF_DOOR_ROTATE_ROLL = 64;
inline constexpr auto SF_DOOR_ROTATE_PITCH = 128;
inline constexpr auto SF_DOOR_PUSE = 256; // door can be opened by player's use button.
inline constexpr auto SF_DOOR_NONPCS = 512; // NPC can't open
inline constexpr auto SF_DOOR_PTOUCH = 1024; // player touch opens
inline constexpr auto SF_DOOR_LOCKED = 2048; // Door is initially locked
inline constexpr auto SF_DOOR_SILENT = 4096; // Door plays no audible sound, and does not alert NPCs when opened
inline constexpr auto SF_DOOR_USE_CLOSES = 8192; // Door can be +used to close before its autoreturn delay has expired.
inline constexpr auto SF_DOOR_SILENT_TO_NPCS = 16384; // Does not alert NPC's when opened.
inline constexpr auto SF_DOOR_IGNORE_USE = 32768; // Completely ignores player +use commands.
inline constexpr auto SF_DOOR_NEW_USE_RULES = 65536; // For func_door entities, behave more like prop_door_rotating with respect to +USE (changelist 242482)
inline constexpr auto SF_DOOR_START_BREAKABLE = 524288;

enum FuncDoorSpawnPos_t
{
	FUNC_DOOR_SPAWN_CLOSED = 0,
	FUNC_DOOR_SPAWN_OPEN,
};

class CBaseDoor : public CBaseToggle
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseDoor" } ]]
	[[= ks::reflect::KeyFrom<"m_ls.sLockedSound", ks::reflect::Key{ .name = "locked_sound", .as = FIELD_SOUNDNAME } >{} ]]
	[[= ks::reflect::KeyFrom<"m_ls.sUnlockedSound", ks::reflect::Key{ .name = "unlocked_sound", .as = FIELD_SOUNDNAME } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBaseDoor, CBaseToggle );

	DECLARE_SERVERCLASS();

	void Spawn_Impl( void );
	void Precache_Impl( void );
	bool CreateVPhysics_Impl();
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void StartBlocked_Impl( CBaseEntity *pOther );
	void Blocked_Impl( CBaseEntity *pOther );
	void EndBlocked_Impl( void );

	void Activate_Impl( void );

	int	ObjectCaps_Impl( void );

	DECLARE_DATADESC();

	// This is ONLY used by the node graph to test movement through a door
	[[= ks::reflect::Input{ .name = "SetToggleState", .type = FIELD_FLOAT } ]] void InputSetToggleState( inputdata_t &inputdata );
	virtual void SetToggleState( int state );

	virtual bool IsRotatingDoor() { return false; }
	bool ShouldSavePhysics_Impl();
	// used to selectivly override defaults
	void DoorTouch( CBaseEntity *pOther );

	// local functions
	int DoorActivate( );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorHitBottom( void );
	void UpdateAreaPortals( bool isOpen );
	void Unlock( void );
	void Lock( void );
	int GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] void InputClose( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Lock", .type = FIELD_VOID } ]] void InputLock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] void InputOpen( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetSpeed( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector m_vecMoveDir;		// The direction of motion for linear moving doors.

	locksound_t m_ls;			// door lock sounds
	
	byte	m_bLockedSentence;	
	byte	m_bUnlockedSentence;

	[[= ks::reflect::Key{ .name = "forceclosed" } ]] bool	m_bForceClosed;			// If set, always close, even if we're blocked.
	bool	m_bDoorGroup;
	bool	m_bLocked;				// Whether the door is locked
	[[= ks::reflect::Key{ .name = "ignoredebris" } ]] bool	m_bIgnoreDebris;
	
	[[= ks::reflect::Key{ .name = "spawnpos" } ]] FuncDoorSpawnPos_t m_eSpawnPosition;

	[[= ks::reflect::Key{ .name = "dmg" } ]] float	m_flBlockDamage;		// Damage inflicted when blocked.
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "noise1" } ]] string_t	m_NoiseMoving;		//Start/Looping sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "noise2" } ]] string_t	m_NoiseArrived;		//End sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "startclosesound" } ]] string_t	m_NoiseMovingClosed;		//Start/Looping sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "closesound" } ]] string_t	m_NoiseArrivedClosed;		//End sound
	[[= ks::reflect::Key{ .name = "chainstodoor" } ]] string_t	m_ChainTarget;		///< Entity name to pass Touch and Use events to

	CNetworkVar( float, m_flWaveHeight, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 8.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "WaveHeight" } ]] );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnBlockedClosing" } ]] COutputEvent m_OnBlockedClosing;		// Triggered when the door becomes blocked while closing.
	[[= ks::reflect::Key{ .name = "OnBlockedOpening" } ]] COutputEvent m_OnBlockedOpening;		// Triggered when the door becomes blocked while opening.
	[[= ks::reflect::Key{ .name = "OnUnblockedClosing" } ]] COutputEvent m_OnUnblockedClosing;		// Triggered when the door becomes unblocked while closing.
	[[= ks::reflect::Key{ .name = "OnUnblockedOpening" } ]] COutputEvent m_OnUnblockedOpening;		// Triggered when the door becomes unblocked while opening.
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;			// Triggered when the door reaches the fully closed position.
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;				// Triggered when the door reaches the fully open position.
	[[= ks::reflect::Key{ .name = "OnClose" } ]] COutputEvent m_OnClose;					// Triggered when the door is told to close.
	[[= ks::reflect::Key{ .name = "OnOpen" } ]] COutputEvent m_OnOpen;					// Triggered when the door is told to open.
	[[= ks::reflect::Key{ .name = "OnLockedUse" } ]] COutputEvent m_OnLockedUse;				// Triggered when the user tries to open a locked door.

	void			StartMovingSound( void );
	virtual void	StopMovingSound( void );
	void			MovingSoundThink( void );
	
	bool		ShouldLoopMoveSound( void ) { return m_bLoopMoveSound; }
	[[= ks::reflect::Key{ .name = "loopmovesound" } ]] bool		m_bLoopMoveSound;			// Move sound loops until stopped

private:
	void ChainUse( void );	///< Chains +use on through to m_ChainTarget
	void ChainTouch( CBaseEntity *pOther );	///< Chains touch on through to m_ChainTarget
	void SetChaining( bool chaining )	{ m_isChaining = chaining; }	///< Latch to prevent recursion
	bool m_isChaining;

	void CloseAreaPortalsThink( void );	///< Delays turning off area portals when closing doors to prevent visual artifacts
};

	EXTERN_SEND_TABLE( DT_EffectData );

inline constexpr auto EFFECTDATA_SERVER_IGNOREPREDICTIONCULL = 0x4;
// NOTE: These flags are specifically *not* networked; so it's placed above the max effect flag bits
inline constexpr auto EFFECTDATA_NO_RECORD = 0x80000000;

inline constexpr auto MAX_EFFECT_FLAG_BITS = 8;

// Note, must match common/qlimits.h!!!
// Must have this value in sync(-1) with qlimits.h and const.h
#define MAX_MODEL_INDEX_BITS	12

inline constexpr auto MAX_EFFECT_DISPATCH_STRING_BITS = 10;
inline constexpr auto MAX_EFFECT_DISPATCH_STRINGS = ( 1 << MAX_EFFECT_DISPATCH_STRING_BITS );

// This is the class that holds whatever data we're sending down to the client to make the effect.
class CEffectData
{
	[[= ks::reflect::NetTable{ .name = "DT_EffectData", .base = false } ]]
	// Six flat props reached through a dotted path: SENDINFO_NOCHECK( m_vOrigin.x ) and
	// RECVINFO( m_vOrigin.x ) both name the prop "m_vOrigin.x" in this table, not a sub-table.
	[[= ks::reflect::From<"m_vOrigin.x", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vOrigin.y", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vOrigin.z", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vStart.x", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vStart.y", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	[[= ks::reflect::From<"m_vStart.z", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD_MP_INTEGRAL }>{} ]]
	static constexpr bool ks_class = true;

public:
	Vector m_vOrigin;
	Vector m_vStart;
	[[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NORMAL } ]] Vector m_vNormal;
	// Sent with SendPropQAngles, received with RecvPropQAngles, which is RecvPropVector.
	[[= ks::reflect::Net{ .bits = 7 } ]] QAngle m_vAngles;
	[[= ks::reflect::Net{ .bits = MAX_EFFECT_FLAG_BITS, .flags = SPROP_UNSIGNED } ]] int		m_fFlags;
	[[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED, .wire = "entindex" } ]] int		m_nEntIndex;
	[[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] float	m_flScale;
	[[= ks::reflect::Net{ .bits = 12, .low = 0.0f, .high = 1023.0f, .flags = SPROP_ROUNDDOWN } ]] float	m_flMagnitude;
	[[= ks::reflect::Net{ .bits = 10, .low = 0.0f, .high = 1023.0f, .flags = SPROP_ROUNDDOWN } ]] float	m_flRadius;
	[[= ks::reflect::Net{ .bits = 5 } ]] int		m_nAttachmentIndex;
	// Send/RecvPropIntWithMinusOneFlag are Send/RecvPropInt wrappers whose whole effect is the
	// default proxy, and this call site overrides it -- with a different function on each side.
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]]
	[[= ks::reflect::Proxy<SendProxy_ShortAddOne, ks::reflect::WireSide::Send>{} ]]
	short	m_nSurfaceProp;

	// Some TF2 specific things
	[[= ks::reflect::Net{ .bits = MAX_MODEL_INDEX_BITS, .flags = SPROP_UNSIGNED } ]] int		m_nMaterial;
	[[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] int		m_nDamageType;
	[[= ks::reflect::Net{ .bits = 11, .flags = SPROP_UNSIGNED } ]] int		m_nHitBox;

	[[= ks::reflect::Net{ .bits = MAX_EDICT_BITS, .flags = SPROP_UNSIGNED } ]] int		m_nOtherEntIndex;
	
	[[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] unsigned char	m_nColor;

	[[= ks::reflect::Net{} ]] bool	m_bPositionsAreRelativeToEntity;

// Don't mess with stuff below here. DispatchEffect handles all of this.
public:
	CEffectData();

	int GetEffectNameIndex() { return m_iEffectName; }

private:

		DECLARE_SERVERCLASS_NOBASE()

	[[= ks::reflect::Net{ .bits = MAX_EFFECT_DISPATCH_STRING_BITS, .flags = SPROP_UNSIGNED } ]] int m_iEffectName;	// Entry in the EffectDispatch network string table. The is automatically handled by DispatchEffect().
};

#undef MAX_MODEL_INDEX_BITS

extern void LoadMapEntities( const char *pMapEntities );
extern void	DispatchObjectCollisionBox( edict_t *pent );
extern float DispatchObjectPhysicsVelocity( edict_t *pent, float moveTime );
extern ServerClass* DispatchGetObjectServerClass(edict_t *pent);
extern ServerClass* GetAllServerClasses();
extern void ResetGlobalState( void );
extern void ClearEntities( void );
extern void FreeContainingEntity( edict_t *ed );

class CBaseEntity;

using namespace ::ks::reflect;

//==================================================================================================================
// Entity Blocker
//==================================================================================================================
class CEntityBlocker : public CBaseEntity
{
	DECLARE_CLASS( CEntityBlocker, CBaseEntity );

public:

	static CEntityBlocker *Create( const Vector &origin, const Vector &mins, const Vector &maxs, CBaseEntity *pOwner = nullptr, bool bBlockPhysics = false );

	void Spawn_Impl( void );
	bool TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );
};

class CBaseEntity;

// This is the hash key type, but it could just as easily be and int or void *

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IEntityDataInstantiator
{
public:
	virtual ~IEntityDataInstantiator() {};

	virtual void *GetDataObject( const CBaseEntity *instance ) = 0;
	virtual void *CreateDataObject( const CBaseEntity *instance ) = 0;
	virtual void DestroyDataObject( const CBaseEntity *instance ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class T>
class CEntityDataInstantiator : public IEntityDataInstantiator
{
public:
	CEntityDataInstantiator()
	{
		m_HashTable.reserve( 64 );
	}

	virtual void *GetDataObject( const CBaseEntity *instance )
	{
		auto it = m_HashTable.find( instance );
		if ( it != m_HashTable.end() )
		{
			return (void *)it->second.get();
		}

		return nullptr;
	}

	virtual void *CreateDataObject( const CBaseEntity *instance )
	{
		// Create it if not already present
		auto [it, bInserted] = m_HashTable.try_emplace( instance );
		if ( bInserted )
		{
			it->second = std::make_unique<T>();

			// Zero-fill legacy POD data objects; types owning resources construct themselves
			if constexpr ( std::is_trivially_destructible_v<T> )
				V_memset( it->second.get(), 0, sizeof( T ) );
		}

		return (void *)it->second.get();
	}

	virtual void DestroyDataObject( const CBaseEntity *instance )
	{
		m_HashTable.erase( instance );
	}

private:
	std::unordered_map< const CBaseEntity *, std::unique_ptr< T > > m_HashTable;
};

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Expose things from GameInterface.cpp. Mostly the engine interfaces.


extern INetworkStringTable *g_pStringTableInfoPanel;

// Player / Client related functions
// Most of this is implemented in gameinterface.cpp, but some of it is per-mod
// in files like cs_gameinterface.cpp, etc.
class CServerGameClients : public IServerGameClients {
public:
  virtual bool ClientConnect(edict_t *pEntity, char const *pszName,
                             char const *pszAddress, char *reject,
                             int maxrejectlen);
  virtual void ClientActive(edict_t *pEntity, bool bLoadGame);
  virtual void ClientFullyConnect(edict_t *pEntity);
  virtual void ClientDisconnect(edict_t *pEntity);
  virtual void ClientPutInServer(edict_t *pEntity, const char *playername);
  virtual void ClientCommand(edict_t *pEntity, const CCommand &args);
  virtual void ClientSettingsChanged(edict_t *pEntity);
  virtual void ClientSetupVisibility(edict_t *pViewEntity, edict_t *pClient,
                                     unsigned char *pvs, int pvssize);
  virtual float ProcessUsercmds(edict_t *player, bf_read *buf, int numcmds,
                                int totalcmds, int dropped_packets, bool ignore,
                                bool paused);
  // Player is running a command
  virtual void PostClientMessagesSent(void);
  virtual void SetCommandClient(int index);
  virtual void *GetPlayerState(edict_t *player);
  virtual void ClientEarPosition(edict_t *pEntity, Vector *pEarOrigin);
  virtual bool
  ClientReplayEvent(edict_t *pEdict,
                    const ClientReplayEventParams_t &params) override;

  virtual void GetPlayerLimits(int &minplayers, int &maxplayers,
                               int &defaultMaxPlayers) const;

  // returns number of delay ticks if player is in Replay mode (0 = no delay)
  virtual int GetReplayDelay(edict_t *player, int &entity);
  // Anything this game .dll wants to add to the bug reporter text (e.g., the
  // entity/model under the picker crosshair)
  //  can be added here
  virtual void GetBugReportInfo(char *buf, int buflen);

  // A player sent a voice packet
  virtual void ClientVoice(edict_t *pEdict);

  virtual void NetworkIDValidated(const char *pszUserName,
                                  const char *pszNetworkID,
                                  CPlayerId playerUID) override;
  virtual int GetMaxSplitscreenPlayers();
  virtual int GetMaxHumanPlayers();

  // The client has submitted a keyvalues command
  virtual void ClientCommandKeyValues(edict_t *pEntity, KeyValues *pKeyValues);

  // Server override for supplied client name
  virtual const char *ClientNameHandler(uint64 xuid,
                                        const char *pchName) override;

  virtual void ClientSvcUserMessage(edict_t *pEntity, int nType,
                                    int nPassthrough, uint32 cbSize,
                                    const void *pvBuffer) override;
};

class CServerGameDLL : public IServerGameDLL {
public:
  virtual bool DLLInit(CreateInterfaceFn engineFactory,
                       CreateInterfaceFn physicsFactory,
                       CreateInterfaceFn fileSystemFactory,
                       CGlobalVars *pGlobals);
  virtual void DLLShutdown(void);
  // Get the simulation interval (must be compiled with identical values into
  // both client and game .dll for MOD!!!)
  virtual float GetTickInterval(void) const;
  virtual bool GameInit(void);
  virtual void GameShutdown(void);
  virtual bool LevelInit(const char *pMapName, char const *pMapEntities,
                         char const *pOldLevel, char const *pLandmarkName,
                         bool loadGame, bool background);
  virtual void ServerActivate(edict_t *pEdictList, int edictCount,
                              int clientMax);
  virtual void LevelShutdown(void);
  virtual void GameFrame(bool simulating); // could be called multiple times
                                           // before sending data to clients
  virtual void
  PreClientUpdate(bool simulating); // called after all GameFrame() calls,
                                    // before sending data to clients

  virtual ServerClass *GetAllServerClasses(void);
  virtual const char *GetGameDescription(void);
  virtual void CreateNetworkStringTables(void);

  virtual bool IsRestoring();

  virtual CStandardSendProxies *GetStandardSendProxies();

  virtual void PostInit();
  virtual void Think(bool finalTick);

  virtual void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie,
                                        edict_t *pPlayerEntity,
                                        EQueryCvarValueStatus eStatus,
                                        const char *pCvarName,
                                        const char *pCvarValue);

  virtual void PreSaveGameLoaded(char const *pSaveName, bool bInGame);

  // Returns true if the game DLL wants the server not to be made public.
  // Used by commentary system to hide multiplayer commentary servers from the
  // master.
  virtual bool ShouldHideServer(void);

  virtual void InvalidateMdlCache();

  // Called to apply lobby settings to a dedicated server
  virtual void ApplyGameSettings(KeyValues *pKV);

  virtual void GetMatchmakingTags(char *buf, std::size_t bufSize);

  virtual void ServerHibernationUpdate(bool bHibernating);

  virtual bool ShouldAllowDirectConnect(void);
  virtual bool FriendsReqdForDirectConnect(void);
  virtual bool IsLoadTestServer(void);

  virtual bool IsValveDS(void);

  // Builds extended server info for new connecting client
  virtual KeyValues *GetExtendedServerInfoForNewClient();

  // Marks the queue matchmaking game as starting
  virtual void ReportGCQueuedMatchStart(int32 iReservationStage,
                                        uint32 *puiConfirmedAccounts,
                                        int numConfirmedAccounts);

  // Precaches particle systems defined in the specific file
  virtual void PrecacheParticleSystemFile(const char *pParticleSystemFile);

  // Matchmaking game data buffer
  virtual void GetMatchmakingGameData(char *buf, std::size_t bufSize);

  float m_fAutoSaveDangerousTime;
  float m_fAutoSaveDangerousMinHealthToCommit;
  bool m_bIsHibernating;

  // Returns which encryption key to use for messages to be encrypted for TV
  virtual EncryptedMessageKeyType_t
  GetMessageEncryptionKey(INetMessage *pMessage);

  // If server game dll needs more time before server process quits then
  // it should return true to hold game server reservation from this interface
  // method. If this method returns false then the server process will clear the
  // reservation and might shutdown to meet uptime or memory limit requirements.
  virtual bool
  ShouldHoldGameServerReservation(float flTimeElapsedWithoutClients);

  // Pure server validation failed for the given client, client supplied
  // data is included in the payload
  virtual void OnPureServerFileValidationFailure(
      edict_t *edictClient, const char *path, const char *fileName, uint32 crc,
      int32 hashType, int32 len, int packNumber, int packFileID);

  // Last chance validation on connect packet for the client, non-NULL return
  // value causes the client connect to be aborted with the provided error
  virtual char const *ClientConnectionValidatePreNetChan(bool bGameServer,
                                                         char const *adr,
                                                         int nAuthProtocol,
                                                         uint64 ullPlayerID);

  // validate if player is a caster and add them to the active caster list
  virtual bool ValidateAndAddActiveCaster(const CPlayerId &playerUID);

  // Network channel notification from engine to game server code
  virtual void OnEngineClientNetworkEvent(edict_t *edictClient,
                                          uint64 ullPlayerID, int nEventType,
                                          void *pvParam) override;

  virtual void EngineGotvSyncPacket(const ks::net::CEngineGotvSyncPacket *pPkt);

  // GOTV client attempt redirect over SDR
  virtual bool OnEngineClientProxiedRedirect(uint64 ullClient,
                                             const char *adrProxiedRedirect,
                                             const char *adrRegular) override;

private:
  // This can just be a wrapper on MapEntity_ParseAllEntities, but CS does some
  // tricks in here with the entity list.
  void LevelInit_ParseAllEntities(const char *pMapEntities);
  void LoadMessageOfTheDay();
};

// Normally, when the engine calls ClientPutInServer, it calls a global function
// in the game DLL by the same name. Use this to override the function that it
// calls. This is used for bots.
typedef CBasePlayer *(*ClientPutInServerOverrideFn)(edict_t *pEdict,
                                                    const char *playername);

void ClientPutInServerOverride(ClientPutInServerOverrideFn fn);

// --------------------------------------------------------------------------------------------
// // Entity list management stuff.
// --------------------------------------------------------------------------------------------
// // These are created for map entities in order as the map entities are
// spawned.
class CMapEntityRef {
public:
  int m_iEdict; // Which edict slot this entity got. -1 if CreateEntityByName
                // failed.
  int m_iSerialNumber; // The edict serial number. TODO used anywhere ?
};

extern std::vector<CMapEntityRef> g_MapEntityRefs;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CMapLoadEntityFilter : public IMapEntityFilter {
public:
  virtual bool ShouldCreateEntity(const char *pClassname);

  virtual CBaseEntity *CreateNextEntity(const char *pClassname);
};

bool IsEngineThreaded();

class CServerGameTags : public IServerGameTags {
public:
  virtual void GetTaggedConVarList(KeyValues *pCvarTagList);
};
inline CServerGameTags s_Singleton_CServerGameTags;
inline InterfaceReg s_Reg_CServerGameTags_IServerGameTags( side_registry(), []() -> void * { return static_cast<IServerGameTags *>( &s_Singleton_CServerGameTags ); }, INTERFACEVERSION_SERVERGAMETAGS );;

// Colors used for nav editing

//--------------------------------------------------------------------------------------------------------------

enum NavEditColor
{
	// Degenerate area colors
	NavDegenerateFirstColor = 0,
	NavDegenerateSecondColor,

	// Place painting color
	NavSamePlaceColor,
	NavDifferentPlaceColor,
	NavNoPlaceColor,

	// Normal colors
	NavSelectedColor,
	NavMarkedColor,
	NavNormalColor,
	NavCornerColor,
	NavBlockedByDoorColor,
	NavBlockedByFuncNavBlockerColor,

	// Hiding spot colors
	NavIdealSniperColor,
	NavGoodSniperColor,
	NavGoodCoverColor,
	NavExposedColor,
	NavApproachPointColor,

	// Connector colors
	NavConnectedTwoWaysColor,
	NavConnectedOneWayColor,
	NavConnectedContiguous,
	NavConnectedNonContiguous,

	// Editing colors
	NavCursorColor,
	NavSplitLineColor,
	NavCreationColor,
	NavInvalidCreationColor,
	NavGridColor,
	NavDragSelectionColor,

	// Nav attribute colors
	NavAttributeCrouchColor,
	NavAttributeJumpColor,
	NavAttributePreciseColor,
	NavAttributeNoJumpColor,
	NavAttributeStopColor,
	NavAttributeRunColor,
	NavAttributeWalkColor,
	NavAttributeAvoidColor,
	NavAttributeStairColor,
};

//--------------------------------------------------------------------------------------------------------------

void NavDrawLine( const Vector& from, const Vector& to, NavEditColor navColor );
void NavDrawTriangle( const Vector& point1, const Vector& point2, const Vector& point3, NavEditColor navColor );
void NavDrawFilledTriangle( const Vector& point1, const Vector& point2, const Vector& point3, NavEditColor navColor, bool dark );
void NavDrawHorizontalArrow( const Vector& from, const Vector& to, float width, NavEditColor navColor );
void NavDrawDashedLine( const Vector& from, const Vector& to, NavEditColor navColor );
void NavDrawVolume( const Vector &vMin, const Vector &vMax, int zMidline, NavEditColor navColor );

//--------------------------------------------------------------------------------------------------------------

class CBaseEntity;
class CNavArea;
struct inputdata_t;

using namespace ::ks::reflect;

// nav_entities.h
// Navigation entities
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//-----------------------------------------------------------------------------------------------------
/**
  * An entity that can block/unblock nav areas.  This is meant for semi-transient areas that block
  * pathfinding but can be ignored for longer-term queries like computing L4D flow distances and
  * escape routes.
  */
class CFuncNavBlocker : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncNavBlocker, CBaseEntity );

public:
	void Spawn_Impl();
	void UpdateOnRemove_Impl( void );

	[[= ks::reflect::Input{ .name = "BlockNav", .type = FIELD_VOID } ]] void InputBlockNav( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UnblockNav", .type = FIELD_VOID } ]] void InputUnblockNav( inputdata_t &inputdata );

	inline bool IsBlockingNav( int teamNumber ) const
	{
		if ( teamNumber == TEAM_ANY )
		{
			bool isBlocked = false;
			for ( int i=0; i<MAX_NAV_TEAMS; ++i )
			{
				isBlocked |= m_isBlockingNav[ i ];
			}

			return isBlocked;
		}

		teamNumber = teamNumber % MAX_NAV_TEAMS;
		return m_isBlockingNav[ teamNumber ];
	}

	int DrawDebugTextOverlays_Impl( void );

	bool operator()( CNavArea *area );	// functor that blocks areas in our extent

	static bool CalculateBlocked( bool *pResultByTeam, const Vector &vecMins, const Vector &vecMaxs );

private:

	void UpdateBlocked();

	static std::vector<CFuncNavBlocker *> gm_NavBlockers;

	void BlockNav( void );
	void UnblockNav( void );
	bool m_isBlockingNav[MAX_NAV_TEAMS];
	[[= ks::reflect::Key{ .name = "teamToBlock" } ]] int m_blockedTeamNumber;
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool m_bDisabled;
	Vector m_CachedMins, m_CachedMaxs;

};

class CNavArea;
class CNavMesh;

using namespace ::ks::reflect;

// nav_node.h
// Navigation Nodes are used when generating a Navigation Mesh by point sampling the map
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

// If DEBUG_NAV_NODES is true, nav_show_nodes controls drawing node positions, and
// nav_show_node_id allows you to show the IDs of nodes that didn't get used to create areas.
#define DEBUG_NAV_NODES 1

//--------------------------------------------------------------------------------------------------------------
/**
 * Navigation Nodes.
 * These Nodes encapsulate world locations, and ways to get from one location to an adjacent one.
 * Note that these links are not necessarily commutative (falling off of a ledge, for example).
 */
class CNavNode
{
public:
	CNavNode( const Vector &pos, const Vector &normal, CNavNode *parent, bool onDisplacement );
	~CNavNode();

	static CNavNode *GetNode( const Vector &pos );					///< return navigation node at the position, or NULL if none exists
	static void CleanupGeneration();

	CNavNode *GetConnectedNode( NavDirType dir ) const;				///< get navigation node connected in given direction, or NULL if cant go that way
	const Vector *GetPosition( void ) const;
	const Vector *GetNormal( void ) const		{ return &m_normal; }
	unsigned int GetID( void ) const			{ return m_id; }

	static CNavNode *GetFirst( void )			{ return m_list; }
	static unsigned int GetListLength( void )	{ return m_listLength; }
	CNavNode *GetNext( void )					{ return m_next; }

	void Draw( void );

	void ConnectTo( CNavNode *node, NavDirType dir, float obstacleHeight, float flObstacleStartDist, float flObstacleEndDist );		///< create a connection FROM this node TO the given node, in the given direction
	CNavNode *GetParent( void ) const;

	void MarkAsVisited( NavDirType dir );							///< mark the given direction as having been visited
	BOOL HasVisited( NavDirType dir );								///< return true if the given direction has already been searched
	BOOL IsBiLinked( NavDirType dir ) const;						///< node is bidirectionally linked to another node in the given direction
	BOOL IsClosedCell( void ) const;								///< node is the NW corner of a bi-linked quad of nodes

	void Cover( void )				{ m_isCovered = true; }			///< @todo Should pass in area that is covering
	BOOL IsCovered( void ) const	{ return m_isCovered; }			///< return true if this node has been covered by an area

	void AssignArea( CNavArea *area );								///< assign the given area to this node
	CNavArea *GetArea( void ) const;								///< return associated area

	void SetAttributes( int bits )		{ m_attributeFlags = bits; }
	int GetAttributes( void ) const		{ return m_attributeFlags; }
	float GetGroundHeightAboveNode( NavCornerType cornerType ) const;	///< return ground height above node in given corner direction (NUM_CORNERS for highest in any direction)
	bool IsBlockedInAnyDirection( void) const;						///< return true if the node is blocked in any direction

	bool IsOnDisplacement( void ) const				{ return m_isOnDisplacement; }

private:
	friend class CNavMesh;

	bool TestForCrouchArea( NavCornerType cornerNum, const Vector& mins, const Vector& maxs, float *groundHeightAboveNode );
	void CheckCrouch( void );

	Vector m_pos;													///< position of this node in the world
	Vector m_normal;												///< surface normal at this location
	CNavNode *m_to[ NUM_DIRECTIONS ];								///< links to north, south, east, and west. NULL if no link
	float  m_obstacleHeight[ NUM_DIRECTIONS ];						///< obstacle height (delta from nav node z position) that must be climbed to reach next node in this direction
	float  m_obstacleStartDist[ NUM_DIRECTIONS ];					///< distance along this direction to reach the beginning of the obstacle
	float  m_obstacleEndDist[ NUM_DIRECTIONS ];						///< distance along this direction to reach the end of the obstacle
	unsigned int m_id;												///< unique ID of this node
	int	m_attributeFlags;											///< set of attribute bit flags (see NavAttributeType)

	static CNavNode *m_list;										///< the master list of all nodes for this map
	static unsigned int m_listLength;
	static unsigned int m_nextID;
	CNavNode *m_next;												///< next link in master list
	CNavNode *m_nextAtXY;											///< next link at a particular position

	// below are only needed when generating
	unsigned char m_visited;										///< flags for automatic node generation. If direction bit is clear, that direction hasn't been explored yet.
	CNavNode *m_parent;												///< the node prior to this in the search, which we pop back to when this node's search is done (a stack)
	bool m_isCovered;												///< true when this node is "covered" by a CNavArea
	CNavArea *m_area;												///< the area this node is contained within

	bool m_isBlocked[ NUM_CORNERS ];
	bool m_crouch[ NUM_CORNERS ];
	float m_groundHeightAboveNode[ NUM_CORNERS ];
	bool m_isOnDisplacement;
};

//--------------------------------------------------------------------------------------------------------------
//
// Inlines
//

#undef DEBUG_NAV_NODES

class CNavArea;
class CNavLadder;

using namespace ::ks::reflect;

// nav_pathfind.h
// Path-finding mechanisms using the Navigation Mesh
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

extern int g_DebugPathfindCounter;

//-------------------------------------------------------------------------------------------------------------------
/**
 * Used when building a path to determine the kind of path to build
 */
enum RouteType
{
	DEFAULT_ROUTE,
	FASTEST_ROUTE,
	SAFEST_ROUTE,
	RETREAT_ROUTE,
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Functor used with NavAreaBuildPath()
 */
class ShortestPathCost
{
public:
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length )
	{
		if ( fromArea == nullptr )
		{
			// first area in path, no cost
			return 0.0f;
		}
		else
		{
			// compute distance traveled along path so far
			float dist;

			if ( ladder )
			{
				dist = ladder->m_length;
			}
			else if ( length > 0.0 )
			{
				dist = length;
			}
			else
			{
				dist = ( area->GetCenter() - fromArea->GetCenter() ).Length();
			}

			float cost = dist + fromArea->GetCostSoFar();

			// if this is a "crouch" area, add penalty
			if ( area->GetAttributes() & NAV_MESH_CROUCH )
			{
				const float crouchPenalty = 20.0f;		// 10
				cost += crouchPenalty * dist;
			}

			// if this is a "jump" area, add penalty
			if ( area->GetAttributes() & NAV_MESH_JUMP )
			{
				const float jumpPenalty = 5.0f;
				cost += jumpPenalty * dist;
			}

			// if this is a 'damaging' area (Fire for example), add penalty
			if ( area->IsDamaging() )
			{
				const float damagingPenalty = 100.0f;
				cost += damagingPenalty * dist;
			}

			return cost;
		}
	}
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Find path from startArea to goalArea via an A* search, using supplied cost heuristic.
 * If cost functor returns -1 for an area, that area is considered a dead end.
 * This doesn't actually build a path, but the path is defined by following parent
 * pointers back from goalArea to startArea.
 * If 'closestArea' is non-NULL, the closest area to the goal is returned (useful if the path fails).
 * If 'goalArea' is NULL, will compute a path as close as possible to 'goalPos'.
 * If 'goalPos' is NULL, will use the center of 'goalArea' as the goal position.
 * If 'maxPathLength' is nonzero, path building will stop when this length is reached.
 * Returns true if a path exists.
 */
inline constexpr auto IGNORE_NAV_BLOCKERS = true;
template< typename CostFunctor >
bool NavAreaBuildPath( CNavArea *startArea, CNavArea *goalArea, const Vector *goalPos, CostFunctor &costFunc, CNavArea **closestArea = nullptr, float maxPathLength = 0.0f, int teamID = TEAM_ANY, bool ignoreNavBlockers = false )
{
	CVProfScope _( "NavAreaBuildPath", 0, "NextBotSpiky", false, BUDGETFLAG_OTHER );

	if ( closestArea )
	{
		*closestArea = startArea;
	}

	bool isDebug = ( g_DebugPathfindCounter-- > 0 );

	if (startArea == nullptr)
		return false;

	if (goalArea != nullptr && goalArea->IsBlocked( teamID, ignoreNavBlockers ))
		goalArea = nullptr;

	if (goalArea == nullptr && goalPos == nullptr)
		return false;

	startArea->SetParent( nullptr );

	// if we are already in the goal area, build trivial path
	if (startArea == goalArea)
	{
		goalArea->SetParent( nullptr );
		return true;
	}

	// determine actual goal position
	Vector actualGoalPos = (goalPos) ? *goalPos : goalArea->GetCenter();

	// start search
	CNavArea::ClearSearchLists();

	// compute estimate of path length
	/// @todo Cost might work as "manhattan distance"
	startArea->SetTotalCost( (startArea->GetCenter() - actualGoalPos).Length() );

	float initCost = costFunc( startArea, nullptr, nullptr, nullptr, -1.0f );	
	if (initCost < 0.0f)
		return false;
	startArea->SetCostSoFar( initCost );
	startArea->SetPathLengthSoFar( 0.0 );

	startArea->AddToOpenList();

	// keep track of the area we visit that is closest to the goal
	if (closestArea)
		*closestArea = startArea;
	float closestAreaDist = startArea->GetTotalCost();

	// do A* search
	while( !CNavArea::IsOpenListEmpty() )
	{
		// get next area to check
		CNavArea *area = CNavArea::PopOpenList();

		if ( isDebug )
		{
			area->DrawFilled( 0, 255, 0, 128, 30.0f );
		}

		// don't consider blocked areas
		if ( area->IsBlocked( teamID, ignoreNavBlockers ) )
			continue;

		// check if we have found the goal area or position
		if (area == goalArea || (goalArea == nullptr && goalPos && area->Contains( *goalPos )))
		{
			if (closestArea)
			{
				*closestArea = area;
			}

			return true;
		}

		// search adjacent areas
		enum SearchType
		{
			SEARCH_FLOOR, SEARCH_LADDERS, SEARCH_ELEVATORS
		};
		SearchType searchWhere = SEARCH_FLOOR;
		int searchIndex = 0;

		int dir = NORTH;
		const NavConnectVector *floorList = area->GetAdjacentAreas( NORTH );

		bool ladderUp = true;
		const NavLadderConnectVector *ladderList = nullptr;
		enum { AHEAD = 0, LEFT, RIGHT, BEHIND, NUM_TOP_DIRECTIONS };
		int ladderTopDir = AHEAD;
		bool bHaveMaxPathLength = ( maxPathLength > 0.0f );
		float length = -1;
		
		while( true )
		{
			CNavArea *newArea = nullptr;
			NavTraverseType how;
			const CNavLadder *ladder = nullptr;
			const CFuncElevator *elevator = nullptr;

			//
			// Get next adjacent area - either on floor or via ladder
			//
			if ( searchWhere == SEARCH_FLOOR )
			{
				// if exhausted adjacent connections in current direction, begin checking next direction
				if ( searchIndex >= std::ssize( *floorList ) )
				{
					++dir;

					if ( dir == NUM_DIRECTIONS )
					{
						// checked all directions on floor - check ladders next
						searchWhere = SEARCH_LADDERS;

						ladderList = area->GetLadders( CNavLadder::LADDER_UP );
						searchIndex = 0;
						ladderTopDir = AHEAD;
					}
					else
					{
						// start next direction
						floorList = area->GetAdjacentAreas( (NavDirType)dir );
						searchIndex = 0;
					}

					continue;
				}

				const NavConnect &floorConnect = (*floorList)[ searchIndex ];
				newArea = floorConnect.area;
				length = floorConnect.length;
				how = (NavTraverseType)dir;
				++searchIndex;

			}
			else if ( searchWhere == SEARCH_LADDERS )
			{
				if ( searchIndex >= std::ssize( *ladderList ) )
				{
					if ( !ladderUp )
					{
						// checked both ladder directions - check elevators next
						searchWhere = SEARCH_ELEVATORS;
						searchIndex = 0;
						ladder = nullptr;
					}
					else
					{
						// check down ladders
						ladderUp = false;
						ladderList = area->GetLadders( CNavLadder::LADDER_DOWN );
						searchIndex = 0;
					}
					continue;
				}

				if ( ladderUp )
				{
					ladder = (*ladderList)[ searchIndex ].ladder;

					// do not use BEHIND connection, as its very hard to get to when going up a ladder
					if ( ladderTopDir == AHEAD )
					{
						newArea = ladder->m_topForwardArea;
					}
					else if ( ladderTopDir == LEFT )
					{
						newArea = ladder->m_topLeftArea;
					}
					else if ( ladderTopDir == RIGHT )
					{
						newArea = ladder->m_topRightArea;
					}
					else
					{
						++searchIndex;
						ladderTopDir = AHEAD;
						continue;
					}

					how = GO_LADDER_UP;
					++ladderTopDir;
				}
				else
				{
					newArea = (*ladderList)[ searchIndex ].ladder->m_bottomArea;
					how = GO_LADDER_DOWN;
					ladder = (*ladderList)[ searchIndex ].ladder;
					++searchIndex;
				}

				if ( newArea == nullptr )
					continue;

				length = -1.0f;
			}
			else // if ( searchWhere == SEARCH_ELEVATORS )
			{
				elevator = nullptr;
				break;
			}

			// don't backtrack
			if ( newArea == area )
				continue;

			// don't consider blocked areas
			if ( newArea->IsBlocked( teamID, ignoreNavBlockers ) )
				continue;

			float newCostSoFar = costFunc( newArea, area, ladder, elevator, length );
			
			// check if cost functor says this area is a dead-end
			if ( newCostSoFar < 0.0f )
				continue;
				
			// stop if path length limit reached
			if ( bHaveMaxPathLength )
			{
				// keep track of path length so far
				float deltaLength = ( newArea->GetCenter() - area->GetCenter() ).Length();
				float newLengthSoFar = area->GetPathLengthSoFar() + deltaLength;
				if ( newLengthSoFar > maxPathLength )
					continue;
				
				newArea->SetPathLengthSoFar( newLengthSoFar );
			}

			if ( ( newArea->IsOpen() || newArea->IsClosed() ) && newArea->GetCostSoFar() <= newCostSoFar )
			{
				// this is a worse path - skip it
				continue;
			}
			else
			{
				// compute estimate of distance left to go
				float distSq = ( newArea->GetCenter() - actualGoalPos ).LengthSqr();
				float newCostRemaining = ( distSq > 0.0 ) ? FastSqrt( distSq ) : 0.0 ;

				// track closest area to goal in case path fails
				if ( closestArea && newCostRemaining < closestAreaDist )
				{
					*closestArea = newArea;
					closestAreaDist = newCostRemaining;
				}
				
				newArea->SetCostSoFar( newCostSoFar );
				newArea->SetTotalCost( newCostSoFar + newCostRemaining );

				if ( newArea->IsClosed() )
				{
					newArea->RemoveFromClosedList();
				}

				if ( newArea->IsOpen() )
				{
					// area already on open list, update the list order to keep costs sorted
					newArea->UpdateOnOpenList();
				}
				else
				{
					newArea->AddToOpenList();
				}

				newArea->SetParent( area, how );
			}
		}

		// we have searched this area
		area->AddToClosedList();
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Compute distance between two areas. Return -1 if can't reach 'endArea' from 'startArea'.
 */
template< typename CostFunctor >
float NavAreaTravelDistance( CNavArea *startArea, CNavArea *endArea, CostFunctor &costFunc, float maxPathLength = 0.0f )
{
	if (startArea == nullptr)
		return -1.0f;

	if (endArea == nullptr)
		return -1.0f;

	if (startArea == endArea)
		return 0.0f;

	// compute path between areas using given cost heuristic
	if (NavAreaBuildPath( startArea, endArea, nullptr, costFunc, nullptr, maxPathLength ) == false)
		return -1.0f;

	// compute distance along path
	float distance = 0.0f;
	for( CNavArea *area = endArea; area->GetParent(); area = area->GetParent() )
	{
		distance += (area->GetCenter() - area->GetParent()->GetCenter()).Length();
	}

	return distance;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do a breadth-first search, invoking functor on each area.
 * If functor returns 'true', continue searching from this area. 
 * If functor returns 'false', the area's adjacent areas are not explored (dead end).
 * If 'maxRange' is 0 or less, no range check is done (all areas will be examined).
 *
 * NOTE: Returns all areas that overlap range, even partially
 *
 * @todo Use ladder connections
 */

// helper function
inline void AddAreaToOpenList( CNavArea *area, CNavArea *parent, const Vector &startPos, float maxRange )
{
	if (area == nullptr)
		return;

	if (!area->IsMarked())
	{
		area->Mark();
		area->SetTotalCost( 0.0f );
		area->SetParent( parent );

		if (maxRange > 0.0f)
		{
			// make sure this area overlaps range
			Vector closePos;
			area->GetClosestPointOnArea( startPos, &closePos );
			if ((closePos - startPos).AsVector2D().IsLengthLessThan( maxRange ))
			{
				// compute approximate distance along path to limit travel range, too
				float distAlong = parent->GetCostSoFar();
				distAlong += (area->GetCenter() - parent->GetCenter()).Length();
				area->SetCostSoFar( distAlong );

				// allow for some fudge due to large size areas
				if (distAlong <= 1.5f * maxRange)
					area->AddToOpenList();
			}
		}
		else
		{
			// infinite range
			area->AddToOpenList();
		}
	}
}

/****************************************************************
 * DEPRECATED: Use filter-based SearchSurroundingAreas below
 ****************************************************************/
inline constexpr auto INCLUDE_INCOMING_CONNECTIONS = 0x1;
inline constexpr auto INCLUDE_BLOCKED_AREAS = 0x2;
inline constexpr auto EXCLUDE_OUTGOING_CONNECTIONS = 0x4;
inline constexpr auto EXCLUDE_ELEVATORS = 0x8;
template < typename Functor >
void SearchSurroundingAreas( CNavArea *startArea, const Vector &startPos, Functor &func, float maxRange = -1.0f, unsigned int options = 0, int teamID = TEAM_ANY )
{
	if (startArea == nullptr)
		return;

	CNavArea::MakeNewMarker();
	CNavArea::ClearSearchLists();

	startArea->AddToOpenList();
	startArea->SetTotalCost( 0.0f );
	startArea->SetCostSoFar( 0.0f );
	startArea->SetParent( nullptr );
	startArea->Mark();

	while( !CNavArea::IsOpenListEmpty() )
	{
		// get next area to check
		CNavArea *area = CNavArea::PopOpenList();

		// don't use blocked areas
		if ( area->IsBlocked( teamID ) && !(options & INCLUDE_BLOCKED_AREAS) )
			continue;

		// invoke functor on area
		if (func( area ))
		{
			// explore adjacent floor areas
			for( int dir=0; dir<NUM_DIRECTIONS; ++dir )
			{
				int count = area->GetAdjacentCount( (NavDirType)dir );
				for( int i=0; i<count; ++i )
				{
					CNavArea *adjArea = area->GetAdjacentArea( (NavDirType)dir, i );
					if ( options & EXCLUDE_OUTGOING_CONNECTIONS )
					{
						if ( !adjArea->IsConnected( area, NUM_DIRECTIONS ) )
						{
							continue;	// skip this outgoing connection
						}
					}
					
					AddAreaToOpenList( adjArea, area, startPos, maxRange );
				}
			}
			
			// potentially include areas that connect TO this area via a one-way link
			if (options & INCLUDE_INCOMING_CONNECTIONS)
			{
				for( int dir=0; dir<NUM_DIRECTIONS; ++dir )
				{
					const NavConnectVector *list = area->GetIncomingConnections( (NavDirType)dir );

					for ( NavConnect connect : *list )
					{
						AddAreaToOpenList( connect.area, area, startPos, maxRange );
					}
				}
			}

			// explore adjacent areas connected by ladders

			// check up ladders
			const NavLadderConnectVector *ladderList = area->GetLadders( CNavLadder::LADDER_UP );
			if (ladderList)
			{
				for ( const NavLadderConnect &ladderConnect : *ladderList )
				{
					const CNavLadder *ladder = ladderConnect.ladder;

					// do not use BEHIND connection, as its very hard to get to when going up a ladder
					AddAreaToOpenList( ladder->m_topForwardArea, area, startPos, maxRange );
					AddAreaToOpenList( ladder->m_topLeftArea, area, startPos, maxRange );
					AddAreaToOpenList( ladder->m_topRightArea, area, startPos, maxRange );
				}
			}

			// check down ladders
			ladderList = area->GetLadders( CNavLadder::LADDER_DOWN );
			if (ladderList)
			{
				for ( const NavLadderConnect &ladderConnect : *ladderList )
				{
					const CNavLadder *ladder = ladderConnect.ladder;

					AddAreaToOpenList( ladder->m_bottomArea, area, startPos, maxRange );
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Derive your own custom search functor from this base for use with SearchSurroundingAreas below.
 * The derived functor must provide
 *   bool operator() ( CNavArea *area, CNavArea *priorArea, float travelDistanceSoFar )
 * which performs a user-defined action on area.  Return 'false' to end the search (ie: you found what you were looking for)
 */
class ISearchSurroundingAreasFunctor
{
public:
	// return true if 'adjArea' should be included in the ongoing search
	bool ShouldSearch( CNavArea *adjArea, CNavArea *currentArea, float travelDistanceSoFar ) 
	{
		return !adjArea->IsBlocked( TEAM_ANY );
	}

	/**
	 * Collect adjacent areas to continue the search by calling 'IncludeInSearch' on each
	 */
	void IterateAdjacentAreas( CNavArea *area, CNavArea *priorArea, float travelDistanceSoFar ) 
	{
		// search adjacent outgoing connections
		for( int dir=0; dir<NUM_DIRECTIONS; ++dir )
		{
			int count = area->GetAdjacentCount( (NavDirType)dir );
			for( int i=0; i<count; ++i )
			{
				CNavArea *adjArea = area->GetAdjacentArea( (NavDirType)dir, i );

				if ( ShouldSearch( adjArea, area, travelDistanceSoFar ) )
				{
					IncludeInSearch( adjArea, area );
				}
			}
		}
	}

	// Invoked after the search has completed
	void PostSearch( void ) { }

	// consider 'area' in upcoming search steps
	void IncludeInSearch( CNavArea *area, CNavArea *priorArea ) 
	{
		if ( area == nullptr )
			return;

		if ( !area->IsMarked() )
		{
			area->Mark();
			area->SetTotalCost( 0.0f );
			area->SetParent( priorArea );

			// compute approximate travel distance from start area of search
			if ( priorArea )
			{
				float distAlong = priorArea->GetCostSoFar();
				distAlong += ( area->GetCenter() - priorArea->GetCenter() ).Length();
				area->SetCostSoFar( distAlong );
			}
			else
			{
				area->SetCostSoFar( 0.0f );
			}

			// adding an area to the open list also marks it
			area->AddToOpenList();
		}
	}
};

/**
 * Do a breadth-first search starting from 'startArea' and continuing outward based on
 * adjacent areas that pass the given filter
 */
template < typename Functor >
inline void SearchSurroundingAreas( CNavArea *startArea, Functor &func )
{
	if ( startArea )
	{
		CNavArea::MakeNewMarker();
		CNavArea::ClearSearchLists();

		startArea->AddToOpenList();
		startArea->SetTotalCost( 0.0f );
		startArea->SetCostSoFar( 0.0f );
		startArea->SetParent( nullptr );
		startArea->Mark();

		std::vector< CNavArea * > adjVector;

		while( !CNavArea::IsOpenListEmpty() )
		{
			// get next area to check
			CNavArea *area = CNavArea::PopOpenList();

			if ( func( area, area->GetParent(), area->GetCostSoFar() ) )
			{
				func.IterateAdjacentAreas( area, area->GetParent(), area->GetCostSoFar() );
			}
			else
			{
				// search aborted
				break;
			}
		}
	}

	func.PostSearch();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Fuctor that returns lowest cost for farthest away areas
 * For use with FindMinimumCostArea()
 */
class FarAwayFunctor
{
public:
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder )
	{
		if (area == fromArea)
			return 9999999.9f;

		return 1.0f/(fromArea->GetCenter() - area->GetCenter()).Length();
	}
};

/**
 * Fuctor that returns lowest cost for areas farthest from given position
 * For use with FindMinimumCostArea()
 */
class FarAwayFromPositionFunctor 
{
public:
	FarAwayFromPositionFunctor( const Vector &pos ) : m_pos( pos )
	{
	}

	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder )
	{
		return 1.0f/(m_pos - area->GetCenter()).Length();
	}

private:
	const Vector &m_pos;
};

/**
 * Pick a low-cost area of "decent" size
 */
template< typename CostFunctor >
CNavArea *FindMinimumCostArea( CNavArea *startArea, CostFunctor &costFunc )
{
	const float minSize = 150.0f;

	// collect N low-cost areas of a decent size
	enum { NUM_CHEAP_AREAS = 32 };
	struct 
	{
		CNavArea *area;
		float cost;
	}
	cheapAreaSet[ NUM_CHEAP_AREAS ];
	int cheapAreaSetCount = 0;

	for ( CNavArea *area : TheNavAreas )
	{
		// skip the small areas
		if ( area->GetSizeX() < minSize || area->GetSizeY() < minSize)
			continue;

		// compute cost of this area

		// HPE_FIX[pfreese]: changed this to only pass three parameters, in accord with the two functors above
		float cost = costFunc( area, startArea, nullptr );

		if (cheapAreaSetCount < NUM_CHEAP_AREAS)
		{
			cheapAreaSet[ cheapAreaSetCount ].area = area;
			cheapAreaSet[ cheapAreaSetCount++ ].cost = cost;
		}
		else
		{
			// replace most expensive cost if this is cheaper
			int expensive = 0;
			for( int i=1; i<NUM_CHEAP_AREAS; ++i )
				if (cheapAreaSet[i].cost > cheapAreaSet[expensive].cost)
					expensive = i;

			if (cheapAreaSet[expensive].cost > cost)
			{
				cheapAreaSet[expensive].area = area;
				cheapAreaSet[expensive].cost = cost;
			}
		}
	}

	if (cheapAreaSetCount)
	{
		// pick one of the areas at random
		return cheapAreaSet[ RandomInt( 0, cheapAreaSetCount-1 ) ].area;
	}
	else
	{
		// degenerate case - no decent sized areas - pick a random area
		int numAreas = std::ssize( TheNavAreas );
		int which = RandomInt( 0, numAreas-1 );

		for ( CNavArea *area : TheNavAreas )
		{
			if (which-- == 0)
				return area;
		}

	}
	return cheapAreaSet[ RandomInt( 0, cheapAreaSetCount-1 ) ].area;
}

class CBaseCombatCharacter;
class CBaseDoor;
class CBaseEntity;
class CBasePropDoor;
class CUserCmd;

using namespace ::ks::reflect;


//--------------------------------------------------------------------------------------------------------------
bool IsPushAwayEntity( CBaseEntity *pEnt );
bool IsPushableEntity( CBaseEntity *pEnt );

//--------------------------------------------------------------------------------------------------------------
bool IsBreakableEntity( CBaseEntity *pEnt );

//--------------------------------------------------------------------------------------------------------------
class CPushAwayEnumerator : public IPartitionEnumerator
{
public:
	// Forced constructor
	CPushAwayEnumerator(CBaseEntity **ents, int nMaxEnts);
	
	// Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );

		if ( IsPushAwayEntity( pEnt ) && m_nAlreadyHit < m_nMaxHits )
		{
			m_AlreadyHit[m_nAlreadyHit] = pEnt;
			m_nAlreadyHit++;
		}

		return ITERATION_CONTINUE;
	}

public:

	CBaseEntity **m_AlreadyHit;
	int m_nAlreadyHit;
	int m_nMaxHits;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * This class will collect breakable objects in a volume.  Physics props that can be damaged, func_breakable*, etc
 * are all collected by this class.
 */
class CBotBreakableEnumerator : public CPushAwayEnumerator
{
public:
	CBotBreakableEnumerator(CBaseEntity **ents, int nMaxEnts) : CPushAwayEnumerator(ents, nMaxEnts)
	{
	}

	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
};

//--------------------------------------------------------------------------------------------------------------
/**
 * This class will collect door objects in a volume.
 */
class CBotDoorEnumerator : public CPushAwayEnumerator
{
public:
	CBotDoorEnumerator(CBaseEntity **ents, int nMaxEnts) : CPushAwayEnumerator(ents, nMaxEnts)
	{
	}

	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns an entity that matches the filter that is along the line segment
 */
CBaseEntity * CheckForEntitiesAlongSegment( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, CPushAwayEnumerator *enumerator );

//--------------------------------------------------------------------------------------------------------------
// Retrieves physics objects near pPushingEntity
void AvoidPushawayProps(  CBaseCombatCharacter *pPlayer, CUserCmd *pCmd );
int GetPushawayEnts( CBaseCombatCharacter *pPushingEntity, CBaseEntity **ents, int nMaxEnts, float flPlayerExpand, int PartitionMask, CPushAwayEnumerator *enumerator = nullptr );

//--------------------------------------------------------------------------------------------------------------
// Pushes physics objects away from the entity
void PerformObstaclePushaway( CBaseCombatCharacter *pPushingEntity );

using namespace ::ks::reflect;

class CParticleFire : public CBaseParticleEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticleFire", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	CParticleFire();

	DECLARE_CLASS( CParticleFire, CBaseParticleEntity );

					DECLARE_SERVERCLASS();

	// The client shoots a ray out and starts creating fire where it hits.
	CNetworkVector( m_vOrigin, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vDirection, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
};

using namespace ::ks::reflect;

//==================================================
// CParticleLight. These are tied to 
//==================================================

inline constexpr const char *PARTICLELIGHT_ENTNAME = "env_particlelight";

class CParticleLight : public CServerOnlyPointEntity
{
public:
	DECLARE_CLASS( CParticleLight, CServerOnlyPointEntity );
	DECLARE_DATADESC();

					CParticleLight();

public:
	[[= ks::reflect::Key{ .name = "Intensity" } ]] float			m_flIntensity;
	[[= ks::reflect::Key{ .name = "Color" } ]] Vector			m_vColor;	// 0-255
	[[= ks::reflect::Key{ .name = "PSName" } ]] string_t		m_PSName;	// Name of the particle system entity this light affects.
	[[= ks::reflect::Key{ .name = "Directional" } ]] bool			m_bDirectional;
};

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: entity class for simple ragdoll physics
//-----------------------------------------------------------------------------

// UNDONE: Move this to a private header
class CRagdollProp : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
	[[= ks::reflect::NetTable{ .name = "DT_Ragdoll" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CRagdollProp, CBaseAnimating );

public:
	CRagdollProp( void );
	~CRagdollProp( void );

	void UpdateOnRemove_Impl( void );

	void DrawDebugGeometryOverlays_Impl();

	void Spawn_Impl( void );
	void Precache_Impl( void );

	// Disable auto fading under dx7 or when level fades are specified
	void DisableAutoFade();

	int ObjectCaps_Impl();

	DECLARE_SERVERCLASS();
	// Don't treat as a live target
	virtual bool IsAlive( void ) { return false; }
	
	void TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &dir, trace_t *ptr );
	bool TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );
	void Teleport_Impl( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts );
	virtual void SetupBones( matrix3x4a_t *pBoneToWorld, int boneMask );
	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	int VPhysicsGetObjectList_Impl( IPhysicsObject **pList, int listMax );

	int DrawDebugTextOverlays_Impl(void);

	// Response system stuff
	ResponseRules::IResponseSystem *GetResponseSystem_Impl();
	void ModifyOrAppendCriteria_Impl( AI_CriteriaSet& set );
	void SetSourceClassName( const char *pClassname );

	// Physics attacker
	CBasePlayer *HasPhysicsAttacker_Impl( float dt );

	// locals
	void InitRagdollAnimation( void );
	void InitRagdoll( const Vector &forceVector, int forceBone, const Vector &forcePos, matrix3x4_t *pPrevBones, matrix3x4_t *pBoneToWorld, float dt, int collisionGroup, bool activateRagdoll, bool bWakeRagdoll = true );
	
	void RecheckCollisionFilter( void );
	void SetDebrisThink();
	void ClearFlagsThink( void );
	inline ragdoll_t *GetRagdoll( void ) { return &m_ragdoll; }

	virtual bool	IsRagdoll() { return true; }

	// Damage passing
	virtual void	SetDamageEntity( CBaseEntity *pEntity );
	int		OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void OnRestore_Impl();

	// Purpose: CDefaultPlayerPickupVPhysics
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
 	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );
	virtual AngularImpulse	PhysGunLaunchAngularImpulse();
	bool HasPhysgunInteraction( const char *pszKeyName, const char *pszValue );
	void HandleFirstCollisionInteractions( int index, gamevcollisionevent_t *pEvent );

	void			SetUnragdoll( CBaseAnimating *pOther );

	void			SetBlendWeight( float weight ) { m_flBlendWeight = weight; }
	void			SetOverlaySequence( Activity activity );
	void			FadeOut( float flDelay = 0, float fadeTime = -1 );
	bool			IsFading();
	CBaseEntity*	GetKiller() { return m_hKiller; }
	void			SetKiller( CBaseEntity *pKiller ) { m_hKiller = pKiller; }
	void			GetAngleOverrideFromCurrentState( char *pOut, int size );

	void			DisableMotion( void );

	// Input/Output
	[[= ks::reflect::Input{ .name = "StartRagdollBoogie", .type = FIELD_VOID } ]] void			InputStartRadgollBoogie( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableMotion", .type = FIELD_VOID } ]] void			InputEnableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableMotion", .type = FIELD_VOID } ]] void			InputDisableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void			InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void			InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeAndRemove", .type = FIELD_FLOAT } ]] void			InputFadeAndRemove( inputdata_t &inputdata );

	DECLARE_DATADESC();

protected:
	void CalcRagdollSize( void );
	ragdoll_t			m_ragdoll;

private:
	void UpdateNetworkDataFromVPhysics( IPhysicsObject *pPhysics, int index );
	void FadeOutThink();

	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool				m_bStartDisabled;

	CNetworkArray( Vector, m_ragPos, RAGDOLL_MAX_ELEMENTS,
	               [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .varlen = true } ]] );
	CNetworkArray( QAngle, m_ragAngles, RAGDOLL_MAX_ELEMENTS,
	               [[= ks::reflect::Net{ .bits = 13, .varlen = true } ]] );

	[[= ks::reflect::Key{ .name = "angleOverride" } ]] string_t			m_anglesOverrideString;

	typedef CHandle<CBaseAnimating> CBaseAnimatingHandle;
	CNetworkVar( CBaseAnimatingHandle, m_hUnragdoll, [[= ks::reflect::Net{} ]] );

	unsigned int		m_lastUpdateTickCount;
	bool				m_allAsleep;
	bool				m_bFirstCollisionAfterLaunch;
	EHANDLE				m_hDamageEntity;
	EHANDLE				m_hKiller;	// Who killed me?
	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;
	float				m_flFadeOutStartTime;
	float				m_flFadeTime;

	string_t			m_strSourceClassName;
	bool				m_bHasBeenPhysgunned;

	// If not 1, then allow underlying sequence to blend in with simulated bone positions
	CNetworkVar( float, m_flBlendWeight, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 1.0f, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_nOverlaySequence, [[= ks::reflect::Net{ .bits = 11 } ]] );
	float	m_flDefaultFadeScale;
	
	Vector				m_ragdollMins[RAGDOLL_MAX_ELEMENTS];
	Vector				m_ragdollMaxs[RAGDOLL_MAX_ELEMENTS];
};

CBaseEntity *CreateServerRagdoll( CBaseAnimating *pAnimating, int forceBone, const CTakeDamageInfo &info, int collisionGroup, bool bUseLRURetirement = false );
CRagdollProp *CreateServerRagdollAttached( CBaseAnimating *pAnimating, const Vector &vecForce, int forceBone, int collisionGroup, IPhysicsObject *pAttached, CBaseAnimating *pParentEntity, int boneAttach, const Vector &originAttached, int parentBoneAttach, const Vector &boneOrigin );
void DetachAttachedRagdoll( CBaseEntity *pRagdollIn );
void DetachAttachedRagdollsForEntity( CBaseEntity *pRagdollParent );
CBaseAnimating *CreateServerRagdollSubmodel( CBaseAnimating *pOwner, const char *pModelName, const Vector &position, const QAngle &angles, int collisionGroup );

bool Ragdoll_IsPropRagdoll( CBaseEntity *pEntity );
void Ragdoll_GetAngleOverrideString( char *pOut, int size, CBaseEntity *pEntity );
ragdoll_t *Ragdoll_GetRagdoll( CBaseEntity *pEntity );

class CBaseAnimating;
class CBaseEntity;
class CTakeDamageInfo;

using namespace ::ks::reflect;


struct outer_collision_obb_t
{
	bool	bDirty;
	Vector	vecPos;
	Vector	vecMins;
	Vector	vecMaxs;
	QAngle	angAngles;

	std::vector< short >	iSpheres;
};

//-----------------------------------------------------------------------------
// Purpose: entity class for simple ragdoll physics
//-----------------------------------------------------------------------------

// UNDONE: Move this to a private header
class CStatueProp : public CPhysicsProp
{
	[[= ks::reflect::NetTable{ .name = "DT_StatueProp" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CStatueProp, CPhysicsProp );
	DECLARE_SERVERCLASS();

public:
	CStatueProp( void );

	void	Spawn_Impl( void );
	void	Precache_Impl();

	bool	CreateVPhysics_Impl();
	void	VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );

	//virtual float	GetAutoAimRadius() { return 24.0f; }

	void	ComputeWorldSpaceSurroundingBox_Impl( Vector *pMins, Vector *pMaxs );
	bool	TestCollision_Impl( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	int		OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void	Event_Killed_Impl( const CTakeDamageInfo &info );

	virtual void	Freeze( float flFreezeAmount = -1.0f, CBaseEntity *pFreezer = nullptr, Ray_t *pFreezeRay = nullptr );

	void	CollisionPartnerThink( void );

private:

	bool	CreateVPhysicsFromHitBoxes( CBaseAnimating *pInitBaseAnimating );
	bool	CreateVPhysicsFromOBBs( CBaseAnimating *pInitBaseAnimating );

public:

	CNetworkHandle( CBaseAnimating,	m_hInitBaseAnimating, [[= ks::reflect::Net{} ]] );

	CNetworkVar( bool, m_bShatter, [[= ks::reflect::Net{} ]] );
	CNetworkVar( int, m_nShatterFlags, [[= ks::reflect::Net{ .bits = 3 } ]] );
	CNetworkVector( m_vShatterPosition, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_vShatterForce, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );

	const std::vector< outer_collision_obb_t >	*m_pInitOBBs;
};

CBaseEntity *CreateServerStatue( CBaseAnimating *pAnimating, int collisionGroup );
CBaseEntity *CreateServerStatueFromOBBs( const std::vector< outer_collision_obb_t > &vecSphereOrigins, CBaseAnimating *pChildEntity );

class CBaseEntity;
class IGameSystem;


typedef CHandle<CBaseEntity> EHANDLE;

class CSoundPatch;

extern IPhysicsObject		*g_PhysWorldObject;
extern IPhysics				*physics;
extern IPhysicsCollision	*physcollision;
extern IPhysicsEnvironment	*physenv;
extern IPhysicsSurfaceProps *physprops;
extern IPhysicsObjectPairHash *g_EntityCollisionHash;

extern const objectparams_t g_PhysDefaultObjectParams;

// Compute enough energy of a reference mass travelling at speed
// makes numbers more intuitive
template <typename T1, typename T2> [[gnu::always_inline]] inline constexpr auto MASS_SPEED2ENERGY( T1 mass, T2 speed ) { return ((speed)*(speed)*(mass)); }

// energy of a 10kg mass moving at speed
template <typename T1> [[gnu::always_inline]] inline auto MASS10_SPEED2ENERGY( T1 speed ) { return MASS_SPEED2ENERGY(10,speed); }

template <typename T1, typename T2> [[gnu::always_inline]] inline auto MASS_ENERGY2SPEED( T1 mass, T2 energy ) { return (FastSqrt((energy)/mass)); }

inline constexpr auto ENERGY_VOLUME_SCALE = (1.0f / 15500.0f);

inline constexpr auto FLUID_TIME_MAX = 2.0f; // keep track of last time hitting fluid for up to 2 seconds

//-----------------------------------------------------------------------------
// Purpose: A little cache of current objects making noises
//-----------------------------------------------------------------------------
struct friction_t
{
	CSoundPatch	*patch;
	CBaseEntity	*pObject;
	float		flLastUpdateTime;
	float		flLastEffectTime;
};

enum
{
	TOUCH_START=0,
	TOUCH_END,
};

struct touchevent_t
{
	CBaseEntity *pEntity0;
	CBaseEntity *pEntity1;
	int			touchType;
	Vector		endPoint;	//sv
	Vector		normal;		//sv
};

struct fluidevent_t
{
	EHANDLE			hEntity;
	float			impactTime;
};

void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit );
void PhysFrictionSound( CBaseEntity *pEntity, IPhysicsObject *pObject, const char *pSoundName, HSOUNDSCRIPTHASH& handle, float flVolume );
void PhysCleanupFrictionSounds( CBaseEntity *pEntity );
void PhysFrictionEffect( Vector &vecPos, Vector vecVel, float energy, int surfaceProps, int surfacePropsHit );
float PhysGetEntityMass( CBaseEntity *pEntity );

// Convenience routine
// ORs gameFlags with the physics object's current game flags
inline unsigned short PhysSetGameFlags( IPhysicsObject *pPhys, unsigned short gameFlags )
{
	unsigned short flags = pPhys->GetGameFlags();
	flags |= gameFlags;
	pPhys->SetGameFlags( flags );
	
	return flags;
}
// mask off gameFlags
inline unsigned short PhysClearGameFlags( IPhysicsObject *pPhys, unsigned short gameFlags )
{
	unsigned short flags = pPhys->GetGameFlags();
	flags &= ~gameFlags;
	pPhys->SetGameFlags( flags );
	
	return flags;
}

// Create a vphysics object based on a model
IPhysicsObject *PhysModelCreate( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles, solid_t *pSolid = nullptr );

IPhysicsObject *PhysModelCreateBox( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, bool isStatic );
IPhysicsObject *PhysModelCreateOBB( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, const QAngle &angle, bool isStatic );

// Create a vphysics object based on a BSP model (unmoveable)
IPhysicsObject *PhysModelCreateUnmoveable( CBaseEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles );

// Create a vphysics object based on an existing collision model
IPhysicsObject *PhysModelCreateCustom( CBaseEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *pName, bool isStatic, solid_t *pSolid = nullptr );

// Create a bbox collision model (these may be shared among entities, they are auto-deleted at end of level. do not manage)
CPhysCollide *PhysCreateBbox( const Vector &mins, const Vector &maxs );

// Create a vphysics sphere object
IPhysicsObject *PhysSphereCreate( CBaseEntity *pEntity, float radius, const Vector &origin, solid_t &solid );

// Destroy a physics object created using PhysModelCreate...()
void PhysDestroyObject( IPhysicsObject *pObject, CBaseEntity *pEntity = nullptr );

void PhysDisableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysDisableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysDisableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
void PhysEnableObjectCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysEnableEntityCollisions( IPhysicsObject *pObject0, IPhysicsObject *pObject1 );
void PhysEnableEntityCollisions( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
bool PhysEntityCollisionsAreDisabled( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );

// create the world physics objects
IPhysicsObject *PhysCreateWorld_Shared( CBaseEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams );

// parse the parameters for a single solid from the model's collision data
bool PhysModelParseSolid( solid_t &solid, CBaseEntity *pEntity, int modelIndex );
// parse the parameters for a solid matching a particular index
bool PhysModelParseSolidByIndex( solid_t &solid, CBaseEntity *pEntity, int modelIndex, int solidIndex );

void PhysParseSurfaceData( class IPhysicsSurfaceProps *pProps, class IFileSystem *pFileSystem );

// fill out this solid_t with the AABB defaults (high inertia/no rotation)
void PhysGetDefaultAABBSolid( solid_t &solid );

// Compute an output velocity based on sliding along the current contact points 
// in the closest direction toward inputVelocity.
void PhysComputeSlideDirection( IPhysicsObject *pPhysics, const Vector &inputVelocity, const AngularImpulse &inputAngularVelocity, 
							   Vector *pOutputVelocity, Vector *pOutputAngularVelocity, float minMass );

void PhysForceClearVelocity( IPhysicsObject *pPhys );
bool PhysHasContactWithOtherInDirection( IPhysicsObject *pPhysics, const Vector &dir );

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* PhysicsGameSystem();

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CBreakable;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;


// ---------------------------------------------------------------------
//
// CPhysBox -- physically simulated brush rectangular solid
//
// ---------------------------------------------------------------------
// Physbox Spawnflags. Start at 0x01000 to avoid collision with CBreakable's
inline constexpr auto SF_PHYSBOX_ASLEEP = 0x01000;
inline constexpr auto SF_PHYSBOX_IGNOREUSE = 0x02000;
inline constexpr auto SF_PHYSBOX_DEBRIS = 0x04000;
inline constexpr auto SF_PHYSBOX_MOTIONDISABLED = 0x08000;
inline constexpr auto SF_PHYSBOX_USEPREFERRED = 0x10000;
inline constexpr auto SF_PHYSBOX_ENABLE_ON_PHYSCANNON = 0x20000;
inline constexpr auto SF_PHYSBOX_NO_ROTORWASH_PUSH = 0x40000; // The rotorwash doesn't push these
inline constexpr auto SF_PHYSBOX_ENABLE_PICKUP_OUTPUT = 0x80000;
inline constexpr auto SF_PHYSBOX_ALWAYS_PICK_UP = 0x100000; // Physcannon can always pick this up, no matter what mass or constraints may apply.
inline constexpr auto SF_PHYSBOX_NEVER_PICK_UP = 0x200000; // Physcannon will never be able to pick this up.
inline constexpr auto SF_PHYSBOX_NEVER_PUNT = 0x400000; // Physcannon will never be able to punt this object.
inline constexpr auto SF_PHYSBOX_PREVENT_PLAYER_TOUCH_ENABLE = 0x800000; // If set, the player will not cause the object to enable its motion when bumped into

// UNDONE: Hook collisions into the physics system to generate touch functions and take damage on falls
// UNDONE: Base class PhysBrush
class CPhysBox : public CBreakable
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysBox" } ]]
	static constexpr bool ks_class = true;

DECLARE_CLASS( CPhysBox, CBreakable );

public:
	DECLARE_SERVERCLASS();

	void	Spawn_Impl ( void );
	bool	CreateVPhysics_Impl();
	void	Move( const Vector &force );
	int ObjectCaps_Impl();
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	int DrawDebugTextOverlays_Impl(void);

	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	int		OnTakeDamage_Impl( const CTakeDamageInfo &info );
	void		 EnableMotion( void );

	bool CanBePickedUpByPhyscannon();

	// IPlayerPickupVPhysics
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );

	bool		 HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle PreferredCarryAngles( void ) { return m_angPreferredCarryAngles; }

	int			ExploitableByPlayer() const { return m_iExploitableByPlayer; }

	// inputs
	[[= ks::reflect::Input{ .name = "Wake", .type = FIELD_VOID } ]] void InputWake( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Sleep", .type = FIELD_VOID } ]] void InputSleep( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableMotion", .type = FIELD_VOID } ]] void InputEnableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableMotion", .type = FIELD_VOID } ]] void InputDisableMotion( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ForceDrop", .type = FIELD_VOID } ]] void InputForceDrop( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableFloating", .type = FIELD_VOID } ]] void InputDisableFloating( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "BecomeDebris", .type = FIELD_VOID } ]] void InputBecomeDebris( inputdata_t &inputdata );

	DECLARE_DATADESC();
	
protected:
	[[= ks::reflect::Key{ .name = "Damagetype" } ]] int				m_damageType;
	[[= ks::reflect::Key{ .name = "massScale" } ]] float			m_massScale;
	[[= ks::reflect::Key{ .name = "overridescript" } ]] string_t		m_iszOverrideScript;
	[[= ks::reflect::Key{ .name = "damagetoenablemotion" } ]] int				m_damageToEnableMotion;
	[[= ks::reflect::Key{ .name = "forcetoenablemotion" } ]] float			m_flForceToEnableMotion;
	[[= ks::reflect::Key{ .name = "preferredcarryangles" } ]] QAngle			m_angPreferredCarryAngles;
	[[= ks::reflect::Key{ .name = "notsolid" } ]] bool			m_bNotSolidToWorld;
	[[= ks::reflect::Key{ .name = "ExploitableByPlayer" } ]] int				m_iExploitableByPlayer;

	// Outputs
	[[= ks::reflect::Key{ .name = "OnDamaged" } ]] COutputEvent	m_OnDamaged;
	[[= ks::reflect::Key{ .name = "OnAwakened" } ]] COutputEvent	m_OnAwakened;
	[[= ks::reflect::Key{ .name = "OnMotionEnabled" } ]] COutputEvent	m_OnMotionEnabled;
	[[= ks::reflect::Key{ .name = "OnPhysGunPickup" } ]] COutputEvent	m_OnPhysGunPickup;
	[[= ks::reflect::Key{ .name = "OnPhysGunPunt" } ]] COutputEvent	m_OnPhysGunPunt;
	[[= ks::reflect::Key{ .name = "OnPhysGunOnlyPickup" } ]] COutputEvent	m_OnPhysGunOnlyPickup;
	[[= ks::reflect::Key{ .name = "OnPhysGunDrop" } ]] COutputEvent	m_OnPhysGunDrop;
	[[= ks::reflect::Key{ .name = "OnPlayerUse" } ]] COutputEvent	m_OnPlayerUse;

	CHandle<CBasePlayer>	m_hCarryingPlayer;	// Player who's carrying us
};

// ---------------------------------------------------------------------
//
// CPhysExplosion -- physically simulated explosion
//
// ---------------------------------------------------------------------
inline constexpr auto SF_PHYSEXPLOSION_NODAMAGE = 0x0001;
inline constexpr auto SF_PHYSEXPLOSION_PUSH_PLAYER = 0x0002;
inline constexpr auto SF_PHYSEXPLOSION_RADIAL = 0x0004;
inline constexpr auto SF_PHYSEXPLOSION_TEST_LOS = 0x0008;
inline constexpr auto SF_PHYSEXPLOSION_DISORIENT_PLAYER = 0x0010;

class CPhysExplosion : public CPointEntity
{
public:
	DECLARE_CLASS( CPhysExplosion, CPointEntity );

	void	Spawn_Impl ( void );
	void	Explode( CBaseEntity *pActivator, CBaseEntity *pCaller );
	void	ExplodeAndRemove( CBaseEntity *pActivator, CBaseEntity *pCaller );

	CBaseEntity *FindEntity( CBaseEntity *pEntity, CBaseEntity *pActivator, CBaseEntity *pCaller );

	int DrawDebugTextOverlays_Impl(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "Explode", .type = FIELD_VOID } ]] void InputExplode( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ExplodeAndRemove", .type = FIELD_VOID } ]] void InputExplodeAndRemove( inputdata_t &inputdata );

	DECLARE_DATADESC();
	
	float		GetRadius( void );
	[[= ks::reflect::Key{ .name = "magnitude" } ]] float		m_damage;
	[[= ks::reflect::Key{ .name = "radius" } ]] float		m_radius;
	[[= ks::reflect::Key{ .name = "targetentityname" } ]] string_t	m_targetEntityName;
	[[= ks::reflect::Key{ .name = "inner_radius" } ]] float		m_flInnerRadius;
	
	[[= ks::reflect::Key{ .name = "OnPushedPlayer" } ]] COutputEvent	m_OnPushedPlayer;	
};

void CreatePhysExplosion( Vector origin, float magnitude, float radius, string_t target, float innerRadius, int flags );

//==================================================
// CPhysImpact
//==================================================

class CPhysImpact : public CPointEntity
{
public:
	DECLARE_CLASS( CPhysImpact, CPointEntity );

	void		Spawn_Impl( void );
	//void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		Activate_Impl( void );

	[[= ks::reflect::Input{ .name = "Impact", .type = FIELD_VOID } ]] void		InputImpact( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	void		PointAtEntity( void );

	[[= ks::reflect::Key{ .name = "magnitude" } ]] float		m_damage;
	[[= ks::reflect::Key{ .name = "distance" } ]] float		m_distance;
	[[= ks::reflect::Key{ .name = "directionentityname" } ]] string_t	m_directionEntityName;
};

//-----------------------------------------------------------------------------
// Purpose: A magnet that creates constraints between itself and anything it touches 
//-----------------------------------------------------------------------------

struct magnetted_objects_t
{
	IPhysicsConstraint *pConstraint;
	EHANDLE			   hEntity;

	DECLARE_SIMPLE_DATADESC();
};

class CPhysMagnet : public CBaseAnimating, public IPhysicsConstraintEvent
{
	[[= ks::reflect::NetTable{ .name = "DT_PhysMagnet" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CPhysMagnet, CBaseAnimating );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPhysMagnet();
	~CPhysMagnet();

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	Touch_Impl( CBaseEntity *pOther );
	void	VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	void	DoMagnetSuck( CBaseEntity *pOther );
	void	SetConstraintGroup( IPhysicsConstraintGroup *pGroup );

	bool	IsOn( void ) { return m_bActive; }
	int		GetNumAttachedObjects( void );
	float	GetTotalMassAttachedObjects( void );
	CBaseEntity *GetAttachedObject( int iIndex );

	// Checking for hitting something
	void	ResetHasHitSomething( void ) { m_bHasHitSomething = false; }
	bool	HasHitSomething( void ) { return m_bHasHitSomething; }

	// Inputs
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff( inputdata_t &inputdata );

	void	InputConstraintBroken( inputdata_t &inputdata );

	void	DetachAll( void );

// IPhysicsConstraintEvent
public:
	void	ConstraintBroken( IPhysicsConstraint *pConstraint );
	
protected:
	// Outputs
	[[= ks::reflect::Key{ .name = "OnAttach" } ]] COutputEvent	m_OnMagnetAttach;
	[[= ks::reflect::Key{ .name = "OnDetach" } ]] COutputEvent	m_OnMagnetDetach;

	// Keys
	[[= ks::reflect::Key{ .name = "massScale" } ]] float			m_massScale;
	[[= ks::reflect::Key{ .name = "overridescript" } ]] string_t		m_iszOverrideScript;
	[[= ks::reflect::Key{ .name = "forcelimit" } ]] float			m_forceLimit;
	[[= ks::reflect::Key{ .name = "torquelimit" } ]] float			m_torqueLimit;

	std::vector< magnetted_objects_t >	m_MagnettedEntities;
	IPhysicsConstraintGroup				*m_pConstraintGroup;

	bool			m_bActive;
	bool			m_bHasHitSomething;
	float			m_flTotalMass;
	float			m_flRadius;
	float			m_flNextSuckTime;
	[[= ks::reflect::Key{ .name = "maxobjects" } ]] int				m_iMaxObjectsAttached;
};

using namespace ::ks::reflect;

//==================================================
// CPlasma
//==================================================

//NOTENOTE: Mirrored in cl_dll/c_plasma.cpp
inline constexpr auto bitsPLASMA_FREE = 0x00000002;

class CPlasma : public CBaseFire
{
	[[= ks::reflect::NetTable{ .name = "DT_Plasma" } ]]
	[[= ks::reflect::From<"m_flScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_flScaleTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
	[[= ks::reflect::From<"m_nFlags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPlasma, CBaseFire );

	CPlasma( void );
	virtual	~CPlasma( void );
	void	EnableSmoke( int state );

	void	Precache_Impl( void );

	DECLARE_SERVERCLASS();

public:

	//Client-side
	CNetworkVar( int, m_nPlasmaModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nPlasmaModelIndex2, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );
	CNetworkVar( int, m_nGlowModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::ModelIndex } ]] );

	//Server-side
};

class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CSteamJet;
class CTakeDamageInfo;
struct gamevcollisionevent_t;
struct inputdata_t;

using namespace ::ks::reflect;



class CThrustController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	void CalcThrust( const Vector &position, const Vector &direction, IPhysicsObject *pPhys );

	Vector			m_thrustVector;
	AngularImpulse	m_torqueVector;
	[[= ks::reflect::Key{ .name = "thrust" } ]] float			m_thrust;
};

class CPhysicsCannister : public CBaseCombatCharacter, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS( CPhysicsCannister, CBaseCombatCharacter );
public:
	~CPhysicsCannister( void );

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void OnRestore_Impl();
	bool CreateVPhysics_Impl();

	DECLARE_DATADESC();
	void VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );

	virtual QAngle PreferredCarryAngles( void ) { return QAngle( -90, 0, 0 ); }
	virtual bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }

	//
	// Input handlers.
	//
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void InputActivate(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void InputDeactivate(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Explode", .type = FIELD_VOID } ]] void InputExplode(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Wake", .type = FIELD_VOID } ]] void InputWake( inputdata_t &data );

	bool TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace );

	int OnTakeDamage_Impl( const CTakeDamageInfo &info );

	int ObjectCaps_Impl() 
	{ 
		return (BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE);
	}
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void CannisterActivate( CBaseEntity *pActivator, const Vector &thrustOffset );
	void CannisterFire( CBaseEntity *pActivator );
	void Deactivate( void );
	void Explode( CBaseEntity *pAttacker );
	void ExplodeTouch( CBaseEntity *pOther );
	void VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );

	// Don't treat as a live target
	virtual bool IsAlive( void ) { return false; }

	void TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &dir, trace_t *ptr );

	void	ShutdownJet( void );
	void	BeginShutdownThink( void );

public:
	virtual bool OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason ) { return true; }
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	CBasePlayer *HasPhysicsAttacker_Impl( float dt );
	virtual bool ShouldPuntUseLaunchForces( PhysGunForce_t reason );
	virtual AngularImpulse PhysGunLaunchAngularImpulse( void ) { return vec3_origin; }
	virtual Vector PhysGunLaunchVelocity( const Vector &forward, float flMass ) { return vec3_origin; }

protected:
	void SetPhysicsAttacker( CBasePlayer *pEntity, float flTime );

public:
	Vector				m_thrustOrigin;
	CThrustController	m_thruster;
	IPhysicsMotionController *m_pController;
	CSteamJet			*m_pJet;
	bool				m_active;
	[[= ks::reflect::Key{ .name = "fuel" } ]] float				m_thrustTime;
	[[= ks::reflect::Key{ .name = "expdamage" } ]] float				m_damage;
	[[= ks::reflect::Key{ .name = "expradius" } ]] float				m_damageRadius;

	float				m_activateTime;
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "gassound" } ]] string_t			m_gasSound;

	bool				m_bFired;		// True if this cannister was fire by a weapon

	[[= ks::reflect::Key{ .name = "OnActivate" } ]] COutputEvent		m_onActivate;
	[[= ks::reflect::Key{ .name = "OnAwakened" } ]] COutputEvent		m_OnAwakened;

	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;
	EHANDLE					m_hLauncher;	// Entity that caused this cannister to launch

private:
	Vector CalcLocalThrust( const Vector &offset );
};

class CCollisionEvent;

class CBaseEntity;
class CTakeDamageInfo;
struct gamevcollisionevent_t;

using namespace ::ks::reflect;

// Purpose: Pulling CCollisionEvent's definition out of physics.cpp so it can be abstracted upon (for the portal mod)

extern CCallQueue g_PostSimulationQueue;

struct damageevent_t
{
	CBaseEntity		*pEntity;
	IPhysicsObject	*pInflictorPhysics;
	CTakeDamageInfo	info;
	bool			bRestoreVelocity;
};

struct inflictorstate_t
{
	Vector			savedVelocity;
	AngularImpulse	savedAngularVelocity;
	IPhysicsObject	*pInflictorPhysics;
	float			otherMassMax;
	short			nextIndex;
	short			restored;
};

enum
{
	COLLSTATE_ENABLED = 0,
	COLLSTATE_TRYDISABLE = 1,
	COLLSTATE_TRYNPCSOLVER = 2,
	COLLSTATE_TRYENTITYSOLVER = 3,
	COLLSTATE_DISABLED = 4
};

struct penetrateevent_t
{
	EHANDLE			hEntity0;
	EHANDLE			hEntity1;
	float			startTime;
	float			timeStamp;
	int				collisionState;
};

class CCollisionEvent : public IPhysicsCollisionEvent, public IPhysicsCollisionSolver, public IPhysicsObjectEvent
{
public:
	CCollisionEvent();
	friction_t *FindFriction( CBaseEntity *pObject );
	void ShutdownFriction( friction_t &friction );
	void FrameUpdate();
	void LevelShutdown( void );

	// IPhysicsCollisionEvent
	void PreCollision( vcollisionevent_t *pEvent );
	void PostCollision( vcollisionevent_t *pEvent );
	void Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData );
	void StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData );
	void EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData );
	void FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid );
	void FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid );
	void PostSimulationFrame();
	void ObjectEnterTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject );
	void ObjectLeaveTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject );
	
	bool GetTriggerEvent( triggerevent_t *pEvent, CBaseEntity *pTriggerEntity );
	void BufferTouchEvents( bool enable ) { m_bBufferTouchEvents = enable; }
	virtual void AddDamageEvent( CBaseEntity *pEntity, const CTakeDamageInfo &info, IPhysicsObject *pInflictorPhysics, bool bRestoreVelocity, const Vector &savedVel, const AngularImpulse &savedAngVel );
	void AddImpulseEvent( IPhysicsObject *pPhysicsObject, const Vector &vecCenterForce, const AngularImpulse &vecCenterTorque );
	void AddSetVelocityEvent( IPhysicsObject *pPhysicsObject, const Vector &vecVelocity );
	void AddRemoveObject(IServerNetworkable *pRemove);
	void FlushQueuedOperations();

	// IPhysicsCollisionSolver
	int		ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 );
	int		ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt );
	bool	ShouldFreezeObject( IPhysicsObject *pObject );
	static const char *ModuleName() { return CBaseEntity::IsServer() ? "SERVER" : "CLIENT"; }
	int		AdditionalCollisionChecksThisTick( int currentChecksDone ) 
	{
		//CallbackContext check(this);
		if ( currentChecksDone < 1200 )
		{
			DevMsg(1,"%s: VPhysics Collision detection getting expensive, check for too many convex pieces!\n", ModuleName());
			return 1200 - currentChecksDone;
		}
		DevMsg(1,"%s: VPhysics exceeded collision check limit (%d)!!!\nInterpenetration may result!\n", ModuleName(), currentChecksDone );
		return 0; 
	}
	bool ShouldFreezeContacts( IPhysicsObject **pObjectList, int objectCount );

	// IPhysicsObjectEvent
	// these can be used to optimize out queries on sleeping objects
	// Called when an object is woken after sleeping
	virtual void ObjectWake( IPhysicsObject *pObject );
	// called when an object goes to sleep (no longer simulating)
	virtual void ObjectSleep( IPhysicsObject *pObject );

	// locals
	bool GetInflictorVelocity( IPhysicsObject *pInflictor, Vector &velocity, AngularImpulse &angVelocity );

	void GetListOfPenetratingEntities( CBaseEntity *pSearch, std::vector< CBaseEntity * > &list );
	bool IsInCallback() { return m_inCallback > 0 ? true : false; }

	void RemovePenetrationEvents( CBaseEntity *pEntity );

private:

	void UpdateFrictionSounds();
	void UpdateTouchEvents();
	void UpdateDamageEvents();
	void UpdatePenetrateEvents( void );
	void UpdateFluidEvents();
	void UpdateRemoveObjects();
	void AddTouchEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1, int touchType, const Vector &point, const Vector &normal );
	penetrateevent_t &FindOrAddPenetrateEvent( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
	float DeltaTimeSinceLastFluid( CBaseEntity *pEntity );

	void RestoreDamageInflictorState( IPhysicsObject *pInflictor );
	void RestoreDamageInflictorState( int inflictorStateIndex, float velocityBlend );
	int AddDamageInflictor( IPhysicsObject *pInflictorPhysics, float otherMass, const Vector &savedVel, const AngularImpulse &savedAngVel, bool addList );
	int	FindDamageInflictor( IPhysicsObject *pInflictorPhysics );

	// make the call into the entity system
	void DispatchStartTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1, const Vector &point, const Vector &normal );
	void DispatchEndTouch( CBaseEntity *pEntity0, CBaseEntity *pEntity1 );
	
	class CallbackContext
	{
	public:
		CallbackContext(CCollisionEvent *pOuter)
		{
			m_pOuter = pOuter;
			m_pOuter->m_inCallback++;
		}
		~CallbackContext()
		{
			m_pOuter->m_inCallback--;
		}
	private:
		CCollisionEvent *m_pOuter;
	};
	friend class CallbackContext;
	
	friction_t					m_current[4];
	gamevcollisionevent_t		m_gameEvent;
	std::vector< triggerevent_t >	m_triggerEvents;
	triggerevent_t				m_currentTriggerEvent;
	std::vector< touchevent_t >	m_touchEvents;
	std::vector< damageevent_t >	m_damageEvents;
	std::vector< inflictorstate_t >	m_damageInflictors;
	std::vector< penetrateevent_t > m_penetrateEvents;
	std::vector< fluidevent_t >	m_fluidEvents;
	std::vector< IServerNetworkable * > m_removeObjects;
	int							m_inCallback;
	int							m_lastTickFrictionError;	// counter to control printing of the dev warning for large contact systems
	bool						m_bBufferTouchEvents;
};

class CBaseEntity;



extern CBaseEntity *EntityPhysics_CreateSolver( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsBlocker, bool disableCollisions, float separationDuration );

class CBaseEntity;
class CBasePlayer;
class CTeam;

using namespace ::ks::reflect;



//-----------------------------------------------------------------------------
// Purpose: A generic filter for determining whom to send message/sounds etc. to and
//  providing a bit of additional state information
//-----------------------------------------------------------------------------
class CRecipientFilter : public IRecipientFilter
{
public:
					CRecipientFilter();
	virtual 		~CRecipientFilter();

	virtual bool	IsReliable( void ) const;
	virtual bool	IsInitMessage( void ) const;

	virtual int		GetRecipientCount( void ) const;
	virtual int		GetRecipientIndex( int slot ) const;

public:

	void			CopyFrom( const CRecipientFilter& src );

	void			Reset( void );

	void			MakeInitMessage( void );

	void			MakeReliable( void );
	
	void			AddAllPlayers( void );
	void			AddRecipientsByPVS( const Vector& origin );
	void			RemoveRecipientsByPVS( const Vector& origin );
	void			AddRecipientsByPAS( const Vector& origin );
	void			AddRecipient( CBasePlayer *player );
	void			RemoveAllRecipients( void );
	void			RemoveRecipient( CBasePlayer *player );
	void			RemoveRecipientByPlayerIndex( int playerindex );
	void			AddRecipientsByTeam( CTeam *team );
	void			RemoveRecipientsByTeam( CTeam *team );
	void			RemoveRecipientsNotOnTeam( CTeam *team );

	void			UsePredictionRules( void );
	bool			IsUsingPredictionRules( void ) const;

	bool			IgnorePredictionCull( void ) const;
	void			SetIgnorePredictionCull( bool ignore );

	void			AddPlayersFromBitMask( CPlayerBitVec& playerbits );
	void			RemovePlayersFromBitMask( CPlayerBitVec& playerbits );

	void			RemoveSplitScreenPlayers();
	void			ReplaceSplitScreenPlayersWithOwners();

	void			RemoveDuplicateRecipients();

protected:

	bool				m_bReliable;
	bool				m_bInitMessage;
	std::vector< int >	m_Recipients;
	
	// If using prediction rules, the filter itself suppresses local player
	bool				m_bUsingPredictionRules;
	// If ignoring prediction cull, then external systems can determine
	//  whether this is a special case where culling should not occur
	bool				m_bIgnorePredictionCull;
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for a single player ( unreliable )
//-----------------------------------------------------------------------------
class CSingleUserRecipientFilter : public CRecipientFilter
{
public:
	CSingleUserRecipientFilter( CBasePlayer *player )
	{
		AddRecipient( player );
	}
};

class CSingleUserAndReplayRecipientFilter : public CRecipientFilter
{
public:
	CSingleUserAndReplayRecipientFilter( CBasePlayer *player );
};
//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players on a given team 
//-----------------------------------------------------------------------------
class CTeamRecipientFilter : public CRecipientFilter
{
public:
	CTeamRecipientFilter( int team, bool isReliable = false );
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players ( unreliable )
//-----------------------------------------------------------------------------
class CBroadcastRecipientFilter : public CRecipientFilter
{
public:
	CBroadcastRecipientFilter( void )
	{
		AddAllPlayers();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Simple class to create a filter for all players ( reliable )
//-----------------------------------------------------------------------------
class CReliableBroadcastRecipientFilter : public CBroadcastRecipientFilter
{
public:
	CReliableBroadcastRecipientFilter( void )
	{
		MakeReliable();
	}
};

//-----------------------------------------------------------------------------
// Purpose: Add players in PAS to recipient list (unreliable)
//-----------------------------------------------------------------------------
class CPASFilter : public CRecipientFilter
{
public:
	CPASFilter( void )
	{
	}

	CPASFilter( const Vector& origin )
	{
		AddRecipientsByPAS( origin );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Add players in PAS to list and if not in single player, use attenuation
//  to remove those that are too far away from source origin
// Source origin can be stated as an entity or just a passed in origin
// (unreliable)
//-----------------------------------------------------------------------------
class CPASAttenuationFilter : public CPASFilter
{
public:
	CPASAttenuationFilter( void )
	{
	}

	CPASAttenuationFilter( CBaseEntity *entity, soundlevel_t soundlevel ) :
		CPASFilter( static_cast<const Vector&>(entity->GetSoundEmissionOrigin()) )
	{
		Filter( entity->GetSoundEmissionOrigin(), SNDLVL_TO_ATTN( soundlevel ) );
	}

	CPASAttenuationFilter( CBaseEntity *entity, float attenuation = ATTN_NORM ) :
		CPASFilter( static_cast<const Vector&>(entity->GetSoundEmissionOrigin()) )
	{
		Filter( entity->GetSoundEmissionOrigin(), attenuation );
	}

	CPASAttenuationFilter( const Vector& origin, soundlevel_t soundlevel ) :
		CPASFilter( origin )
	{
		Filter( origin, SNDLVL_TO_ATTN( soundlevel ) );
	}

	CPASAttenuationFilter( const Vector& origin, float attenuation = ATTN_NORM ) :
		CPASFilter( origin )
	{
		Filter( origin, attenuation );
	}

	CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound );

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound );

	CPASAttenuationFilter( CBaseEntity *entity, const char *lookupSound, HSOUNDSCRIPTHASH& handle );

	CPASAttenuationFilter( const Vector& origin, const char *lookupSound, HSOUNDSCRIPTHASH& handle );

public:
	void Filter( const Vector& origin, float attenuation = ATTN_NORM );
};

//-----------------------------------------------------------------------------
// Purpose: Simple PVS based filter ( unreliable )
//-----------------------------------------------------------------------------
class CPVSFilter : public CRecipientFilter
{
public:
	CPVSFilter( const Vector& origin )
	{
		AddRecipientsByPVS( origin );
	}
};

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;


class CRopeKeyframe : public CBaseEntity, public IPositionWatcher
{
	[[= ks::reflect::NetTable{ .name = "DT_RopeKeyframe", .base = false } ]]
	[[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
	[[= ks::reflect::From<"m_iParentAttachment", ks::reflect::Net{ .bits = NUM_PARENTATTACHMENT_BITS, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nMinCPULevel", ks::reflect::Net{ .bits = CPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nMaxCPULevel", ks::reflect::Net{ .bits = CPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nMinGPULevel", ks::reflect::Net{ .bits = GPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_nMaxGPULevel", ks::reflect::Net{ .bits = GPU_LEVEL_BIT_COUNT, .flags = SPROP_UNSIGNED }>{} ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CRopeKeyframe, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

					CRopeKeyframe();
	virtual			~CRopeKeyframe();

	// Create a rope and attach it to two entities.
	// Attachment points on the entities are optional.
	static CRopeKeyframe* Create(
		CBaseEntity *pStartEnt,
		CBaseEntity *pEndEnt,
		int iStartAttachment=0,
		int iEndAttachment=0,
		int ropeWidth = 2,
		const char *pMaterialName = "cable/cable.vmt",		// Note: whoever creates the rope must
															// use PrecacheModel for whatever material
															// it specifies here.
		int numSegments = 5,
		const char *pClassName = "keyframe_rope"
		);

	static CRopeKeyframe* CreateWithSecondPointDetached(
		CBaseEntity *pStartEnt,
		int iStartAttachment = 0,	// must be 0 if you don't want to use a specific model attachment.
		int ropeLength = 20,
		int ropeWidth = 2,
		const char *pMaterialName = "cable/cable.vmt",		// Note: whoever creates the rope
															// use PrecacheModel for whatever material
															// it specifies here.
		int numSegments = 5,
		bool bInitialHang = false,
		const char *pClassName = "keyframe_rope"
		);

	bool		SetupHangDistance( float flHangDist );
	void		ActivateStartDirectionConstraints( bool bEnable );
	void		ActivateEndDirectionConstraints( bool bEnable );

	// Shakes all ropes near vCenter. The higher flMagnitude is, the larger the shake will be.
	static void ShakeRopes( const Vector &vCenter, float flRadius, float flMagnitude );
	static void PrecacheShakeRopes();

// CBaseEntity overrides.
public:
	
	// don't cross transitions
	int		ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }
	void	Activate_Impl();
	void	Precache_Impl();
	void	Spawn_Impl( void );
	int		OnTakeDamage_Impl( const CTakeDamageInfo &info );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	void			PropagateForce(CBaseEntity *pActivator, CBaseEntity *pCaller, CBaseEntity *pFirstLink, float x, float y, float z);

	// Once-off length recalculation
	void			RecalculateLength( void );

	// Kill myself when I next come to rest
	void			DieAtNextRest( void );

	int		UpdateTransmitState_Impl(void);
	void	SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );
	void	SetParent_Impl( CBaseEntity *pParentEntity, int iAttachment );

// Input functions.
public:

	[[= ks::reflect::Input{ .name = "SetScrollSpeed", .type = FIELD_FLOAT } ]] void InputSetScrollSpeed( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetForce", .type = FIELD_VECTOR } ]] void InputSetForce( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Break", .type = FIELD_VOID } ]] void InputBreak( inputdata_t &inputdata );

public:

	bool			Break( void );
	void			DetachPoint( int iPoint );
	
	void			EndpointsChanged();

	// By default, ropes don't collide with the world. Call this to enable it.
	void			EnableCollision();

	// Toggle wind.
	void			EnableWind( bool bEnable );

	CBaseEntity*	GetStartPoint() { return m_hStartPoint.Get(); }
	int				GetStartAttachment() { return m_iStartAttachment; };

	CBaseEntity*	GetEndPoint() { return m_hEndPoint.Get(); }
	int				GetEndAttachment() { return m_iEndAttachment; };

	void			SetStartPoint( CBaseEntity *pStartPoint, int attachment = 0 );
	void			SetEndPoint( CBaseEntity *pEndPoint, int attachment = 0 );

	// See ROPE_PLAYER_WPN_ATTACH for info.
	void			EnablePlayerWeaponAttach( bool bAttach );

	// IPositionWatcher
	virtual void NotifyPositionChanged( CBaseEntity *pEntity );

private:
	// Unless this is called during initialization, the caller should have done
	// PrecacheModel on whatever material they specify in here.
	void			SetMaterial( const char *pName );

protected:
	void			SetAttachmentPoint( CBaseHandle &hOutEnt, short &iOutAttachment, CBaseEntity *pEnt, int iAttachment );

	// This is normally called by Activate but if you create the rope at runtime,
	// you must call it after you have setup its variables.
	virtual void	Init();

private:
	// These work just like the client-side versions.
	bool			GetEndPointPos2( CBaseEntity *pEnt, int iAttachment, Vector &v );
	bool			GetEndPointPos( int iPt, Vector &v );

	void			UpdateBBox( bool bForceRelink );

public:

	CNetworkVar( int, m_RopeFlags, [[= ks::reflect::Net{ .bits = ROPE_NUMFLAGS, .flags = SPROP_UNSIGNED } ]] );		// Combination of ROPE_ defines in rope_shared.h
	
	[[= ks::reflect::Key{ .name = "NextKey" } ]] string_t	m_iNextLinkName;
	CNetworkVar( int, m_Slack, [[= ks::reflect::Net{ .bits = 13 } ]] [[= ks::reflect::Key{ .name = "Slack" } ]] );
	CNetworkVar( float, m_Width, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Width" } ]] );
	CNetworkVar( float, m_TextureScale, [[= ks::reflect::Net{ .bits = 10, .low = 0.1f, .high = 10.0f } ]] [[= ks::reflect::Key{ .name = "TextureScale" } ]] );
	CNetworkVar( int, m_nSegments, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );		// Number of segments.
	CNetworkVar( bool, m_bConstrainBetweenEndpoints, [[= ks::reflect::Net{} ]] );

	string_t m_strRopeMaterialModel;
	CNetworkVar( int, m_iRopeMaterialModelIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );	// Index of sprite model with the rope's material.
	
	// Number of subdivisions in between segments.
	CNetworkVar( int, m_Subdiv, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "Subdiv" } ]] );
	
	// Used simply to wake up rope on the client side if it has gone to sleep
	CNetworkVar( unsigned char, m_nChangeCount, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );

	//EHANDLE		m_hNextLink;
	
	CNetworkVar( int, m_RopeLength, [[= ks::reflect::Net{ .bits = 15 } ]] );	// Rope length at startup, used to calculate tension.

	CNetworkVar( int, m_fLockedPoints, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );

	bool		m_bCreatedFromMapFile; // set to false when creating at runtime

	CNetworkVar( float, m_flScrollSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "ScrollSpeed" } ]] );

	CNetworkVar( int, m_iDefaultRopeMaterialModelIndex, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] );

private:
	// Used to detect changes.
	bool		m_bStartPointValid;
	bool		m_bEndPointValid;
	
	CNetworkHandle( CBaseEntity, m_hStartPoint, [[= ks::reflect::Net{} ]] );		// StartPoint/EndPoint are entities
	CNetworkHandle( CBaseEntity, m_hEndPoint, [[= ks::reflect::Net{} ]] );
	CNetworkVar( short, m_iStartAttachment, [[= ks::reflect::Net{ .bits = 5 } ]] );	// StartAttachment/EndAttachment are attachment points.
	CNetworkVar( short, m_iEndAttachment, [[= ks::reflect::Net{ .bits = 5 } ]] );
};

class CBaseEntity;
class CBaseFlex;

using namespace ::ks::reflect;


// List of the last 5 lines of speech from NPCs for bug reports
inline constexpr auto SPEECH_LIST_MAX_SOUNDS = 5;

struct recentNPCSpeech_t
{
	float	time;
	char	name[ 512 ];
	char	sceneName[ 128 ];
};

int GetRecentNPCSpeech( recentNPCSpeech_t speech[ SPEECH_LIST_MAX_SOUNDS ] );
float InstancedScriptedScene( CBaseFlex *pActor, const char *pszScene,  EHANDLE *phSceneEnt = nullptr, float flPostDelay = 0.0f, bool bIsBackground = false, AI_Response *response = nullptr, bool bMultiplayer = false, IRecipientFilter *filter = nullptr );
float InstancedAutoGeneratedSoundScene( CBaseFlex *pActor, char const *soundname, EHANDLE *phSceneEnt = nullptr );
void StopScriptedScene( CBaseFlex *pActor, EHANDLE hSceneEnt );
void RemoveActorFromScriptedScenes( CBaseFlex *pActor, bool instancedscenesonly, bool nonidlescenesonly = false, const char *pszThisSceneOnly = nullptr );
void PauseActorsScriptedScenes( CBaseFlex *pActor, bool instancedscenesonly );
void ResumeActorsScriptedScenes( CBaseFlex *pActor, bool instancedscenesonly );
void QueueActorsScriptedScenesToResume( CBaseFlex *pActor, bool instancedscenesonly );
bool IsRunningScriptedScene( CBaseFlex *pActor, bool bIgnoreInstancedScenes = true );
bool IsRunningScriptedSceneAndNotPaused( CBaseFlex *pActor, bool bIgnoreInstancedScenes = true );
bool IsRunningScriptedSceneWithSpeech( CBaseFlex *pActor, bool bIgnoreInstancedScenes = false );
bool IsRunningScriptedSceneWithSpeechAndNotPaused( CBaseFlex *pActor, bool bIgnoreInstancedScenes = false );
float GetSceneDuration( char const *pszScene );
int GetSceneSpeechCount( char const *pszScene );
bool IsInInterruptableScenes( CBaseFlex *pActor );

void PrecacheInstancedScene( char const *pszScene );
HSCRIPT ScriptCreateSceneEntity( char const *pszScene );

char const *GetSceneFilename( CBaseEntity *ent );
void ReloadSceneFromDisk( CBaseEntity *ent );

class CBaseEntity;

using namespace ::ks::reflect;

namespace physicssound
{
	struct impactsound_t
	{
		void			*pGameData;
		int				entityIndex;
		int				soundChannel;
		float			volume;
		float			impactSpeed;
		unsigned short	surfaceProps;
		unsigned short	surfacePropsHit;
		Vector			origin;
	};

	// UNDONE: Use a sorted container and sort by volume/distance?
	struct soundlist_t
	{
		std::vector< impactsound_t >	elements;
		impactsound_t	&GetElement(int index) { return elements[index]; }
		impactsound_t	&AddElement() { return elements.emplace_back(); }
		int Count() { return std::ssize( elements ); }
		void RemoveAll() { elements.clear(); }
	};

	void PlayImpactSounds( soundlist_t &list )
	{
		for ( int i = list.Count()-1; i >= 0; --i )
		{
			impactsound_t &sound = list.GetElement(i);
			const surfacedata_t *psurf = physprops->GetSurfaceData( sound.surfaceProps );
			if ( psurf->sounds.impactHard )
			{
				const surfacedata_t *pHit = physprops->GetSurfaceData( sound.surfacePropsHit );
				unsigned short soundName = psurf->sounds.impactHard;
				if ( pHit && psurf->sounds.impactSoft )
				{
					if ( pHit->audio.hardnessFactor < psurf->audio.hardThreshold || 
						(psurf->audio.hardVelocityThreshold > 0 && psurf->audio.hardVelocityThreshold > sound.impactSpeed) )
					{
						soundName = psurf->sounds.impactSoft;
					}
				}
				const char *pSound = physprops->GetString( soundName );

				CSoundParameters params;
				if ( !CBaseEntity::GetParametersForSound( pSound, params, nullptr ) )
					break;

				if ( sound.volume > 1 )
					sound.volume = 1;
				CBroadcastRecipientFilter filter;
				// JAY: If this entity gets deleted, the sound comes out at the world origin
				// this sounds bad!  Play on ent 0 for now.
				EmitSound_t ep;
				ep.m_nChannel = sound.soundChannel;
				ep.m_pSoundName = params.soundname;
				ep.m_flVolume = params.volume * sound.volume;
				ep.m_SoundLevel = params.soundlevel;
				ep.m_nPitch = params.pitch;
				ep.m_pOrigin = &sound.origin;
				ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
				ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

				CBaseEntity::EmitSound( filter, 0 /*sound.entityIndex*/, ep );
			}
		}
		list.RemoveAll();
	}
	void AddImpactSound( soundlist_t &list, void *pGameData, int entityIndex, int soundChannel, IPhysicsObject *pObject, int surfaceProps, int surfacePropsHit, float volume, float impactSpeed )
	{
		impactSpeed += 1e-4;
		for ( int i = list.Count()-1; i >= 0; --i )
		{
			impactsound_t &sound = list.GetElement(i);
			// UNDONE: Compare entity or channel somehow?
			// UNDONE: Doing one slot per entity is too noisy.  So now we use one slot per material

			// heuristic - after 4 impacts sounds in one frame, start merging everything
			if ( surfaceProps == sound.surfaceProps || list.Count() > 4 )
			{
				// UNDONE: Store instance volume separate from aggregate volume and compare that?
				if ( volume > sound.volume )
				{
					pObject->GetPosition( &sound.origin, nullptr );
					sound.pGameData = pGameData;
					sound.entityIndex = entityIndex;
					sound.soundChannel = soundChannel;
					sound.surfacePropsHit = surfacePropsHit;
				}
				sound.volume += volume;
				sound.impactSpeed = std::max(impactSpeed,sound.impactSpeed);
				return;
			}
		}

		impactsound_t &sound = list.AddElement();
		sound.pGameData = pGameData;
		sound.entityIndex = entityIndex;
		sound.soundChannel = soundChannel;
		pObject->GetPosition( &sound.origin, nullptr );
		sound.surfaceProps = surfaceProps;
		sound.surfacePropsHit = surfacePropsHit;
		sound.volume = volume;
		sound.impactSpeed = impactSpeed;
	}

	struct breaksound_t
	{
		Vector			origin;
		int				surfacePropsBreak;
	};

	void AddBreakSound( std::vector< breaksound_t > &list, const Vector &origin, unsigned short surfaceProps )
	{
		const surfacedata_t *psurf = physprops->GetSurfaceData( surfaceProps );
		if ( !psurf->sounds.breakSound )
			return;

		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			breaksound_t &sound = list[ i ];
			// Allow 3 break sounds before you start merging anything.
			if ( std::ssize( list ) > 2 && surfaceProps == sound.surfacePropsBreak )
			{
				sound.origin = (sound.origin + origin) * 0.5f;
				return;
			}
		}
		breaksound_t sound;
		sound.origin = origin;
		sound.surfacePropsBreak = surfaceProps;
		list.push_back( sound );

	}

	void PlayBreakSounds( std::vector< breaksound_t > &list )
	{
		for ( int i = std::ssize( list )-1; i >= 0; --i )
		{
			breaksound_t &sound = list[ i ];

			const surfacedata_t *psurf = physprops->GetSurfaceData( sound.surfacePropsBreak );
			const char *pSound = physprops->GetString( psurf->sounds.breakSound );
			CSoundParameters params;
			if ( !CBaseEntity::GetParametersForSound( pSound, params, nullptr ) )
				return;

			// Play from the world, because the entity is breaking, so it'll be destroyed soon
			CBroadcastRecipientFilter filter;
			EmitSound_t ep;
			ep.m_nChannel = CHAN_STATIC;
			ep.m_pSoundName = params.soundname;
			ep.m_flVolume = params.volume;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = params.pitch;
			ep.m_pOrigin = &sound.origin;
			ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
			ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

			CBaseEntity::EmitSound( filter, 0 /*sound.entityIndex*/, ep );
		}
		list.clear();
	}
};
