// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

export module ks.game.in_buttons;

export
{
inline constexpr auto IN_ATTACK = (1 << 0);
inline constexpr auto IN_JUMP = (1 << 1);
inline constexpr auto IN_DUCK = (1 << 2);
inline constexpr auto IN_FORWARD = (1 << 3);
inline constexpr auto IN_BACK = (1 << 4);
inline constexpr auto IN_USE = (1 << 5);
inline constexpr auto IN_CANCEL = (1 << 6);
inline constexpr auto IN_LEFT = (1 << 7);
inline constexpr auto IN_RIGHT = (1 << 8);
inline constexpr auto IN_MOVELEFT = (1 << 9);
inline constexpr auto IN_MOVERIGHT = (1 << 10);
inline constexpr auto IN_ATTACK2 = (1 << 11);
inline constexpr auto IN_RUN = (1 << 12);
inline constexpr auto IN_RELOAD = (1 << 13);
inline constexpr auto IN_ALT1 = (1 << 14);
inline constexpr auto IN_ALT2 = (1 << 15);
inline constexpr auto IN_SCORE = (1 << 16); // Used by client.dll for when scoreboard is held down
inline constexpr auto IN_SPEED = (1 << 17); // Player is holding the speed key
inline constexpr auto IN_WALK = (1 << 18); // Player holding walk key
inline constexpr auto IN_ZOOM = (1 << 19); // Zoom key for HUD zoom
inline constexpr auto IN_WEAPON1 = (1 << 20); // weapon defines these bits
inline constexpr auto IN_WEAPON2 = (1 << 21); // weapon defines these bits
inline constexpr auto IN_BULLRUSH = (1 << 22);
inline constexpr auto IN_GRENADE1 = (1 << 23); // grenade 1
inline constexpr auto IN_GRENADE2 = (1 << 24); // grenade 2
inline constexpr auto IN_LOOKSPIN = (1 << 25);
}
