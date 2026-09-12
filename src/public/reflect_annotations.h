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

#include "reflect_wirevar.h"   // WireVar; a leaf header so networkvar.h can reach it cheaply
#include "datamap.h"
#include <limits>

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

consteval bool same_name( const name_t &a, const name_t &b )
{
	for ( int i = 0; i < 64; ++i )
	{
		if ( a.data[i] != b.data[i] ) return false;
		if ( !a.data[i] ) break;
	}
	return true;
}

// SENDINFO and RECVINFO stringify the whole member expression, so a prop reached through a dotted
// path is named by the path and not by the leaf member's identifier.
consteval bool is_path( const name_t &n )
{
	for ( int i = 0; i < 64 && n.data[i]; ++i )
		if ( n.data[i] == '.' ) return true;
	return false;
}

// ---- vocabulary ---------------------------------------------------------------
// As refines a structural type into a semantic one. Prefer it on a *type*; on a member
// it is an escape hatch for when one type has to mean two things.
struct As { fieldtype_t type = FIELD_VOID; };

// Wire parameters, reproducing the SendProp arguments. These cannot be derived because
// they are bandwidth decisions. `wire` covers the SENDINFO_NAME cases.
//
// bits = kBitsDefault means "whatever the factory would have defaulted to", which differs by
// kind. -1 is *not* usable as that sentinel: call sites pass it literally (CColorCorrection's
// origin does), and it reaches AssignRangeMultiplier's `1 << nBits` as a negative shift.
//
// enc selects an encoding the member's type cannot imply: a float sent as a 0..360 angle, an
// int sent as a model index, or a QAngle sent through SendPropVector rather than
// SendPropQAngles -- the two differ in range and rounding, and the member type does not say
// which the table wants. Both are bandwidth decisions, so they belong here.
inline constexpr int kBitsDefault = std::numeric_limits<int>::min();
enum class WireEnc
{
	Auto, Angle, ModelIndex, Vector, QAngles, Int, Float,
	// Send/RecvPropVectorXY: the z component is sent separately, usually as its own
	// component prop, so this cannot be derived from the member type.
	VectorXY,
	// The element of a char[N][M] or string_t[N] array: Send/RecvPropString, which no
	// element type can imply -- char[M] tags as FIELD_CHARACTER, i.e. an int.
	String,
};

// A shared member need not appear in both tables. m_iRecoilIndex's SendPropInt is commented out
// while its RecvPropInt is live, and one annotation on the member serves both directions, so the
// annotation is what says which side it is for.
enum class WireSide { Both, Send, Recv };

struct Net
{
	int   bits  = kBitsDefault;
	float low   = 0.f;
	float high  = kHighDefault;
	int   flags = 0;
	WireEnc enc = WireEnc::Auto;
	WireSide side = WireSide::Both;
	name_t wire{};
	// Which of the class's tables this member belongs to; empty means the primary one. A class
	// can send several tables (CBaseCombatCharacter has DT_BCCLocalPlayerExclusive and
	// DT_BCCNonLocalPlayerExclusive besides its own), and the member is what knows which.
	name_t table{};
	// One component of a Vector or QAngle member, reproducing SENDINFO_VECTORELEM. Repeatable:
	// one annotation per component, and the components must stay in order, which is why the
	// emitter walks the annotations in declaration order. Appended last so that no existing
	// designated initialiser has to be reordered.
	int    index = -1;
	// One element of an array member, reproducing RECVINFO( m_audio.localSound[0] ): the prop is a
	// scalar of the element type at that element's offset. Distinct from `index`, which is a lane of
	// a Vector or QAngle and whose stride is a third of the member. Set by find_member_path from a
	// bracketed path rather than written by hand.
	int    elem  = -1;
	// SendProp's trailing priority argument, which SendTable_SortByPriority uses to order the
	// flattened array. -1 leaves it at SENDPROP_DEFAULT_PRIORITY. Send-only: RecvProp has no such
	// field. The comparator already checks m_priority, so getting this wrong fails the gate rather
	// than shipping quietly.
	int    priority = -1;
	// The OLD Send/RecvPropArray, i.e. Send/RecvPropVariableLengthArray: emits TWO entries, the
	// element template followed by an InternalSend/RecvPropArray carrying the count and stride.
	// Distinct from the default array handling, which is the single-prop SendPropArray3.
	bool   varlen = false;
	// SENDINFO_NOCHECK takes the raw offset, where SENDINFO ORs the networkvar flags into its high
	// bits. C_SmokeStack sends m_DirLight.m_vPos that way even though the member is a CNetworkVector
	// whose GetNetworkVarFlags() is non-zero, so the flags have to be suppressible.
	bool   raw = false;
};

consteval Net with_elem( Net n, int elem )
{
	n.elem = elem;
	return n;
}

consteval Net wire_from_path( Net n, const name_t &path )
{
	if ( n.wire.empty() && is_path( path ) ) n.wire = path;
	return n;
}

consteval bool on_side( const Net &n, WireSide want )
{
	return n.side == WireSide::Both || n.side == want;
}

// A map keyvalue; an empty name means use the identifier. DEFINE_INPUT is the same field also
// reachable as an input, which is a flag on the field rather than a separate entry.
// index selects one element of an array member, reproducing DEFINE_KEYFIELD( m_x[3], ... ).
// Key is repeatable: one annotation per element, each with its own map name.
// as refines the field type for this entry alone. A member-level As applies to every
// annotation on the member, which is wrong when one array holds elements of
// different semantic types -- two of CPostProcessController's twelve are FIELD_TIME.
// global reproduces DEFINE_GLOBAL_KEYFIELD, which is DEFINE_KEYFIELD plus FTYPEDESC_GLOBAL.
struct Key  { name_t name{}; bool input = false; int index = -1;
              fieldtype_t as = FIELD_VOID; bool global = false; };

// On a member *function*, not a field: reproduces DEFINE_INPUTFUNC. type is the fieldtype the
// input's argument is parsed as, which is a map-format decision and so cannot be derived from
// the signature -- every input handler takes inputdata_t regardless.
// Repeatable: one handler can answer to several names. CDynamicProp::InputTurnOn is reached by
// both "TurnOn" and "Enable", and CBaseAnimating::InputIgnite by three names.
struct Input { name_t name{}; fieldtype_t type = FIELD_VOID; };

// Prediction participation: the FTYPEDESC_ set DEFINE_PRED_FIELD takes, plus the _TOL extra.
// Nothing else -- the field type is the member's, and an embedded entry
// (DEFINE_PRED_TYPEDESCRIPTION) needs no flags because the macro hardcodes them.
struct Pred { int flags = 0; float tolerance = 0.f; };

// A custom proxy function, carried as a template argument rather than a member: an annotation
// value may not hold a pointer to an external function, and this keeps the function reachable
// as a reflection the emitter can splice.
// A proxy can differ per side, and a member carries one annotation for both: m_flSpriteScale sends
// with no proxy and receives through RecvProxy_SpriteScale. Extracting a recv proxy as a send one
// would not merely be wrong, it would fail to compile, so the side is part of the annotation.
template <auto F, WireSide S = WireSide::Both> struct Proxy {};

// A sub-table prop named directly rather than through a member. "localdata" is not a member of
// CBasePlayer: it is the name the wire gives DT_LocalPlayerExclusive, and the proxy decides which
// clients receive it. Table and Fn are template arguments because an annotation value may not hold
// a pointer to an external object or function. AtHead because position is wire-visible and the two
// observed positions are exactly these: DT_BaseCombatCharacter lists its two right after the
// baseclass prop, DT_BasePlayer lists its one last.
template <name_t Name, auto Table, auto Fn = nullptr, bool AtHead = false> struct SubTable {};

// A scalar prop named by a bare string with no member behind it: the proxy writes wherever it
// likes. C_Sun and C_LightGlow both receive RecvPropFloat( "HDRColorScale", 0, kSizeofIgnore, 0,
// RecvProxy_HDRColorScale ), which fans one wire value out into two overlay structs. There is no
// member to annotate, so the kind cannot be derived: N.enc must name it.
template <name_t Name, Net N = Net{}, auto Fn = nullptr> struct Bare {};

// A CUtlVector prop: Send/RecvPropUtlVector. Member is the vector, Max its element cap, Table the
// sub-table each element is described by (the element prop is unnamed, so it cannot come from a
// member). Fn is the recv-side resize callback, unused on the send side. Everything else --
// offset, element size, the ensure-capacity and resize thunks -- is derivable from the member.
template <name_t Member, int Max, auto Table, auto Fn = nullptr, name_t Tbl = name_t{}>
struct UtlVec {};

// Send/RecvPropArray2: an element template and an InternalSend/RecvPropArray, neither of which is
// backed by a member -- both are named by string literals and sit at offset 0, and a proxy does
// the addressing. CTeam's "player_array" is the only instance. Note the legacy macro stringifies
// its already-quoted arrayName argument, so the array prop's name keeps the quotes.
template <name_t ElemName, name_t ArrayName, int Count, int Stride, int Size,
          Net N = Net{}, auto ElemFn = nullptr, auto LenFn = nullptr>
struct BareArray {};

// Class-level and repeatable: reproduces SendPropExclude, which names a prop in another table
// rather than a member of this one.
struct Exclude { name_t table{}; name_t prop{}; };

// Class-level and repeatable: a member declared in a *base* that this table sends itself, which
// is what every _NOBASE table does. Named by identifier rather than by ^^Base::member so that a
// private base member is still reachable, and so the annotation reads like the SENDINFO it
// replaces. Fn is an optional proxy, as in Proxy<F>. Table is the sub-table an embedded member
// points at: that pointer is not derivable from the member's type, because a Source sub-table
// lives in a namespace the class does not reference, so it has to be named when the embedded
// type is not itself migrated.
template <name_t Name, Net N = Net{}, auto Fn = nullptr, auto Table = nullptr> struct From {};

// From<> for prediction, and the common case rather than the exception: prediction is declared on
// base classes, so 8 of CBaseViewModel's 11 entries are inherited. The base's annotation cannot
// just be inherited -- flags differ per class (m_flCycle is INSENDTABLE|NOERRORCHECK in
// C_BaseAnimating, PRIVATE|OVERRIDE|NOERRORCHECK in CBaseViewModel). No position: prediction map
// order is not observable, see reflect_predmap_verify.cpp.
template <name_t Name, Pred P = Pred{}> struct PredFrom {};

// From<> for the datamap. A keyfield or output reached through a *dotted path* --
// DEFINE_KEYFIELD( m_fog.colorPrimary, ... ) -- cannot annotate its leaf member: the leaf lives in
// a shared struct (fogparams_t is embedded by CFogController, CSkyCamera and CGameText at three
// different paths), and the entry's offset is relative to the embedding class. So the embedding
// class names the path. Name is the path exactly as the macro stringifies it, because that is the
// entry's fieldName.
template <name_t Name, Key K = Key{}> struct KeyFrom {};

// Class-level. The table name is not derivable: aliased classes send under a name unrelated
// to the identifier. `base` is the BEGIN_SEND_TABLE / _NOBASE distinction.
struct NetTable { name_t name{}; bool base = true; };

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
