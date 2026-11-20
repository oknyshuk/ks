// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define LINK_ENTITY_TO_CLASS(mapClassName,DLLClassName) \
	KS_ENTITY_KINDS_DUMP_CLASS( DLLClassName ) \
	KS_ENTITY_DEFINE_THUNKS( DLLClassName ) \
	static CEntityFactory<DLLClassName> mapClassName( #mapClassName );
#define LINK_ENTITY_TO_CLASS_ALIAS(mapClassName,DLLClassName) \
	static CEntityFactory<DLLClassName> mapClassName( #mapClassName );
#define KS_ENTITY_UNSAVED_CLASS(DLLClassName) \
	KS_ENTITY_KINDS_DUMP_CLASS( DLLClassName ) \
	KS_ENTITY_DEFINE_THUNKS( DLLClassName )
#ifdef	DEBUG
#define ASSERT(f)		DBG_AssertFunction((bool)((f)!=0), #f, __FILE__, __LINE__, nullptr)
#define ASSERTSZ(f, sz)	DBG_AssertFunction((bool)((f)!=0), #f, __FILE__, __LINE__, sz)
#else	// !DEBUG
#define ASSERT(f)
#define ASSERTSZ(f, sz)
#endif	// !DEBUG
