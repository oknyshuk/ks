#pragma once

// Maximum valence we expect to encounter for extraordinary vertices
inline constexpr auto MAX_VALENCE = 19;

inline constexpr auto CORNER_WITH_SMOOTHBNDTANGENTS = 2;

import std;
import ks.math;

// Undef this to use the unoptimized conversion routines

// Uncomment this to separate computation between regular patches and extraordinary patches

// Uncomment this to disable tangent patch computation
//#define NO_TANGENTS

// if you change this make sure to adjust NUM_TOPOLOGY_INDICES_ATTRIBUTES in studiorendercontext.cpp as well!
struct TopologyIndexStruct
{
	unsigned short *vtx1RingSize;
	unsigned short *vtx1RingCenterQuadOffset;

	unsigned short *valences;
	unsigned short *minOneRingOffset;

	unsigned short *bndVtx;
	unsigned short *bndEdge;
	unsigned short *cornerVtx;

	unsigned short *loopGapAngle;
	
	unsigned short *edgeBias;

	unsigned short *nbCornerVtx;

	unsigned short *oneRing;

	unsigned short *vUV0;
	unsigned short *vUV1;
	unsigned short *vUV2;
	unsigned short *vUV3;
};

void set_ShowACCGeometryTangents(bool v);
void set_CornerCorrection(bool v);
void set_UseCornerTangents(bool v);

void FillTables(); // fill patch stencil buffers

// compute patch control points
void ComputeACCAllPatches( fltx4* pWSVertices, TopologyIndexStruct* quad, Vector4D* Pos, Vector4D* TanU, Vector4D* TanV, bool bRegularPatch = false );

void EvaluateACC(unsigned short nPoints, Vector2D *UVs, Vector4D *cpP, Vector4D *cpU, Vector4D *cpV, Vector4D *pos, Vector *nor, Vector2D *uv, Vector4D *tanU, Vector4D *tanV );
void EvaluateGregory(unsigned short nPoints, Vector2D *UVs, Vector4D *cpP, Vector4D *pos, Vector *nor, Vector2D *uv, Vector4D *tanU, Vector4D *tanV );


