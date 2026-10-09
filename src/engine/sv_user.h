#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Send a command to the specified client (as though the client typed the command in their console).
class client_t;
void SV_FinishParseStringCommand( client_t *cl, const char *pCommand );
