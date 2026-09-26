//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "extkeyvalues.h"

#include <ctype.h>
#include "fmtstr.h"



//
// Rule evaluation
//

static class CPropertyRule_Max : public IPropertyRule
{
public:
	virtual bool ApplyRuleUint64( uint64 uiBase, uint64 &uiNew )
	{
		bool bResult = ( uiNew > uiBase );
		uiNew = std::max( uiNew, uiBase );
		return bResult;
	}
	virtual bool ApplyRuleFloat( float flBase, float &flNew )
	{
		bool bResult = ( flNew > flBase );
		flNew = std::max( flNew, flBase );
		return bResult;
	}
}
g_PropertyRule_Max;

static class CPropertyRule_None : public IPropertyRule
{
public:
	virtual bool ApplyRuleUint64( uint64 uiBase, uint64 &uiNew ) { return true; }
	virtual bool ApplyRuleFloat( float flBase, float &flNew ) { return true; }
}
g_PropertyRule_None;

IPropertyRule * GetRuleByName( char const *szRuleName )
{
	if ( !Q_stricmp( szRuleName, "max" ) )
		return &g_PropertyRule_Max;

	return &g_PropertyRule_None;
}
