//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#include "simtimer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CSimTimer, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CRandSimTimer, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CStopwatchBase, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CStopwatch, CStopwatchBase )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CRandStopwatch, CStopwatchBase )

//-----------------------------------------------------------------------------
