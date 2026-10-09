#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

struct factorylist_t
{
	CreateInterfaceFn appSystemFactory;
};

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData );

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData );
