// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Marks a wire wrapper: a type that stores the value crossing the wire in a single nested data
// member, so the table emitters can see through it and derive a prop kind from the payload.
//
// Deliberately separate from reflect_annotations.h. networkvar.h has to reach this, that header is
// included tree-wide, and the rest of the annotation vocabulary depends on datamap.h and
// dt_common.h -- neither of which is free there.
//
// The payload is found structurally, not by name: an annotated type must declare exactly one
// non-static data member, and unwrap() rejects it at compile time otherwise. That is what keeps
// the wrapper's internals private to the wrapper. reflect.h used to match the member *names*
// "m_Value" and "m_Val", which meant renaming a wrapper's payload -- or replacing the wrapper
// family outright -- silently changed every prop kind derived from it.

#ifndef KS_REFLECT_WIREVAR_H
#define KS_REFLECT_WIREVAR_H

namespace ks::reflect
{

struct WireVar {};

} // namespace ks::reflect

#endif // KS_REFLECT_WIREVAR_H
