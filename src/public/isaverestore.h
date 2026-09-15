//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The keyvalue parse hook for FIELD_CUSTOM datamap entries.
//
// The save/restore system that used to live here -- ISave, IRestore, CSaveRestoreData,
// the block handlers, and the client and server save hooks -- is gone. What remains is
// the one method datamap_keyvalue.cpp still calls.
//
// $NoKeywords: $
//=============================================================================//

#ifndef ISAVERESTORE_H
#define ISAVERESTORE_H

#include "datamap.h"


struct SaveRestoreFieldInfo_t
{
	void *			   pField;

	// Note that it is legal for the following two fields to be NULL,
	// though it may be disallowed by implementors of ISaveRestoreOps
	void *			   pOwner;
	typedescription_t *pTypeDesc;
};

abstract_class ISaveRestoreOps
{
public:
	virtual bool Parse( const SaveRestoreFieldInfo_t &fieldInfo, char const* szValue ) = 0;

	//---------------------------------
};

//-------------------------------------

class CDefSaveRestoreOps : public ISaveRestoreOps
{
public:
	virtual bool Parse( const SaveRestoreFieldInfo_t &fieldInfo, char const* szValue ) { return false; }
};


//=============================================================================

#endif // ISAVERESTORE_H
