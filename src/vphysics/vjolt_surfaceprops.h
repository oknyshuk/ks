// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

import ks.tier1;
#include "isaverestore.h"

import std;
#include "vphysics_interface.h"

struct JoltSurfaceProp
{
	surfacedata_t data;
};

class JoltPhysicsSurfaceProps final : public IPhysicsSurfaceProps
{
public:
	JoltPhysicsSurfaceProps();

	int					ParseSurfaceData( const char *pFilename, const char *pTextfile );
	int					SurfacePropCount( void ) const;

	int					GetSurfaceIndex( const char *pSurfacePropName ) const;
	void				GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const;

	surfacedata_t		*GetSurfaceData( int surfaceDataIndex );
	const char			*GetString( unsigned short stringTableIndex ) const;

	const char			*GetPropName( int surfaceDataIndex ) const;

	void				SetWorldMaterialIndexTable( int *pMapArray, int mapSize );

	void				GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const;

	//ISaveRestoreOps		*GetMaterialIndexDataOps() const override_portal2;

	// GMod-specific internal gubbins that was exposed in the public interface.
	void				*GetIVPMaterial( int nIndex ) ;
	int					GetIVPMaterialIndex( const void *pMaterial ) const ;
	void				*GetIVPManager( void ) ;
	int					RemapIVPMaterialIndex( int nIndex ) const ;
	const char 			*GetReservedMaterialName( int nMaterialIndex ) const ;

public:
	static JoltPhysicsSurfaceProps& GetInstance() { return s_PhysicsSurfaceProps; }

	unsigned short		RegisterSound( const char *pName );

private:
	static JoltPhysicsSurfaceProps s_PhysicsSurfaceProps;

	// Indices are dense, in first-parse order, and escape as surfaceprop ids; deque keeps
	// GetSurfaceData pointers valid across later parses.
	std::deque< JoltSurfaceProp >		m_SurfaceProps;
	std::vector< std::string >			m_SurfacePropNames;
	ks::idict< int >					m_SurfacePropIndex;
	// Sound names; dense indices escape through surfacedata_t::sounds.
	std::vector< std::string >			m_SoundStrings;
	std::map< std::string, unsigned short, std::less<> > m_SoundStringIndex;
	
	static constexpr int BaseMaterialIdx = 0;

	KeyValues *SurfacePropsToKeyValues( const char *pszBuffer );
};
