// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once
import ks.tier1.convar;
import ks.tier1.iconvar;

#define HOOK_MESSAGE(x) \
	m_UMCMsg##x.Bind< ks::net::CS_UM_##x, ks::net::CCSUsrMsg_##x >( __MsgFunc_##x )
#define HOOK_HUD_MESSAGE(y, x) \
	m_UMCMsg##x.Bind< ks::net::CS_UM_##x, ks::net::CCSUsrMsg_##x >( __MsgFunc_##y##_##x )
#define DECLARE_HUD_MESSAGE(y, x) bool __MsgFunc_##y##_##x(const ks::net::CCSUsrMsg_##x &msg) \
	{																\
		CHudElement *pElement = GetHud().FindElement( #y );			\
		if ( pElement )												\
		{															\
			return ((y *)pElement)->MsgFunc_##x( msg );				\
		}															\
		return true;												\
	}
#define HOOK_COMMAND(x, y) static ConCommand x( #x, __CmdFunc_##y, "", FCVAR_SERVER_CAN_EXECUTE );
#define DECLARE_HUD_COMMAND_NAME(y, x, name) void __CmdFunc_##x( void )									\
	{																\
		CHudElement *pElement = GetHud().FindElement( name );			\
		if ( pElement )												\
		{															\
			((y *)pElement)->UserCmd_##x( );						\
		}															\
	}
