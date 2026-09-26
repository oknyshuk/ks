//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "stdlib.h"
#include "vstdlib/random.h"

#define __cdecl 

void srand(unsigned int)
{
}

int rand()
{
	return RandomInt( 0, VALVE_RAND_MAX );
}

