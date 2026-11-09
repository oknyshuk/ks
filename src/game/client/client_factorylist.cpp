// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "client_factorylist.h"

static factorylist_t s_factories;

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData )
{
	s_factories = sourceData;
}

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData )
{
	destData = s_factories;
}
