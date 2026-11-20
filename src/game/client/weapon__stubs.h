// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define STUB_WEAPON_CLASS_IMPLEMENT( entityName, className )		\
	IMPLEMENT_REFLECT_PREDMAP( className );							\
	LINK_ENTITY_TO_CLASS( entityName, className );
#define STUB_WEAPON_CLASS( entityName, className, baseClassName )	\
	class [[= ks::reflect::NetTable{ .name = "DT_" #className } ]]			\
	      C##className : public baseClassName						\
	{																\
		DECLARE_CLASS( C##className, baseClassName );				\
	public:															\
		DECLARE_PREDICTABLE();										\
		DECLARE_CLIENTCLASS();										\
		C##className() {};											\
	private:														\
		C##className( const C##className & );						\
	};																\
	IMPLEMENT_REFLECT_CLIENTCLASS( C##className, DT_##className, C##className )	\
	STUB_WEAPON_CLASS_IMPLEMENT( entityName, C##className );
