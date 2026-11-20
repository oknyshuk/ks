// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.core.minidump;

import ks.core.platform;

export extern "C++" {

// writes out a minidump of the current stack trace with a unique filename
void WriteMiniDump( const char *pszFilenameSuffix = nullptr );

}
