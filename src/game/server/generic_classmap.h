//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_GENERIC_CLASSMAP_H
#define ASW_GENERIC_CLASSMAP_H

#include "tier1/map_ops.h"

template <class T>
class CGenericClassmap
{
public:
	typedef T *( *DISPATCHFUNCTION )( void );

				CGenericClassmap() { }
				~CGenericClassmap() { }

	void		Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory = 0 );
	char const	*Lookup( const char *classname );
	T			*CreateInstance( const char *mapname );
	int			GetClassSize( const char *classname );

private:
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
			Q_strncpy( mapname, newname, sizeof( mapname ) );
		}

		DISPATCHFUNCTION	factory;
		int					size;
	private:
		char				mapname[ 40 ];
	};

	ks::idict< classentry_t > m_ClassDict;
};

template <class T>
void CGenericClassmap< T >::Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory )
{
	const char *map = Lookup( classname );
	if ( map && !Q_strcasecmp( mapname, map ) )
		return;

	classentry_t element;
	element.SetMapName( mapname );
	element.factory = factory;
	element.size = size;
	m_ClassDict.insert_or_assign( classname, element );
}

template <class T>
const char *CGenericClassmap< T >::Lookup( const char *classname )
{
	static classentry_t lookup; 

	auto it = m_ClassDict.find( classname );
	if ( it == m_ClassDict.end() )
		return nullptr;

	lookup = it->second;
	return lookup.GetMapName();
}

template <class T>
T *CGenericClassmap< T >::CreateInstance( const char *mapname )
{
	for ( auto &[name, entry] : m_ClassDict )
	{
		classentry_t *lookup = &entry;

		if ( Q_stricmp( lookup->GetMapName(), mapname ) )
			continue;

		if ( !lookup->factory )
		{
#if defined( _DEBUG )
			Msg( "No factory for %s/%s\n", lookup->GetMapName(), name.c_str() );
#endif
			continue;
		}

		return ( *lookup->factory )();
	}

	return nullptr;
}

template <class T>
int CGenericClassmap< T >::GetClassSize( const char *classname )
{
	for ( auto &[name, entry] : m_ClassDict )
	{
		classentry_t *lookup = &entry;

		if ( Q_strcmp( lookup->GetMapName(), classname ) )
			continue;

		return lookup->size;
	}

	return -1;
}


#endif // ASW_GENERIC_CLASSMAP_H
