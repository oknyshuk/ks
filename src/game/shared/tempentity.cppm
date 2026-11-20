// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

export module ks.game.tempentity;

export
{
inline constexpr auto TE_EXPLFLAG_NONE = 0x0; // all flags clear makes default Half-Life explosion
inline constexpr auto TE_EXPLFLAG_NOADDITIVE = 0x1; // sprite will be drawn opaque (ensure that the sprite you send is a non-additive sprite)
inline constexpr auto TE_EXPLFLAG_DLIGHT = 0x2; // explosion has a DLIGHT
inline constexpr auto TE_EXPLFLAG_NOSOUND = 0x4; // do not play client explosion sound
inline constexpr auto TE_EXPLFLAG_NOPARTICLES = 0x8; // do not draw particles
inline constexpr auto TE_EXPLFLAG_DRAWALPHA = 0x10; // sprite will be drawn alpha
inline constexpr auto TE_EXPLFLAG_ROTATE = 0x20; // rotate the sprite randomly
inline constexpr auto TE_EXPLFLAG_NOFIREBALL = 0x40; // do not draw a fireball
inline constexpr auto TE_EXPLFLAG_NOFIREBALLSMOKE = 0x80; // do not draw smoke with the fireball
inline constexpr auto TE_EXPLFLAG_ICE = 0x100; // do ice effects
inline constexpr auto TE_EXPLFLAG_SCALEPARTICLES = 0x200;

inline constexpr auto TE_BEAMPOINTS = 0; // beam effect between two points
inline constexpr auto TE_SPRITE = 1; // additive sprite, plays 1 cycle
inline constexpr auto TE_BEAMDISK = 2; // disk that expands to max radius over lifetime
inline constexpr auto TE_BEAMCYLINDER = 3; // cylinder that expands to max radius over lifetime
inline constexpr auto TE_BEAMFOLLOW = 4; // create a line of decaying beam segments until entity stops moving
inline constexpr auto TE_BEAMRING = 5; // connect a beam ring to two entities
inline constexpr auto TE_BEAMSPLINE = 6;
inline constexpr auto TE_BEAMRINGPOINT = 7;
inline constexpr auto TE_BEAMLASER = 8; // Fades according to viewpoint
inline constexpr auto TE_BEAMTESLA = 9;
}
