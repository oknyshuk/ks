// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation
// Copyright (c) 2026 oknyshuk

// Purpose: Win32 Console API helpers

export module ks.core:console;

import :platform;

export extern "C++" {

// Function to attach a console for I/O to a Win32 GUI application in a reasonably smart fashion.
bool SetupWin32ConsoleIO();

// Win32 Console Color API Helpers, originally from cmdlib.

struct Win32ConsoleColorContext_t
{
	int  m_InitialColor;
	uint16 m_LastColor;
	uint16 m_BadColor;
	uint16 m_BackgroundFlags;
};

void InitWin32ConsoleColorContext( Win32ConsoleColorContext_t *pContext );

uint16 SetWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, int nRed, int nGreen, int nBlue, int nIntensity );

void RestoreWin32ConsoleColor( Win32ConsoleColorContext_t *pContext, uint16 prevColor );

}
