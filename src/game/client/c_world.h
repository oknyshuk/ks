//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( C_WORLD_H )
#define C_WORLD_H

#include "reflect_annotations.h"

#include "c_baseentity.h"

#if defined( CLIENT_DLL )
#define CWorld C_World
#endif

class [[= ks::reflect::NetTable{ .name = "DT_World" } ]]
      [[= ks::reflect::From<"m_flWaveHeight", ks::reflect::Net{}>{} ]]
      [[= ks::reflect::From<"m_iszDetailSpriteMaterial", ks::reflect::Net{}>{} ]]
      C_World : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_World, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_World( void );
	~C_World( void );
	
	// Override the factory create/delete functions since the world is a singleton.
	virtual bool Init( int entnum, int iSerialNum );
	virtual void Release();

	virtual void Precache();
	virtual void Spawn();

	// Don't worry about adding the world to the collision list; it's already there
	virtual CollideType_t	GetCollideType( void )	{ return ENTITY_SHOULD_NOT_COLLIDE; }

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void PreDataUpdate( DataUpdateType_t updateType );

	float GetWaveHeight() const;
	const char *GetDetailSpriteMaterial() const;


public:
	enum
	{
		MAX_DETAIL_SPRITE_MATERIAL_NAME_LENGTH = 256,
	};

	float	m_flWaveHeight;
	[[= ks::reflect::Net{} ]] Vector	m_WorldMins;
	[[= ks::reflect::Net{} ]] Vector	m_WorldMaxs;
	[[= ks::reflect::Net{} ]] bool	m_bStartDark;
	[[= ks::reflect::Net{} ]] float	m_flMaxOccludeeArea;
	[[= ks::reflect::Net{} ]] float	m_flMinOccluderArea;
	[[= ks::reflect::Net{} ]] float	m_flMinPropScreenSpaceWidth;
	[[= ks::reflect::Net{} ]] float	m_flMaxPropScreenSpaceWidth;
	[[= ks::reflect::Net{} ]] bool	m_bColdWorld;
	[[= ks::reflect::Net{} ]] int		m_iTimeOfDay;

private:
	char	m_iszDetailSpriteMaterial[MAX_DETAIL_SPRITE_MATERIAL_NAME_LENGTH];

};

inline float C_World::GetWaveHeight() const
{
	return m_flWaveHeight;
}

inline const char *C_World::GetDetailSpriteMaterial() const
{
	return m_iszDetailSpriteMaterial;
}

void ClientWorldFactoryInit();
void ClientWorldFactoryShutdown();
C_World* GetClientWorldEntity();

#endif // C_WORLD_H
