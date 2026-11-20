#pragma once
import ks.core.vprof;
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Allows matching of initialization and shutdown function calls

void TraceInit( const char *i, const char *s, int list );
void TraceShutdown( const char *s, int list );

#define TRACEINITNUM( initfunc, shutdownfunc, num )	\
	TraceInit( #initfunc, #shutdownfunc, num );		\
	initfunc;

#define TRACESHUTDOWNNUM( shutdownfunc, num )	\
	TraceShutdown( #shutdownfunc, num );		\
	shutdownfunc;

#define TRACEINIT( initfunc, shutdownfunc )	TRACEINITNUM( initfunc, shutdownfunc, 0 )
#define TRACESHUTDOWN( shutdownfunc ) TRACESHUTDOWNNUM( shutdownfunc, 0 )
