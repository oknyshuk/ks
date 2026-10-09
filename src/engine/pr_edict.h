#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Expose things from pr_edict.cpp.

#include "edict.h"

void InitializeEntityDLLFields( edict_t *pEdict );

// If iForceEdictIndex is not -1, then it will return the edict with that index. If that edict index
// is already used, it'll return null.
edict_t *ED_Alloc( int iForceEdictIndex = -1 );
void	ED_Free( edict_t *ed );

edict_t *EDICT_NUM(int n);
int NUM_FOR_EDICT(const edict_t *e);

void ED_AllowImmediateReuse();
void ED_ClearTimes();
