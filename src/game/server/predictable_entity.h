// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define DECLARE_NETWORKCLASS()											\
		DECLARE_SERVERCLASS()
	#define DECLARE_PREDICTABLE()	
#define LINK_ENTITY_TO_CLASS_ALIASED( localName, className ) LINK_ENTITY_TO_CLASS(localName, C##className )
#define IMPLEMENT_NETWORKCLASS_ALIASED(className, dataTable)	\
	IMPLEMENT_SERVERCLASS( C##className, dataTable )
#define LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( classNameDerived, classNameBase, dataTableName, entity_name )		\
	class [[= ks::reflect::NetTable{ .name = #dataTableName } ]]									\
	      C##classNameDerived : public C##classNameBase								\
	{																						\
	public:																					\
		DECLARE_CLASS( C##classNameDerived, C##classNameBase );							\
		DECLARE_NETWORKCLASS();																\
	};																						\
	IMPLEMENT_REFLECT_TABLE( C##classNameDerived, dataTableName );	\
	IMPLEMENT_NETWORKCLASS_ALIASED( classNameDerived, dataTableName )	\
	LINK_ENTITY_TO_CLASS_ALIASED( entity_name, classNameDerived )
