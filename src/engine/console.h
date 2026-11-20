#pragma once
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

typedef unsigned char byte;

//
// console
//
extern bool con_initialized;

void Con_Init (void);
void Con_Shutdown (void);   // Free overlay line buffer.
void Con_ClearNotify (void);

bool Con_IsVisible();

/*
Read the console log from disk and return it in 'buf'. Buf should come
in as an empty text_writer.
Returns true if the log file is successfully read.
*/
namespace ks { class text_writer; }
bool GetConsoleLogFileData( ks::text_writer& buf );
