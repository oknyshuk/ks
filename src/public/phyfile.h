#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "datamap.h"

typedef struct phyheader_s
{
	int		size;
	int		id;
	int		solidCount;
	int		checkSum;	// checksum of source .mdl file
} phyheader_t;
