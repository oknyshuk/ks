//========== Copyright (c) Valve Corporation, All rights reserved. ============
//
// Purpose: performs serialization for CSOAContainer (avoids tier1 depending on datamodel)
//
//=============================================================================

#ifndef UTLSOACONTAINER_SERIALIZATION_H
#define UTLSOACONTAINER_SERIALIZATION_H


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CSOAContainer;


//-----------------------------------------------------------------------------
// Serialization/Unserialization
//-----------------------------------------------------------------------------
bool SerializeCSOAContainer(   const CSOAContainer *pContainer, CDmxElement *pRootElement );
bool UnserializeCSOAContainer( const CSOAContainer *pContainer, const CDmxElement *pRootElement );


#endif // UTLSOACONTAINER_SERIALIZATION_H
