//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#ifndef MODELINFO_H
#define MODELINFO_H


#include "engine/ivmodelinfo.h"


extern IVModelInfo *modelinfo;			// server version
#ifndef DEDICATED
extern IVModelInfoClient *modelinfoclient;	// client version
#endif


#endif // MODELINFO_H
