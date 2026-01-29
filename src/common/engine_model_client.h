//========= Copyright � 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Sharing model structures and flags between engine and client
//
//=============================================================================//

#ifndef ENGINE_MODEL_CLIENT_H
#define ENGINE_MODEL_CLIENT_H

#ifdef _WIN32
#pragma once
#endif

#define ENGINE_MODEL_CLIENT_MODELFLAG_RENDER_DISABLED		0x0080	// excluded for compliance with government regulations

#define ENGINE_MODEL_CLIENT_MODELT_OFFSET_FLAGS			280

inline int EngineModelClientFlags( const void *pModel ) { return *reinterpret_cast< const int * >( ((const char*)pModel) + ENGINE_MODEL_CLIENT_MODELT_OFFSET_FLAGS ); }

#endif // ENGINE_MODEL_CLIENT_H
