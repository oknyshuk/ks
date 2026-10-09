// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;

#include <string.h>


#include <SDL3/SDL.h>

module ks.desktop;

namespace
{

constexpr const char *game_window_property = "ks.game_window";

SDL_Window *native( ks::desktop::window *w ) noexcept
{
	return reinterpret_cast<SDL_Window *>( w );
}

SDL_Cursor *native( ks::desktop::cursor *c ) noexcept
{
	return reinterpret_cast<SDL_Cursor *>( c );
}

bool convert( const SDL_DisplayMode *source, ks::desktop::display_mode *out ) noexcept
{
	if ( source == nullptr || out == nullptr )
		return false;
	out->width = source->w;
	out->height = source->h;
	out->refresh_rate = static_cast<int>( source->refresh_rate );
	return true;
}

SDL_SystemCursor system_cursor( ks::desktop::cursor_kind kind ) noexcept
{
	switch ( kind )
	{
	case ks::desktop::cursor_kind::arrow: return SDL_SYSTEM_CURSOR_DEFAULT;
	case ks::desktop::cursor_kind::text: return SDL_SYSTEM_CURSOR_TEXT;
	case ks::desktop::cursor_kind::wait: return SDL_SYSTEM_CURSOR_WAIT;
	case ks::desktop::cursor_kind::crosshair: return SDL_SYSTEM_CURSOR_CROSSHAIR;
	case ks::desktop::cursor_kind::progress: return SDL_SYSTEM_CURSOR_PROGRESS;
	case ks::desktop::cursor_kind::diagonal_resize: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
	case ks::desktop::cursor_kind::move: return SDL_SYSTEM_CURSOR_MOVE;
	case ks::desktop::cursor_kind::not_allowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
	case ks::desktop::cursor_kind::pointer: return SDL_SYSTEM_CURSOR_POINTER;
	}
	return SDL_SYSTEM_CURSOR_DEFAULT;
}

} // namespace

void ks::desktop::set_game_window( window *w ) noexcept
{
	SDL_SetPointerProperty( SDL_GetGlobalProperties(), game_window_property, w );
}

ks::desktop::window *ks::desktop::game_window() noexcept
{
	return static_cast<window *>( SDL_GetPointerProperty( SDL_GetGlobalProperties(), game_window_property, nullptr ) );
}

ks::desktop::window *ks::desktop::focused_window() noexcept
{
	return reinterpret_cast<window *>( SDL_GetKeyboardFocus() );
}

ks::desktop::size ks::desktop::window_pixel_size( window *w ) noexcept
{
	size result{};
	SDL_GetWindowSizeInPixels( native( w ), &result.width, &result.height );
	return result;
}

ks::desktop::size ks::desktop::window_size( window *w ) noexcept
{
	size result{};
	SDL_GetWindowSize( native( w ), &result.width, &result.height );
	return result;
}

void ks::desktop::resize_window( window *w, int width, int height ) noexcept
{
	// Applied synchronously, so the resulting RESIZED event is pumped while the caller still knows it was self-inflicted.
	SDL_SetWindowSize( native( w ), width, height );
	SDL_SyncWindow( native( w ) );
}

void ks::desktop::move_window( window *w, int x, int y ) noexcept
{
	SDL_SetWindowPosition( native( w ), x, y );
}

void ks::desktop::raise_window( window *w ) noexcept
{
	SDL_RaiseWindow( native( w ) );
}

void ks::desktop::post_expose( window *w ) noexcept
{
	SDL_Event expose;
	memset( &expose, 0, sizeof( expose ) );
	expose.type = SDL_EVENT_WINDOW_EXPOSED;
	expose.window.windowID = SDL_GetWindowID( native( w ) );
	SDL_PushEvent( &expose );
}

bool ks::desktop::video_ready() noexcept
{
	return SDL_WasInit( SDL_INIT_VIDEO ) != 0;
}

ks::desktop::display_id ks::desktop::primary_display() noexcept
{
	return SDL_GetPrimaryDisplay();
}

ks::desktop::display_id ks::desktop::display_of( window *w ) noexcept
{
	display_id id = w != nullptr ? SDL_GetDisplayForWindow( native( w ) ) : 0;
	return id != 0 ? id : SDL_GetPrimaryDisplay();
}

int ks::desktop::displays( display_id *out, int capacity ) noexcept
{
	int count = 0;
	SDL_DisplayID *ids = SDL_GetDisplays( &count );
	if ( ids == nullptr )
		return 0;
	if ( count > capacity )
		count = capacity;
	for ( int i = 0; i < count; i++ )
		out[i] = ids[i];
	SDL_free( ids );
	return count;
}

bool ks::desktop::desktop_mode( display_id id, display_mode *mode ) noexcept
{
	return convert( SDL_GetDesktopDisplayMode( id ), mode );
}

bool ks::desktop::current_mode( display_id id, display_mode *mode ) noexcept
{
	return convert( SDL_GetCurrentDisplayMode( id ), mode );
}

bool ks::desktop::display_bounds( display_id id, rect *bounds ) noexcept
{
	SDL_Rect source{};
	if ( bounds == nullptr || !SDL_GetDisplayBounds( id, &source ) )
		return false;
	*bounds = rect{ source.x, source.y, source.w, source.h };
	return true;
}

int ks::desktop::fullscreen_modes( display_id id, display_mode *out, int capacity ) noexcept
{
	int count = 0;
	SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes( id, &count );
	if ( modes == nullptr )
		return 0;
	if ( count > capacity )
		count = capacity;
	for ( int i = 0; i < count; i++ )
		convert( modes[i], &out[i] );
	SDL_free( modes );
	return count;
}

ks::desktop::cursor *ks::desktop::create_cursor( cursor_kind kind ) noexcept
{
	return reinterpret_cast<cursor *>( SDL_CreateSystemCursor( system_cursor( kind ) ) );
}

void ks::desktop::destroy_cursor( cursor *c ) noexcept
{
	if ( c != nullptr )
		SDL_DestroyCursor( native( c ) );
}

void ks::desktop::use_cursor( cursor *c ) noexcept
{
	SDL_SetCursor( native( c ) );
}

void ks::desktop::show_cursor( bool visible ) noexcept
{
	if ( visible )
		SDL_ShowCursor();
	else
		SDL_HideCursor();
}

bool ks::desktop::cursor_visible() noexcept
{
	return SDL_CursorVisible();
}

bool ks::desktop::set_clipboard( const char *text ) noexcept
{
	return text != nullptr && SDL_SetClipboardText( text );
}

unsigned long ks::desktop::clipboard( char *buffer, unsigned long capacity ) noexcept
{
	if ( buffer == nullptr || capacity == 0 )
		return 0;
	buffer[0] = 0;
	char *text = SDL_GetClipboardText();
	if ( text == nullptr )
		return 0;
	unsigned long length = strlen( text );
	if ( length >= capacity )
		length = capacity - 1;
	memcpy( buffer, text, length );
	buffer[length] = 0;
	SDL_free( text );
	return length;
}

int ks::desktop::message_box( window *parent, const char *title, const char *text, const char *const *labels, int count, int default_index, int escape_index ) noexcept
{
	if ( labels == nullptr || count <= 0 || count > 8 )
		return -1;

	SDL_MessageBoxButtonData buttons[8];
	for ( int i = 0; i < count; i++ )
	{
		buttons[i].flags = 0;
		if ( i == default_index )
			buttons[i].flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		if ( i == escape_index )
			buttons[i].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		buttons[i].buttonID = i;
		buttons[i].text = labels[i];
	}

	SDL_MessageBoxData box{};
	box.window = native( parent );
	box.title = title;
	box.message = text;
	box.numbuttons = count;
	box.buttons = buttons;

	int chosen = -1;
	if ( !SDL_ShowMessageBox( &box, &chosen ) )
		return -1;
	return chosen;
}
