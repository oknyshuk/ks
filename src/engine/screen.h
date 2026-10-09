#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

void SCR_Init( void );
void SCR_Shutdown( void );

void SCR_UpdateScreen( void );
void SCR_BeginLoadingPlaque( const char *levelName = 0 );
void SCR_EndLoadingPlaque( void );
void SCR_FatalDiskError( void );

extern	bool	scr_disabled_for_loading;
extern  int		scr_nextdrawtick;
