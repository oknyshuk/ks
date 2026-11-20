// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#pragma once
import ks.core.types;
import ks.core.vprof;

#define IMPLEMENT_REFLECT_CLIENTCLASS( clientClassName, dataTable, serverClassName )             \
	namespace dataTable { RecvTable g_RecvTable; }                                               \
	static const int g_##clientClassName##_ReflectRecvInit = []                                  \
	{                                                                                            \
		auto &p = ksr::recv::table_props<clientClassName>();                             \
		dataTable::g_RecvTable.Construct( p.data(), (int)p.size(), #dataTable );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static IClientNetworkable *_##clientClassName##_CreateObject( int entnum, int serialNum )     \
	{                                                                                            \
		clientClassName *pRet = new clientClassName;                                             \
		if ( !pRet ) return 0;                                                                    \
		pRet->Init( entnum, serialNum );                                                          \
		return pRet;                                                                             \
	}                                                                                            \
	ClientClass __g_##clientClassName##ClientClass( #serverClassName,                             \
	                                                _##clientClassName##_CreateObject,            \
	                                                nullptr, &dataTable::g_RecvTable );               \
	RecvTable *clientClassName::m_pClassRecvTable = &dataTable::g_RecvTable;                       \
	int clientClassName::YouForgotToImplementOrDeclareClientClass() { return 0; }                  \
	ClientClass *clientClassName::GetClientClass() { return &__g_##clientClassName##ClientClass; } \

#define IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( clientClassName, dataTable, serverClassName )        \
	namespace dataTable { RecvTable g_RecvTable; }                                               \
	static const int g_##clientClassName##_ReflectRecvInit = []                                  \
	{                                                                                            \
		auto &p = ksr::recv::table_props<clientClassName>();                             \
		dataTable::g_RecvTable.Construct( p.data(), (int)p.size(), #dataTable );                  \
		return 1;                                                                                \
	}();                                                                                         \
	static clientClassName __g_##clientClassName;                                                \
	static IClientNetworkable *_##clientClassName##_CreateObject()                                \
	{                                                                                            \
		return &__g_##clientClassName;                                                           \
	}                                                                                            \
	ClientClass __g_##clientClassName##ClientClass( #serverClassName, nullptr,                       \
	                                                _##clientClassName##_CreateObject,            \
	                                                &dataTable::g_RecvTable );                     \
	RecvTable *clientClassName::m_pClassRecvTable = &dataTable::g_RecvTable;                       \
	int clientClassName::YouForgotToImplementOrDeclareClientClass() { return 0; }                  \
	ClientClass *clientClassName::GetClientClass() { return &__g_##clientClassName##ClientClass; } \

#define IMPLEMENT_REFLECT_TABLE_IN( className, tableName )                                        \
	namespace tableName { RecvTable g_RecvTable; }                                                       \
	static const int g_##className##_##tableName##_ReflectInit = []                               \
	{                                                                                            \
		auto &p = ksr::recv::table_props_in<className, #tableName>();                        \
		tableName::g_RecvTable.Construct( p.data(), (int)p.size(), #tableName );                    \
		return 1;                                                                                \
	}();                                                                                         \

#define IMPLEMENT_REFLECT_TABLE( className, tableName )                                   \
	namespace tableName { RecvTable g_RecvTable; }                                               \
	static const int g_##className##_ReflectRecvInit = []                                        \
	{                                                                                            \
		auto &p = ksr::recv::table_props<className>();                                    \
		tableName::g_RecvTable.Construct( p.data(), (int)p.size(), #tableName );                  \
		return 1;                                                                                \
	}();                                                                                         \

