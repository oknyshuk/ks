#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "c_baseentity.h"
#include "fx.h"
import std;

class [[= ks::reflect::NetTable{ .name = "DT_Tesla" } ]]
      C_Tesla : public C_BaseEntity
{
public:
	
	DECLARE_CLASS( C_Tesla, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_Tesla();

	virtual void ReceiveMessage( int classID, bf_read &msg );
	virtual void ClientThink();


public:

	std::vector<CTeslaInfo> m_QueuedCommands;
	[[= ks::reflect::Net{} ]] char m_SoundName[64];
	[[= ks::reflect::Net{} ]] char m_iszSpriteName[256];
};
