//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTITYDATAINSTANTIATOR_H
#define ENTITYDATAINSTANTIATOR_H

#include <memory>
#include <type_traits>
#include <unordered_map>

// This is the hash key type, but it could just as easily be and int or void *
class CBaseEntity;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
abstract_class IEntityDataInstantiator
{
public:
	virtual ~IEntityDataInstantiator() {};

	virtual void *GetDataObject( const CBaseEntity *instance ) = 0;
	virtual void *CreateDataObject( const CBaseEntity *instance ) = 0;
	virtual void DestroyDataObject( const CBaseEntity *instance ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class T>
class CEntityDataInstantiator : public IEntityDataInstantiator
{
public:
	CEntityDataInstantiator()
	{
		m_HashTable.reserve( 64 );
	}

	virtual void *GetDataObject( const CBaseEntity *instance )
	{
		auto it = m_HashTable.find( instance );
		if ( it != m_HashTable.end() )
		{
			return (void *)it->second.get();
		}

		return nullptr;
	}

	virtual void *CreateDataObject( const CBaseEntity *instance )
	{
		// Create it if not already present
		auto [it, bInserted] = m_HashTable.try_emplace( instance );
		if ( bInserted )
		{
			it->second = std::make_unique<T>();

			// Zero-fill legacy POD data objects; types owning resources construct themselves
			if constexpr ( std::is_trivially_destructible_v<T> )
				Q_memset( it->second.get(), 0, sizeof( T ) );
		}

		return (void *)it->second.get();
	}

	virtual void DestroyDataObject( const CBaseEntity *instance )
	{
		m_HashTable.erase( instance );
	}

private:
	std::unordered_map< const CBaseEntity *, std::unique_ptr< T > > m_HashTable;
};

#endif // ENTITYDATAINSTANTIATOR_H
