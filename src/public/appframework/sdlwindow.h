// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Shared access to the game's SDL window, without an interface.
//
// libSDL3.so is already linked by every module that cares about the window
// (launcher, engine, inputsystem, rocketui, shaderapidx9), so SDL's own global
// property store is a perfectly good place to publish the handle. That removes the
// only real reason ILauncherMgr had to exist: marshalling a window pointer across
// shared-library boundaries. Callers just use SDL directly on the result.

#ifndef APPFRAMEWORK_SDLWINDOW_H
#define APPFRAMEWORK_SDLWINDOW_H

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#define KS_PROP_GAME_WINDOW "ks.game_window"

// Not spelled Set/GetGameWindow(): CGame and IVideoMode have SetGameWindow( void * )
// members, and void * accepts SDL_Window * silently, so an unqualified call from
// inside those classes would bind to the member instead.
inline void SetGameSDLWindow( SDL_Window *pWindow )
{
	SDL_SetPointerProperty( SDL_GetGlobalProperties(), KS_PROP_GAME_WINDOW, pWindow );
}

// The game's main window, NULL before it is created and after it is destroyed.
inline SDL_Window *GetGameSDLWindow()
{
	return (SDL_Window *)SDL_GetPointerProperty( SDL_GetGlobalProperties(), KS_PROP_GAME_WINDOW, NULL );
}

#endif // APPFRAMEWORK_SDLWINDOW_H
