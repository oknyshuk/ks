//====== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "imaterialproxydict.h"
#include "materialsystem/imaterialproxy.h"
#include "tier1/map_ops.h"

class CMaterialProxyDict : public IMaterialProxyDict
{
public:
	IMaterialProxy *	CreateProxy( const char *proxyName );
	void				Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory );
private:
	ks::idict<MaterialProxyFactory_t *> m_StringToProxyFactoryMap;
};

void CMaterialProxyDict::Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory )
{
	Assert( pMaterialProxyName );
	m_StringToProxyFactoryMap[pMaterialProxyName] = pMaterialProxyFactory;
}

IMaterialProxyDict &GetMaterialProxyDict()
{
	static CMaterialProxyDict g_MaterialProxyDict;
	return g_MaterialProxyDict;
}

IMaterialProxy *CMaterialProxyDict::CreateProxy( const char *pMaterialProxyName )
{
	MaterialProxyFactory_t **ppFactory = ks::find_ptr( m_StringToProxyFactoryMap, pMaterialProxyName );
	if ( !ppFactory )
	{
		return nullptr;
	}
	MaterialProxyFactory_t *pMaterialProxyFactory = *ppFactory;
	Assert( pMaterialProxyFactory );
	return (*pMaterialProxyFactory)();
}
