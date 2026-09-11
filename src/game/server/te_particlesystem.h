//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TE_PARTICLESYSTEM_H
#define TE_PARTICLESYSTEM_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif


#include "basetempentity.h"


class [[= ks::reflect::NetTable{ .name = "DT_TEParticleSystem" } ]]
      CTEParticleSystem : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEParticleSystem, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEParticleSystem(const char *pName) : BaseClass(pName)
	{
		m_vecOrigin.GetForModify().Init();
	}

	CNetworkVectorXYZ( m_vecOrigin, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .index = 2 } ]]  [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .index = 1 } ]]  [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .index = 0 } ]] );
};


#endif // TE_PARTICLESYSTEM_H
