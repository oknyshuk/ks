#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

void Host_ResetGlobalConfiguration();
void Host_ResetConfiguration( const int iController );
void Host_ReadConfiguration( const int iController, const bool readDefault );
void Host_WriteConfiguration( const int iController, const char *filename );

void Host_SubscribeForProfileEvents( bool bSubscribe );
