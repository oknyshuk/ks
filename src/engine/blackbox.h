#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "engine/iblackbox.h"

extern IBlackBox *gBlackBox;

// helper function
void BlackBox_Record(const char *type, const char *pFormat, ...);
