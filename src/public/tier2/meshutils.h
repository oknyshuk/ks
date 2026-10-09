#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// Purpose: A set of utilities to help with generating meshes

//-----------------------------------------------------------------------------
// Helper methods to create various standard index buffer types
//-----------------------------------------------------------------------------
void GenerateSequentialIndexBuffer( unsigned short* pIndexMemory, int nIndexCount, int nFirstVertex );
void GenerateQuadIndexBuffer( unsigned short* pIndexMemory, int nIndexCount, int nFirstVertex );
void GeneratePolygonIndexBuffer( unsigned short* pIndexMemory, int nIndexCount, int nFirstVertex );
void GenerateLineStripIndexBuffer( unsigned short* pIndexMemory, int nIndexCount, int nFirstVertex );
void GenerateLineLoopIndexBuffer( unsigned short* pIndexMemory, int nIndexCount, int nFirstVertex );
