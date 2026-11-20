// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define DECLARE_CLIENT_EFFECT_INTERNAL( effectName, callbackFunction ) \
	static CClientEffectRegistration ClientEffectReg_##callbackFunction( #effectName, callbackFunction );
#define DECLARE_CLIENT_EFFECT( effectName, callbackFunction )		\
	DECLARE_CLIENT_EFFECT_INTERNAL( effectName, callbackFunction )	\
	PRECACHE_REGISTER_BEGIN( DISPATCH_EFFECT, effectName )			\
	PRECACHE_REGISTER_END()
#define DECLARE_CLIENT_EFFECT_BEGIN( effectName, callbackFunction ) \
	DECLARE_CLIENT_EFFECT_INTERNAL( effectName, callbackFunction )	\
	PRECACHE_REGISTER_BEGIN( DISPATCH_EFFECT, effectName )
#define DECLARE_CLIENT_EFFECT_END()	PRECACHE_REGISTER_END()
