//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
	   
#ifndef CHEATCODES_H
#define CHEATCODES_H



#include "inputsystem/ButtonCode.h"


void	ClearCheatCommands( void );
void	ReadCheatCommandsFromFile( const char *pchFileName );
void	LogKeyPress( ButtonCode_t code );
void	CheckCheatCodes();


#endif // CHEATCODES_H
