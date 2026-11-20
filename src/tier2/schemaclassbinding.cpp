// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "resourcefile/schema.h"
#include "resourcesystem/iresourcesystem.h"
import ks.tier1.interfaces;

CSchemaClassBindingBase *CSchemaClassBindingBase::sm_pClassBindingList;

void CSchemaClassBindingBase::Install()
{
	for ( CSchemaClassBindingBase *pBinding = sm_pClassBindingList; pBinding; pBinding = pBinding->m_pNextBinding )
	{
		g_pResourceSystem->InstallSchemaClassBinding( pBinding );
	}
}

const CResourceStructIntrospection *CSchemaClassBindingBase::GetIntrospection() const
{
	if ( m_pIntrospection == nullptr )
	{
		m_pIntrospection = g_pResourceSystem->FindStructIntrospection( m_pClassName );
	}

	return m_pIntrospection;
}
