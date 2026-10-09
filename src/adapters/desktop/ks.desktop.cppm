// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.desktop;

export namespace ks::desktop
{

struct window;
struct cursor;
struct event;

using display_id = unsigned int;

struct size
{
	int width;
	int height;
};

struct rect
{
	int x;
	int y;
	int width;
	int height;
};

struct display_mode
{
	int width;
	int height;
	int refresh_rate;
};

// The game window is published process-wide, so every module resolves the same handle.
void set_game_window( window *w ) noexcept;
window *game_window() noexcept;
window *focused_window() noexcept;

size window_pixel_size( window *w ) noexcept;
size window_size( window *w ) noexcept;
void resize_window( window *w, int width, int height ) noexcept;
void move_window( window *w, int x, int y ) noexcept;
void raise_window( window *w ) noexcept;
void post_expose( window *w ) noexcept;

bool video_ready() noexcept;
display_id primary_display() noexcept;
display_id display_of( window *w ) noexcept;
int displays( display_id *out, int capacity ) noexcept;
bool desktop_mode( display_id id, display_mode *mode ) noexcept;
bool current_mode( display_id id, display_mode *mode ) noexcept;
bool display_bounds( display_id id, rect *bounds ) noexcept;
int fullscreen_modes( display_id id, display_mode *out, int capacity ) noexcept;

enum class cursor_kind
{
	arrow,
	text,
	wait,
	crosshair,
	progress,
	diagonal_resize,
	move,
	not_allowed,
	pointer,
};

inline constexpr int cursor_kind_count = 9;

cursor *create_cursor( cursor_kind kind ) noexcept;
void destroy_cursor( cursor *c ) noexcept;
void use_cursor( cursor *c ) noexcept;
void show_cursor( bool visible ) noexcept;
bool cursor_visible() noexcept;

bool set_clipboard( const char *text ) noexcept;
unsigned long clipboard( char *buffer, unsigned long capacity ) noexcept;

// Returns the index of the label chosen, or -1 when the dialog could not be shown.
int message_box( window *parent, const char *title, const char *text, const char *const *labels, int count, int default_index, int escape_index ) noexcept;

} // namespace ks::desktop
