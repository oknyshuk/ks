// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This class allows the game dll to control what functions 3rd party plugins can
// call on clients.

#include "eiface.h"

//-----------------------------------------------------------------------------
// Purpose: An implementation 
//-----------------------------------------------------------------------------
class CPluginHelpersCheck : public IPluginHelpersCheck
{
public:
	virtual bool CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data );
};

CPluginHelpersCheck s_PluginCheck;
static InterfaceReg s_Reg_CPluginHelpersCheck_IPluginHelpersCheck( []() -> void * { return static_cast<IPluginHelpersCheck *>( &s_PluginCheck ); }, INTERFACEVERSION_PLUGINHELPERSCHECK );;

bool CPluginHelpersCheck::CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data )
{
	// return false here to disallow a plugin from running this command on this client
	return true;
}
