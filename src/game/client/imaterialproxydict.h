// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#pragma once

#define EXPOSE_MATERIAL_PROXY( className, proxyName )						\
	static IMaterialProxy *C##className##Factory( void )					\
	{																		\
		return static_cast< IMaterialProxy * >( new className );			\
	};																		\
	class C##proxyName##Foo													\
	{																		\
	public:																	\
		C##proxyName##Foo( void )											\
		{																	\
			GetMaterialProxyDict().Add( #proxyName,							\
				&C##className##Factory );									\
		}																	\
	};																		\
	static C##proxyName##Foo g_C##proxyName##Foo;
