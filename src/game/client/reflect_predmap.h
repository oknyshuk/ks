// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#pragma once

#define IMPLEMENT_REFLECT_PREDMAP_BASE( className, base )                                         \
	datamap_t className::m_PredMap = { .dataClassName = #className, .baseMap = base };                                   \
	datamap_t *className::GetPredDescMap( void ) { return &m_PredMap; }                            \
	namespace className##_ReflectPredInit                                                          \
	{                                                                                              \
		static const bool g_filled = []                                                             \
		{                                                                                            \
			auto &f = ksr::pred::fields<className>();                                          \
			className::m_PredMap.dataDesc      = f.data();                                             \
			className::m_PredMap.dataNumFields = (int)f.size();                                        \
			return true;                                                                               \
		}();                                                                                        \
	}
#define IMPLEMENT_REFLECT_PREDMAP( className )                                                     \
	IMPLEMENT_REFLECT_PREDMAP_BASE( className, &BaseClass::m_PredMap )
#define IMPLEMENT_REFLECT_PREDMAP_NO_BASE( className )                                            \
	IMPLEMENT_REFLECT_PREDMAP_BASE( className, nullptr )
