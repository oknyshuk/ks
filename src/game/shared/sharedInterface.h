//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Exposes client-server neutral interfaces implemented in both places
//
// $NoKeywords: $
//=============================================================================//

#ifndef SHAREDINTERFACE_H
#define SHAREDINTERFACE_H

#define random random_valve// stdlib.h defined random() and our class defn conflicts so under POSIX rename it using the preprocessor

class IFileSystem;
class IUniformRandomStream;
class CGaussianRandomStream;
class IEngineSound;
class IMapData;
class IGameTypes;

extern IFileSystem				*filesystem;
extern IUniformRandomStream		*random;
extern CGaussianRandomStream *randomgaussian;
extern IEngineSound				*enginesound;
extern IMapData					*g_pMapData;			// TODO: current implementations of the 
														// interface are in TF2, should probably move
														// to TF2/HL2 neutral territory
extern IGameTypes				*g_pGameTypes;


#endif // SHAREDINTERFACE_H

