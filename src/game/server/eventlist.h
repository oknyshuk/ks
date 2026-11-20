// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define REGISTER_SHARED_ANIMEVENT( _n, b ) EventList_RegisterSharedEvent(#_n, _n, b );
#define REGISTER_PRIVATE_ANIMEVENT( _n ) _n = EventList_RegisterPrivateEvent( #_n );
