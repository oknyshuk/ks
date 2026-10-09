#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
class IToolSystem;


//-----------------------------------------------------------------------------
// Purpose: Every tool dll sitting in bin\tools must expose this interface
//  The engine will load the .dll, get this interface, and then ask for all
//  tools in the .dll
// The engine will call CreateTools just before querying for the tools, so you 
//  can instance any dynamically instanced tools during that call
//-----------------------------------------------------------------------------
class IToolDictionary : public IAppSystem
{
public:
	virtual void		CreateTools() = 0;
	virtual int			GetToolCount() const = 0;
	virtual IToolSystem	*GetTool( int index ) = 0;
};

inline constexpr const char *VTOOLDICTIONARY_INTERFACE_VERSION = "VTOOLDICTIONARY003";
