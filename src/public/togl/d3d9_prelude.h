#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// DXVK's windows_base.h redefines these Source macros.
#undef TRUE
#undef FALSE
#undef WAIT_OBJECT_0
#undef WAIT_ABANDONED
#undef WAIT_TIMEOUT
#undef WAIT_FAILED

#pragma push_macro("ARRAYSIZE")
#undef ARRAYSIZE

// DXVK's headers expect the C library in the global namespace.
import std.compat;

#include <d3d9.h>
import ks.platform;
import ks.simd;

#pragma pop_macro("ARRAYSIZE")
