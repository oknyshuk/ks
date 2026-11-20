// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once
import ks.core.basetypes;

#define DECLARE_NETWORKCLASS()											\
		DECLARE_CLIENTCLASS()
#define DECLARE_PREDICTABLE()											\
	public:																\
		static typedescription_t m_PredDesc[];							\
		static datamap_t m_PredMap;										\
		virtual datamap_t *GetPredDescMap( void );						\
		template <typename T> friend datamap_t *PredMapInit(T *)
#define LINK_ENTITY_TO_CLASS( localName, className )						\
	static CBaseEntity *C##className##Factory( void )						\
	{																		\
		return static_cast< CBaseEntity * >( new className );				\
	};																		\
	class C##localName##Foo													\
	{																		\
	public:																	\
		C##localName##Foo( void )											\
		{																	\
			GetClassMap().Add( #localName, #className, sizeof( className ),	\
				&C##className##Factory );									\
			__g_##className##ClientClass.m_pMapClassname = #localName;		\
		}																	\
	};																		\
	static C##localName##Foo g_C##localName##Foo;
#define LINK_ENTITY_TO_CLASS_CLIENTONLY( localName, className )				\
	static CBaseEntity *C##className##Factory( void )						\
	{																		\
		return static_cast< CBaseEntity * >( new className );				\
	};																		\
	class C##localName##Foo													\
	{																		\
	public:																	\
		C##localName##Foo( void )											\
		{																	\
			GetClassMap().Add( #localName, #className, sizeof( className ),	\
				&C##className##Factory );									\
		}																	\
	};																		\
	static C##localName##Foo g_C##localName##Foo;
#define LINK_ENTITY_TO_CLASS_ALIASED( localName, className ) LINK_ENTITY_TO_CLASS(localName, C##className )
#define IMPLEMENT_NETWORKCLASS_ALIASED(className, dataTable)			\
	IMPLEMENT_CLIENTCLASS( C##className, dataTable, C##className )
#define LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( classNameDerived, classNameBase, dataTableName, entity_name )		\
	class [[= ks::reflect::NetTable{ .name = #dataTableName } ]]									\
	      C##classNameDerived : public C##classNameBase								\
	{																						\
	public:																					\
		DECLARE_CLASS( C##classNameDerived, C##classNameBase );						\
		DECLARE_NETWORKCLASS();																\
	};																						\
	IMPLEMENT_REFLECT_TABLE( C##classNameDerived, dataTableName );	\
	IMPLEMENT_NETWORKCLASS_ALIASED( classNameDerived, dataTableName )	\
	LINK_ENTITY_TO_CLASS_ALIASED( entity_name, classNameDerived )
