#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Defines the entry point for the application.

class IResListGenerator
{
public:

	virtual void Init( char const *pchBaseDir, char const *pchGameDir ) = 0;
	virtual void Shutdown() = 0;
	virtual bool IsActive() = 0;
	// Returns true if processing should continue, otherwise false
	virtual bool TickAndFixupCommandLine() = 0;
	virtual bool ShouldContinue() = 0;
};

extern IResListGenerator *reslistgenerator;
