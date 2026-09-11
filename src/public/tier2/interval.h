//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef INTERVAL_H
#define INTERVAL_H

#include "basetypes.h"


interval_t ReadInterval( const char *pString );
float RandomInterval( const interval_t &interval );

#endif // INTERVAL_H
