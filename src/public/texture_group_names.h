#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// These are given to FindMaterial to reference the texture groups that show up on the 
inline constexpr char TEXTURE_GROUP_LIGHTMAP[] = "Lightmaps";
inline constexpr char TEXTURE_GROUP_WORLD[] = "World textures";
inline constexpr char TEXTURE_GROUP_MODEL[] = "Model textures";
inline constexpr char TEXTURE_GROUP_STATIC_PROP[] = "StaticProp textures";
inline constexpr char TEXTURE_GROUP_COMBINED[] = "Combined Textures";
inline constexpr char TEXTURE_GROUP_COMPOSITE[] = "Composited Textures";
inline constexpr char TEXTURE_GROUP_VGUI[] = "VGUI textures";
inline constexpr char TEXTURE_GROUP_PARTICLE[] = "Particle textures";
inline constexpr char TEXTURE_GROUP_DECAL[] = "Decal textures";
inline constexpr char TEXTURE_GROUP_SKYBOX[] = "SkyBox textures";
inline constexpr char TEXTURE_GROUP_CLIENT_EFFECTS[] = "ClientEffect textures";
inline constexpr char TEXTURE_GROUP_OTHER[] = "Other textures";
inline constexpr char TEXTURE_GROUP_PRECACHED[] = "Precached";	// TODO: assign texture groups to the precached materials
inline constexpr char TEXTURE_GROUP_CUBE_MAP[] = "CubeMap textures";
inline constexpr char TEXTURE_GROUP_RENDER_TARGET[] = "RenderTargets";
inline constexpr char TEXTURE_GROUP_UNACCOUNTED[] = "Unaccounted textures";	// Textures that weren't assigned a texture group.
//#define TEXTURE_GROUP_STATIC_VERTEX_BUFFER		"Static Vertex"
inline constexpr char TEXTURE_GROUP_STATIC_INDEX_BUFFER[] = "Static Indices";
inline constexpr char TEXTURE_GROUP_STATIC_VERTEX_BUFFER_DISP[] = "Displacement Verts";
inline constexpr char TEXTURE_GROUP_STATIC_VERTEX_BUFFER_COLOR[] = "Lighting Verts";
inline constexpr char TEXTURE_GROUP_STATIC_VERTEX_BUFFER_WORLD[] = "World Verts";
inline constexpr char TEXTURE_GROUP_STATIC_VERTEX_BUFFER_MODELS[] = "Model Verts";
inline constexpr char TEXTURE_GROUP_STATIC_VERTEX_BUFFER_OTHER[] = "Other Verts";
inline constexpr char TEXTURE_GROUP_DYNAMIC_INDEX_BUFFER[] = "Dynamic Indices";
inline constexpr char TEXTURE_GROUP_DYNAMIC_VERTEX_BUFFER[] = "Dynamic Verts";
inline constexpr char TEXTURE_GROUP_DEPTH_BUFFER[] = "DepthBuffer";
inline constexpr char TEXTURE_GROUP_VIEW_MODEL[] = "ViewModel";
inline constexpr char TEXTURE_GROUP_PIXEL_SHADERS[] = "Pixel Shaders";
inline constexpr char TEXTURE_GROUP_VERTEX_SHADERS[] = "Vertex Shaders";
inline constexpr char TEXTURE_GROUP_RENDER_TARGET_SURFACE[] = "RenderTarget Surfaces";
inline constexpr char TEXTURE_GROUP_MORPH_TARGETS[] = "Morph Targets";
