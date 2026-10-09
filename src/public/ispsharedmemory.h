#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core;

class ISPSharedMemory
{
public:
	virtual bool	Init( std::size_t iSize ) = 0; //Initial implementation assumes the size is fixed/hardcoded, returns true if this call actually created the memory, false if it already existed
	virtual uint8 *	Base( void ) = 0;
	virtual std::size_t	Size( void ) = 0;
	
	virtual void	AddRef( void ) = 0;
	virtual void	Release( void ) = 0;
};
