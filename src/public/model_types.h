#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

inline constexpr int STUDIO_NONE = 0x00000000;
inline constexpr int STUDIO_RENDER = 0x00000001;
inline constexpr int STUDIO_VIEWXFORMATTACHMENTS = 0x00000002;
inline constexpr int STUDIO_DRAWTRANSLUCENTSUBMODELS = 0x00000004;
inline constexpr int STUDIO_TWOPASS = 0x00000008;
inline constexpr int STUDIO_STATIC_LIGHTING = 0x00000010;
inline constexpr int STUDIO_WIREFRAME = 0x00000020;
inline constexpr int STUDIO_ITEM_BLINK = 0x00000040;
inline constexpr int STUDIO_NOSHADOWS = 0x00000080;
inline constexpr int STUDIO_WIREFRAME_VCOLLIDE = 0x00000100;
inline constexpr int STUDIO_NOLIGHTING_OR_CUBEMAP = 0x00000200;
inline constexpr int STUDIO_SKIP_FLEXES = 0x00000400;
#define STUDIO_DONOTMODIFYSTENCILSTATE	0x00000800	// TERROR

// not a studio flag, but used to signify that the draw call should cause a depth imposter to be drawn as well.
inline constexpr int STUDIO_DEPTH_ONLY = 0x01000000;

// Not a studio flag, but used to flag model to render using special settings for AO pre-pass
inline constexpr int STUDIO_AOPREPASSTEXURE = 0x02000000;

// Not a studio flag, but used to flag model to keep the shadow state it starts with
inline constexpr int STUDIO_KEEP_SHADOWS = 0x04000000;

// Not a studio flag, but used to flag model as using shadow depth material override
inline constexpr int STUDIO_SSAODEPTHTEXTURE = 0x08000000;

// Not a studio flag, but used to flag model as a non-sorting brush model
inline constexpr int STUDIO_TRANSPARENCY = 0x80000000;

// Not a studio flag, but used to flag model as using shadow depth material override
inline constexpr int STUDIO_SHADOWDEPTHTEXTURE = 0x40000000;

// Not a studio flag, but used to flag model as doing custom rendering into shadow texture
inline constexpr int STUDIO_SHADOWTEXTURE = 0x20000000;

inline constexpr int STUDIO_SKIP_DECALS = 0x10000000;

enum modtype_t
{
	mod_bad = 0, 
	mod_brush, 
	mod_sprite, 
	mod_studio
};
