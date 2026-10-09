#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Some little utility drawing methods

class IMaterial;
IMaterial	*GL_LoadMaterial( const char *pName, const char *pTextureGroupName, bool bPrecache = false );
void		GL_UnloadMaterial( IMaterial *pMaterial );
