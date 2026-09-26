// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)


#pragma once

#include "isaverestore.h"

#include <map>
#include <string>
#include <vector>

struct JoltSurfaceProp
{
	surfacedata_t data;
};

class JoltPhysicsMaterialIndexSaveOps : public CDefSaveRestoreOps
{
public:
	static JoltPhysicsMaterialIndexSaveOps& GetInstance() { return s_Instance; }

private:
	static JoltPhysicsMaterialIndexSaveOps s_Instance;
};

class JoltPhysicsSurfaceProps final : public IPhysicsSurfaceProps
{
public:
	JoltPhysicsSurfaceProps();

	int					ParseSurfaceData( const char *pFilename, const char *pTextfile ) override;
	int					SurfacePropCount( void ) const override;

	int					GetSurfaceIndex( const char *pSurfacePropName ) const override;
	void				GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const override;

	surfacedata_t		*GetSurfaceData( int surfaceDataIndex ) override;
	const char			*GetString( unsigned short stringTableIndex ) const override;

	const char			*GetPropName( int surfaceDataIndex ) const override;

	void				SetWorldMaterialIndexTable( int *pMapArray, int mapSize ) override;

	void				GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const override;

	//ISaveRestoreOps		*GetMaterialIndexDataOps() const override_portal2;

	// GMod-specific internal gubbins that was exposed in the public interface.
	void				*GetIVPMaterial( int nIndex ) override_gmod;
	int					GetIVPMaterialIndex( const void *pMaterial ) const override_gmod;
	void				*GetIVPManager( void ) override_gmod;
	int					RemapIVPMaterialIndex( int nIndex ) const override_gmod;
	const char 			*GetReservedMaterialName( int nMaterialIndex ) const override_gmod;

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
