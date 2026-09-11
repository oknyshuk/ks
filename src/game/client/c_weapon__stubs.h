//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_WEAPON__STUBS_H
#define C_WEAPON__STUBS_H

#include "client_class.h"
#include "reflect_predmap.h"

// This is an ugly hack to link client classes to weapons for now
//  these will be removed once we predict all weapons, especially TF2 weapons
// STUB_WEAPON_CLASS builds its table by reflection
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

#define STUB_WEAPON_CLASS_IMPLEMENT( entityName, className )		\
	IMPLEMENT_REFLECT_PREDMAP( className );							\
	LINK_ENTITY_TO_CLASS( entityName, className );


#define STUB_WEAPON_CLASS( entityName, className, baseClassName )	\
	class [[= ks::reflect::NetTable{ .name = "DT_" #className } ]]			\
	      C_##className : public baseClassName						\
	{																\
		DECLARE_CLASS( C_##className, baseClassName );				\
	public:															\
		DECLARE_PREDICTABLE();										\
		DECLARE_CLIENTCLASS();										\
		C_##className() {};											\
	private:														\
		C_##className( const C_##className & );						\
	};																\
	IMPLEMENT_REFLECT_CLIENTCLASS( C_##className, DT_##className, C##className )	\
	STUB_WEAPON_CLASS_IMPLEMENT( entityName, C_##className );

#endif // C_WEAPON__STUBS_H
