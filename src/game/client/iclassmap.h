#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class C_BaseEntity;
typedef C_BaseEntity* (*DISPATCHFUNCTION)( void );

class IClassMap
{
public:
	virtual					~IClassMap() {}

	virtual void			Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory = 0 ) = 0;
	virtual char const		*Lookup( const char *classname ) = 0;
	virtual C_BaseEntity	*CreateEntity( const char *mapname ) = 0;
	virtual int				GetClassSize( const char *classname ) = 0;
};

extern IClassMap& GetClassMap();
