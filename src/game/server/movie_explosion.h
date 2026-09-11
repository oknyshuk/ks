//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef MOVIE_EXPLOSION_H
#define MOVIE_EXPLOSION_H

#include "reflect_annotations.h"


#include "baseparticleentity.h"


class [[= ks::reflect::NetTable{ .name = "DT_MovieExplosion" } ]]
      MovieExplosion : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( MovieExplosion, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	static MovieExplosion* CreateMovieExplosion(const Vector &pos);
};


#endif


