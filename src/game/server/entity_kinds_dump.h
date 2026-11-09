#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Generation of entity_kind_list.h (src/tools/entity_kinds/README.md): with KS_ENTITY_KINDS_DUMP defined,
// every registered entity class fails a static_assert whose message is the reflected class chain of the
// class (its bases and, per class, the virtual members and the members that share a name with a CBaseEntity
// virtual). Otherwise this header is empty.
#ifdef KS_ENTITY_KINDS_DUMP
class CBaseEntity;
namespace ks_dump
{
consteval bool derives( std::meta::info t, std::meta::info base )
{
	if ( t == base ) return true;
	if ( !std::meta::is_class_type( t ) || !std::meta::is_complete_type( t ) ) return false;
	for ( auto b : std::meta::bases_of( t, std::meta::access_context::unchecked() ) )
		if ( derives( std::meta::remove_cv( std::meta::type_of( b ) ), base ) ) return true;
	return false;
}
consteval std::string num( unsigned n ) { std::string r; do { r.insert( r.begin(), char( 48 + n % 10 ) ); n /= 10; } while ( n ); return r; }

consteval std::string cbe_names()
{
	std::string v = "|";
	for ( auto m : std::meta::members_of( ^^CBaseEntity, std::meta::access_context::unchecked() ) )
		if ( std::meta::is_function( m ) && std::meta::has_identifier( m ) && std::meta::is_virtual( m ) )
			{ v += std::meta::identifier_of( m ); v += "|"; }
	return v;
}
template <int = 0> inline constexpr const char *kCbeNames = std::define_static_string( cbe_names() );
template <int = 0> consteval bool is_cbe_virtual_name( std::string_view id )
{
	std::string key = "|"; key += id; key += "|";
	return std::string_view( kCbeNames<> ).find( key ) != std::string_view::npos;
}

template <class C>
consteval std::string describe()
{
	std::string s = "\nKS|REG|" + std::string( std::meta::display_string_of( ^^C ) ) + "\n";
	std::meta::info x = ^^C;
	for ( int depth = 0; depth < 40; ++depth )
	{
		auto loc = std::meta::source_location_of( x );
		s += "KS|CLASS|" + std::string( std::meta::display_string_of( x ) ) + "|" + ( std::meta::is_union_type( x ) ? "union" : "cls" ) + "|"
		   + std::string( loc.file_name() ) + "|" + num( loc.line() ) + "|" + ( std::meta::has_template_arguments( x ) ? "tmpl" : "plain" ) + "\n";
		int nbases = 0; std::meta::info next{};
		for ( auto b : std::meta::bases_of( x, std::meta::access_context::unchecked() ) )
		{
			s += "KS|BASE|" + std::string( std::meta::display_string_of( std::meta::type_of( b ) ) ) + "\n";
			if ( derives( std::meta::remove_cv( std::meta::type_of( b ) ), ^^CBaseEntity ) ) { ++nbases; next = std::meta::remove_cv( std::meta::type_of( b ) ); }
		}
		if ( x != ^^CBaseEntity )
		{
			for ( auto m : std::meta::members_of( x, std::meta::access_context::unchecked() ) )
			{
				if ( !std::meta::is_function( m ) || !std::meta::has_identifier( m ) ) continue;
				if ( std::meta::is_constructor( m ) || std::meta::is_destructor( m ) ) continue;
				std::string_view id = std::meta::identifier_of( m );
				bool v = std::meta::is_virtual( m );
				if ( !v && !is_cbe_virtual_name( id ) ) continue;
				s += std::string( "KS|FN|" ) + ( v ? "V" : "H" ) + "|" + std::string( id ) + "|" + std::string( std::meta::display_string_of( std::meta::type_of( m ) ) )
				   + "|" + ( std::meta::is_public( m ) ? "pub" : std::meta::is_protected( m ) ? "prot" : "priv" ) + "\n";
			}
		}
		if ( x == ^^CBaseEntity ) break;
		if ( nbases != 1 ) { s += "KS|BADCHAIN|" + num( nbases ) + "\n"; break; }
		x = next;
	}
	return s;
}
}
#define KS_ENTITY_KINDS_DUMP_CLASS( C ) static_assert( false, std::string_view{ std::define_static_string( ks_dump::describe<C>() ) } );
#else
#define KS_ENTITY_KINDS_DUMP_CLASS( C )
#endif
