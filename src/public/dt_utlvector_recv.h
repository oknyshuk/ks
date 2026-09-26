//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#ifndef DT_UTLVECTOR_RECV_H
#define DT_UTLVECTOR_RECV_H
#pragma once


#include "dt_recv.h"
#include "dt_utlvector_common.h"


// Produced by game/shared/reflect_recvtable.h, with ops from WireVec (resize may be a class callback).
RecvProp RecvPropUtlVector(
	const char *pVarName,
	int offset,
	int sizeofVar,
	const WireVecOps &ops,

	int nMaxElements,	// Max # of elements in the array. Keep this as low as possible.
	RecvProp pArrayProp	// The definition of the property you're receiving into.
						// You can leave all of its parameters at 0 (name, offset, size, etc).
	);


#endif // DT_UTLVECTOR_RECV_H
