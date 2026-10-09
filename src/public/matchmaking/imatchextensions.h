#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

class IMatchExtensions
{
public:
	// Registers an extension interface
	virtual void RegisterExtensionInterface( char const *szInterfaceString, void *pvInterface ) = 0;

	// Unregisters an extension interface
	virtual void UnregisterExtensionInterface( char const *szInterfaceString, void *pvInterface ) = 0;

	// Gets a pointer to a registered extension interface
	virtual void * GetRegisteredExtensionInterface( char const *szInterfaceString ) = 0;
};
