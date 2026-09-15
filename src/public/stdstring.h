//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "isaverestore.h"

#ifndef STDSTRING_H
#define STDSTRING_H



#include <string>


class CStdStringSaveRestoreOps : public CDefSaveRestoreOps
{
public:
	enum
	{
		MAX_SAVE_LEN = 4096,
	};
};

//-------------------------------------

inline ISaveRestoreOps *GetStdStringDataOps()
{
	static CStdStringSaveRestoreOps ops;
	return &ops;
}

//-------------------------------------

#define DEFINE_STDSTRING(name) \
	{ FIELD_CUSTOM, #name, (int)offsetof(classNameTypedef,name), 1, FTYPEDESC_SAVE, nullptr, GetStdStringDataOps(), nullptr }

#endif // STDSTRING_H
