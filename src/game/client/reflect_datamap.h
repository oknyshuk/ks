// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#pragma once

#define IMPLEMENT_REFLECT_DATAMAP( className )                                                  \
	datamap_t className::m_DataMap = { .dataClassName = #className };                                  \
	datamap_t *className::GetDataDescMap( void )                                                   \
	{                                                                                             \
		/* BaseClass is a private typedef in places, so this member is the one scope that can    \
		   name it. See base_owner_matches: the reflective walk and the legacy chain must        \
		   resolve to the same declaring class. */                                               \
		static_assert( ks::reflect::base_owner_matches<className, BaseClass>(),                    \
		               "reflective base map is declared in a different class than BaseClass's" ); \
		return &m_DataMap;                                                                         \
	}                                                                                             \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                           \
		{                                                                                         \
			auto &f = ksr::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			Map().baseMap       = &[: ks::reflect::static_member_of(                               \
			    ks::reflect::base_with_static_member( ^^className, "m_DataMap" ),                  \
			    "m_DataMap" ) :];                                                                   \
			return true;                                                                           \
		}();                                                                                     \
	}
#define IMPLEMENT_REFLECT_DATAMAP_SIMPLE( className )                                             \
	datamap_t className::m_DataMap = { .dataClassName = #className };                                  \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                         \
		{                                                                                         \
			auto &f = ksr::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			return true;                                                                           \
		}();                                                                                     \
	}
#define IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( className, baseClass )                                  \
	static_assert( ks::reflect::base_owner_matches<className, baseClass>(),                     \
	               "reflective base map is declared in a different class than the named base" ); \
	datamap_t className::m_DataMap = { .dataClassName = #className };                                  \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                         \
		{                                                                                         \
			auto &f = ksr::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			Map().baseMap       = &[: ks::reflect::static_member_of(                              \
			    ks::reflect::base_with_static_member( ^^className, "m_DataMap" ),                  \
			    "m_DataMap" ) :];                                                                  \
			return true;                                                                           \
		}();                                                                                     \
	}
#define IMPLEMENT_REFLECT_DATAMAP_NO_BASE( className )                                             \
	datamap_t className::m_DataMap = { .dataClassName = #className };                                  \
	datamap_t *className::GetDataDescMap( void ) { return &m_DataMap; }                            \
	namespace className##_ReflectDataDescInit                                                     \
	{                                                                                             \
		static datamap_t &Map() { return [: ks::reflect::static_member_of(                         \
		    ^^className, "m_DataMap" ) :]; }                                                       \
		static const bool g_filled = []                                                         \
		{                                                                                         \
			auto &f = ksr::dmap::fields<className>();                                     \
			Map().dataDesc      = f.data();                                                       \
			Map().dataNumFields = (int)f.size();                                                   \
			return true;                                                                           \
		}();                                                                                     \
	}
