#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// view.h

class CViewSetup;
class CViewSetupV1;


void V_Init (void);
void V_Shutdown( void );
void ConvertViewSetup( const CViewSetup &setup, CViewSetupV1 &setupV1, int nFlags );
