#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

void *Mem_Malloc( size_t size );
void *Mem_ZeroMalloc( size_t size );
void *Mem_Calloc( int num, size_t size );
void *Mem_Realloc( void *memblock, size_t size );
char *Mem_Strdup( const char *strSource );
void Mem_Free( void *p );
