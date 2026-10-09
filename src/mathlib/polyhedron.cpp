// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Notes: For some semblance of clarity. All spatial orientation notation assumes you're
//			looking at the polyhedron from an outside point located spherically above
//			the primitive in question

import ks.math;
import std;

struct GeneratePolyhedronFromPlanes_Point;
struct GeneratePolyhedronFromPlanes_PointLL;
struct GeneratePolyhedronFromPlanes_Line;
struct GeneratePolyhedronFromPlanes_LineLL;
struct GeneratePolyhedronFromPlanes_Polygon;
struct GeneratePolyhedronFromPlanes_PolygonLL;

struct GeneratePolyhedronFromPlanes_UnorderedPointLL;
struct GeneratePolyhedronFromPlanes_UnorderedLineLL;
struct GeneratePolyhedronFromPlanes_UnorderedPolygonLL;

Vector FindPointInPlanes( const float *pPlanes, int planeCount );
bool FindConvexShapeLooseAABB( const float *pInwardFacingPlanes, int iPlaneCount, Vector *pAABBMins, Vector *pAABBMaxs );
CPolyhedron *ClipLinkedGeometry( GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pPolygons, GeneratePolyhedronFromPlanes_UnorderedLineLL *pLines, GeneratePolyhedronFromPlanes_UnorderedPointLL *pPoints, int iPointCount, const fltx4 *pOutwardFacingPlanes, int iPlaneCount, float fOnPlaneEpsilon, bool bUseTemporaryMemory, fltx4 vShiftResultPositions );
CPolyhedron *ConvertLinkedGeometryToPolyhedron( GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pPolygons, GeneratePolyhedronFromPlanes_UnorderedLineLL *pLines, GeneratePolyhedronFromPlanes_UnorderedPointLL *pPoints, bool bUseTemporaryMemory, fltx4 vShiftResultPositions );

//#define ENABLE_DEBUG_POLYHEDRON_DUMPS //Dumps debug information to disk for use with glview. Requires that tier2 also be in all projects using debug mathlib
//#define DEBUG_DUMP_POLYHEDRONS_TO_NUMBERED_GLVIEWS //dumps successfully generated polyhedrons




void CPolyhedron_AllocByNew::Release( void )
{
	delete this;
}

CPolyhedron_AllocByNew *CPolyhedron_AllocByNew::Allocate( unsigned short iVertices, unsigned short iLines, unsigned short iIndices, unsigned short iPolygons ) //creates the polyhedron along with enough memory to hold all it's data in a single allocation
{
	void *pMemory = new unsigned char [ sizeof( CPolyhedron_AllocByNew ) +
										(iVertices * sizeof(Vector)) + 
										(iLines * sizeof(Polyhedron_IndexedLine_t)) + 
										(iIndices * sizeof( Polyhedron_IndexedLineReference_t )) + 
										(iPolygons * sizeof( Polyhedron_IndexedPolygon_t ))];

	CPolyhedron_AllocByNew *pAllocated = new ( pMemory ) CPolyhedron_AllocByNew;

	pAllocated->iVertexCount = iVertices;
	pAllocated->iLineCount = iLines;
	pAllocated->iIndexCount = iIndices;
	pAllocated->iPolygonCount = iPolygons;
	pAllocated->pVertices = (Vector *)(pAllocated + 1); //start vertex memory at the end of the class
	pAllocated->pLines = (Polyhedron_IndexedLine_t *)(pAllocated->pVertices + iVertices);
	pAllocated->pIndices = (Polyhedron_IndexedLineReference_t *)(pAllocated->pLines + iLines);
	pAllocated->pPolygons = (Polyhedron_IndexedPolygon_t *)(pAllocated->pIndices + iIndices);

	return pAllocated;
}


class CPolyhedron_TempMemory : public CPolyhedron
{
public:

	virtual void Release( void )
	{
	}

	CPolyhedron_TempMemory( void )
	{ };
};


static std::vector< unsigned char > s_TempMemoryPolyhedron_Buffer;
static CPolyhedron_TempMemory s_TempMemoryPolyhedron;

CPolyhedron *GetTempPolyhedron( unsigned short iVertices, unsigned short iLines, unsigned short iIndices, unsigned short iPolygons ) //grab the temporary polyhedron. Avoids new/delete for quick work. Can only be in use by one chunk of code at a time
{
	s_TempMemoryPolyhedron_Buffer.assign( (sizeof( Vector ) * iVertices) +
											(sizeof( Polyhedron_IndexedLine_t ) * iLines) +
											(sizeof( Polyhedron_IndexedLineReference_t ) * iIndices) +
											(sizeof( Polyhedron_IndexedPolygon_t ) * iPolygons), {} );

	s_TempMemoryPolyhedron.iVertexCount = iVertices;
	s_TempMemoryPolyhedron.iLineCount = iLines;
	s_TempMemoryPolyhedron.iIndexCount = iIndices;
	s_TempMemoryPolyhedron.iPolygonCount = iPolygons;

	s_TempMemoryPolyhedron.pVertices = (Vector *)s_TempMemoryPolyhedron_Buffer.data();
	s_TempMemoryPolyhedron.pLines = (Polyhedron_IndexedLine_t *)(&s_TempMemoryPolyhedron.pVertices[s_TempMemoryPolyhedron.iVertexCount]);
	s_TempMemoryPolyhedron.pIndices = (Polyhedron_IndexedLineReference_t *)(&s_TempMemoryPolyhedron.pLines[s_TempMemoryPolyhedron.iLineCount]);
	s_TempMemoryPolyhedron.pPolygons = (Polyhedron_IndexedPolygon_t *)(&s_TempMemoryPolyhedron.pIndices[s_TempMemoryPolyhedron.iIndexCount]);

	return &s_TempMemoryPolyhedron;
}


Vector CPolyhedron::Center( void ) const
{
	if( iVertexCount == 0 )
		return vec3_origin;

	Vector vAABBMin, vAABBMax;
	vAABBMin = vAABBMax = pVertices[0];
	for( int i = 1; i != iVertexCount; ++i )
	{
		Vector &vPoint = pVertices[i];
		if( vPoint.x < vAABBMin.x )
			vAABBMin.x = vPoint.x;
		if( vPoint.y < vAABBMin.y )
			vAABBMin.y = vPoint.y;
		if( vPoint.z < vAABBMin.z )
			vAABBMin.z = vPoint.z;

		if( vPoint.x > vAABBMax.x )
			vAABBMax.x = vPoint.x;
		if( vPoint.y > vAABBMax.y )
			vAABBMax.y = vPoint.y;
		if( vPoint.z > vAABBMax.z )
			vAABBMax.z = vPoint.z;
	}
	return ((vAABBMin + vAABBMax) * 0.5f);
}

enum PolyhedronPointPlanarity
{
	POINT_DEAD,
	POINT_ONPLANE,
	POINT_ALIVE	
};

struct GeneratePolyhedronFromPlanes_Point //must be aligned to 16 byte boundary
{
	fltx4 ptPosition; //w = -1 to make plane distance an all addition operation after the multiply
	GeneratePolyhedronFromPlanes_LineLL *pConnectedLines; //keep these in a clockwise order, circular linking
	float fPlaneDist; //used in plane cutting
	PolyhedronPointPlanarity planarity;
	union //temporary work variables
	{
		int iSaveIndices;
	};

};

enum PolyhedronLinePlanarity
{
	LINE_ONPLANE = 0,
	LINE_ALIVE = (1 << 0),
	LINE_DEAD = (1 << 1),
	LINE_CUT = LINE_ALIVE | LINE_DEAD,
};

PolyhedronLinePlanarity &operator|=( PolyhedronLinePlanarity &a, const PolyhedronLinePlanarity &b )
{
	a = (PolyhedronLinePlanarity)((int)a | int(b));
	return a;
}

struct GeneratePolyhedronFromPlanes_LineLL
{
	GeneratePolyhedronFromPlanes_Line *pLine;
	//whatever is referencing the line should know which side of the line it's on (points and polygons).
	//for polygons, it's the index back to the polygon's self. It's also which point to follow to continue going clockwise, which makes polygon 0 the one on the left side of an upward facing line vector
	//for points, it's the OTHER point's index
	int iReferenceIndex;
	GeneratePolyhedronFromPlanes_LineLL *pPrev;
	GeneratePolyhedronFromPlanes_LineLL *pNext;
};

struct GeneratePolyhedronFromPlanes_Line
{
	GeneratePolyhedronFromPlanes_Point *pPoints[2]; //the 2 connecting points in no particular order
	GeneratePolyhedronFromPlanes_Polygon *pPolygons[2]; //viewing from the outside with the point connections going up, 0 is the left polygon, 1 is the right
	int iSaveIndices;
	PolyhedronLinePlanarity planarity;
	bool bNewLengthThisPass;

	GeneratePolyhedronFromPlanes_LineLL PointLineLinks[2]; //rather than going into a point and searching for its link to this line, lets just cache it to eliminate searching
	GeneratePolyhedronFromPlanes_LineLL PolygonLineLinks[2]; //rather than going into a polygon and searching for its link to this line, lets just cache it to eliminate searching

	void InitLineLinks( void )
	{
		PointLineLinks[0].iReferenceIndex = 1;
		PointLineLinks[1].iReferenceIndex = 0;
		PolygonLineLinks[0].iReferenceIndex = 0;
		PolygonLineLinks[1].iReferenceIndex = 1;

		PointLineLinks[0].pLine = PointLineLinks[1].pLine = PolygonLineLinks[0].pLine = PolygonLineLinks[1].pLine = this;
	}

};

struct GeneratePolyhedronFromPlanes_Polygon
{
	Vector vSurfaceNormal; 
	//float fNormalDist;
	GeneratePolyhedronFromPlanes_LineLL *pLines; //keep these in a clockwise order, circular linking
	
	bool bDead;
	bool bHasNewPoints;
	bool bMovedExistingPoints;

};

struct GeneratePolyhedronFromPlanes_UnorderedPolygonLL //an unordered collection of polygons
{
	GeneratePolyhedronFromPlanes_Polygon polygon;
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pNext;
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pPrev;
};

struct GeneratePolyhedronFromPlanes_UnorderedLineLL //an unordered collection of lines
{
	GeneratePolyhedronFromPlanes_Line line;
	GeneratePolyhedronFromPlanes_UnorderedLineLL *pNext;
	GeneratePolyhedronFromPlanes_UnorderedLineLL *pPrev;
};

#pragma pack( push )
#pragma pack( 16 ) //help align the position to 16 byte boundaries when in arrays
struct GeneratePolyhedronFromPlanes_UnorderedPointLL //an unordered collection of points
{
	GeneratePolyhedronFromPlanes_Point point;
	GeneratePolyhedronFromPlanes_UnorderedPointLL *pNext;
	GeneratePolyhedronFromPlanes_UnorderedPointLL *pPrev;
};
#pragma pack(pop)




CPolyhedron *ClipPolyhedron( const CPolyhedron *pExistingPolyhedron, const float *pOutwardFacingPlanes, int iPlaneCount, float fOnPlaneEpsilon, bool bUseTemporaryMemory )
{
	ks::ScratchArena<> scratch;
	if( pExistingPolyhedron == nullptr )
		return nullptr;

	contract_assert( ( (pExistingPolyhedron->iVertexCount >= 3) && (pExistingPolyhedron->iPolygonCount >= 2) ) && "Polyhedron doesn't meet absolute minimum spec" );

	const std::size_t kFltX4Align = sizeof( fltx4 ) - 1;
	uint8 *pAlignedAlloc = (uint8 *)scratch.AllocBytes( (sizeof( fltx4 ) * iPlaneCount) + kFltX4Align );
	pAlignedAlloc = (uint8 *)(((std::size_t)(pAlignedAlloc + kFltX4Align)) & ~kFltX4Align);
	fltx4 *pUsefulPlanes = (fltx4 *)pAlignedAlloc;
	int iUsefulPlaneCount = 0;
	Vector *pExistingVertices = pExistingPolyhedron->pVertices;

	//A large part of clipping will either eliminate the polyhedron entirely, or clip nothing at all, so lets just check for those first and throw away useless planes
	{
		int iLiveCount = 0;
		int iDeadCount = 0;
		const float fNegativeOnPlaneEpsilon = -fOnPlaneEpsilon;

		for( int i = 0; i != iPlaneCount; ++i )
		{
			Vector vNormal = *((Vector *)&pOutwardFacingPlanes[(i * 4) + 0]);
			float fPlaneDist = pOutwardFacingPlanes[(i * 4) + 3];

			for( int j = 0; j != pExistingPolyhedron->iVertexCount; ++j )
			{
				float fPointDist = vNormal.Dot( pExistingVertices[j] ) - fPlaneDist;
				
				if( fPointDist <= fNegativeOnPlaneEpsilon )
					++iLiveCount;
				else if( fPointDist > fOnPlaneEpsilon )
					++iDeadCount;
			}

			if( iLiveCount == 0 )
			{
				//all points are dead or on the plane, so the polyhedron is dead
				return nullptr;
			}

			if( iDeadCount != 0 )
			{
				//at least one point died, this plane yields useful results				
				SubFloat( pUsefulPlanes[iUsefulPlaneCount], 0 ) = vNormal.x; //PolyhedronFloatStandardize( vNormal.x );
				SubFloat( pUsefulPlanes[iUsefulPlaneCount], 1 ) = vNormal.y; //PolyhedronFloatStandardize( vNormal.y );
				SubFloat( pUsefulPlanes[iUsefulPlaneCount], 2 ) = vNormal.z; //PolyhedronFloatStandardize( vNormal.z );
				SubFloat( pUsefulPlanes[iUsefulPlaneCount], 3 ) = fPlaneDist; //PolyhedronFloatStandardize( fPlaneDist );
				++iUsefulPlaneCount;
			}
		}
	}

	if( iUsefulPlaneCount == 0 )
	{
		//testing shows that the polyhedron won't even be cut, clone the existing polyhedron and return that

		CPolyhedron *pReturn;
		if( bUseTemporaryMemory )
		{
			pReturn = GetTempPolyhedron( pExistingPolyhedron->iVertexCount, 
											pExistingPolyhedron->iLineCount, 
											pExistingPolyhedron->iIndexCount, 
											pExistingPolyhedron->iPolygonCount );
		}
		else
		{
			pReturn = CPolyhedron_AllocByNew::Allocate( pExistingPolyhedron->iVertexCount, 
														pExistingPolyhedron->iLineCount, 
														pExistingPolyhedron->iIndexCount, 
														pExistingPolyhedron->iPolygonCount );
		}

		std::memcpy( pReturn->pVertices, pExistingPolyhedron->pVertices, sizeof( Vector ) * pReturn->iVertexCount );
		std::memcpy( pReturn->pLines, pExistingPolyhedron->pLines, sizeof( Polyhedron_IndexedLine_t ) * pReturn->iLineCount );
		std::memcpy( pReturn->pIndices, pExistingPolyhedron->pIndices, sizeof( Polyhedron_IndexedLineReference_t ) * pReturn->iIndexCount );
		std::memcpy( pReturn->pPolygons, pExistingPolyhedron->pPolygons, sizeof( Polyhedron_IndexedPolygon_t ) * pReturn->iPolygonCount );


		return pReturn;
	}



	//convert the polyhedron to linked geometry
	GeneratePolyhedronFromPlanes_UnorderedPointLL *pPoints = (GeneratePolyhedronFromPlanes_UnorderedPointLL *)scratch.AllocBytes( pExistingPolyhedron->iVertexCount * sizeof( GeneratePolyhedronFromPlanes_UnorderedPointLL ) );
	GeneratePolyhedronFromPlanes_UnorderedLineLL *pLines = (GeneratePolyhedronFromPlanes_UnorderedLineLL *)scratch.AllocBytes( pExistingPolyhedron->iLineCount * sizeof( GeneratePolyhedronFromPlanes_UnorderedLineLL ) );
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pPolygons = (GeneratePolyhedronFromPlanes_UnorderedPolygonLL *)scratch.AllocBytes( pExistingPolyhedron->iPolygonCount * sizeof( GeneratePolyhedronFromPlanes_UnorderedPolygonLL ) );

	fltx4 vPointOffset = LoadZeroSIMD();

	//setup points
	for( int i = 0; i != pExistingPolyhedron->iVertexCount; ++i )
	{
		SubFloat( pPoints[i].point.ptPosition, 0 ) = pExistingPolyhedron->pVertices[i].x;
		SubFloat( pPoints[i].point.ptPosition, 1 ) = pExistingPolyhedron->pVertices[i].y;
		SubFloat( pPoints[i].point.ptPosition, 2 ) = pExistingPolyhedron->pVertices[i].z;
		SubFloat( pPoints[i].point.ptPosition, 3 ) = -1.0f;
		vPointOffset = AddSIMD( vPointOffset, pPoints[i].point.ptPosition );
		pPoints[i].point.pConnectedLines = nullptr; //we won't be circular linking until later
	}

	//move everything towards origin for more precise math
	vPointOffset = MulSIMD( vPointOffset, ReplicateX4( -1.0f / (float)pExistingPolyhedron->iVertexCount ) );
	vPointOffset = FloorSIMD( vPointOffset );
	SubFloat( vPointOffset, 3 ) = -1.0f;

	for( int i = 0; i != pExistingPolyhedron->iVertexCount; ++i )
	{
		pPoints[i].point.ptPosition = AddSIMD( pPoints[i].point.ptPosition, vPointOffset );
		SubFloat( pPoints[i].point.ptPosition, 3 ) = -1.0f;
	}
	for( int i = 0; i != iUsefulPlaneCount; ++i )
	{
		fltx4 vMul = MulSIMD( pUsefulPlanes[i], vPointOffset );
		SubFloat( pUsefulPlanes[i], 3 ) = SubFloat(vMul, 0) + SubFloat(vMul, 1) + SubFloat(vMul, 2) - SubFloat(vMul, 3 );
	}
	fltx4 vResultOffset = NegSIMD( vPointOffset );
	SubFloat( vResultOffset, 3 ) = 0.0f;

	//setup lines and interlink to points (line links are not yet circularly linked, and are unordered)
	for( int i = 0; i != pExistingPolyhedron->iLineCount; ++i )
	{
		pLines[i].line.InitLineLinks();
		for( int j = 0; j != 2; ++j )
		{
			pLines[i].line.pPoints[j] = &pPoints[pExistingPolyhedron->pLines[i].iPointIndices[j]].point;

			GeneratePolyhedronFromPlanes_LineLL *pLineLink = &pLines[i].line.PointLineLinks[j];
			//pLineLink->pPrev = NULL;
			pLineLink->pNext = pLines[i].line.pPoints[j]->pConnectedLines;
			pLines[i].line.pPoints[j]->pConnectedLines = pLineLink;
		}
	}



	//setup polygons
	for( int i = 0; i != pExistingPolyhedron->iPolygonCount; ++i )
	{
		pPolygons[i].polygon.vSurfaceNormal = pExistingPolyhedron->pPolygons[i].polyNormal;
		Polyhedron_IndexedLineReference_t *pOffsetPolyhedronLines = &pExistingPolyhedron->pIndices[pExistingPolyhedron->pPolygons[i].iFirstIndex];
		
		GeneratePolyhedronFromPlanes_LineLL *pFirstLink = &pLines[pOffsetPolyhedronLines[0].iLineIndex].line.PolygonLineLinks[pOffsetPolyhedronLines[0].iEndPointIndex];
		pPolygons[i].polygon.pLines = pFirstLink; //technically going to link to itself on first pass, then get linked properly immediately afterward
		for( int j = 0; j != pExistingPolyhedron->pPolygons[i].iIndexCount; ++j )
		{
			GeneratePolyhedronFromPlanes_LineLL *pLineLink = &pLines[pOffsetPolyhedronLines[j].iLineIndex].line.PolygonLineLinks[pOffsetPolyhedronLines[j].iEndPointIndex];
			pLineLink->pLine->pPolygons[pLineLink->iReferenceIndex] = &pPolygons[i].polygon;

			pLineLink->pPrev = pPolygons[i].polygon.pLines;
			pPolygons[i].polygon.pLines->pNext = pLineLink;
			pPolygons[i].polygon.pLines = pLineLink;
		}
		
		pFirstLink->pPrev = pPolygons[i].polygon.pLines;
		pPolygons[i].polygon.pLines->pNext = pFirstLink;
	}

	//go back to point line links so we can circularly link them as well as order them now that every point has all its line links
	for( int i = 0; i != pExistingPolyhedron->iVertexCount; ++i )
	{
		//interlink the points
		{
			GeneratePolyhedronFromPlanes_LineLL *pLastVisitedLink = pPoints[i].point.pConnectedLines;
			GeneratePolyhedronFromPlanes_LineLL *pCurrentLink = pLastVisitedLink;
			
			do
			{
				pCurrentLink->pPrev = pLastVisitedLink;
				pLastVisitedLink = pCurrentLink;
				pCurrentLink = pCurrentLink->pNext;
			} while( pCurrentLink );

			//circular link
			pLastVisitedLink->pNext = pPoints[i].point.pConnectedLines;
			pPoints[i].point.pConnectedLines->pPrev = pLastVisitedLink;
		}


		//fix ordering
		GeneratePolyhedronFromPlanes_LineLL *pFirstLink = pPoints[i].point.pConnectedLines;
		GeneratePolyhedronFromPlanes_LineLL *pWorkLink = pFirstLink;
		GeneratePolyhedronFromPlanes_LineLL *pSearchLink;
		GeneratePolyhedronFromPlanes_Polygon *pLookingForPolygon;
		contract_assert( pFirstLink->pNext != pFirstLink );
		do
		{
			pLookingForPolygon = pWorkLink->pLine->pPolygons[1 - pWorkLink->iReferenceIndex]; //grab pointer to left polygon
			pSearchLink = pWorkLink->pPrev;

			while( pSearchLink->pLine->pPolygons[pSearchLink->iReferenceIndex] != pLookingForPolygon )
				pSearchLink = pSearchLink->pPrev;

			contract_assert( pSearchLink->pLine->pPolygons[pSearchLink->iReferenceIndex] == pWorkLink->pLine->pPolygons[1 - pWorkLink->iReferenceIndex] );

			//pluck the search link from wherever it is
			pSearchLink->pPrev->pNext = pSearchLink->pNext;
			pSearchLink->pNext->pPrev = pSearchLink->pPrev;

			//insert the search link just before the work link			
			pSearchLink->pPrev = pWorkLink->pPrev;
			pSearchLink->pNext = pWorkLink;
			
			pSearchLink->pPrev->pNext = pSearchLink;
			pWorkLink->pPrev = pSearchLink;

			pWorkLink = pSearchLink;
		} while( pWorkLink != pFirstLink );
	}

	//setup point collection
	{
		pPoints[0].pPrev = nullptr;
		pPoints[0].pNext = &pPoints[1];
		int iLastPoint = pExistingPolyhedron->iVertexCount - 1;
		for( int i = 1; i != iLastPoint; ++i )
		{
			pPoints[i].pPrev = &pPoints[i - 1];
			pPoints[i].pNext = &pPoints[i + 1];
		}
		pPoints[iLastPoint].pPrev = &pPoints[iLastPoint - 1];
		pPoints[iLastPoint].pNext = nullptr;
	}

	//setup line collection
	{
		pLines[0].pPrev = nullptr;
		pLines[0].pNext = &pLines[1];
		int iLastLine = pExistingPolyhedron->iLineCount - 1;
		for( int i = 1; i != iLastLine; ++i )
		{
			pLines[i].pPrev = &pLines[i - 1];
			pLines[i].pNext = &pLines[i + 1];
		}
		pLines[iLastLine].pPrev = &pLines[iLastLine - 1];
		pLines[iLastLine].pNext = nullptr;
	}

	//setup polygon collection
	{
		pPolygons[0].pPrev = nullptr;
		pPolygons[0].pNext = &pPolygons[1];
		int iLastPolygon = pExistingPolyhedron->iPolygonCount - 1;
		for( int i = 1; i != iLastPolygon; ++i )
		{
			pPolygons[i].pPrev = &pPolygons[i - 1];
			pPolygons[i].pNext = &pPolygons[i + 1];
		}
		pPolygons[iLastPolygon].pPrev = &pPolygons[iLastPolygon - 1];
		pPolygons[iLastPolygon].pNext = nullptr;
	}

	CPolyhedron *pRetVal = ClipLinkedGeometry( pPolygons, pLines, pPoints, pExistingPolyhedron->iVertexCount, pUsefulPlanes, iUsefulPlaneCount, fOnPlaneEpsilon, bUseTemporaryMemory, vResultOffset );

	return pRetVal;
}



Vector FindPointInPlanes( const float *pPlanes, int planeCount )
{
	Vector point = vec3_origin;

	for ( int i = 0; i < planeCount; i++ )
	{
		float fD = DotProduct( *(Vector *)&pPlanes[i*4], point ) - pPlanes[i*4 + 3];
		if ( fD < 0 )
		{
			point -= fD * (*(Vector *)&pPlanes[i*4]);
		}
	}
	return point;
}



bool FindConvexShapeLooseAABB( const fltx4 *pInwardFacingPlanes, int iPlaneCount, Vector *pAABBMins, Vector *pAABBMaxs ) //bounding box of the convex shape (subject to floating point error)
{
	ks::ScratchArena<> scratch;
	//returns false if the AABB hasn't been set
	if( pAABBMins == nullptr && pAABBMaxs == nullptr ) //no use in actually finding out what it is
		return false;

	struct FindConvexShapeAABB_Polygon_t
	{
		float *verts;
		int iVertCount;
	};

	const std::size_t kPlaneAlign = sizeof( Vector4D ) - 1;
	uint8 *pAlignedAlloc = (uint8 *)scratch.AllocBytes( (sizeof( Vector4D ) * iPlaneCount) + kPlaneAlign );
	pAlignedAlloc = (uint8 *)(((std::size_t)(pAlignedAlloc + kPlaneAlign)) & ~kPlaneAlign);
	Vector4D *pMovedPlanes = (Vector4D *)pAlignedAlloc;
	for( int i = 0; i != iPlaneCount; ++i )
	{
		pMovedPlanes[i].Init( SubFloat( pInwardFacingPlanes[i], 0 ), SubFloat( pInwardFacingPlanes[i], 1 ), SubFloat( pInwardFacingPlanes[i], 2 ), SubFloat( pInwardFacingPlanes[i], 3 ) - 100.0f ); //move planes out a lot to kill some imprecision problems
	}

	//vAABBMins = vAABBMaxs = FindPointInPlanes( pPlanes, iPlaneCount );
	float *vertsIn = nullptr; //we'll be allocating a new buffer for this with each new polygon, and moving it off to the polygon array
	float *vertsOut = (float *)scratch.AllocBytes( (iPlaneCount + 4) * (sizeof( float ) * 3) ); //each plane will initially have 4 points in its polygon representation, and each plane clip has the possibility to add 1 point to the polygon
	float *vertsSwap;

	FindConvexShapeAABB_Polygon_t *pPolygons = scratch.Alloc<FindConvexShapeAABB_Polygon_t>( iPlaneCount );
	int iPolyCount = 0;

	for ( int i = 0; i < iPlaneCount; i++ )
	{
		Vector vPlaneNormal = pMovedPlanes[i].AsVector3D();
		float fPlaneDist = pMovedPlanes[i].w;// + 50.0f;

		if( vertsIn == nullptr )
			vertsIn = (float *)scratch.AllocBytes( (iPlaneCount + 4) * (sizeof( float ) * 3) );

		// Build a big-ass poly in this plane
		int vertCount = PolyFromPlane( (Vector *)vertsIn, vPlaneNormal, fPlaneDist, 100000.0f );

		//chop it by every other plane
		for( int j = 0; j < iPlaneCount; j++ )
		{
			// don't clip planes with themselves
			if ( i == j )
				continue;

			// Chop the polygon against this plane
			vertCount = ClipPolyToPlane( (Vector *)vertsIn, vertCount, (Vector *)vertsOut, pMovedPlanes[j].AsVector3D(), pMovedPlanes[j].w, 0.0f );

			//swap the input and output arrays
			vertsSwap = vertsIn; vertsIn = vertsOut; vertsOut = vertsSwap;

			// Less than a poly left, something's wrong, don't bother with this polygon
			if ( vertCount < 3 )
				break;
		}

		if ( vertCount < 3 )
			continue; //not enough to work with

		pPolygons[iPolyCount].iVertCount = vertCount;
		pPolygons[iPolyCount].verts = vertsIn;
		vertsIn = nullptr;
		++iPolyCount;
	}

	if( iPolyCount == 0 )
		return false;

	//initialize the AABB to the first point available
	Vector vAABBMins, vAABBMaxs;
	vAABBMins = vAABBMaxs = ((Vector *)pPolygons[0].verts)[0];

	if( pAABBMins && pAABBMaxs ) //they want the full box
	{
		for( int i = 0; i != iPolyCount; ++i )
		{
			Vector *PolyVerts = (Vector *)pPolygons[i].verts;
			for( int j = 0; j != pPolygons[i].iVertCount; ++j )
			{
				if( PolyVerts[j].x < vAABBMins.x ) 
					vAABBMins.x = PolyVerts[j].x;
				if( PolyVerts[j].y < vAABBMins.y ) 
					vAABBMins.y = PolyVerts[j].y;
				if( PolyVerts[j].z < vAABBMins.z ) 
					vAABBMins.z = PolyVerts[j].z;

				if( PolyVerts[j].x > vAABBMaxs.x ) 
					vAABBMaxs.x = PolyVerts[j].x;
				if( PolyVerts[j].y > vAABBMaxs.y ) 
					vAABBMaxs.y = PolyVerts[j].y;
				if( PolyVerts[j].z > vAABBMaxs.z ) 
					vAABBMaxs.z = PolyVerts[j].z;
			}
		}
		*pAABBMins = vAABBMins;
		*pAABBMaxs = vAABBMaxs;
	}
	else if( pAABBMins ) //they only want the min
	{
		for( int i = 0; i != iPolyCount; ++i )
		{
			Vector *PolyVerts = (Vector *)pPolygons[i].verts;
			for( int j = 0; j != pPolygons[i].iVertCount; ++j )
			{
				if( PolyVerts[j].x < vAABBMins.x ) 
					vAABBMins.x = PolyVerts[j].x;
				if( PolyVerts[j].y < vAABBMins.y ) 
					vAABBMins.y = PolyVerts[j].y;
				if( PolyVerts[j].z < vAABBMins.z ) 
					vAABBMins.z = PolyVerts[j].z;
			}
		}
		*pAABBMins = vAABBMins;
	}
	else //they only want the max
	{
		for( int i = 0; i != iPolyCount; ++i )
		{
			Vector *PolyVerts = (Vector *)pPolygons[i].verts;
			for( int j = 0; j != pPolygons[i].iVertCount; ++j )
			{
				if( PolyVerts[j].x > vAABBMaxs.x ) 
					vAABBMaxs.x = PolyVerts[j].x;
				if( PolyVerts[j].y > vAABBMaxs.y ) 
					vAABBMaxs.y = PolyVerts[j].y;
				if( PolyVerts[j].z > vAABBMaxs.z ) 
					vAABBMaxs.z = PolyVerts[j].z;
			}
		}
		*pAABBMaxs = vAABBMaxs;
	}

	return true;
}







CPolyhedron *ConvertLinkedGeometryToPolyhedron( GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pPolygons, GeneratePolyhedronFromPlanes_UnorderedLineLL *pLines, GeneratePolyhedronFromPlanes_UnorderedPointLL *pPoints, bool bUseTemporaryMemory, fltx4 vShiftResultPositions )
{
	contract_assert( (pPolygons != nullptr) && (pLines != nullptr) && (pPoints != nullptr) );
	unsigned int iPolyCount = 0, iLineCount = 0, iPointCount = 0, iIndexCount = 0;

	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pActivePolygonWalk = pPolygons;	
	do
	{
		++iPolyCount;
		GeneratePolyhedronFromPlanes_LineLL *pLineWalk = pActivePolygonWalk->polygon.pLines;
		GeneratePolyhedronFromPlanes_LineLL *pFirstLine = pLineWalk;
		contract_assert( pLineWalk != nullptr );
		
		do
		{
			++iIndexCount;
			pLineWalk = pLineWalk->pNext;
		} while( pLineWalk != pFirstLine );

		pActivePolygonWalk = pActivePolygonWalk->pNext;
	} while( pActivePolygonWalk );

	GeneratePolyhedronFromPlanes_UnorderedLineLL *pActiveLineWalk = pLines;
	do
	{
		++iLineCount;
		pActiveLineWalk = pActiveLineWalk->pNext;
	} while( pActiveLineWalk );

	GeneratePolyhedronFromPlanes_UnorderedPointLL *pActivePointWalk = pPoints;
	do
	{
		++iPointCount;
		pActivePointWalk = pActivePointWalk->pNext;
	} while( pActivePointWalk );	
	
	CPolyhedron *pReturn;
	if( bUseTemporaryMemory )
	{
		pReturn = GetTempPolyhedron( iPointCount, iLineCount, iIndexCount, iPolyCount );
	}
	else
	{
		pReturn = CPolyhedron_AllocByNew::Allocate( iPointCount, iLineCount, iIndexCount, iPolyCount );
	}

	Vector *pVertexArray = pReturn->pVertices;
	Polyhedron_IndexedLine_t *pLineArray = pReturn->pLines;
	Polyhedron_IndexedLineReference_t *pIndexArray = pReturn->pIndices;
	Polyhedron_IndexedPolygon_t *pPolyArray = pReturn->pPolygons;

	//copy points
	pActivePointWalk = pPoints;
	for( unsigned int i = 0; i != iPointCount; ++i )
	{
		fltx4 vShiftedResult = AddSIMD( pActivePointWalk->point.ptPosition, vShiftResultPositions );
		pVertexArray[i].Init( SubFloat( vShiftedResult, 0 ), SubFloat( vShiftedResult, 1 ), SubFloat( vShiftedResult, 2 ) );
		pActivePointWalk->point.iSaveIndices = i; //storing array indices
		pActivePointWalk = pActivePointWalk->pNext;
	}

	//copy lines
	pActiveLineWalk = pLines;
	for( unsigned int i = 0; i != iLineCount; ++i )
	{
		pLineArray[i].iPointIndices[0] = (unsigned short)pActiveLineWalk->line.pPoints[0]->iSaveIndices;
		pLineArray[i].iPointIndices[1] = (unsigned short)pActiveLineWalk->line.pPoints[1]->iSaveIndices;

		pActiveLineWalk->line.iSaveIndices = i; //storing array indices

		pActiveLineWalk = pActiveLineWalk->pNext;
	}

	//copy polygons and indices at the same time
	pActivePolygonWalk = pPolygons;
	iIndexCount = 0;
	for( unsigned int i = 0; i != iPolyCount; ++i )
	{
		pPolyArray[i].polyNormal = pActivePolygonWalk->polygon.vSurfaceNormal;
		pPolyArray[i].iFirstIndex = iIndexCount;		
		
		GeneratePolyhedronFromPlanes_LineLL *pLineWalk = pActivePolygonWalk->polygon.pLines;
		GeneratePolyhedronFromPlanes_LineLL *pFirstLine = pLineWalk;
		do
		{
			pIndexArray[iIndexCount].iLineIndex = pLineWalk->pLine->iSaveIndices;
			pIndexArray[iIndexCount].iEndPointIndex = pLineWalk->iReferenceIndex;
			
			++iIndexCount;
			pLineWalk = pLineWalk->pNext;
		} while( pLineWalk != pFirstLine );

		pPolyArray[i].iIndexCount = iIndexCount - pPolyArray[i].iFirstIndex;

		pActivePolygonWalk = pActivePolygonWalk->pNext;	
	}


	return pReturn;
}



//a little class that acts like a small block heap, using stack memory given to it
class CStackMemoryDispenser
{
public:
	CStackMemoryDispenser( void *pStackAllocation, std::size_t iStackAllocationSize )
	{
		m_pDispenserBuffer = (unsigned char *)pStackAllocation;
		m_iDispenserSizeLeft = iStackAllocationSize;
		m_pDeleteList = nullptr;
	}

	~CStackMemoryDispenser( void )
	{
		RecurseDelete( m_pDeleteList );
	}

	void *Allocate( std::size_t iSize, std::size_t iAlignTo = 16 )
	{
		contract_assert( ( ((iAlignTo - 1) & iAlignTo) == 0 ) && "Alignment must be a power of 2" );
		std::size_t iAlignOffset = iAlignTo - ((std::size_t)m_pDispenserBuffer) & (iAlignTo - 1);
		m_iDispenserSizeLeft -= iAlignOffset;
		m_pDispenserBuffer += iAlignOffset;

		if( iSize > m_iDispenserSizeLeft )
		{
			//allocate a new buffer
			std::size_t iNewBufferSize = std::max<std::common_type_t<int, std::size_t>>( 128 * 1024, (iSize + iAlignTo) * 2 ); //either allocate 128k or enough to hold 2x the allocation.
			unsigned char *pNewBuffer = new unsigned char [iNewBufferSize]; //allocate 128k at a time
			*(void **)pNewBuffer = nullptr;

			//insert this allocation into the linked list of allocations to delete on destruct
			void **pWriteDeleteAddress = &m_pDeleteList;
			while( *pWriteDeleteAddress != nullptr )
			{
				pWriteDeleteAddress = (void **)*pWriteDeleteAddress;
			}
			*pWriteDeleteAddress = pNewBuffer;			
			
			//save this as the new dispenser buffer, skipping the linked list pointer
			m_pDispenserBuffer = pNewBuffer + sizeof( void * );
			m_iDispenserSizeLeft = iNewBufferSize - sizeof( void * );

			iAlignOffset = iAlignTo - ((std::size_t)m_pDispenserBuffer) & (iAlignTo - 1); //recompute alignment offset
			m_iDispenserSizeLeft -= iAlignOffset;
			m_pDispenserBuffer += iAlignOffset;
		}

		void *pRetVal = m_pDispenserBuffer;
		m_pDispenserBuffer += iSize;
		m_iDispenserSizeLeft -= iSize;
		contract_assert( (((std::size_t)pRetVal) & (iAlignTo - 1)) == 0 );
		return pRetVal;
	}

private:
	static void RecurseDelete( void *pDelete )
	{
		if( pDelete != nullptr )
		{
			RecurseDelete( *(void **)pDelete );
			delete [](void**)pDelete;
		}
	}

	unsigned char *m_pDispenserBuffer;
	std::size_t m_iDispenserSizeLeft;
	void *m_pDeleteList; //a linked list of pointers to actual memory allocations we had to make that need to be deleted. The first thing in each allocation is a reserved space for another pointer
};

template<class T>
class CStackItemDispenser
{
public:
	CStackItemDispenser( CStackMemoryDispenser &MemoryDispenser ) : m_FallbackDispenser( MemoryDispenser )
	{
		static_assert( sizeof( T ) > sizeof( void * ) );
		m_pHead = nullptr;
	}

	T *Allocate( void )
	{
		if( m_pHead != nullptr )
		{
			T *pRetVal = m_pHead;
			m_pHead = *(T **)m_pHead;
			return pRetVal;
		}
		else
		{
			return (T *)m_FallbackDispenser.Allocate( sizeof( T ) );
		}
	}

	void Free( T *pFree )
	{
		*(T **)pFree = m_pHead;
		m_pHead = pFree;
	}

private:
	CStackMemoryDispenser &m_FallbackDispenser;
	T *m_pHead;
};



inline void ComputePlanarDistances( GeneratePolyhedronFromPlanes_UnorderedPointLL *pAllPoints, int iPointCount, fltx4 fPlane )
{
	ks::ScratchArena<> scratch;
	uint8 *pAlignedAlloc = (uint8 *)scratch.AllocBytes( (iPointCount) * sizeof(fltx4) + 15 );
	pAlignedAlloc = (uint8 *)(((std::size_t)(pAlignedAlloc + 15)) & ~15);
	fltx4 *pIntermediateResults = (fltx4 *)pAlignedAlloc;

	int i = 0;
	GeneratePolyhedronFromPlanes_UnorderedPointLL *pPointWalk = pAllPoints;
	do
	{
		contract_assert( SubFloat( pPointWalk->point.ptPosition, 3 ) == -1.0f );
		pIntermediateResults[i] = MulSIMD( fPlane, pPointWalk->point.ptPosition );
		++i;
		pPointWalk = pPointWalk->pNext;
	} while( pPointWalk != nullptr );

	i = 0;
	pPointWalk = pAllPoints;
	do
	{
		pPointWalk->point.fPlaneDist = SubFloat( pIntermediateResults[i], 0 ) + SubFloat( pIntermediateResults[i], 1 ) + SubFloat( pIntermediateResults[i], 2 ) + SubFloat( pIntermediateResults[i], 3 );
		++i;
		pPointWalk = pPointWalk->pNext;
	} while( pPointWalk != nullptr );
}

class CClipLinkedGeometryDestructors
{
public:
	int &iPointCount;
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *&pAllPolygons;
	GeneratePolyhedronFromPlanes_UnorderedLineLL *&pAllLines;
	GeneratePolyhedronFromPlanes_UnorderedPointLL *&pAllPoints;
	GeneratePolyhedronFromPlanes_UnorderedPointLL *&pDeadPointCollection;	

	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPolygonLL> &polygonAllocator;
	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedLineLL> &lineAllocator;

	CClipLinkedGeometryDestructors( int &iPointCount_IN,
		GeneratePolyhedronFromPlanes_UnorderedPolygonLL *&pAllPolygons_IN, 
		CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPolygonLL> &polygonAllocator_IN,
		GeneratePolyhedronFromPlanes_UnorderedLineLL *&pAllLines_IN,
		CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedLineLL> &lineAllocator_IN,
		GeneratePolyhedronFromPlanes_UnorderedPointLL *&pAllPoints_IN, 
		GeneratePolyhedronFromPlanes_UnorderedPointLL *&pDeadPointCollection_IN )
		: iPointCount( iPointCount_IN ), pAllPolygons( pAllPolygons_IN ), pAllLines( pAllLines_IN ), polygonAllocator( polygonAllocator_IN ), lineAllocator( lineAllocator_IN ), pAllPoints( pAllPoints_IN ), pDeadPointCollection( pDeadPointCollection_IN ) {};

};

[[gnu::always_inline]] static inline GeneratePolyhedronFromPlanes_UnorderedPointLL *DestructPoint( GeneratePolyhedronFromPlanes_UnorderedPointLL *pKillPoint, CClipLinkedGeometryDestructors &destructors )
{

	
	GeneratePolyhedronFromPlanes_UnorderedPointLL *pRetVal = pKillPoint->pNext;
	if( pKillPoint->pNext )
	{
		pKillPoint->pNext->pPrev = pKillPoint->pPrev;
	}

	if( pKillPoint == destructors.pAllPoints )
	{
		destructors.pAllPoints = pKillPoint->pNext;
	}
	else
	{
		pKillPoint->pPrev->pNext = pKillPoint->pNext;
	}

	pKillPoint->pNext = destructors.pDeadPointCollection;
	destructors.pDeadPointCollection = pKillPoint;

	--destructors.iPointCount;

	return pRetVal;
}
[[gnu::always_inline]] static inline GeneratePolyhedronFromPlanes_UnorderedPointLL *DestructPoint( GeneratePolyhedronFromPlanes_Point *pKillPoint, CClipLinkedGeometryDestructors &destructors )
{
	// This COMPILE_TIME_ASSERT was breaking gcc under OSX
	static_assert( __builtin_offsetof(GeneratePolyhedronFromPlanes_UnorderedPointLL, point) == 0 );
	return DestructPoint( (GeneratePolyhedronFromPlanes_UnorderedPointLL *)pKillPoint, destructors );
}

[[gnu::always_inline]] static inline GeneratePolyhedronFromPlanes_UnorderedLineLL *DestructLine( GeneratePolyhedronFromPlanes_UnorderedLineLL *pKillLine, CClipLinkedGeometryDestructors &destructors )
{
	GeneratePolyhedronFromPlanes_UnorderedLineLL *pRetVal = pKillLine->pNext;

	if( pKillLine->pNext )
	{
		pKillLine->pNext->pPrev = pKillLine->pPrev;
	}

	if( pKillLine == destructors.pAllLines )
	{
		destructors.pAllLines = pKillLine->pNext;
	}
	else
	{
		pKillLine->pPrev->pNext = pKillLine->pNext;
	}

	destructors.lineAllocator.Free( pKillLine );

	return pRetVal;
}
[[gnu::always_inline]] static inline GeneratePolyhedronFromPlanes_UnorderedLineLL *DestructLine( GeneratePolyhedronFromPlanes_Line *pKillLine, CClipLinkedGeometryDestructors &destructors )
{
	// This COMPILE_TIME_ASSERT was breaking gcc under OSX
	static_assert( __builtin_offsetof(GeneratePolyhedronFromPlanes_UnorderedLineLL, line) == 0 );
	return DestructLine( (GeneratePolyhedronFromPlanes_UnorderedLineLL *)pKillLine, destructors );
}

[[gnu::always_inline]] static inline void UnlinkLine( GeneratePolyhedronFromPlanes_Line *pUnlinkLine )
{
	//disconnect the line from everything	
	for( int i = 0; i != 2; ++i )
	{
		pUnlinkLine->PointLineLinks[i].pNext->pPrev = pUnlinkLine->PointLineLinks[i].pPrev;
		pUnlinkLine->PointLineLinks[i].pPrev->pNext = pUnlinkLine->PointLineLinks[i].pNext;

		pUnlinkLine->PolygonLineLinks[i].pNext->pPrev = pUnlinkLine->PolygonLineLinks[i].pPrev;
		pUnlinkLine->PolygonLineLinks[i].pPrev->pNext = pUnlinkLine->PolygonLineLinks[i].pNext;

		pUnlinkLine->pPoints[i]->pConnectedLines = pUnlinkLine->PointLineLinks[i].pNext;
		pUnlinkLine->pPolygons[i]->pLines = pUnlinkLine->PolygonLineLinks[i].pNext;

		contract_assert( (pUnlinkLine->pPoints[i]->pConnectedLines != &pUnlinkLine->PointLineLinks[i]) || (pUnlinkLine->pPoints[i]->planarity == POINT_DEAD) );
		contract_assert( (pUnlinkLine->pPolygons[i]->pLines != &pUnlinkLine->PolygonLineLinks[i]) || (pUnlinkLine->pPolygons[i]->bDead == true) );
	}
}

[[gnu::always_inline]] static inline void UnlinkLine( GeneratePolyhedronFromPlanes_UnorderedLineLL *pUnlinkLine )
{
	UnlinkLine( &pUnlinkLine->line );	
}



[[gnu::always_inline]] static inline GeneratePolyhedronFromPlanes_UnorderedPolygonLL *DestructPolygon( GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pKillPolygon, CClipLinkedGeometryDestructors &destructors )
{
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pRetVal = pKillPolygon->pNext;
	if( pKillPolygon->pNext )
	{
		pKillPolygon->pNext->pPrev = pKillPolygon->pPrev;
	}

	if( pKillPolygon == destructors.pAllPolygons )
	{
		destructors.pAllPolygons = pKillPolygon->pNext;
	}
	else
	{
		pKillPolygon->pPrev->pNext = pKillPolygon->pNext;
	}

	destructors.polygonAllocator.Free( pKillPolygon );

	return pRetVal;
}

[[gnu::always_inline]] static inline GeneratePolyhedronFromPlanes_UnorderedPolygonLL *DestructPolygon( GeneratePolyhedronFromPlanes_Polygon *pKillPolygon, CClipLinkedGeometryDestructors &destructors )
{
	// This COMPILE_TIME_ASSERT was breaking gcc under OSX
	static_assert( __builtin_offsetof(GeneratePolyhedronFromPlanes_UnorderedPolygonLL, polygon) == 0 );
	return DestructPolygon( (GeneratePolyhedronFromPlanes_UnorderedPolygonLL *)pKillPolygon, destructors );
}

//remove a known degenerate polygon. Returns INVALID pointer to the line we destroy in here.
GeneratePolyhedronFromPlanes_Line *RemoveDegeneratePolygon( GeneratePolyhedronFromPlanes_Polygon *pDegeneratePolygon, CClipLinkedGeometryDestructors &destructors )
{
	contract_assert( pDegeneratePolygon->pLines->pNext == pDegeneratePolygon->pLines->pPrev );
	contract_assert( pDegeneratePolygon->pLines->pNext != pDegeneratePolygon->pLines ); //just for the sake of paranoia. Check that it's not a single-lined polygon

	contract_assert( (pDegeneratePolygon->pLines->pLine->pPoints[pDegeneratePolygon->pLines->iReferenceIndex] == pDegeneratePolygon->pLines->pNext->pLine->pPoints[1 - pDegeneratePolygon->pLines->pNext->iReferenceIndex]) &&
							(pDegeneratePolygon->pLines->pLine->pPoints[1 - pDegeneratePolygon->pLines->iReferenceIndex] == pDegeneratePolygon->pLines->pNext->pLine->pPoints[pDegeneratePolygon->pLines->pNext->iReferenceIndex]) );

	//both the lines are the same, just ditch one and link up the real polygons
	GeneratePolyhedronFromPlanes_LineLL *pKeepLine = pDegeneratePolygon->pLines;
	GeneratePolyhedronFromPlanes_LineLL *pDeadLine = pKeepLine->pNext;
	GeneratePolyhedronFromPlanes_Line *pRetVal = pDeadLine->pLine;

	pKeepLine->pLine->pPolygons[pKeepLine->iReferenceIndex] = pDeadLine->pLine->pPolygons[1 - pDeadLine->iReferenceIndex];

	//unlink pDeadLine from the dead polygon so it doesn't mess with pKeepLine when we unlink it
	pDeadLine->pLine->PolygonLineLinks[pDeadLine->iReferenceIndex].pNext = &pDeadLine->pLine->PolygonLineLinks[pDeadLine->iReferenceIndex];
	pDeadLine->pLine->PolygonLineLinks[pDeadLine->iReferenceIndex].pPrev = &pDeadLine->pLine->PolygonLineLinks[pDeadLine->iReferenceIndex];

	//insert pKeepLine just after pDeadLine on pDeadLine's flip side polygon, so when we unlink pDeadLine, everything links into place smoothly
	pKeepLine->pLine->PolygonLineLinks[pKeepLine->iReferenceIndex].pPrev = &pDeadLine->pLine->PolygonLineLinks[1 - pDeadLine->iReferenceIndex];
	pKeepLine->pLine->PolygonLineLinks[pKeepLine->iReferenceIndex].pNext = pDeadLine->pLine->PolygonLineLinks[1 - pDeadLine->iReferenceIndex].pNext;
	pKeepLine->pLine->PolygonLineLinks[pKeepLine->iReferenceIndex].pPrev->pNext = &pKeepLine->pLine->PolygonLineLinks[pKeepLine->iReferenceIndex];
	pKeepLine->pLine->PolygonLineLinks[pKeepLine->iReferenceIndex].pNext->pPrev = &pKeepLine->pLine->PolygonLineLinks[pKeepLine->iReferenceIndex];

	UnlinkLine( pDeadLine->pLine );
	DestructLine( pDeadLine->pLine, destructors );
	DestructPolygon( pDegeneratePolygon, destructors );
	return pRetVal;
}


//search/kill redundant points on the specified polygon
void RemoveDegeneratePoints( GeneratePolyhedronFromPlanes_Polygon *pSearchPolygon, CClipLinkedGeometryDestructors &destructors )
{
	contract_assert( ( destructors.pAllPolygons->pNext != nullptr && destructors.pAllPolygons->pNext->pNext != nullptr ) && "RemoveDegeneratePoints() is not safe to run on 2D polyhedrons, early out before you get here" );
	GeneratePolyhedronFromPlanes_LineLL *pHeadLine = pSearchPolygon->pLines;
	GeneratePolyhedronFromPlanes_LineLL *pWalkLine = pHeadLine;
	do 
	{
		while( true ) //inner loop to support retesting the same line over and over again( even if it's the head )
		{
			GeneratePolyhedronFromPlanes_LineLL *pPointLineLink = &pWalkLine->pLine->PointLineLinks[1 - pWalkLine->iReferenceIndex];

			contract_assert( (pPointLineLink->pLine->pPolygons[0]->pLines != pPointLineLink->pLine->pPolygons[0]->pLines->pNext) &&
									(pPointLineLink->pLine->pPolygons[0]->pLines != pPointLineLink->pLine->pPolygons[0]->pLines->pNext->pNext) &&
									(pPointLineLink->pLine->pPolygons[1]->pLines != pPointLineLink->pLine->pPolygons[1]->pLines->pNext) &&
									(pPointLineLink->pLine->pPolygons[1]->pLines != pPointLineLink->pLine->pPolygons[1]->pLines->pNext->pNext) );

			//try iterating forward 2 lines, jumping over already-dead lines.
			//if we end up where we started, the point is redundant

			//go forward 1
			GeneratePolyhedronFromPlanes_LineLL *pCircleBackLineLink = pPointLineLink->pNext;

			//and again
			pCircleBackLineLink = pCircleBackLineLink->pNext;

			//point is connected to only 2 lines. This can only be part of a convex if that convex is entirely 2D or if the lines perfectly agree with eachother.
			//Based on the assumption that the convex is 3D. We can force the lines to perfectly agree with each other by eliminating one and patching the other to do the work of both.
			if( pCircleBackLineLink == pPointLineLink )
			{
				//connect the root of the next line to our root, this way multiple occurrences in a row can chain to the same root
				GeneratePolyhedronFromPlanes_Point *pDeadPoint = pWalkLine->pLine->pPoints[1 - pWalkLine->iReferenceIndex];					
				GeneratePolyhedronFromPlanes_LineLL *pRootLine = &pWalkLine->pPrev->pLine->PointLineLinks[1 - pWalkLine->pPrev->iReferenceIndex];
				GeneratePolyhedronFromPlanes_LineLL *pSurvivingLine = pWalkLine;

				contract_assert( pDeadPoint->planarity != POINT_DEAD );
				contract_assert( pRootLine->pLine->planarity != LINE_DEAD );

				if( pWalkLine->pPrev == pHeadLine )
				{
					pHeadLine = pWalkLine;
				}

				pSurvivingLine = &pSurvivingLine->pLine->PointLineLinks[1 - pSurvivingLine->iReferenceIndex]; //convert it to point space

				//the dead line/point removal code will unlink these properly. But will leave surviving line's endpoint pointing at the dead point.
				//relink surviving to the root point
				{
					//unlink dead point from surviving line
					pSurvivingLine->pNext->pPrev = pSurvivingLine->pPrev;
					pSurvivingLine->pPrev->pNext = pSurvivingLine->pNext;

					//arbitrarily insert after dead line on the root point, before or after doesn't matter as it'll be unlinked soon anyway
					pSurvivingLine->pNext = pRootLine->pNext;
					pSurvivingLine->pNext->pPrev = pSurvivingLine;
					pSurvivingLine->pPrev = pRootLine;
					pRootLine->pNext = pSurvivingLine;

					//steal root from dead line
					pSurvivingLine->pLine->pPoints[1 - pSurvivingLine->iReferenceIndex] = pRootLine->pLine->pPoints[1 - pRootLine->iReferenceIndex];
				}

				//pRootLine is fully connected to the root point, dead point, and both polygons. Unlink should work properly
				UnlinkLine( pRootLine->pLine );
				DestructLine( pRootLine->pLine, destructors );
				DestructPoint( pDeadPoint, destructors );
			}
			else
			{
				break;
			}
		}
		
		pWalkLine = pWalkLine->pNext;
	} while (pWalkLine != pHeadLine);
}

//given two lines that are both connected to the same two points, merge them.
//returns true if any deleted line was on the new polygon's edge. (could have deleted pValidLineForNewPolygon)
//pMergeLine[1] needs to be deleted after completion
static bool MergeTwoLines( GeneratePolyhedronFromPlanes_Line *pMergeLines[2], int iDyingPolygonReferenceIndices[2], CClipLinkedGeometryDestructors &destructors, bool bAllowNullPolygonCollapse )
{
	//pMergeLines[0] will be the surviving line and pMergeLines[1] will be eliminated.
	contract_assert( pMergeLines[0]->pPolygons[iDyingPolygonReferenceIndices[0]] == pMergeLines[1]->pPolygons[iDyingPolygonReferenceIndices[1]] );
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pKillPolygon = (GeneratePolyhedronFromPlanes_UnorderedPolygonLL *)pMergeLines[1]->pPolygons[iDyingPolygonReferenceIndices[1]];
	
	//merge the polygon pointer from good side of 1 to the dead side of 0
	pMergeLines[0]->pPolygons[iDyingPolygonReferenceIndices[0]] = pMergeLines[1]->pPolygons[1 - iDyingPolygonReferenceIndices[1]];

	bool bDeletedALineBorderingNewPolygon = (pMergeLines[1]->pPolygons[0] == nullptr) || (pMergeLines[1]->pPolygons[1] == nullptr);

	//swap the link from the polygon attached to 1 so it points at 0 instead of 1
	{
		GeneratePolyhedronFromPlanes_LineLL *pEditLink = &pMergeLines[1]->PolygonLineLinks[1 - iDyingPolygonReferenceIndices[1]];
		GeneratePolyhedronFromPlanes_LineLL *pSwappedLink = &pMergeLines[0]->PolygonLineLinks[iDyingPolygonReferenceIndices[0]];
		pSwappedLink->pNext = pEditLink->pNext;
		pSwappedLink->pPrev = pEditLink->pPrev;
		pSwappedLink->pPrev->pNext = pSwappedLink;
		pSwappedLink->pNext->pPrev = pSwappedLink;
		if( pMergeLines[1]->pPolygons[1 - iDyingPolygonReferenceIndices[1]]->pLines == pEditLink )
		{
			pMergeLines[1]->pPolygons[1 - iDyingPolygonReferenceIndices[1]]->pLines = pSwappedLink;
		}
	}

	//have all associated points use 0 as their head pointer in case any were using 1 as a head pointer.
	pMergeLines[0]->pPoints[0]->pConnectedLines = &pMergeLines[0]->PointLineLinks[0];
	pMergeLines[0]->pPoints[1]->pConnectedLines = &pMergeLines[0]->PointLineLinks[1];

	//unlink 1 from both points
	{
		for( int i = 0; i != 2; ++i )
		{
			GeneratePolyhedronFromPlanes_LineLL *pUnlinkLine = &pMergeLines[1]->PointLineLinks[i];
			pUnlinkLine->pNext->pPrev = pUnlinkLine->pPrev;
			pUnlinkLine->pPrev->pNext = pUnlinkLine->pNext;
		}
	}

	//kill the line
	DestructLine( pMergeLines[1], destructors );

	if( pKillPolygon || bAllowNullPolygonCollapse )
	{
		if( pKillPolygon ) //kill the polygon as well
		{
			DestructPolygon( pKillPolygon, destructors );
		}

		//in cases where we collapse a polygon, it's possible for the new line to be completely redundant along with 1 point in a set of 2 polygons
		for( int i = 0; i != 2; ++i )
		{
			GeneratePolyhedronFromPlanes_LineLL *pPointLink = &pMergeLines[0]->PointLineLinks[i];
			GeneratePolyhedronFromPlanes_LineLL *pNextLink = pPointLink->pNext;

			if( (pNextLink->pNext == pPointLink) && //only 2 lines connected to this point
				(pPointLink->pLine->pPolygons[1 - pPointLink->iReferenceIndex] == pNextLink->pLine->pPolygons[pNextLink->iReferenceIndex]) &&
				(pPointLink->pLine->pPolygons[pPointLink->iReferenceIndex] == pNextLink->pLine->pPolygons[1 - pNextLink->iReferenceIndex]) ) //and they're bounded by the same 2 polygons
			{
				contract_assert( pPointLink->pLine->pPolygons[pPointLink->iReferenceIndex] == pNextLink->pLine->pPolygons[1 - pNextLink->iReferenceIndex] ); //one rotation around a point should yield this as a shared polygon in all cases

				GeneratePolyhedronFromPlanes_Point *pRedundantPoint = pMergeLines[0]->pPoints[i];
				GeneratePolyhedronFromPlanes_LineLL *pSurvivingLine = pRedundantPoint->pConnectedLines;
				GeneratePolyhedronFromPlanes_LineLL *pRedundantLine = pSurvivingLine->pNext;
				contract_assert( pSurvivingLine->pNext == pRedundantLine );
				contract_assert( pRedundantLine->pNext == pSurvivingLine );

				//link over the redundant point in the surviving line
				pSurvivingLine->pLine->pPoints[1 - pSurvivingLine->iReferenceIndex] = pRedundantLine->pLine->pPoints[pRedundantLine->iReferenceIndex];

				//link over the redunant line from the side opposite the redundant point
				{
					GeneratePolyhedronFromPlanes_LineLL *pRedunantPointLink = &pRedundantLine->pLine->PointLineLinks[pRedundantLine->iReferenceIndex];
					GeneratePolyhedronFromPlanes_LineLL *pSwapLink = &pSurvivingLine->pLine->PointLineLinks[1 - pSurvivingLine->iReferenceIndex];
					pSwapLink->pNext = pRedunantPointLink->pNext;
					pSwapLink->pPrev = pRedunantPointLink->pPrev;
					pSwapLink->pNext->pPrev = pSwapLink;
					pSwapLink->pPrev->pNext = pSwapLink;
					pRedundantLine->pLine->pPoints[pRedundantLine->iReferenceIndex]->pConnectedLines = pSwapLink;
				}

				//link past the redundant line in both polygons
				for( int j = 0; j != 2; ++j )
				{
					if( pRedundantLine->pLine->pPolygons[i] == nullptr )
					{
						bDeletedALineBorderingNewPolygon = true;
					}
					else
					{						
						GeneratePolyhedronFromPlanes_LineLL *pLinkOver = &pRedundantLine->pLine->PolygonLineLinks[j];

						pLinkOver->pNext->pPrev = pLinkOver->pPrev;
						pLinkOver->pPrev->pNext = pLinkOver->pNext;

						if( pRedundantLine->pLine->pPolygons[j]->pLines == pLinkOver )
							pRedundantLine->pLine->pPolygons[j]->pLines = pLinkOver->pNext;
					}
				}

				//kill the redundant line
				DestructLine( pRedundantLine->pLine, destructors );

				//kill the redundant point
				DestructPoint( pRedundantPoint, destructors );
				break;
			}
		}
	}
	
	return bDeletedALineBorderingNewPolygon;
}


static inline GeneratePolyhedronFromPlanes_Point *AllocatePoint( GeneratePolyhedronFromPlanes_UnorderedPointLL * &pAllPoints, CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPointLL> &pointAllocator, int &iPointCount )
{
	pAllPoints->pPrev = pointAllocator.Allocate();
	pAllPoints->pPrev->pNext = pAllPoints;
	pAllPoints = pAllPoints->pPrev;
	pAllPoints->pPrev = nullptr;

	contract_assert( (((std::size_t)&pAllPoints->point.ptPosition) & 15) == 0 );
	++iPointCount;

	return &pAllPoints->point;
}


static inline GeneratePolyhedronFromPlanes_Line *AllocateLine( GeneratePolyhedronFromPlanes_UnorderedLineLL * &pAllLines, CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedLineLL> &lineAllocator )
{
	//before we forget, add this line to the active list
	pAllLines->pPrev = lineAllocator.Allocate();
	pAllLines->pPrev->pNext = pAllLines;
	pAllLines = pAllLines->pPrev;
	pAllLines->pPrev = nullptr;

	pAllLines->line.InitLineLinks();
	pAllLines->line.planarity = LINE_ONPLANE;
	pAllLines->line.bNewLengthThisPass = true;

	return &pAllLines->line;
}

static inline GeneratePolyhedronFromPlanes_Polygon *AllocatePolygon( GeneratePolyhedronFromPlanes_UnorderedPolygonLL * &pAllPolygons, CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPolygonLL> &polygonAllocator, const Vector &vSurfaceNormal/*, float fPlaneDist*/ )
{
	pAllPolygons->pPrev = polygonAllocator.Allocate();
	pAllPolygons->pPrev->pNext = pAllPolygons;
	pAllPolygons = pAllPolygons->pPrev;
	pAllPolygons->pPrev = nullptr;
	
	pAllPolygons->polygon.bDead = false; //technically missing all it's sides, but we're fixing it now
	pAllPolygons->polygon.bHasNewPoints = true;
	pAllPolygons->polygon.bMovedExistingPoints = false;
	pAllPolygons->polygon.vSurfaceNormal = vSurfaceNormal;
	//pAllPolygons->polygon.fNormalDist = fPlaneDist;
	
	return &pAllPolygons->polygon;
}

struct MarkPlanarityControlStruct_t
{
	MarkPlanarityControlStruct_t( GeneratePolyhedronFromPlanes_Polygon *pPolygon, 
									CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedLineLL> &lineAlloc, GeneratePolyhedronFromPlanes_UnorderedLineLL *&pLines, 
									CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPointLL> &pointAlloc, GeneratePolyhedronFromPlanes_UnorderedPointLL *&pPoints, int &iPoints, float fPlaneEpsilon )
									: pNewPolygon( pPolygon ),
									lineAllocator( lineAlloc ), pAllLines( pLines ), 
									pointAllocator( pointAlloc ), pAllPoints( pPoints ), iPointCount( iPoints ), 
									fOnPlaneEpsilon( fPlaneEpsilon ), fNegativeOnPlaneEpsilon( -fPlaneEpsilon )
	{
		bAllPointsDead = true;

		GeneratePolyhedronFromPlanes_Line *pStartLine = AllocateLine( pAllLines, lineAllocator );
		{
			//A bit of setup on the dummy line, links to nothing
			pStartLine->pPolygons[0] = nullptr;						
			pStartLine->PolygonLineLinks[0].pNext = &pStartLine->PolygonLineLinks[0];
			pStartLine->PolygonLineLinks[0].pPrev = &pStartLine->PolygonLineLinks[0];

			pStartLine->pPolygons[1] = pNewPolygon;
			pStartLine->PolygonLineLinks[1].pNext = &pStartLine->PolygonLineLinks[1];
			pStartLine->PolygonLineLinks[1].pPrev = &pStartLine->PolygonLineLinks[1];

			pStartLine->pPoints[0] = nullptr;						
			pStartLine->PointLineLinks[0].pNext = &pStartLine->PointLineLinks[0];
			pStartLine->PointLineLinks[0].pPrev = &pStartLine->PointLineLinks[0];

			pStartLine->pPoints[1] = nullptr;						
			pStartLine->PointLineLinks[1].pNext = &pStartLine->PointLineLinks[1];
			pStartLine->PointLineLinks[1].pPrev = &pStartLine->PointLineLinks[1];

			pStartLine->planarity = LINE_ONPLANE;
		}

		pActivePolyLine = &pStartLine->PolygonLineLinks[1];

		pPolygon->pLines = pActivePolyLine;
	}

	GeneratePolyhedronFromPlanes_LineLL *pActivePolyLine;	
	GeneratePolyhedronFromPlanes_Polygon *pNewPolygon;

	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedLineLL> &lineAllocator;
	GeneratePolyhedronFromPlanes_UnorderedLineLL *&pAllLines;

	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPointLL> &pointAllocator;
	GeneratePolyhedronFromPlanes_UnorderedPointLL *&pAllPoints;
	int &iPointCount;
	
	float fOnPlaneEpsilon;
	float fNegativeOnPlaneEpsilon;
	bool bAllPointsDead;
	
};

GeneratePolyhedronFromPlanes_Line *MarkPlanarity_CreateNewPolyLine( MarkPlanarityControlStruct_t &control )
{
	GeneratePolyhedronFromPlanes_Line *pNewLine = AllocateLine( control.pAllLines, control.lineAllocator );
	//make sure we can link into it
	{
		pNewLine->pPoints[0] = nullptr;
		pNewLine->PointLineLinks[0].pPrev = nullptr;
		pNewLine->PointLineLinks[0].pNext = nullptr;

		pNewLine->pPoints[1] = nullptr;
		pNewLine->PointLineLinks[1].pPrev = nullptr;
		pNewLine->PointLineLinks[1].pNext = nullptr;

		pNewLine->pPolygons[0] = nullptr;
		pNewLine->PolygonLineLinks[0].pNext = nullptr;
		pNewLine->PolygonLineLinks[0].pPrev = nullptr;

		pNewLine->pPolygons[1] = control.pNewPolygon;
		pNewLine->PolygonLineLinks[1].pNext = control.pActivePolyLine->pNext;
		pNewLine->PolygonLineLinks[1].pPrev = control.pActivePolyLine;

		control.pActivePolyLine->pNext->pPrev = &pNewLine->PolygonLineLinks[1];
		control.pActivePolyLine->pNext = &pNewLine->PolygonLineLinks[1];
		control.pActivePolyLine = &pNewLine->PolygonLineLinks[1];

		control.pActivePolyLine->pLine->planarity = LINE_ONPLANE;
	}
	return pNewLine;
}

//design the following algorithms to never crawl past the cutting plane. That way we can get consistent results
void Recursive_MarkPlanarity_OnPlane( GeneratePolyhedronFromPlanes_LineLL *pLineWalk, MarkPlanarityControlStruct_t &control );

void Recursive_MarkPlanarity_Dead( GeneratePolyhedronFromPlanes_LineLL *pLineWalk, MarkPlanarityControlStruct_t &control )
{
	contract_assert( pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->planarity == POINT_DEAD );
	//Assert( !pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->debugdata.bVisited || (pLineWalk->pLine->planarity == LINE_DEAD) );

	while( pLineWalk->pLine->planarity == LINE_ALIVE ) //any line connected to a dead point can't be alive, therefore has not been traversed since we only travel through dead points
	{
		GeneratePolyhedronFromPlanes_Point *pConnectedPoint = pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex];
		if( pConnectedPoint->fPlaneDist > control.fOnPlaneEpsilon )
		{
			//point dead, line is also dead
			pConnectedPoint->planarity = POINT_DEAD;
			pLineWalk->pLine->planarity = LINE_DEAD;

			//branch into the point as a new root
			Recursive_MarkPlanarity_Dead( pLineWalk->pLine->PointLineLinks[pLineWalk->iReferenceIndex].pNext, control );
		}
		else if( pConnectedPoint->fPlaneDist >= control.fNegativeOnPlaneEpsilon )
		{
			//point onplane, line is dead
			pConnectedPoint->planarity = POINT_ONPLANE;
			pLineWalk->pLine->planarity = LINE_DEAD;

			control.pNewPolygon->bMovedExistingPoints = true; //mark the polygon as using off-plane points

			Recursive_MarkPlanarity_OnPlane( pLineWalk->pLine->PointLineLinks[pLineWalk->iReferenceIndex].pNext, control );

			control.pActivePolyLine->pLine->pPoints[0] = pConnectedPoint;
			control.pActivePolyLine->pLine->PointLineLinks[0].pNext = &pLineWalk->pLine->PointLineLinks[pLineWalk->iReferenceIndex];
			control.pActivePolyLine->pLine->PointLineLinks[0].pPrev = pLineWalk->pLine->PointLineLinks[pLineWalk->iReferenceIndex].pPrev;
		}
		else
		{
			//point alive, line is cut
			contract_assert( pConnectedPoint->planarity == POINT_ALIVE );
			pLineWalk->pLine->planarity = LINE_CUT;
			control.bAllPointsDead = false;


			int iDeadIndex = 1 - pLineWalk->iReferenceIndex;
			int iLivingIndex = pLineWalk->iReferenceIndex;
			
			GeneratePolyhedronFromPlanes_Point *pDeadPoint = pLineWalk->pLine->pPoints[iDeadIndex];
			GeneratePolyhedronFromPlanes_Point *pLivingPoint = pLineWalk->pLine->pPoints[iLivingIndex];

			//Generate a new point
			GeneratePolyhedronFromPlanes_Point *pNewPoint = AllocatePoint( control.pAllPoints, control.pointAllocator, control.iPointCount );
			{
				contract_assert( (pDeadPoint->fPlaneDist - pLivingPoint->fPlaneDist) > control.fOnPlaneEpsilon );
				float fInvTotalDist = 1.0f/(pDeadPoint->fPlaneDist - pLivingPoint->fPlaneDist); //subtraction because the living index is known to be negative
				pNewPoint->ptPosition = SubSIMD( MulSIMD(pLivingPoint->ptPosition, ReplicateX4(pDeadPoint->fPlaneDist * fInvTotalDist)), 
													MulSIMD(pDeadPoint->ptPosition, ReplicateX4(pLivingPoint->fPlaneDist * fInvTotalDist)) );
				SubFloat( pNewPoint->ptPosition, 3 ) = -1.0f;

				pNewPoint->planarity = POINT_ONPLANE;
				pNewPoint->fPlaneDist = 0.0f;
			}

			pLineWalk->pLine->pPolygons[0]->bHasNewPoints = true;
			pLineWalk->pLine->pPolygons[1]->bHasNewPoints = true;


			GeneratePolyhedronFromPlanes_Line *pCompletedPolyLine = control.pActivePolyLine->pLine;
			GeneratePolyhedronFromPlanes_Line *pNewPolyLine = MarkPlanarity_CreateNewPolyLine( control );

			GeneratePolyhedronFromPlanes_Line *pCutLine = pLineWalk->pLine;
			GeneratePolyhedronFromPlanes_Line *pNewLivingLine = AllocateLine( control.pAllLines, control.lineAllocator );
			
			//going to relink the cut line to be between the dead point and the new point
			//also going to create a new line between new point and live point
			//The new line will copy the cut line's iReferenceIndex orientation
			{
				//relink point pointers
				pNewLivingLine->pPoints[iDeadIndex] = pNewPoint;
				pNewLivingLine->pPoints[iLivingIndex] = pCutLine->pPoints[iLivingIndex];
				pCutLine->pPoints[iLivingIndex] = pNewPoint;
				pNewPoint->pConnectedLines = &pNewLivingLine->PointLineLinks[iDeadIndex];
				pCutLine->planarity = LINE_DEAD;
				pNewLivingLine->planarity = LINE_ALIVE;
				

				//new line steals cut line's living index linkages
				pNewLivingLine->PointLineLinks[iLivingIndex].pNext = pCutLine->PointLineLinks[iLivingIndex].pNext;
				pNewLivingLine->PointLineLinks[iLivingIndex].pPrev = pCutLine->PointLineLinks[iLivingIndex].pPrev;
				pNewLivingLine->PointLineLinks[iLivingIndex].pNext->pPrev = &pNewLivingLine->PointLineLinks[iLivingIndex];
				pNewLivingLine->PointLineLinks[iLivingIndex].pPrev->pNext = &pNewLivingLine->PointLineLinks[iLivingIndex];
				pNewLivingLine->pPoints[iLivingIndex]->pConnectedLines = &pNewLivingLine->PointLineLinks[iLivingIndex];

				//crosslink cut living to new dead
				pCutLine->PointLineLinks[iLivingIndex].pNext = pCutLine->PointLineLinks[iLivingIndex].pPrev = &pNewLivingLine->PointLineLinks[iDeadIndex];
				pNewLivingLine->PointLineLinks[iDeadIndex].pNext = pNewLivingLine->PointLineLinks[iDeadIndex].pPrev = &pCutLine->PointLineLinks[iLivingIndex];

				//fix up polygon linkages
				pNewLivingLine->pPolygons[0] = pCutLine->pPolygons[0];
				pNewLivingLine->pPolygons[1] = pCutLine->pPolygons[1];
				
				//insert after cut line for 0 polygon
				pNewLivingLine->PolygonLineLinks[0].pNext = pCutLine->PolygonLineLinks[0].pNext;
				pNewLivingLine->PolygonLineLinks[0].pPrev = &pCutLine->PolygonLineLinks[0];
				pCutLine->PolygonLineLinks[0].pNext = &pNewLivingLine->PolygonLineLinks[0];
				pNewLivingLine->PolygonLineLinks[0].pNext->pPrev = &pNewLivingLine->PolygonLineLinks[0];

				//insert before cut line for 1 polygon
				pNewLivingLine->PolygonLineLinks[1].pNext = &pCutLine->PolygonLineLinks[1];
				pNewLivingLine->PolygonLineLinks[1].pPrev = pCutLine->PolygonLineLinks[1].pPrev;
				pCutLine->PolygonLineLinks[1].pPrev = &pNewLivingLine->PolygonLineLinks[1];
				pNewLivingLine->PolygonLineLinks[1].pPrev->pNext = &pNewLivingLine->PolygonLineLinks[1];
			}
			

			//We now should have everything we need to finish constructing pCompletedPolyLine
			{
				//link polygon completed line to new point
				pCompletedPolyLine->pPoints[1] = pNewPoint;

				//Make a T junction between cut line, new line, and completed polygon line
				pCompletedPolyLine->PointLineLinks[1].pPrev = &pCutLine->PointLineLinks[iLivingIndex];
				pCutLine->PointLineLinks[iLivingIndex].pNext = &pCompletedPolyLine->PointLineLinks[1];

				pCompletedPolyLine->PointLineLinks[1].pNext = &pNewLivingLine->PointLineLinks[iDeadIndex];
				pNewLivingLine->PointLineLinks[iDeadIndex].pPrev = &pCompletedPolyLine->PointLineLinks[1];


				//pCompletedLine->pLine->pPoints[0] should already have valid values that just need to be linked back in
				pCompletedPolyLine->PointLineLinks[0].pNext->pPrev = &pCompletedPolyLine->PointLineLinks[0];
				pCompletedPolyLine->PointLineLinks[0].pPrev->pNext = &pCompletedPolyLine->PointLineLinks[0];

				//link outwardly into the patched up polygon
				pCompletedPolyLine->pPolygons[0] = pNewLivingLine->pPolygons[iDeadIndex]; //left side of line going to living point is the polygon that will survive the planar clip
				pCompletedPolyLine->PolygonLineLinks[0].pPrev = &pNewLivingLine->PolygonLineLinks[iDeadIndex];
				pCompletedPolyLine->PolygonLineLinks[0].pNext = pNewLivingLine->PolygonLineLinks[iDeadIndex].pNext;
				pCompletedPolyLine->PolygonLineLinks[0].pNext->pPrev = &pCompletedPolyLine->PolygonLineLinks[0];
				pCompletedPolyLine->PolygonLineLinks[0].pPrev->pNext = &pCompletedPolyLine->PolygonLineLinks[0];

			}

			//keep updating the drag line
			pNewPolyLine->pPoints[0] = pNewPoint;
			pNewPolyLine->PointLineLinks[0].pNext = &pCutLine->PointLineLinks[iLivingIndex];
			pNewPolyLine->PointLineLinks[0].pPrev = &pNewLivingLine->PointLineLinks[iDeadIndex];
		}

		pLineWalk = pLineWalk->pNext;
	}

	/*if( pLineWalk->pLine->planarity == LINE_DEAD )
	{
		//left polygon is dead

		Assert( !pLineWalk->pLine->pPolygons[1 - pLineWalk->iReferenceIndex]->bDead ); //not already marked dead
		pLineWalk->pLine->pPolygons[1 - pLineWalk->iReferenceIndex]->bDead = true;
	}*/
}

bool Recursive_CanOnPlanePolyCrawlDead( GeneratePolyhedronFromPlanes_LineLL *pLineWalk, MarkPlanarityControlStruct_t &control );

//Once we start traversing on-plane points, our options reduce. We do this to ensure we never traverse a section that isn't touching the cut plane
void Recursive_MarkPlanarity_OnPlane( GeneratePolyhedronFromPlanes_LineLL *pLineWalk, MarkPlanarityControlStruct_t &control )
{
	contract_assert( pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->planarity == POINT_ONPLANE );
	//Assert( !pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->debugdata.bVisited );

	//onplane's are only allowed to crawl within their polygon. So if the very next line is onplane, recurse. Alive has no work to do, and dead will be picked up later in the dead-crawling algorithm
	if( pLineWalk->pLine->planarity == LINE_ALIVE ) //not already traversed
	{
		//DBG_ONLY( pLineWalk->pLine->debugdata.bTested = true; );
		GeneratePolyhedronFromPlanes_Point *pConnectedPoint = pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex];
		if( (pConnectedPoint->fPlaneDist > control.fOnPlaneEpsilon) && !Recursive_CanOnPlanePolyCrawlDead( pLineWalk, control ) )
		{
			//do nothing, for consistency we want to be sure we traverse from the dead point to here
		}
		else if( pConnectedPoint->fPlaneDist >= control.fNegativeOnPlaneEpsilon )
		{
			//point onplane, line is onplane, line is part of the new polygon
			pConnectedPoint->planarity = POINT_ONPLANE;
			pLineWalk->pLine->planarity = LINE_ONPLANE;

			Recursive_MarkPlanarity_OnPlane( pLineWalk->pLine->PointLineLinks[pLineWalk->iReferenceIndex].pNext, control );

			//Stitch the line we just traversed into the polygon as-is
			{
				//remove from existing dead polygon
				int iDeadSide = 1 - pLineWalk->iReferenceIndex;
				pLineWalk->pLine->PolygonLineLinks[iDeadSide].pNext->pPrev = pLineWalk->pLine->PolygonLineLinks[iDeadSide].pPrev;
				pLineWalk->pLine->PolygonLineLinks[iDeadSide].pPrev->pNext = pLineWalk->pLine->PolygonLineLinks[iDeadSide].pNext;

				/*if( pLineWalk->pLine->PolygonLineLinks[iDeadSide].pNext == &pLineWalk->pLine->PolygonLineLinks[iDeadSide] )
				{
					//this was the last line in the polygon
					pLineWalk->pLine->pPolygons[iDeadSide]->pLines = NULL;
					pLineWalk->pLine->pPolygons[iDeadSide]->bDead = true;
				}
				else*/
				{
					pLineWalk->pLine->pPolygons[iDeadSide]->pLines = pLineWalk->pLine->PolygonLineLinks[iDeadSide].pNext;
				}

				//now replace it with the new polygon
				pLineWalk->pLine->pPolygons[iDeadSide] = control.pNewPolygon;
				
				//insert before pActivePolyLine
				pLineWalk->pLine->PolygonLineLinks[iDeadSide].pPrev = control.pActivePolyLine->pPrev;
				control.pActivePolyLine->pPrev->pNext = &pLineWalk->pLine->PolygonLineLinks[iDeadSide];
				
				pLineWalk->pLine->PolygonLineLinks[iDeadSide].pNext = control.pActivePolyLine;
				control.pActivePolyLine->pPrev = &pLineWalk->pLine->PolygonLineLinks[iDeadSide];
			}
		}
		else
		{
			//point alive, line is alive, root point stitched into the new polygon
			GeneratePolyhedronFromPlanes_Line *pCompletedPolyLine = control.pActivePolyLine->pLine;

			//We now should have everything we need to finish constructing pCompletedLine
			{
				//pCompletedLine->pLine->pPoints[0] should already have valid values that just need to be linked back in
				pCompletedPolyLine->PointLineLinks[0].pNext->pPrev = &pCompletedPolyLine->PointLineLinks[0];
				pCompletedPolyLine->PointLineLinks[0].pPrev->pNext = &pCompletedPolyLine->PointLineLinks[0];

				pCompletedPolyLine->pPoints[1] = pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]; //root point is endpoint for completed polygon line
				pCompletedPolyLine->PointLineLinks[1].pPrev = pLineWalk->pPrev;
				pCompletedPolyLine->PointLineLinks[1].pNext = pLineWalk;
				pCompletedPolyLine->PointLineLinks[1].pNext->pPrev = &pCompletedPolyLine->PointLineLinks[1];
				pCompletedPolyLine->PointLineLinks[1].pPrev->pNext = &pCompletedPolyLine->PointLineLinks[1];

				//link outwardly into the patched up polygon
				pCompletedPolyLine->pPolygons[0] = pLineWalk->pLine->pPolygons[1 - pLineWalk->iReferenceIndex]; //left side of line going to live point is the polygon that will survive the planar clip
				pCompletedPolyLine->PolygonLineLinks[0].pPrev = &pLineWalk->pLine->PolygonLineLinks[1 - pLineWalk->iReferenceIndex];
				pCompletedPolyLine->PolygonLineLinks[0].pNext = pCompletedPolyLine->PolygonLineLinks[0].pPrev->pNext;
				pCompletedPolyLine->PolygonLineLinks[0].pNext->pPrev = &pCompletedPolyLine->PolygonLineLinks[0];
				pCompletedPolyLine->PolygonLineLinks[0].pPrev->pNext = &pCompletedPolyLine->PolygonLineLinks[0];				
			}

			//point 0 will be filled in as we backtrack to a dead point that recursed into Recursive_MarkPlanarity_OnPlane(). If we recursed from another onplane point, we'll be recycling its line
			MarkPlanarity_CreateNewPolyLine( control );
		}
	}


	/*if( pLineWalk->pLine->planarity == LINE_DEAD )
	{
		//left polygon is dead		

		Assert( !pLineWalk->pLine->pPolygons[1 - pLineWalk->iReferenceIndex]->bDead ); //not already marked dead
		pLineWalk->pLine->pPolygons[1 - pLineWalk->iReferenceIndex]->bDead = true;
	}*/
}


bool Recursive_CanOnPlanePolyCrawlDead( GeneratePolyhedronFromPlanes_LineLL *pLineWalk, MarkPlanarityControlStruct_t &control )
{
	pLineWalk = pLineWalk->pLine->PointLineLinks[pLineWalk->iReferenceIndex].pNext;
	if( pLineWalk->pLine->planarity == LINE_ALIVE ) //not already traversed
	{
		GeneratePolyhedronFromPlanes_Point *pConnectedPoint = pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex];
		if( pConnectedPoint->fPlaneDist > control.fOnPlaneEpsilon )
		{
			GeneratePolyhedronFromPlanes_LineLL *pTestAlive = pLineWalk->pNext;
			if( pTestAlive->pLine->pPoints[pTestAlive->iReferenceIndex]->fPlaneDist < control.fNegativeOnPlaneEpsilon )
			{
				//couldn't have possibly crawled here from the other direction if we continue the onplane streak
				return Recursive_CanOnPlanePolyCrawlDead( pLineWalk, control );
			}
		}
		if( pConnectedPoint->fPlaneDist >= control.fNegativeOnPlaneEpsilon )
		{
			GeneratePolyhedronFromPlanes_LineLL *pTestAlive = pLineWalk->pNext;
			if( pTestAlive->pLine->pPoints[pTestAlive->iReferenceIndex]->fPlaneDist < control.fNegativeOnPlaneEpsilon )
			{
				//couldn't have possibly crawled here from the other direction
				return true;
			}
		}
	}
	

	return false;
}

void RecomputePolygonSurfaceNormal( GeneratePolyhedronFromPlanes_Polygon *pPolygon )
{
	Vector vAggregateNormal = vec3_origin;			

	GeneratePolyhedronFromPlanes_LineLL *pLineWalkHead = pPolygon->pLines;
	GeneratePolyhedronFromPlanes_LineLL *pLineWalk = pLineWalkHead->pPrev;
	Vector vLastLine;			

	fltx4 vLineTemp = SubSIMD( pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex]->ptPosition, pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->ptPosition );
	pLineWalk = pLineWalk->pNext;
	vLastLine.Init( SubFloat( vLineTemp, 0 ), SubFloat( vLineTemp, 1 ), SubFloat( vLineTemp, 2 ) );
	do 
	{
		vLineTemp = SubSIMD( pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex]->ptPosition, pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->ptPosition );
		Vector vThisLine( SubFloat( vLineTemp, 0 ), SubFloat( vLineTemp, 1 ), SubFloat( vLineTemp, 2 ) );
		Vector vCross = vThisLine.Cross( vLastLine );
		vAggregateNormal += vThisLine.Cross( vLastLine ); //intentionally not normalizing until the end. Larger lines deserve more influence in the result

		vLastLine = vThisLine;
		pLineWalk = pLineWalk->pNext;
	} while ( pLineWalk != pLineWalkHead );

	vAggregateNormal.NormalizeInPlace();
	pPolygon->vSurfaceNormal = vAggregateNormal;
}

CPolyhedron *ClipLinkedGeometry( GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pAllPolygons, GeneratePolyhedronFromPlanes_UnorderedLineLL *pAllLines, GeneratePolyhedronFromPlanes_UnorderedPointLL *pAllPoints, int iPointCount, const fltx4 *pOutwardFacingPlanes, int iPlaneCount, float fOnPlaneEpsilon, bool bUseTemporaryMemory, fltx4 vShiftResultPositions )
{
	ks::ScratchArena<64 * 1024> scratch;
	//const float fNegativeOnPlaneEpsilon = -fOnPlaneEpsilon;
	const float fOnPlaneEpsilonSquared = fOnPlaneEpsilon * fOnPlaneEpsilon;

	const std::size_t iStackMemorySize = 64 * 1024;
	void *pStackMemory = scratch.AllocBytes( iStackMemorySize );
	CStackMemoryDispenser memoryDispenser( pStackMemory, iStackMemorySize );

	//Collections of dead pointers for reallocation, data in them shouldn't be touched until the current loop iteration is done.
	GeneratePolyhedronFromPlanes_UnorderedPointLL	*pDeadPointCollection = nullptr;

	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPointLL> pointAllocator( memoryDispenser );
	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedLineLL> lineAllocator( memoryDispenser );
	CStackItemDispenser<GeneratePolyhedronFromPlanes_UnorderedPolygonLL> polygonAllocator( memoryDispenser );

	CClipLinkedGeometryDestructors destructors( iPointCount, pAllPolygons, polygonAllocator, pAllLines, lineAllocator, pAllPoints, pDeadPointCollection );


	for( int iCurrentPlane = 0; iCurrentPlane != iPlaneCount; ++iCurrentPlane )
	{
		//clear out work variables
		{
			GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pActivePolygonWalk = pAllPolygons;
			do
			{
				pActivePolygonWalk->polygon.bDead = false;
				pActivePolygonWalk->polygon.bHasNewPoints = false;
				pActivePolygonWalk->polygon.bMovedExistingPoints = false;

				pActivePolygonWalk = pActivePolygonWalk->pNext;
			} while( pActivePolygonWalk );

			GeneratePolyhedronFromPlanes_UnorderedLineLL *pActiveLineWalk = pAllLines;
			do
			{
				pActiveLineWalk->line.planarity = LINE_ALIVE;
				pActiveLineWalk->line.bNewLengthThisPass = false;

				pActiveLineWalk = pActiveLineWalk->pNext;
			} while( pActiveLineWalk );

			GeneratePolyhedronFromPlanes_UnorderedPointLL *pActivePointWalk = pAllPoints;
			do
			{
				pActivePointWalk->point.planarity = POINT_ALIVE;

				pActivePointWalk = pActivePointWalk->pNext;
			} while( pActivePointWalk );
		}
		
		while( pDeadPointCollection != nullptr )
		{
			GeneratePolyhedronFromPlanes_UnorderedPointLL *pFree = pDeadPointCollection;
			pDeadPointCollection = pDeadPointCollection->pNext;
			pointAllocator.Free( pFree );
		}

		
		//find point distances from the plane		
		ComputePlanarDistances( pAllPoints, iPointCount, pOutwardFacingPlanes[iCurrentPlane] );
	
		//find "most dead" point. We'll be using that as a starting point for an algorithm that walks the edge of the new polygon
		//this walk method lets us categorize points without getting into impossible situations where 2 diagonal corners of a quad are dead, but the other points are onplane/alive
		GeneratePolyhedronFromPlanes_Point *pMostDeadPoint = &pAllPoints->point;
		float fMostDeadPointDist = pAllPoints->point.fPlaneDist;
		{
			GeneratePolyhedronFromPlanes_UnorderedPointLL *pActivePointWalk = pAllPoints->pNext;
			do
			{
				if( pActivePointWalk->point.fPlaneDist > fMostDeadPointDist )
				{
					pMostDeadPoint = &pActivePointWalk->point;
					fMostDeadPointDist = pActivePointWalk->point.fPlaneDist;
				}

				pActivePointWalk = pActivePointWalk->pNext;
			} while( pActivePointWalk );
		}

		if( fMostDeadPointDist <= fOnPlaneEpsilon )
		{
			//no cuts made
			continue;
		}

		Vector vSurfaceNormal( SubFloat( pOutwardFacingPlanes[iCurrentPlane], 0 ), SubFloat( pOutwardFacingPlanes[iCurrentPlane], 1 ), SubFloat( pOutwardFacingPlanes[iCurrentPlane], 2 ) );			
		GeneratePolyhedronFromPlanes_Polygon *pNewPolygon = AllocatePolygon( pAllPolygons, polygonAllocator, vSurfaceNormal );
		pMostDeadPoint->planarity = POINT_DEAD;
		{
			MarkPlanarityControlStruct_t control( pNewPolygon, lineAllocator, pAllLines, pointAllocator, pAllPoints, iPointCount, fOnPlaneEpsilon );
			Recursive_MarkPlanarity_Dead( pMostDeadPoint->pConnectedLines, control );

			//it's possible that the crawling algorithm didn't encounter any live points if the plane didn't cut any lines (All intersections between plane and mesh were at an existing vertex)
			if( control.bAllPointsDead )
			{
				//doublecheck
				GeneratePolyhedronFromPlanes_UnorderedPointLL *pPointWalk = pAllPoints;
				while( pPointWalk )
				{
					if( pPointWalk->point.planarity == POINT_ALIVE )
					{
						control.bAllPointsDead = false;
						break;
					}
					pPointWalk = pPointWalk->pNext;
				}
			}

			if( control.bAllPointsDead ) //all the points either died or are on the plane, no polyhedron left at all
			{

				return nullptr; 
			}


			//search/mark dead polygons, there should be a way to do this in the crawl algorithm
			{
				GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pPolygonWalk = pAllPolygons;

				while( pPolygonWalk )
				{
					if( &pPolygonWalk->polygon != control.pNewPolygon )
					{
						bool bDead = true;
						GeneratePolyhedronFromPlanes_LineLL *pStartLine = pPolygonWalk->polygon.pLines;
						GeneratePolyhedronFromPlanes_LineLL *pWalkLine = pStartLine;
						do 
						{
							contract_assert( pWalkLine->pLine->pPolygons[pWalkLine->iReferenceIndex] == &pPolygonWalk->polygon );
							if( pWalkLine->pLine->planarity & LINE_ALIVE )
							{
								bDead = false;
								break;
							}
							pWalkLine = pWalkLine->pNext;
						} while ( pWalkLine != pStartLine );
						if( bDead )
						{
							pPolygonWalk->polygon.bDead = true;
						}
					}
					
					pPolygonWalk = pPolygonWalk->pNext;
				}
			}

			GeneratePolyhedronFromPlanes_LineLL *pFinal = control.pActivePolyLine;
			
			if( pFinal->pNext->pLine->pPoints[1 - pFinal->pNext->iReferenceIndex] == nullptr )
			{
				//last line redundant, copy partial data into first line
				GeneratePolyhedronFromPlanes_LineLL *pFirst = pFinal->pNext;
				contract_assert( pFinal != pFirst );
				contract_assert( pFirst->pLine->pPoints[pFirst->iReferenceIndex] != nullptr );
				contract_assert( pFinal->pLine->pPoints[1 - pFinal->iReferenceIndex] != nullptr );
				
				//link over redundant last line
				pFirst->pPrev = pFinal->pPrev;
				pFinal->pPrev->pNext = pFirst;
				
				//fill in first point of first line
				pFirst->pLine->pPoints[1 - pFirst->iReferenceIndex] = pFinal->pLine->pPoints[1 - pFinal->iReferenceIndex];
				pFirst->pLine->PointLineLinks[1 - pFirst->iReferenceIndex].pNext = pFinal->pLine->PointLineLinks[1 - pFinal->iReferenceIndex].pNext;
				pFirst->pLine->PointLineLinks[1 - pFirst->iReferenceIndex].pPrev = pFinal->pLine->PointLineLinks[1 - pFinal->iReferenceIndex].pPrev;

				pFirst->pLine->PointLineLinks[1 - pFirst->iReferenceIndex].pNext->pPrev = &pFirst->pLine->PointLineLinks[1 - pFirst->iReferenceIndex];
				pFirst->pLine->PointLineLinks[1 - pFirst->iReferenceIndex].pPrev->pNext = &pFirst->pLine->PointLineLinks[1 - pFirst->iReferenceIndex];

				control.pNewPolygon->pLines = pFirst;
				DestructLine( pFinal->pLine, destructors );

			}
			else
			{
				if( pFinal->pLine->pPoints[1 - pFinal->iReferenceIndex] != pFinal->pNext->pLine->pPoints[1 - pFinal->pNext->iReferenceIndex] )
				{
					//link up the last line to the first line
					GeneratePolyhedronFromPlanes_LineLL *pFirstLine = pFinal->pNext;

					GeneratePolyhedronFromPlanes_Line *pCompletedPolyLine = pFinal->pLine;

					//We now should have everything we need to finish constructing pCompletedLine
					{
						//pCompletedLine->pLine->pPoints[0] should already have valid values that just need to be linked back in
						pCompletedPolyLine->PointLineLinks[0].pNext->pPrev = &pCompletedPolyLine->PointLineLinks[0];
						pCompletedPolyLine->PointLineLinks[0].pPrev->pNext = &pCompletedPolyLine->PointLineLinks[0];

						pCompletedPolyLine->pPoints[1] = pFirstLine->pLine->pPoints[1 - pFirstLine->iReferenceIndex];
						pCompletedPolyLine->PointLineLinks[1].pPrev = &pFirstLine->pLine->PointLineLinks[1 - pFirstLine->iReferenceIndex];
						pCompletedPolyLine->PointLineLinks[1].pNext = pFirstLine->pLine->PointLineLinks[1 - pFirstLine->iReferenceIndex].pNext;
						contract_assert( pCompletedPolyLine->PointLineLinks[1].pNext->pLine->planarity != LINE_DEAD );
						pCompletedPolyLine->PointLineLinks[1].pNext->pPrev = &pCompletedPolyLine->PointLineLinks[1];
						pCompletedPolyLine->PointLineLinks[1].pPrev->pNext = &pCompletedPolyLine->PointLineLinks[1];

						GeneratePolyhedronFromPlanes_LineLL *pLivingPolySide = pCompletedPolyLine->PointLineLinks[1].pNext;
						pLivingPolySide = &pLivingPolySide->pLine->PolygonLineLinks[1 - pLivingPolySide->iReferenceIndex]; //convert the link from a point space to polygon space

						//link outwardly into the patched up polygon
						pCompletedPolyLine->pPolygons[0] = pLivingPolySide->pLine->pPolygons[pLivingPolySide->iReferenceIndex];
						pCompletedPolyLine->PolygonLineLinks[0].pPrev = &pLivingPolySide->pLine->PolygonLineLinks[pLivingPolySide->iReferenceIndex];
						pCompletedPolyLine->PolygonLineLinks[0].pNext = pCompletedPolyLine->PolygonLineLinks[0].pPrev->pNext;
						pCompletedPolyLine->PolygonLineLinks[0].pNext->pPrev = &pCompletedPolyLine->PolygonLineLinks[0];
						pCompletedPolyLine->PolygonLineLinks[0].pPrev->pNext = &pCompletedPolyLine->PolygonLineLinks[0];
						contract_assert( pCompletedPolyLine->pPolygons[0] != pCompletedPolyLine->pPolygons[1] );

					}
				}
				else
				{
					//first and last lines were on-plane
					control.pNewPolygon->pLines = control.pActivePolyLine->pNext;
					control.pActivePolyLine->pNext->pPrev = control.pActivePolyLine->pPrev;
					control.pActivePolyLine->pPrev->pNext = control.pActivePolyLine->pNext;
					DestructLine( control.pActivePolyLine->pLine, destructors );

				}
			}

		}


		//remove dead lines
		{
			GeneratePolyhedronFromPlanes_UnorderedLineLL *pActiveLineWalk = pAllLines;
			do
			{
				if( pActiveLineWalk->line.planarity == LINE_DEAD )
				{
					UnlinkLine( pActiveLineWalk );

					//move the line to the dead list
					pActiveLineWalk = DestructLine( pActiveLineWalk, destructors );
				}
				else
				{
					pActiveLineWalk = pActiveLineWalk->pNext;
				}
			} while( pActiveLineWalk );
		}

		//remove dead polygons
		{
			GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pActivePolygonWalk = pAllPolygons;
			do
			{
				if( pActivePolygonWalk->polygon.bDead )
				{
					pActivePolygonWalk = DestructPolygon( pActivePolygonWalk, destructors );
				}
				else
				{
					pActivePolygonWalk = pActivePolygonWalk->pNext;
				}
			} while( pActivePolygonWalk );
		}

		//remove dead points
		{
			GeneratePolyhedronFromPlanes_UnorderedPointLL *pActivePointWalk = pAllPoints;
			do
			{
				if( pActivePointWalk->point.planarity == POINT_DEAD )
				{
					pActivePointWalk = DestructPoint( pActivePointWalk, destructors );
				}
				else
				{
					pActivePointWalk = pActivePointWalk->pNext;
				}				
			} while( pActivePointWalk );
		}


		//When clipping a 2D polyhedron, the result of any clipping is a faux 3rd polygon which is degenerate (2 lines, both connected to the same set of points). Remove it now
		if( pNewPolygon->pLines->pNext == pNewPolygon->pLines->pPrev )
		{
			contract_assert( (pAllPolygons->pNext != nullptr) && //more than 1
				(pAllPolygons->pNext->pNext != nullptr) && //more than 2
				(pAllPolygons->pNext->pNext->pNext == nullptr) ); //exactly 3 polygons, and no more. This case should only pop up if the input to the last cutting pass was a 2 sided polyhedron

			RemoveDegeneratePolygon( pNewPolygon, destructors );
			pNewPolygon = nullptr;
		}

		//remove super-short lines
		{
			//ideally this behavior should be integrated with the Recursive_MarkPlanarity_Dead() behavior to avoid creating them in the first place.
			//But choosing safety over performance for now since Recursive_MarkPlanarity_Dead() is stable right now
			GeneratePolyhedronFromPlanes_UnorderedLineLL *pActiveLineWalk = pAllLines;
			do
			{
				if( pActiveLineWalk->line.bNewLengthThisPass )
				{
					fltx4 vDiff = SubSIMD( pActiveLineWalk->line.pPoints[0]->ptPosition, pActiveLineWalk->line.pPoints[1]->ptPosition );
					vDiff = MulSIMD( vDiff, vDiff );
					float fLengthSqr = SubFloat( vDiff, 0 ) + SubFloat( vDiff, 1 ) + SubFloat( vDiff, 2 ); 
					if( fLengthSqr <= fOnPlaneEpsilonSquared ) //eq for 0.0 epsilon case
					{
						//this line needs to go. But removing it might have repercussions
						GeneratePolyhedronFromPlanes_Polygon *pPolygons[2] = { pActiveLineWalk->line.pPolygons[0], pActiveLineWalk->line.pPolygons[1] };
						
						GeneratePolyhedronFromPlanes_Line *pShortLine = &pActiveLineWalk->line;
						GeneratePolyhedronFromPlanes_Point *pDeadPoint = pShortLine->pPoints[1];
						GeneratePolyhedronFromPlanes_Point *pKeepPoint = pShortLine->pPoints[0];

						//reposition point 0 to be the midpoint? In my experience it's best to just leave it alone when dealing with small distances.
						
						//relink point 1 links to point 0
						{
							//update point pointers
							{
								GeneratePolyhedronFromPlanes_LineLL *pPointLineWalkHead = pDeadPoint->pConnectedLines;
								GeneratePolyhedronFromPlanes_LineLL *pPointLineWalk = pPointLineWalkHead;
								do
								{
									pPointLineWalk->pLine->pPoints[1- pPointLineWalk->iReferenceIndex] = pKeepPoint;
									pPointLineWalk->pLine->pPolygons[pPointLineWalk->iReferenceIndex]->bMovedExistingPoints = true;//also inform touching polygons that we're changing their geometry
									pPointLineWalk = pPointLineWalk->pNext;
								} while( pPointLineWalk != pPointLineWalkHead );

								//except the line we're killing
								pShortLine->pPoints[1] = pDeadPoint;
							}

							//insert all lines from point 1 as a fan between point 0 prev and the short line.
							pShortLine->PointLineLinks[1].pNext->pPrev = pShortLine->PointLineLinks[0].pPrev;
							pShortLine->PointLineLinks[0].pPrev->pNext = pShortLine->PointLineLinks[1].pNext;

							pShortLine->PointLineLinks[1].pPrev->pNext = &pShortLine->PointLineLinks[0];							
							pShortLine->PointLineLinks[0].pPrev = pShortLine->PointLineLinks[1].pPrev;

							pShortLine->PointLineLinks[1].pNext = &pShortLine->PointLineLinks[1];
							pShortLine->PointLineLinks[1].pPrev = &pShortLine->PointLineLinks[1];
						}


						UnlinkLine( pShortLine );
						pActiveLineWalk = DestructLine( pShortLine, destructors );
						DestructPoint( pDeadPoint, destructors );

						for( int i = 0; i != 2; ++i )
						{
							if( pPolygons[i]->pLines->pNext == pPolygons[i]->pLines->pPrev )
							{
								//this polygon is dead
								if( pAllPolygons->pNext->pNext == nullptr )
								{
									//It's conceivably possible to either start or collapse down to a 2D polyhedron with every line cut save one.
									//We can't have 1 polygon, and I don't want to mentally run down what would happen below if we tried to collapse down to 1.
									//So just bail now
									return nullptr;
								}

								if( pPolygons[i] == pNewPolygon )
								{
									pNewPolygon = nullptr;
								}

								if( pActiveLineWalk )
								{
									GeneratePolyhedronFromPlanes_UnorderedLineLL *pActiveLineWalkNext = pActiveLineWalk->pNext;
									GeneratePolyhedronFromPlanes_Line *pTestDelete = &pActiveLineWalk->line;

									if( pTestDelete == RemoveDegeneratePolygon( pPolygons[i], destructors ) )
									{
										pActiveLineWalk = pActiveLineWalkNext;
									}
								}
								else
								{
									RemoveDegeneratePolygon( pPolygons[i], destructors );
								}
							}
						}

						continue; //pActiveLineWalk already updated
					}
				}
					
				pActiveLineWalk = pActiveLineWalk->pNext;
			} while( pActiveLineWalk );
		}


		contract_assert( ( pAllPolygons->pNext != nullptr ) && "A polyhedron must have at least 2 sides to be a 2D Polyhedron, and at least 4 to be a 3D polyhedron" );

		//if any polygons had their geometry adjusted (or new poly uses existing on-plane points), we need to recompute the surface normal and check for degenerate points
		{
			GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pActivePolygonWalk = pAllPolygons;
			if( pAllPolygons->pNext->pNext != nullptr )
			{
				//for 3D polyhedrons, remove degenerate points and recompute normals
				do
				{
					if( pActivePolygonWalk->polygon.bMovedExistingPoints || pActivePolygonWalk->polygon.bHasNewPoints )
					{
						RemoveDegeneratePoints( &pActivePolygonWalk->polygon, destructors );
						RecomputePolygonSurfaceNormal( &pActivePolygonWalk->polygon );
					}
					pActivePolygonWalk = pActivePolygonWalk->pNext;
				} while( pActivePolygonWalk );
			}
			else
			{
				//for 2D polyhedrons, only recompute normals. RemoveDegeneratePoints() is not safe for 2D polyhedrons as it assumes the shape is 3 dimensional to avoid angle computations and checks
				do
				{
					if( pActivePolygonWalk->polygon.bMovedExistingPoints || pActivePolygonWalk->polygon.bHasNewPoints )
					{
						RecomputePolygonSurfaceNormal( &pActivePolygonWalk->polygon );
					}
					pActivePolygonWalk = pActivePolygonWalk->pNext;
				} while( pActivePolygonWalk );
			}
		}


		//remove points which, although seemingly useful. Actually create concave shapes
		{
			GeneratePolyhedronFromPlanes_UnorderedPolygonLL *pActivePolygonWalk = pAllPolygons;
			do
			{
				if( pActivePolygonWalk->polygon.bHasNewPoints )
				{
					Vector vTestNormal = pActivePolygonWalk->polygon.vSurfaceNormal;
					GeneratePolyhedronFromPlanes_LineLL *pLineWalkHead = pActivePolygonWalk->polygon.pLines;
					GeneratePolyhedronFromPlanes_LineLL *pLineWalk = pLineWalkHead->pPrev;
					Vector vLastLine;			

					fltx4 vLineTemp = SubSIMD( pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex]->ptPosition, pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->ptPosition );
					pLineWalk = pLineWalk->pNext;
					vLastLine.Init( SubFloat( vLineTemp, 0 ), SubFloat( vLineTemp, 1 ), SubFloat( vLineTemp, 2 ) );
					do 
					{
						vLineTemp = SubSIMD( pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex]->ptPosition, pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->ptPosition );
						Vector vThisLine( SubFloat( vLineTemp, 0 ), SubFloat( vLineTemp, 1 ), SubFloat( vLineTemp, 2 ) );
						Vector vCross = vThisLine.Cross( vLastLine );
						if( vCross.Dot( vTestNormal ) <= 0.0f )
						{
							//this point is a troublemaker
							GeneratePolyhedronFromPlanes_Point *pProblemPoint = pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex];

							if( pLineWalk->pPrev == pLineWalkHead )
							{
								//about to delete our start/stop point. Avoid infinite loops
								pLineWalkHead = pLineWalk;
							}

							//what we're going to do is eliminate the problem point and every line connected to it save one. 
							//That one saved line will take the place of both lines on the new poly edge connected to this point
							//As part of eliminating all the lines connected to this point, we need to merge polygons separated by those lines.
							//To do that we'll just take the first polygon we encounter and stitch the other polygon lines into it.
							GeneratePolyhedronFromPlanes_LineLL *pPatchStop = &pLineWalk->pLine->PolygonLineLinks[1 - pLineWalk->iReferenceIndex]; //where the polygon patching algorithm should stop
							GeneratePolyhedronFromPlanes_LineLL *pPatchWalk = pLineWalk->pPrev; //rewind to prev line
							pPatchWalk = &pPatchWalk->pLine->PolygonLineLinks[1 - pPatchWalk->iReferenceIndex]; //convert to the non-new polygon walk space, pPatchWalk is the on-polygon line we'll be killing
							GeneratePolyhedronFromPlanes_Polygon *pPatchPolygon = pPatchWalk->pLine->pPolygons[pPatchWalk->iReferenceIndex];

							//pPatchStop doubles as the surviving on-polygon line. Go ahead and do all the fix ups necessary to allow us to clean up the dead lines as we come across them
							pPatchStop->pLine->pPoints[pPatchStop->iReferenceIndex] = pPatchWalk->pLine->pPoints[pPatchWalk->iReferenceIndex];
							{
								GeneratePolyhedronFromPlanes_LineLL *pRelink = &pPatchStop->pLine->PointLineLinks[pPatchStop->iReferenceIndex];
								//unlink pPatchStop from dead point
								pRelink->pNext->pPrev = pRelink->pPrev;
								pRelink->pPrev->pNext = pRelink->pNext;
								
								//relink into pPatchWalk's living point (insert arbitrarily before pPatchWalk)
								pRelink->pNext = &pPatchWalk->pLine->PointLineLinks[pPatchWalk->iReferenceIndex];
								pRelink->pPrev = pRelink->pNext->pPrev;
								pRelink->pNext->pPrev = pRelink;
								pRelink->pPrev->pNext = pRelink;
							}

							pPatchWalk = pPatchWalk->pNext; //forward to the next line that won't be dead when we're done. From here on it's a clockwise walk on this polygon until we reach a line we'll kill, then we switch to the next
							
							//go ahead and kill the dead on-polygon line
							{
								GeneratePolyhedronFromPlanes_Line *pKillLine = pPatchWalk->pPrev->pLine;
								UnlinkLine( pKillLine );
								DestructLine( pKillLine, destructors );
							}
							
							do
							{
								if( pPatchWalk->pLine->pPoints[pPatchWalk->iReferenceIndex] == pProblemPoint )
								{
									//done walking this polygon, stitch it to the next
									GeneratePolyhedronFromPlanes_Line *pKillLine = pPatchWalk->pLine;
									GeneratePolyhedronFromPlanes_Polygon *pKillPolygon = pPatchWalk->pLine->pPolygons[pPatchWalk->iReferenceIndex];

									GeneratePolyhedronFromPlanes_LineLL *pPatchPrev = pPatchWalk->pPrev;
									pPatchWalk = pPatchWalk->pLine->PolygonLineLinks[1 - pPatchWalk->iReferenceIndex].pNext; //flip to the other side's polygon, then forward to the next line we'll keep
									
									//patching will be more straightforward if we kill all the dead stuff now
									UnlinkLine( pKillLine );
									DestructLine( pKillLine, destructors );
									if( pKillPolygon != pPatchPolygon )
									{
										DestructPolygon( pKillPolygon, destructors );
									}

									//link the far end of the polygon to our start (which should continually be the first living pPatchWalk line on pPatchPolygon
									pPatchWalk->pPrev->pNext = pPatchPrev->pNext;
									pPatchPrev->pNext->pPrev = pPatchWalk->pPrev;

									pPatchPrev->pNext = pPatchWalk;
									pPatchWalk->pPrev = pPatchPrev; //at this point, the two polygons's line linkages are one large polygon except that the new polygon's line are pointing at the dead polygon	which will be fixed as we continue to walk it					
								}
								pPatchWalk->pLine->pPolygons[pPatchWalk->iReferenceIndex] = pPatchPolygon;

								pPatchWalk = pPatchWalk->pNext;
							} while( pPatchWalk != pPatchStop );
							
							//final patch is just slightly different than other patches
							{
								DestructPolygon( pPatchStop->pLine->pPolygons[pPatchStop->iReferenceIndex], destructors );
								pPatchStop->pLine->pPolygons[pPatchStop->iReferenceIndex] = pPatchPolygon;
							}

							DestructPoint( pProblemPoint, destructors );

							contract_assert( pAllPolygons->pNext != nullptr );
							if( pAllPolygons->pNext->pNext != nullptr )
							{
								RemoveDegeneratePoints( pPatchPolygon, destructors );
							}

							//vLastLine and vThisLine are invalidated. Luckily we just need to recompute vThisLine and it'll copy to vLastLine
							vLineTemp = SubSIMD( pLineWalk->pLine->pPoints[pLineWalk->iReferenceIndex]->ptPosition, pLineWalk->pLine->pPoints[1 - pLineWalk->iReferenceIndex]->ptPosition );
							vThisLine.Init( SubFloat( vLineTemp, 0 ), SubFloat( vLineTemp, 1 ), SubFloat( vLineTemp, 2 ) );
						}
						
						vLastLine = vThisLine;
						pLineWalk = pLineWalk->pNext;
					} while ( pLineWalk != pLineWalkHead );
				}

				pActivePolygonWalk = pActivePolygonWalk->pNext;
			} while( pActivePolygonWalk );
		}


	}

	return ConvertLinkedGeometryToPolyhedron( pAllPolygons, pAllLines, pAllPoints, bUseTemporaryMemory, vShiftResultPositions );
}






CPolyhedron *GeneratePolyhedronFromPlanes( const float *pOutwardFacingPlanes, int iPlaneCount, float fOnPlaneEpsilon, bool bUseTemporaryMemory )
{
	ks::ScratchArena<> scratch;
	//this is version 2 of the polyhedron generator, version 1 made individual polygons and joined points together, some guesswork is involved and it therefore isn't a solid method
	//this version will start with a cube and hack away at it (retaining point connection information) to produce a polyhedron with no guesswork involved, this method should be rock solid
	
	//the polygon clipping functions we're going to use want inward facing planes
	const std::size_t kFltX4Align = sizeof( fltx4 ) - 1;
	uint8 *pAlignedAlloc = (uint8 *)scratch.AllocBytes( (sizeof( fltx4 ) * iPlaneCount) + kFltX4Align );
	pAlignedAlloc = (uint8 *)(((std::size_t)(pAlignedAlloc + kFltX4Align)) & ~kFltX4Align);
	fltx4 *pAlteredPlanes = (fltx4 *)pAlignedAlloc;
	for( int i = 0; i != iPlaneCount; ++i )
	{
		SubFloat( pAlteredPlanes[i], 0 ) = -pOutwardFacingPlanes[(i * 4) + 0];
		SubFloat( pAlteredPlanes[i], 1 ) = -pOutwardFacingPlanes[(i * 4) + 1];
		SubFloat( pAlteredPlanes[i], 2 ) = -pOutwardFacingPlanes[(i * 4) + 2];
		SubFloat( pAlteredPlanes[i], 3 ) = -pOutwardFacingPlanes[(i * 4) + 3];
	}

	//our first goal is to find the size of a cube big enough to encapsulate all points that will be in the final polyhedron
	Vector vAABBMinsVec, vAABBMaxsVec;
	if( FindConvexShapeLooseAABB( pAlteredPlanes, iPlaneCount, &vAABBMinsVec, &vAABBMaxsVec ) == false )
		return nullptr; //no shape to work with apparently

	fltx4 vAABBMins;
	SubFloat( vAABBMins, 0 ) = vAABBMinsVec.x;
	SubFloat( vAABBMins, 1 ) = vAABBMinsVec.y;
	SubFloat( vAABBMins, 2 ) = vAABBMinsVec.z;
	SubFloat( vAABBMins, 3 ) = 1.0f;

	fltx4 vAABBMaxs;
	SubFloat( vAABBMaxs, 0 ) = vAABBMaxsVec.x;
	SubFloat( vAABBMaxs, 1 ) = vAABBMaxsVec.y;
	SubFloat( vAABBMaxs, 2 ) = vAABBMaxsVec.z;
	SubFloat( vAABBMaxs, 3 ) = 1.0f;

	fltx4 vPointOffset = MulSIMD( AddSIMD( vAABBMins, vAABBMaxs ), ReplicateX4( -0.5f ) );
	vPointOffset = FloorSIMD( vPointOffset );
	SubFloat( vPointOffset, 3 ) = -1.0f;

	for( int i = 0; i != iPlaneCount; ++i )
	{
		pAlteredPlanes[i] = NegSIMD( pAlteredPlanes[i] );
	}

	//shift all the planes toward origin
	for( int i = 0; i != iPlaneCount; ++i )
	{
		fltx4 vMul = MulSIMD( pAlteredPlanes[i], vPointOffset );
		SubFloat( pAlteredPlanes[i], 3 ) = SubFloat( vMul, 0 ) + SubFloat( vMul, 1 ) + SubFloat( vMul, 2 ) - SubFloat( vMul, 3 );
	}
	//shift the AABB (and all starting points) toward the origin
	vAABBMins = AddSIMD( vAABBMins, vPointOffset );
	vAABBMaxs = AddSIMD( vAABBMaxs, vPointOffset );
	fltx4 vResultOffset = NegSIMD( vPointOffset );
	SubFloat( vResultOffset, 3 ) = 0.0f;
	
	//grow the bounding box to a larger size since it's probably inaccurate a bit
	{
		fltx4 vGrow = MulSIMD( SubSIMD( vAABBMaxs, vAABBMins ), Four_PointFives );
		vGrow = AddSIMD( vGrow, ReplicateX4( 10.0f ) );
		SubFloat( vGrow, 3 ) = 0.0f;
		
		vAABBMins = SubSIMD( vAABBMins, vGrow );
		vAABBMaxs = AddSIMD( vAABBMaxs, vGrow );
	}
	
	vAABBMins = FloorSIMD( vAABBMins );
	vAABBMaxs = FloorSIMD( vAABBMaxs );	

	vAABBMinsVec.x = SubFloat( vAABBMins, 0 );
	vAABBMinsVec.y = SubFloat( vAABBMins, 1 );
	vAABBMinsVec.z = SubFloat( vAABBMins, 2 );
	vAABBMaxsVec.x = SubFloat( vAABBMaxs, 0 );
	vAABBMaxsVec.y = SubFloat( vAABBMaxs, 1 );
	vAABBMaxsVec.z = SubFloat( vAABBMaxs, 2 );

	//generate our starting cube using the 2x AABB so we can start hacking away at it	

	//create our starting box on the stack
	GeneratePolyhedronFromPlanes_UnorderedPolygonLL StartingPolygonList[6]; //6 polygons
	GeneratePolyhedronFromPlanes_UnorderedLineLL StartingLineList[12]; //12 lines
	GeneratePolyhedronFromPlanes_UnorderedPointLL StartingPointList[8]; //8 points

	auto startPointToLineLinks = [&]( int iPointNum, int lineindex1, int iOtherPointIndex1, int lineindex2, int iOtherPointIndex2, int lineindex3, int iOtherPointIndex3 )
	{
		StartingPointList[iPointNum].point.pConnectedLines = &StartingLineList[lineindex1].line.PointLineLinks[1 - iOtherPointIndex1];
		StartingPointList[iPointNum].point.pConnectedLines->pNext = &StartingLineList[lineindex2].line.PointLineLinks[1 - iOtherPointIndex2];
		StartingPointList[iPointNum].point.pConnectedLines->pPrev = &StartingLineList[lineindex3].line.PointLineLinks[1 - iOtherPointIndex3];
		StartingPointList[iPointNum].point.pConnectedLines->pNext->pNext = StartingPointList[iPointNum].point.pConnectedLines->pPrev;
		StartingPointList[iPointNum].point.pConnectedLines->pNext->pPrev = StartingPointList[iPointNum].point.pConnectedLines;
		StartingPointList[iPointNum].point.pConnectedLines->pPrev->pNext = StartingPointList[iPointNum].point.pConnectedLines;
		StartingPointList[iPointNum].point.pConnectedLines->pPrev->pPrev = StartingPointList[iPointNum].point.pConnectedLines->pNext;
	};
	auto startBoxConnection = [&]( int linenum, int point1, int point2, int poly1, int poly2 )
	{
		StartingLineList[linenum].line.pPoints[0] = &StartingPointList[point1].point;
		StartingLineList[linenum].line.pPoints[1] = &StartingPointList[point2].point;
		StartingLineList[linenum].line.pPolygons[0] = &StartingPolygonList[poly1].polygon;
		StartingLineList[linenum].line.pPolygons[1] = &StartingPolygonList[poly2].polygon;
	};
	auto startPolygonToLineLinks = [&]( int polynum, int lineindex1, int iThisPolyIndex1, int lineindex2, int iThisPolyIndex2, int lineindex3, int iThisPolyIndex3, int lineindex4, int iThisPolyIndex4 )
	{
		StartingPolygonList[polynum].polygon.pLines = &StartingLineList[lineindex1].line.PolygonLineLinks[iThisPolyIndex1];
		StartingPolygonList[polynum].polygon.pLines->pNext = &StartingLineList[lineindex2].line.PolygonLineLinks[iThisPolyIndex2];
		StartingPolygonList[polynum].polygon.pLines->pPrev = &StartingLineList[lineindex4].line.PolygonLineLinks[iThisPolyIndex4];
		StartingPolygonList[polynum].polygon.pLines->pNext->pPrev = StartingPolygonList[polynum].polygon.pLines;
		StartingPolygonList[polynum].polygon.pLines->pPrev->pNext = StartingPolygonList[polynum].polygon.pLines;
		StartingPolygonList[polynum].polygon.pLines->pNext->pNext = &StartingLineList[lineindex3].line.PolygonLineLinks[iThisPolyIndex3];
		StartingPolygonList[polynum].polygon.pLines->pPrev->pPrev = StartingPolygonList[polynum].polygon.pLines->pNext->pNext;
		StartingPolygonList[polynum].polygon.pLines->pNext->pNext->pPrev = StartingPolygonList[polynum].polygon.pLines->pNext;
		StartingPolygonList[polynum].polygon.pLines->pPrev->pPrev->pNext = StartingPolygonList[polynum].polygon.pLines->pPrev;
	};

	for( int i = 0; i != 12; ++i )
	{
		StartingLineList[i].line.InitLineLinks();
	}


	//I had to work all this out on a whiteboard if it seems completely unintuitive.
	{
		SubFloat( StartingPointList[0].point.ptPosition, 0 ) = vAABBMinsVec.x;
		SubFloat( StartingPointList[0].point.ptPosition, 1 ) = vAABBMinsVec.y;
		SubFloat( StartingPointList[0].point.ptPosition, 2 ) = vAABBMinsVec.z;
		SubFloat( StartingPointList[0].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 0, 0, 1, 4, 1, 3, 0 );

		SubFloat( StartingPointList[1].point.ptPosition, 0 ) = vAABBMinsVec.x;
		SubFloat( StartingPointList[1].point.ptPosition, 1 ) = vAABBMaxsVec.y;
		SubFloat( StartingPointList[1].point.ptPosition, 2 ) = vAABBMinsVec.z;
		SubFloat( StartingPointList[1].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 1, 0, 0, 1, 1, 5, 1 );

		SubFloat( StartingPointList[2].point.ptPosition, 0 ) = vAABBMinsVec.x;
		SubFloat( StartingPointList[2].point.ptPosition, 1 ) = vAABBMinsVec.y;
		SubFloat( StartingPointList[2].point.ptPosition, 2 ) = vAABBMaxsVec.z;
		SubFloat( StartingPointList[2].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 2, 4, 0, 8, 1, 11, 0 );

		SubFloat( StartingPointList[3].point.ptPosition, 0 ) = vAABBMinsVec.x;
		SubFloat( StartingPointList[3].point.ptPosition, 1 ) = vAABBMaxsVec.y;
		SubFloat( StartingPointList[3].point.ptPosition, 2 ) = vAABBMaxsVec.z;
		SubFloat( StartingPointList[3].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 3, 5, 0, 9, 1, 8, 0 );

		SubFloat( StartingPointList[4].point.ptPosition, 0 ) = vAABBMaxsVec.x;
		SubFloat( StartingPointList[4].point.ptPosition, 1 ) = vAABBMinsVec.y;
		SubFloat( StartingPointList[4].point.ptPosition, 2 ) = vAABBMinsVec.z;
		SubFloat( StartingPointList[4].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 4, 2, 0, 3, 1, 7, 1 );

		SubFloat( StartingPointList[5].point.ptPosition, 0 ) = vAABBMaxsVec.x;
		SubFloat( StartingPointList[5].point.ptPosition, 1 ) = vAABBMaxsVec.y;
		SubFloat( StartingPointList[5].point.ptPosition, 2 ) = vAABBMinsVec.z;
		SubFloat( StartingPointList[5].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 5, 1, 0, 2, 1, 6, 1 );

		SubFloat( StartingPointList[6].point.ptPosition, 0 ) = vAABBMaxsVec.x;
		SubFloat( StartingPointList[6].point.ptPosition, 1 ) = vAABBMinsVec.y;
		SubFloat( StartingPointList[6].point.ptPosition, 2 ) = vAABBMaxsVec.z;
		SubFloat( StartingPointList[6].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 6, 7, 0, 11, 1, 10, 0 );

		SubFloat( StartingPointList[7].point.ptPosition, 0 ) = vAABBMaxsVec.x;
		SubFloat( StartingPointList[7].point.ptPosition, 1 ) = vAABBMaxsVec.y;
		SubFloat( StartingPointList[7].point.ptPosition, 2 ) = vAABBMaxsVec.z;
		SubFloat( StartingPointList[7].point.ptPosition, 3 ) = -1.0f;
		startPointToLineLinks( 7, 6, 0, 10, 1, 9, 0 );

		startBoxConnection( 0, 0, 1, 0, 5 );
		startBoxConnection( 1, 1, 5, 1, 5 );
		startBoxConnection( 2, 5, 4, 2, 5 );
		startBoxConnection( 3, 4, 0, 3, 5 );
		startBoxConnection( 4, 0, 2, 3, 0 );
		startBoxConnection( 5, 1, 3, 0, 1 );
		startBoxConnection( 6, 5, 7, 1, 2 );
		startBoxConnection( 7, 4, 6, 2, 3 );
		startBoxConnection( 8, 2, 3, 4, 0 );
		startBoxConnection( 9, 3, 7, 4, 1 );
		startBoxConnection( 10, 7, 6, 4, 2 );
		startBoxConnection( 11, 6, 2, 4, 3 );


		startBoxConnection( 0, 0, 1, 5, 0 );
		startBoxConnection( 1, 1, 5, 5, 1 );
		startBoxConnection( 2, 5, 4, 5, 2 );
		startBoxConnection( 3, 4, 0, 5, 3 );
		startBoxConnection( 4, 0, 2, 0, 3 );
		startBoxConnection( 5, 1, 3, 1, 0 );
		startBoxConnection( 6, 5, 7, 2, 1 );
		startBoxConnection( 7, 4, 6, 3, 2 );
		startBoxConnection( 8, 2, 3, 0, 4 );
		startBoxConnection( 9, 3, 7, 1, 4 );
		startBoxConnection( 10, 7, 6, 2, 4 );
		startBoxConnection( 11, 6, 2, 3, 4 );

		StartingPolygonList[0].polygon.vSurfaceNormal.Init( -1.0f, 0.0f, 0.0f );
		//StartingPolygonList[0].polygon.fNormalDist = -vAABBMinsVec.x;
		StartingPolygonList[1].polygon.vSurfaceNormal.Init( 0.0f, 1.0f, 0.0f );
		//StartingPolygonList[1].polygon.fNormalDist = vAABBMaxsVec.y;
		StartingPolygonList[2].polygon.vSurfaceNormal.Init( 1.0f, 0.0f, 0.0f );
		//StartingPolygonList[2].polygon.fNormalDist = vAABBMaxsVec.x;
		StartingPolygonList[3].polygon.vSurfaceNormal.Init( 0.0f, -1.0f, 0.0f );
		//StartingPolygonList[3].polygon.fNormalDist = -vAABBMinsVec.y;
		StartingPolygonList[4].polygon.vSurfaceNormal.Init( 0.0f, 0.0f, 1.0f );
		//StartingPolygonList[4].polygon.fNormalDist = vAABBMaxsVec.z;
		StartingPolygonList[5].polygon.vSurfaceNormal.Init( 0.0f, 0.0f, -1.0f );
		//StartingPolygonList[5].polygon.fNormalDist = -vAABBMinsVec.z;


		startPolygonToLineLinks( 0, 0, 1, 5, 1, 8, 0, 4, 0 );
		startPolygonToLineLinks( 1, 1, 1, 6, 1, 9, 0, 5, 0 );
		startPolygonToLineLinks( 2, 2, 1, 7, 1, 10, 0, 6, 0 );
		startPolygonToLineLinks( 3, 3, 1, 4, 1, 11, 0, 7, 0 );
		startPolygonToLineLinks( 4, 8, 1, 9, 1, 10, 1, 11, 1 );
		startPolygonToLineLinks( 5, 0, 0, 3, 0, 2, 0, 1, 0 );


		{
			StartingPolygonList[0].pNext = &StartingPolygonList[1];
			StartingPolygonList[0].pPrev = nullptr;

			StartingPolygonList[1].pNext = &StartingPolygonList[2];
			StartingPolygonList[1].pPrev = &StartingPolygonList[0];

			StartingPolygonList[2].pNext = &StartingPolygonList[3];
			StartingPolygonList[2].pPrev = &StartingPolygonList[1];

			StartingPolygonList[3].pNext = &StartingPolygonList[4];
			StartingPolygonList[3].pPrev = &StartingPolygonList[2];

			StartingPolygonList[4].pNext = &StartingPolygonList[5];
			StartingPolygonList[4].pPrev = &StartingPolygonList[3];

			StartingPolygonList[5].pNext = nullptr;
			StartingPolygonList[5].pPrev = &StartingPolygonList[4];
		}



		{
			StartingLineList[0].pNext = &StartingLineList[1];
			StartingLineList[0].pPrev = nullptr;

			StartingLineList[1].pNext = &StartingLineList[2];
			StartingLineList[1].pPrev = &StartingLineList[0];

			StartingLineList[2].pNext = &StartingLineList[3];
			StartingLineList[2].pPrev = &StartingLineList[1];

			StartingLineList[3].pNext = &StartingLineList[4];
			StartingLineList[3].pPrev = &StartingLineList[2];

			StartingLineList[4].pNext = &StartingLineList[5];
			StartingLineList[4].pPrev = &StartingLineList[3];

			StartingLineList[5].pNext = &StartingLineList[6];
			StartingLineList[5].pPrev = &StartingLineList[4];

			StartingLineList[6].pNext = &StartingLineList[7];
			StartingLineList[6].pPrev = &StartingLineList[5];

			StartingLineList[7].pNext = &StartingLineList[8];
			StartingLineList[7].pPrev = &StartingLineList[6];

			StartingLineList[8].pNext = &StartingLineList[9];
			StartingLineList[8].pPrev = &StartingLineList[7];

			StartingLineList[9].pNext = &StartingLineList[10];
			StartingLineList[9].pPrev = &StartingLineList[8];

			StartingLineList[10].pNext = &StartingLineList[11];
			StartingLineList[10].pPrev = &StartingLineList[9];

			StartingLineList[11].pNext = nullptr;
			StartingLineList[11].pPrev = &StartingLineList[10];
		}

		{
			StartingPointList[0].pNext = &StartingPointList[1];
			StartingPointList[0].pPrev = nullptr;

			StartingPointList[1].pNext = &StartingPointList[2];
			StartingPointList[1].pPrev = &StartingPointList[0];

			StartingPointList[2].pNext = &StartingPointList[3];
			StartingPointList[2].pPrev = &StartingPointList[1];

			StartingPointList[3].pNext = &StartingPointList[4];
			StartingPointList[3].pPrev = &StartingPointList[2];

			StartingPointList[4].pNext = &StartingPointList[5];
			StartingPointList[4].pPrev = &StartingPointList[3];

			StartingPointList[5].pNext = &StartingPointList[6];
			StartingPointList[5].pPrev = &StartingPointList[4];

			StartingPointList[6].pNext = &StartingPointList[7];
			StartingPointList[6].pPrev = &StartingPointList[5];

			StartingPointList[7].pNext = nullptr;
			StartingPointList[7].pPrev = &StartingPointList[6];
		}
	}

	CPolyhedron *pRetVal = ClipLinkedGeometry( StartingPolygonList, StartingLineList, StartingPointList, 8, pAlteredPlanes, iPlaneCount, fOnPlaneEpsilon, bUseTemporaryMemory, vResultOffset );


	return pRetVal;
}
