//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PREDICTABLE_ENTITY_H
#define PREDICTABLE_ENTITY_H
#ifdef _WIN32
#pragma once
#endif

// For introspection
#include "tier0/platform.h"
#include "predictioncopy.h"
#include "shared_classnames.h"

#ifndef NO_ENTITY_PREDICTION
#define UsePrediction() 1
#else
#define UsePrediction() 0
#endif

// CLIENT DLL includes
#if defined( CLIENT_DLL ) || defined( TOOL_DLL )

#include "iclassmap.h"
#include "recvproxy.h"

class SendTable;

// Game DLL includes
#else

#include "sendproxy.h"

#endif  // !CLIENT_DLL

#if defined( CLIENT_DLL )

#define DECLARE_NETWORKCLASS()											\
		DECLARE_CLIENTCLASS()

#define DECLARE_NETWORKCLASS_NOBASE()									\
		DECLARE_CLIENTCLASS_NOBASE()							

#else

#define DECLARE_NETWORKCLASS()											\
		DECLARE_SERVERCLASS()

#define DECLARE_NETWORKCLASS_NOBASE()									\
		DECLARE_SERVERCLASS_NOBASE()	

#endif

#if defined( CLIENT_DLL )

#ifndef NO_ENTITY_PREDICTION
#define DECLARE_PREDICTABLE()											\
	public:																\
		static typedescription_t m_PredDesc[];							\
		static datamap_t m_PredMap;										\
		virtual datamap_t *GetPredDescMap( void );						\
		template <typename T> friend datamap_t *PredMapInit(T *)
#else
#define DECLARE_PREDICTABLE()	template <typename T> friend datamap_t *PredMapInit(T *)
#endif

// The prediction descriptor DSL is gone: prediction maps are generated from Pred and PredFrom
// annotations by game/shared/reflect_predmap.h, and every one of the 53 in the tree was proven
// byte-equal to its macro-built predecessor before the block was removed. DECLARE_PREDICTABLE
// above stays -- it declares m_PredMap, which the emitter fills.

#else

	// nothing, only client has a prediction system
	#define DECLARE_PREDICTABLE()	

#endif

#if defined( CLIENT_DLL )

// On the client .dll this creates a mapping between a classname and
//  a client side class.  Probably could be templatized at some point.

#define LINK_ENTITY_TO_CLASS( localName, className )						\
	static C_BaseEntity *C##className##Factory( void )						\
	{																		\
		return static_cast< C_BaseEntity * >( new className );				\
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
	static C_BaseEntity *C##className##Factory( void )						\
	{																		\
		return static_cast< C_BaseEntity * >( new className );				\
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



#define LINK_ENTITY_TO_CLASS_ALIASED( localName, className ) LINK_ENTITY_TO_CLASS(localName, C_##className )

#define IMPLEMENT_NETWORKCLASS_ALIASED(className, dataTable)			\
	IMPLEMENT_CLIENTCLASS( C_##className, dataTable, C##className )
#define IMPLEMENT_NETWORKCLASS(className, dataTable)					\
	IMPLEMENT_CLIENTCLASS(className, dataTable, className)
#define IMPLEMENT_NETWORKCLASS_DT(className, dataTable)					\
	IMPLEMENT_CLIENTCLASS_DT(className, dataTable, className)

// LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED builds its table by reflection
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

#define LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( classNameDerived, classNameBase, dataTableName, entity_name )		\
	class [[= ks::reflect::NetTable{ .name = #dataTableName } ]]									\
	      C_##classNameDerived : public C_##classNameBase								\
	{																						\
	public:																					\
		DECLARE_CLASS( C_##classNameDerived, C_##classNameBase );						\
		DECLARE_NETWORKCLASS();																\
	};																						\
	IMPLEMENT_REFLECT_TABLE( C_##classNameDerived, dataTableName );	\
	IMPLEMENT_NETWORKCLASS_ALIASED( classNameDerived, dataTableName )	\
	LINK_ENTITY_TO_CLASS_ALIASED( entity_name, classNameDerived )

#else



#define LINK_ENTITY_TO_CLASS_ALIASED( localName, className ) LINK_ENTITY_TO_CLASS(localName, C##className )

#define IMPLEMENT_NETWORKCLASS_ALIASED(className, dataTable)	\
	IMPLEMENT_SERVERCLASS( C##className, dataTable )
#define IMPLEMENT_NETWORKCLASS(className, dataTable)			\
	IMPLEMENT_SERVERCLASS(className, dataTable)
#define IMPLEMENT_NETWORKCLASS_DT(className, dataTable)			\
	IMPLEMENT_SERVERCLASS_ST(className, dataTable)

#include "reflect_sendtable.h"
#include "reflect_annotations.h"

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

#endif																	

#endif // PREDICTABLE_ENTITY_H
