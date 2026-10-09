#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCenterPrint
{
public:
						CCenterPrint( void );

	virtual void		Destroy( void );
	
	virtual void		SetTextColor( int r, int g, int b, int a );
	virtual void		Print( const char *text );
	virtual void		Print( const wchar_t *text );
	virtual void		ColorPrint( int r, int g, int b, int a, char *text );
	virtual void		ColorPrint( int r, int g, int b, int a, wchar_t *text );
	virtual void		Clear( void );
};

extern CCenterPrint *GetCenterPrint();
