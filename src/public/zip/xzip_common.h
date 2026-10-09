#pragma once
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// Handle types shared by XZip.h (zip creation) and XUnzip.h (zip extraction).
// Both headers can be included in the same translation unit.

typedef void * HANDLE;

struct HZIP__ { int unused; };
using HZIP = HZIP__ *;		// An HZIP identifies a zip file that is being created or opened
