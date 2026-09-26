//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#ifndef DT_UTLVECTOR_SEND_H
#define DT_UTLVECTOR_SEND_H
#pragma once


#include "dt_send.h"
#include "dt_utlvector_common.h"


// Produced by game/shared/reflect_sendtable.h, with ops from WireVec. Uses ops.base/count only.
SendProp SendPropUtlVector(
	const char *pVarName,
	int offset,
	int sizeofVar,
	const WireVecOps &ops,

	int nMaxElements,			// Max # of elements in the array. Keep this as low as possible.
	SendProp pArrayProp,		// Describe the data inside of each element in the array.
	SendTableProxyFn varProxy=SendProxy_DataTableToDataTable	// This can be overridden to control who the array is sent to.
	);


#endif	// DT_UTLVECTOR_SEND_H
