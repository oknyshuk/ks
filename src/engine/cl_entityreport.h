#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Bits needed for action
import std;
import ks.tier1;
class ClientClass;

void CL_RecordEntityBits( int entnum, int bitcount );

// Special events to record
void CL_RecordAddEntity( int entnum );
void CL_RecordLeavePVS( int entnum );
void CL_RecordDeleteEntity( int entnum, ClientClass *pclass );

extern ConVar cl_entityreport;
