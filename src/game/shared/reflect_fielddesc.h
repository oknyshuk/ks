// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// One typedescription_t builder, shared by the datamap emitter (server) and the prediction
// emitter (client). Nothing here reaches for entityoutput.h or predictable_entity.h, so both
// sides can include it.

#ifndef KS_REFLECT_FIELDDESC_H
#define KS_REFLECT_FIELDDESC_H

#include "datamap.h"

namespace ks::reflect
{

struct FieldDesc
{
	fieldtype_t type = FIELD_VOID;
	const char *name = nullptr;
	const char *external = nullptr;
	int         offset = 0;
	int         bytes  = 0;
	int         count  = 1;
	short       flags  = 0;
	float       tolerance = 0.f;
	ISaveRestoreOps *ops = nullptr;
	inputfunc_t input = nullptr;
	// The nested map an embedded entry points at: DEFINE_EMBEDDED, DEFINE_PRED_TYPEDESCRIPTION.
	datamap_t  *td = nullptr;
};

// The macros are aggregate initialisers that stop early and leave the trailing members to
// static zero-initialisation. These land in a heap vector, so every one has to be set.
inline typedescription_t MakeField( const FieldDesc &d )
{
	typedescription_t t = {};
	t.fieldType        = d.type;
	t.fieldName        = d.name;
	t.fieldOffset      = d.offset;
	t.fieldSize        = static_cast<unsigned short>( d.count );
	t.flags            = d.flags;
	t.externalName     = d.external;
	t.pSaveRestoreOps  = d.ops;
	t.inputFunc        = d.input;
	t.td               = d.td;
	t.fieldSizeInBytes = d.bytes;
	t.override_field   = NULL;
	t.override_count   = 0;
	t.fieldTolerance   = d.tolerance;
	return t;
}

} // namespace ks::reflect

#endif // KS_REFLECT_FIELDDESC_H
