#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core;

void Memory_Init (void);
void Memory_Shutdown( void );

void Hunk_OnMapStart( int nEstimatedBytes );

void *Hunk_AllocName (int size, const char *name, bool bClear = true );

int	Hunk_LowMark (void);
void Hunk_FreeToLowMark (int mark);

void Hunk_Check (void);

int Hunk_MallocSize();
int Hunk_Size();

void Hunk_Print();
