#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

// Purpose: Is replay enabled?

bool Replay_IsEnabled();

//----------------------------------------------------------------------------------------
// Called from CNetChan when a file has been completely sent down to the client.
//----------------------------------------------------------------------------------------
void Replay_OnFileSendComplete( const char *pFilename, int nSize );
