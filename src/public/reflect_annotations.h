// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The annotation vocabulary, and nothing else. Entity headers include this; it must
// stay cheap. <meta> alone is 49,552 preprocessed lines and ~0.6s of parse, which is
// unaffordable in a header reached by hundreds of TUs, so everything that reads these
// annotations lives in reflect.h instead.
//
// Annotate only what cannot be derived: which systems a member takes part in, and the
// wire parameters. The semantic type goes in the member's *type* (see the As-annotated
// types below), so it is checked at every use site rather than only inside a table.

#ifndef KS_REFLECT_ANNOTATIONS_H
#define KS_REFLECT_ANNOTATIONS_H

#include "datamap.h"
#include "dt_common.h"   // SPROP_* flags, which Net::flags carries
#include "mathlib/vector.h"

namespace ks::reflect
{

// Compile-time only, so the fixed size costs nothing at runtime. An annotation may not
// hold a const char* to a literal: extract() then fails wholesale, even when reading an
// unrelated integer field of the same annotation.
struct name_t
{
	char data[64]{};
	consteval name_t() = default;
	consteval name_t( const char *s ) { for ( int i = 0; s[i] && i < 63; ++i ) data[i] = s[i]; }
	consteval bool empty() const { return data[0] == '\0'; }
};

// ---- vocabulary ---------------------------------------------------------------
// As refines a structural type into a semantic one. Prefer it on a *type*; on a member
// it is an escape hatch for when one type has to mean two things.
struct As { fieldtype_t type = FIELD_VOID; };

// Wire parameters, reproducing the SendProp arguments. These cannot be derived because
// they are bandwidth decisions. `wire` covers the SENDINFO_NAME cases.
//
// bits = -1 means "whatever the SendProp constructor defaults to", which differs by kind:
// 32 for float and vector, -1 (auto from the member size) for integers. high defaults to
// the HIGH_DEFAULT sentinel, not 0, or every generated float prop would be misquantised.
struct Net
{
	int   bits  = -1;
	float low   = 0.f;
	float high  = HIGH_DEFAULT;
	int   flags = 0;
	name_t wire{};
};

struct Key  { name_t name{}; };        // map keyvalue; empty name = use the identifier
struct Pred { float tolerance = 0.f; };

// ---- semantic types ----------------------------------------------------------
// The compiler derives structure, never meaning: that a float is a time, or a Vector a
// world coordinate needing fixup across level transitions. Stating it in the type makes
// the tag derivable and, unlike a table entry, makes wrong uses fail to compile.
struct [[= As{ FIELD_TIME } ]] GameTime
{
	float v{};
	GameTime() = default;
	constexpr GameTime( float f ) : v( f ) {}
	constexpr operator float() const { return v; }
};

struct [[= As{ FIELD_TICK } ]] TickCount
{
	int v{};
	TickCount() = default;
	constexpr TickCount( int i ) : v( i ) {}
	constexpr operator int() const { return v; }
};

struct [[= As{ FIELD_POSITION_VECTOR } ]] WorldPos : Vector
{
	using Vector::Vector;
};

} // namespace ks::reflect

#endif // KS_REFLECT_ANNOTATIONS_H
