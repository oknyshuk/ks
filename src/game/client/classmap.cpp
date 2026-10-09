// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "iclassmap.h"

class classentry_t
{
public:
	classentry_t()
	{
		mapname[ 0 ] = 0;
		factory = 0;
		size = -1;
	}

	char const *GetMapName() const
	{
		return mapname;
	}

	void SetMapName( char const *newname )
	{
		V_strncpy( mapname, newname, sizeof( mapname ) );
	}

	DISPATCHFUNCTION	factory;
	int					size;
private:
	char				mapname[ 40 ];
};

class CClassMap : public IClassMap
{
public:
	virtual void			Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory /*= 0*/ );
	virtual const char		*Lookup( const char *classname );
	virtual C_BaseEntity	*CreateEntity( const char *mapname );
	virtual int				GetClassSize( const char *classname );

private:
	ks::idict< classentry_t > m_ClassDict;
};

IClassMap& GetClassMap( void )
{
	static CClassMap g_Classmap;
	return g_Classmap;
}

void CClassMap::Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory = 0 )
{
	const char *map = Lookup( classname );
	if ( map && !V_strcasecmp( mapname, map ) )
		return;

	classentry_t element;
	element.SetMapName( mapname );
	element.factory = factory;
	element.size = size;
	m_ClassDict.insert_or_assign( classname, element );
}

const char *CClassMap::Lookup( const char *classname )
{
	static classentry_t lookup; 

	auto it = m_ClassDict.find( classname );
	if ( it == m_ClassDict.end() )
		return nullptr;

	lookup = it->second;
	return lookup.GetMapName();
}

C_BaseEntity *CClassMap::CreateEntity( const char *mapname )
{
	for ( auto &[classname, entry] : m_ClassDict )
	{
		classentry_t *lookup = &entry;

		if ( V_stricmp( lookup->GetMapName(), mapname ) )
			continue;

		if ( !lookup->factory )
		{
#if defined( _DEBUG )
			Msg( "No factory for %s/%s\n", lookup->GetMapName(), classname.c_str() );
#endif
			continue;
		}

		return ( *lookup->factory )();
	}

	return nullptr;
}

int CClassMap::GetClassSize( const char *classname )
{
	for ( auto &[name, entry] : m_ClassDict )
	{
		classentry_t *lookup = &entry;

		if ( V_strcmp( lookup->GetMapName(), classname ) )
			continue;

		return lookup->size;
	}

	return -1;
}
