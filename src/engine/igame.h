#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IGame
{
public:

	bool	Init( void *pvInstance );
	bool	Shutdown( void );

	bool	CreateGameWindow( void );
	void	DestroyGameWindow( void );

	// This is used in edit mode to specify a particular game window (created by hammer)
	void	SetGameWindow( void* hWnd );

	// This is used in edit mode to override the default wnd proc associated w/
	// the game window specified in SetGameWindow. 
	bool	InputAttachToGameWindow();
	void	InputDetachFromGameWindow();

	void	PlayStartupVideos( void );

	void*	GetMainWindow( void );
	void**	GetMainWindowAddress( void );
	void	GetDesktopInfo( int &width, int &height, int &refreshrate );

	void	SetWindowXY( int x, int y );
	void	SetWindowSize( int w, int h );

	void	GetWindowRect( int *x, int *y, int *w, int *h );

	// Not Alt-Tabbed away
	bool	IsActiveApp( void );

	void	DispatchAllStoredGameMessages();

	void    OnScreenSizeChanged( int nOldWidth, int nOldHeight );
};

extern IGame *game;
