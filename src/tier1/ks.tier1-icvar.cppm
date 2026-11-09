// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:icvar;

import std;
import ks.math;
export import :iappsystem;
export import :iconvar;
export import :utlvector;

export extern "C++" {

class ConCommandBase;
class ConCommand;
class ConVar;

//-----------------------------------------------------------------------------
// ConVars/ComCommands are marked as having a particular DLL identifier
//-----------------------------------------------------------------------------
typedef int CVarDLLIdentifier_t;

//-----------------------------------------------------------------------------
// Used to display console messages
//-----------------------------------------------------------------------------
enum class ConsoleDisplayOp_t
{
	ColorPrint,
	Print,
	DPrint,
};

// Receives console output; pMessage is the text, clr is only meaningful for ColorPrint.
typedef std::function_ref< void( ConsoleDisplayOp_t op, const Color &clr, const char *pMessage ) > ConsoleDisplayFn_t;

//-----------------------------------------------------------------------------
// Purpose: Decides whether two same-named ConVars from different modules may be
// aliased to one another. The engine installs its rules via ICvar::InstallCVarQuery.
//-----------------------------------------------------------------------------
typedef std::function_ref< bool( const ConVar *child, const ConVar *parent ) > FnConVarsLinkable_t;

class ICvar;

//-----------------------------------------------------------------------------
// These global names are defined by tier1.h, duplicated here so you
// don't have to include tier1.h
//-----------------------------------------------------------------------------

// These are marked DLL_EXPORT for Linux.
extern ICvar *cvar;
extern ICvar *g_pCVar;

}
