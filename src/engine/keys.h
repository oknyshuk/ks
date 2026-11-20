#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "inputsystem/ButtonCode.h"
#include "cdll_int.h"
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

namespace ks { class text_writer; }
struct InputEvent_t;


void		Key_Event( const InputEvent_t &event );

void		Key_Init( void );
void		Key_Shutdown( void );
void		Key_WriteBindings( ks::text_writer &buf, const int iSplitscreenSlot = -1 );
int			Key_CountBindings( void );
void		Key_SetBinding( ButtonCode_t code, const char *pBinding );
const char	*Key_BindingForKey( ButtonCode_t code );
const char	*Key_NameForBinding( const char *pBinding , int userId = -1, int iStartCount = 0, BindingLookupOption_t nFlags = BINDINGLOOKUP_ALL );
int			Key_CodeForBinding( const char *pBinding , int userId = -1, int iStartCount = 0, BindingLookupOption_t nFlags = BINDINGLOOKUP_ALL );

void		Key_StartTrapMode( void );
bool		Key_CheckDoneTrapping( ButtonCode_t& key );
