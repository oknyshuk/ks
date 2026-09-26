//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_tesla.h"
#include "fx.h"
#include <bitbuf.h>

IMPLEMENT_REFLECT_CLIENTCLASS( C_Tesla, DT_Tesla, CTesla )


C_Tesla::C_Tesla()
{
}

void C_Tesla::ReceiveMessage( int classID, bf_read &msg )
{
	CTeslaInfo teslaInfo;

	msg.ReadBitVec3Coord( teslaInfo.m_vPos );
	teslaInfo.m_vAngles.Init();

	teslaInfo.m_nEntIndex = msg.ReadShort();
	teslaInfo.m_flRadius = msg.ReadFloat();

	teslaInfo.m_vColor.x = ((unsigned char)msg.ReadChar()) / 255.0f;
	teslaInfo.m_vColor.y = ((unsigned char)msg.ReadChar()) / 255.0f;
	teslaInfo.m_vColor.z = ((unsigned char)msg.ReadChar()) / 255.0f;
	
	float flAlpha = 0;
	flAlpha = ((unsigned char)msg.ReadChar()) / 255.0f;

	teslaInfo.m_nBeams = msg.ReadChar();
	
	teslaInfo.m_flBeamWidth = msg.ReadFloat();
	teslaInfo.m_flTimeVisible = msg.ReadFloat();
	teslaInfo.m_pszSpriteName = m_iszSpriteName;

	EmitSound( m_SoundName );

	m_QueuedCommands.push_back( teslaInfo );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}


void C_Tesla::ClientThink()
{
	for ( const CTeslaInfo &info : m_QueuedCommands )
	{
		FX_Tesla( info );
	}
	m_QueuedCommands.clear();
	SetNextClientThink( CLIENT_THINK_NEVER );
}


