// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "client_class.h"
#include "reflect_recvtable.h"
module ks.client;

import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :util;
import :precache;
import :noopanimcontroller;
import :baseentity;
import :sharedvar;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CBaseDoor, DT_BaseDoor, CBaseDoor )

CBaseDoor::CBaseDoor( void )
{
	m_flWaveHeight = 0.0f;
}

CBaseDoor::~CBaseDoor( void )
{
}
