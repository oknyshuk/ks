#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Real-Time Hierarchical Profiling

#include "igameserverdata.h"
import ks.core.vprof;

class IVProfExport;
class CVProfile;


void PreUpdateProfile( float filteredtime );
void PostUpdateProfile();
void UpdateVXConsoleProfile();

void RegisterVProfDataListener( ra_listener_id listenerID ); // remote data clients
void RemoveVProfDataListener( ra_listener_id listenerID ); // remote data clients
void WriteRemoteVProfData();

// Used to point the budget panel at remote data
void OverrideVProfExport( IVProfExport *pExport );
void ResetVProfExport( IVProfExport *pExport );

// Take a snapshot of vprof data so we can send it to the dedicated server.
void VProfExport_SnapshotVProfHistory();

// Called on shutdown.
void VProfRecord_Shutdown();

// Used by rpt
void VProfExport_Pause();
void VProfExport_Resume();

extern IVProfExport *g_pVProfExport; // used by engine's budget panel

// The budget panel and vprof panels use this for display.
extern CVProfile *g_pVProfileForDisplay;

extern unsigned g_VProfTargetThread;
