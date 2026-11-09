#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

class IMatchExtensions
{
public:
	// Registers an extension interface
	void RegisterExtensionInterface( char const *szInterfaceString, void *pvInterface );

	// Unregisters an extension interface
	void UnregisterExtensionInterface( char const *szInterfaceString, void *pvInterface );

	// Gets a pointer to a registered extension interface
	void * GetRegisteredExtensionInterface( char const *szInterfaceString );
};
