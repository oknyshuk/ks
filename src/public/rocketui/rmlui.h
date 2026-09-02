// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Include RmlUi through this header, never <RmlUi/...> directly.
//
// tier0/dbg.h defines Assert() as a function-like macro, which mangles the
// declaration of Rml::Assert in <RmlUi/Core/Debug.h> -- a header reached from
// essentially every RmlUi public header (Vector2.h, Matrix4.h, Traits.h, ...).
// Suppressing the macro for the duration of the include keeps the vendored
// RmlUi tree pristine, which is the point: see src/thirdparty/RmlUi-*/UPSTREAM.
//
// The macro is restored afterwards, so Source's Assert() still works in the
// rest of the translation unit. RmlUi's own RMLUI_ASSERT expands inside this
// window (and to nothing at all unless RMLUI_DEBUG is set), so it needs no
// further care.

#ifndef ROCKETUI_RMLUI_H
#define ROCKETUI_RMLUI_H

#pragma push_macro("Assert")
#undef Assert

// clang-format off
#include <RmlUi/Core.h>       // the umbrella header: covers every Core/ and
                              // Core/Elements/ header this project uses
#include <RmlUi/Debugger.h>
// clang-format on

#pragma pop_macro("Assert")

#endif // ROCKETUI_RMLUI_H
