// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define REGISTER_EFFECT( effect )														\
	IParticleEffect* effect##_Factory()													\
	{																					\
		return new effect;																\
	}																					\
	struct effect##_RegistrationHelper													\
	{																					\
		effect##_RegistrationHelper()													\
		{																				\
			ParticleMgr()->RegisterEffect( typeid( effect ).name(), effect##_Factory );	\
		}																				\
	};																					\
	static effect##_RegistrationHelper g_##effect##_RegistrationHelper
#define REGISTER_EFFECT_USING_CREATE( effect )											\
	IParticleEffect* effect##_Factory()													\
	{																					\
		return effect::Create( #effect ).get();									\
	}																					\
	struct effect##_RegistrationHelper													\
	{																					\
		effect##_RegistrationHelper()													\
		{																				\
			ParticleMgr()->RegisterEffect( typeid( effect ).name(), effect##_Factory );	\
		}																				\
	};																					\
	static effect##_RegistrationHelper g_##effect##_RegistrationHelper
