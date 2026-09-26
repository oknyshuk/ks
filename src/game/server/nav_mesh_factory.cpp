//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NavMeshFactory.cpp
// Factory to create the NavMesh singleton
// Author: Michael S. Booth, June 2005

#include "cbase.h"
#include "nav_mesh.h"


#ifdef CSTRIKE_DLL
#include "cs_nav_mesh.h"
#endif

CNavMesh *NavMeshFactory( void )
{

#ifdef CSTRIKE_DLL
	return new CSNavMesh;
#endif

	return new CNavMesh;
}
