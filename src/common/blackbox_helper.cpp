// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#include "engine/iblackbox.h"

extern IBlackBox *blackboxrecorder;

void BlackBox_Record( const char *type, const char *pFormat, ... )
{
	static ConVarRef blackbox( "blackbox" );

	if ( !blackbox.IsValid() || !blackbox.GetBool() )
		 return;

	int type_num;
	for ( type_num = 0; type_num < blackboxrecorder->GetTypeCount(); type_num++ )
	{
		if ( !V_strcasecmp( blackboxrecorder->GetTypeName( type_num ), type ) )
			break;
	}

	if ( type_num >= blackboxrecorder->GetTypeCount() )
	{
		Msg( "Invalid blackbox type: %s\n", type );
		return;
	}

	char szMessage[1024];	
	va_list marker;

	__builtin_va_start( marker, pFormat);
	V_vsnprintf( szMessage, sizeof( szMessage ), pFormat, marker);
	__builtin_va_end( marker );	

	//Msg( "Record: %s: %s\n", type, szMessage );
	blackboxrecorder->Record( type_num, szMessage );
}
