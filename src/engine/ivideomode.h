#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "vmodes.h"
import std;
#include "vtf/vtf.h"

struct MovieInfo_t;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IVideoMode
{
public:	
	bool			Init( );
	void			Shutdown( void );

	// Shows the start-up graphics based on the mod 
	// (Filesystem path for the mod must be set up first)
	void			DrawStartupGraphic();

	// Creates the game window, plays the startup movie, starts up the material system
	bool			CreateGameWindow( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder );

	// Sets the game window in editor mode
	void			SetGameWindow( void *hWnd );

	// Sets the video mode, and re-sizes the window
	bool			SetMode( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder );

	// Returns the fullscreen modes for the adapter the game was started on
	int				GetModeCount( void );
	struct vmode_s	*GetMode( int num );

	// Purpose: This is called in response to a WM_MOVE message
	// or whatever the equivalent that would be under linux
	void			UpdateWindowPosition( void );

	// Alt-tab handling
	void			RestoreVideo( void );
	void			ReleaseVideo( void );

	void			DrawNullBackground( void *hdc, int w, int h );
	void			InvalidateWindow();

	// Returns the video mode width + height. In the case of windowed mode,
	// it returns the width and height of the drawable region of the window.
	// (it doesn't include the window borders)
	int				GetModeWidth() const;
	int				GetModeHeight() const;
	bool			IsWindowedMode() const;
	bool			NoWindowBorder() const;

	// Returns the subrect to draw the client view into.
	// Coordinates are measured relative to the drawable region of the window
	const vrect_t &	GetClientViewRect( ) const;
	void			SetClientViewRect( const vrect_t &viewRect );

	// Lazily recomputes client view rect
	void			MarkClientViewRectDirty();

	void			TakeSnapshotTGA( const char *pFileName );
	void			TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM = false, CubeMapFaceIndex_t faceIndex = CUBEMAP_FACE_RIGHT );
	void			WriteMovieFrame( const MovieInfo_t& info );

	// Takes snapshots
	void			TakeSnapshotJPEG( const char *pFileName, int quality );
	bool			TakeSnapshotJPEGToBuffer( ks::byte_writer& buf, int quality );

	// Synchronizes cached mode width/height to actual SDL window size.
	// Call this after window creation, display changes, or when resolution may have changed.
	// Returns true if the values changed.
	bool			SyncToActualWindowSize();

	// Called when the window size changes (e.g., when moving between displays on Wayland).
	// Syncs all systems atomically and notifies panels.
	void			OnWindowSizeChanged( int nNewWidth, int nNewHeight );
};


//-----------------------------------------------------------------------------
// Utilities for virtual screen coordinates
//-----------------------------------------------------------------------------
#define XRES(x)	( x  * ( ( float )videomode->GetModeWidth() / 640.0 ) )
#define YRES(y)	( y  * ( ( float )videomode->GetModeHeight() / 480.0 ) )


//-----------------------------------------------------------------------------
// Singleton accessor 
//-----------------------------------------------------------------------------
#if !defined( DEDICATED )
extern IVideoMode *videomode;
#endif


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
void VideoMode_Create();
void VideoMode_Destroy();
