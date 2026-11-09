#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core;

class ISPSharedMemory
{
public:
	bool	Init( std::size_t iSize ); //Initial implementation assumes the size is fixed/hardcoded, returns true if this call actually created the memory, false if it already existed
	uint8 *	Base( void );
	std::size_t	Size( void );
	
	void	AddRef( void );
	void	Release( void );
};
