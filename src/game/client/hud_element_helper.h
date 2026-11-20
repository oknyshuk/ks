// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define DECLARE_HUDELEMENT( className )											\
	static CHudElement *Create_##className( void )							\
		{																		\
			return new className( #className );									\
		};																		\
	static CHudElementHelper g_##className##_Helper( Create_##className, 50 );
#define DECLARE_HUDELEMENT_DEPTH( className, depth )							\
	static CHudElement *Create_##className( void )							\
		{																		\
			return new className( #className );									\
		};																		\
	static CHudElementHelper g_##className##_Helper( Create_##className, depth );
#define GET_HUDELEMENT( className )												\
	(className*)GetHud().FindElement( #className )
#define DECLARE_MULTIPLY_INHERITED()							\
    void *operator new( size_t stAllocateBlock )				\
	{															\
		return CHudElement::operator new ( stAllocateBlock );	\
	}															\
	void* operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )	\
	{ 																								\
		return CHudElement::operator new ( stAllocateBlock, nBlockUse, pFileName, nLine );	\
	}															\
	void operator delete( void *pMem )							\
	{															\
		CHudElement::operator delete ( pMem );					\
	}															\
	void operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine )	\
	{															\
		CHudElement::operator delete ( pMem, nBlockUse, pFileName, nLine );	\
	}
