//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENV_ZOOM_H
#define ENV_ZOOM_H

#include "reflect_annotations.h"

bool CanOverrideEnvZoomOwner( CBaseEntity *pZoomOwner );
float GetZoomOwnerDesiredFOV( CBaseEntity *pZoomOwner );

#endif //ENV_ZOOM_H
