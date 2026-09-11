//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef MATERIALPROXYFACTORY_H
#define MATERIALPROXYFACTORY_H


#include "materialsystem/imaterialproxyfactory.h"
#include "tier1/interface.h"


class CMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	IMaterialProxy *CreateProxy( const char *proxyName );
	void DeleteProxy( IMaterialProxy *pProxy );
	CreateInterfaceFn GetFactory();
};

#endif // MATERIALPROXYFACTORY_H
