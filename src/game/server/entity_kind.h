#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The closed set of server entity classes.
//
// Every class that can be the most-derived type of a CBaseEntity -- and every class between such a
// class and CBaseEntity -- is one entry of entity_kind_list.h, in depth-first pre-order of the class
// hierarchy. That order is what makes the rest cheap:
//   - EntityKind is the position in the list;
//   - the classes derived from T are exactly the kinds [ kind_of<T>, subtree_end( kind_of<T> ) ), so
//     is_a<T>( kind ) is two comparisons and replaces dynamic_cast on entities;
//   - the dispatchers of entity_dispatch.h switch on the kind.
//
// The list is generated (src/tools/entity_kinds, from the compiler's own view of the registered classes)
// and verified by the compiler: LINK_ENTITY_TO_CLASS and _CreateEntityTemplate require the class to be an
// instantiable entry (kind_of<T>), and LINK_ENTITY_TO_CLASS walks the class' real base chain with
// reflection and checks every link against the list (check_registered_class below). A stale list
// is a compile error in the translation unit that registers the class.

#include "entity_kind_list.h"

enum class EntityKind : uint16
{
	None = 0,	// not a kind: an entity that was not made by the factory has this, and dispatching on it is fatal
#define KS_ENTITY_KIND_ENUM( cls, parent, instantiable ) cls,
	KS_ENTITY_KINDS( KS_ENTITY_KIND_ENUM )
#undef KS_ENTITY_KIND_ENUM
	Count
};

// the classes, so the list can be spelled before their definitions
#define KS_ENTITY_KIND_FWD( cls, parent, instantiable ) class cls;
KS_ENTITY_KINDS( KS_ENTITY_KIND_FWD )
#undef KS_ENTITY_KIND_FWD

namespace entity_kinds
{
inline constexpr int kCount = static_cast<int>( EntityKind::Count );

// kind_of<T>: the kind of a listed class (a class that is not listed does not compile)
template <class T> inline constexpr EntityKind kind_of = EntityKind::None;
#define KS_ENTITY_KIND_OF( cls, parent, instantiable ) \
	template <> inline constexpr EntityKind kind_of<cls> = EntityKind::cls;
KS_ENTITY_KINDS( KS_ENTITY_KIND_OF )
#undef KS_ENTITY_KIND_OF

struct Node
{
	EntityKind parent;
	bool       instantiable;
};
inline constexpr Node kNodes[kCount] = {
	{ EntityKind::None, false },
#define KS_ENTITY_KIND_NODE( cls, parent, instantiable ) { EntityKind::parent, instantiable != 0 },
	KS_ENTITY_KINDS( KS_ENTITY_KIND_NODE )
#undef KS_ENTITY_KIND_NODE
};

consteval int index_of( EntityKind k ) { return static_cast<int>( k ); }

// The first kind after the subtree of k. The list is in pre-order, so the subtree is contiguous.
consteval EntityKind compute_subtree_end( EntityKind k )
{
	const int i = index_of( k );
	int e = i + 1;
	while ( e < kCount )
	{
		bool inside = false;
		for ( int p = index_of( kNodes[e].parent ); p > 0; p = index_of( kNodes[p].parent ) )
			if ( p == i ) { inside = true; break; }
		if ( !inside ) break;
		++e;
	}
	return static_cast<EntityKind>( e );
}

// The list must be a pre-order: a parent precedes its children (the root, CBaseEntity, is its own parent),
// and the descendants of every node are contiguous right behind it.
consteval bool list_is_preorder()
{
	for ( int i = 2; i < kCount; ++i )
		if ( index_of( kNodes[i].parent ) >= i )
			return false;
	for ( int i = 1; i < kCount; ++i )
	{
		const int end = index_of( compute_subtree_end( static_cast<EntityKind>( i ) ) );
		for ( int d = end; d < kCount; ++d )
			for ( int p = index_of( kNodes[d].parent ); p > 0; p = index_of( kNodes[p].parent ) )
				if ( p == i ) return false;
	}
	return true;
}
static_assert( list_is_preorder(), "entity_kind_list.h is not a depth-first pre-order of the class hierarchy: regenerate it" );

template <class T> inline constexpr EntityKind subtree_end_of = compute_subtree_end( kind_of<T> );

} // namespace entity_kinds

using entity_kinds::kind_of;

// Is an entity of this (most-derived) kind a T, i.e. would dynamic_cast<T*> succeed?
template <class T>
inline constexpr bool is_a( EntityKind kind )
{
	static_assert( kind_of<T> != EntityKind::None, "is_a<T>: T is not an entity kind (entity_kind_list.h)" );
	return kind >= kind_of<T> && kind < entity_kinds::subtree_end_of<T>;
}

// The checked cast from an entity base pointer to a listed class: nullptr when the entity is not a T.
// Accepts null. Replaces dynamic_cast<T *>( pEntity ) (see CBaseEntity::GetKind).
template <class T, class E>
inline T *entity_cast( E *pEntity )
{
	return ( pEntity && is_a<std::remove_const_t<T>>( pEntity->GetKind() ) ) ? static_cast<T *>( pEntity ) : nullptr;
}

// the kind CBaseEntity::CBaseEntity gives the entity it constructs (set by _CreateEntityTemplate)
extern EntityKind g_PendingEntityKind;

// Fatal contract failures (entity_dispatch.cpp). Contract assertions are ignored in this build, and a missing kind must never be.
[[noreturn]] void EntityKindFailure( const char *pszWhat, const char *pszClassname, int nKind );

namespace entity_kinds
{
template <class T> consteval bool is_instantiable()
{
	return kind_of<T> != EntityKind::None && kNodes[index_of( kind_of<T> )].instantiable;
}
}

// ---- verification of a registration (LINK_ENTITY_TO_CLASS) ---------------------------------
namespace entity_kinds
{
inline constexpr std::meta::info kTypes[kCount] = {
	std::meta::info{},
#define KS_ENTITY_KIND_TYPE( cls, parent, instantiable ) ^^cls,
	KS_ENTITY_KINDS( KS_ENTITY_KIND_TYPE )
#undef KS_ENTITY_KIND_TYPE
};

consteval EntityKind listed_kind_of( std::meta::info t )
{
	t = std::meta::remove_cv( t );
	for ( int i = 1; i < kCount; ++i )
		if ( kTypes[i] == t ) return static_cast<EntityKind>( i );
	return EntityKind::None;
}

consteval bool derives_from_entity( std::meta::info t )
{
	if ( t == kTypes[1] ) return true;
	if ( !std::meta::is_class_type( t ) || !std::meta::is_complete_type( t ) ) return false;
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
		if ( derives_from_entity( std::meta::remove_cv( std::meta::type_of( b ) ) ) ) return true;
	return false;
}

// The nearest listed class below t on the path towards CBaseEntity (classes that are not listed, such as
// the CAI_ExpresserHost<> mixins, are looked through).
consteval EntityKind nearest_listed_base( std::meta::info t )
{
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
	{
		const std::meta::info bt = std::meta::remove_cv( std::meta::type_of( b ) );
		if ( !derives_from_entity( bt ) ) continue;
		const EntityKind k = listed_kind_of( bt );
		return k != EntityKind::None ? k : nearest_listed_base( bt );
	}
	return EntityKind::None;
}

// The list's parent link of every listed class on the chain from t to CBaseEntity is the real one, and every
// base of the chain that has several entity bases is rejected (there is a single kind per object).
consteval bool chain_matches_list( std::meta::info t )
{
	for ( ;; )
	{
		int entity_bases = 0;
		for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
			if ( derives_from_entity( std::meta::remove_cv( std::meta::type_of( b ) ) ) ) ++entity_bases;
		if ( t == kTypes[1] ) return true;
		if ( entity_bases != 1 ) return false;
		const EntityKind self = listed_kind_of( t );
		const EntityKind real_parent = nearest_listed_base( t );
		if ( self != EntityKind::None && kNodes[index_of( self )].parent != real_parent )
			return false;
		for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
			if ( derives_from_entity( std::meta::remove_cv( std::meta::type_of( b ) ) ) )
				{ t = std::meta::remove_cv( std::meta::type_of( b ) ); break; }
	}
}

template <class C>
consteval void check_registered_class()
{
	static_assert( kind_of<C> != EntityKind::None, "entity class is not in entity_kind_list.h: regenerate it (src/tools/entity_kinds)" );
	static_assert( kNodes[index_of( kind_of<C> )].instantiable, "entity_kind_list.h lists this registered class as not instantiable" );
	static_assert( chain_matches_list( ^^C ), "entity_kind_list.h does not match the class hierarchy: regenerate it (src/tools/entity_kinds)" );
}
} // namespace entity_kinds
