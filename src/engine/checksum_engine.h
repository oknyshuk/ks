#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Engine crc routines

import std;
import ks.tier1;
#include "quakedef.h"

bool CRC_File(CRC32_t *crcvalue, const char *pszFileName);
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence);
bool CRC_MapFile(CRC32_t *crcvalue, const char *pszFileName);

bool MD5_Hash_File(unsigned char digest[16], const char *pszFileName, bool bSeed, unsigned int seed[4]);
