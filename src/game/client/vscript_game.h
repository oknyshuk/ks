// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2008 Valve Corporation

#pragma once
import ks.core.basetypes;

#define ALLOW_SCRIPT_ACCESS() template <typename T> friend ScriptClassDesc_t *GetScriptDesc(T *);
#define BEGIN_SCRIPTDESC( className, baseClass, description )								BEGIN_SCRIPTDESC_NAMED( className, baseClass, #className, description )
#define BEGIN_SCRIPTDESC_ROOT( className, description )										BEGIN_SCRIPTDESC_ROOT_NAMED( className, #className, description )
	#define DEFINE_SCRIPTDESC_FUNCTION( className, baseClass ) \
		template <> ScriptClassDesc_t * GetScriptDesc<baseClass>( baseClass *); \
		template <> ScriptClassDesc_t * GetScriptDesc<className>( className *)
#define BEGIN_SCRIPTDESC_NAMED( className, baseClass, scriptName, description ) \
	IScriptInstanceHelper *GetScriptInstanceHelper_##baseClass(); \
	KS_BEGIN_SCRIPTDESC( className, baseClass, scriptName, description, GetScriptInstanceHelper_##baseClass() )
#define BEGIN_SCRIPTDESC_ROOT_NAMED( className, scriptName, description ) \
	KS_BEGIN_SCRIPTDESC( className, ScriptNoBase_t, scriptName, description, nullptr )
#define KS_BEGIN_SCRIPTDESC( className, baseClass, scriptName, description, baseHelper ) \
	IScriptInstanceHelper *GetScriptInstanceHelper_##className() \
	{ \
		return GetScriptInstanceHelperOverride< className >( baseHelper ); \
	}; \
	extern void Init##className##ScriptDesc(); \
	ScriptClassDesc_t g_##className##_ScriptDesc( &Init##className##ScriptDesc ); \
	DEFINE_SCRIPTDESC_FUNCTION( className, baseClass ) \
	{ \
		return &g_##className##_ScriptDesc; \
	} \
	\
	void Init##className##ScriptDesc() \
	{ \
		static bool bInitialized; \
		if ( bInitialized ) \
		{ \
			return; \
		} \
		\
		bInitialized = true; \
		\
		typedef className _className; \
		ScriptClassDesc_t *pDesc = &g_##className##_ScriptDesc; \
		pDesc->m_pszDescription = description; \
		ScriptInitClassDescNamed( pDesc, className, GetScriptDescForClass( baseClass ), scriptName ); \
		pDesc->pHelper = GetScriptInstanceHelper_##className();
#define END_SCRIPTDESC() \
		return; \
	}
#define DEFINE_SCRIPTFUNC( func, description )												DEFINE_SCRIPTFUNC_NAMED( func, #func, description )
#define DEFINE_SCRIPTFUNC_NAMED( func, scriptName, description )							ScriptAddFunctionToClassDescNamed( pDesc, _className, func, scriptName, description );
#define GetScriptDescForClass( className ) GetScriptDesc( ( className *)nullptr )
