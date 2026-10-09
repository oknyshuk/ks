#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class ISceneTokenProcessor
{
public:
	virtual const char	*CurrentToken( void ) = 0;
	virtual bool		GetToken( bool crossline ) = 0;
	virtual bool		TokenAvailable( void ) = 0;
	virtual void		Error( const char *fmt, ... ) = 0;
};
