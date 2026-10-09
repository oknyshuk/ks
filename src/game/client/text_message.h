#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IHudTextMessage
{
public:
	virtual char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size ) = 0;
	virtual const char * const BufferedLocaliseTextString( const char *msg ) = 0;
	virtual const char *LookupString( const char *msg_name, int *msg_dest = nullptr ) = 0;
};

extern IHudTextMessage *hudtextmessage;
