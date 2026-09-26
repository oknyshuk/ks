//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dt_utlvector_recv.h"


extern char const *s_ClientElementNames[MAX_ARRAY_ELEMENTS];


class CRecvPropExtra_UtlVector
{
public:
	DataTableRecvVarProxyFn m_DataTableProxyFn = nullptr;	// If it's a datatable, then this is the proxy they specified.
	RecvVarProxyFn m_ProxyFn = nullptr;				// If it's a non-datatable, then this is the proxy they specified.
	WireVecOps m_Ops;
	int m_ElementStride = 0;						// Distance between each element in the array.
	int m_Offset = 0;								// Offset of the vector from its parent structure.
	int m_nMaxElements = 0;
};

// Applied before any datatable proxy runs (RecvTable_Decode).
void RecvProxy_UtlVectorLength( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CRecvPropExtra_UtlVector *pExtra = (CRecvPropExtra_UtlVector*)pData->m_pRecvProp->GetExtraData();

	// The field can encode more than max; don't trust a malformed stream.
	int len = pData->m_Value.m_Int;
	if ( len < 0 || len > pExtra->m_nMaxElements )
	{
		Assert( false );
		len = len < 0 ? 0 : pExtra->m_nMaxElements;
	}

	pExtra->m_Ops.resize( pStruct, pExtra->m_Offset, len );
}

void RecvProxy_UtlVectorElement( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CRecvPropExtra_UtlVector *pExtra = (CRecvPropExtra_UtlVector*)pData->m_pRecvProp->GetExtraData();

	// Kind of lame overloading element stride to hold the element index,
	// but we can easily move it into its SetExtraData stuff if we need to.
	int iElement = pData->m_pRecvProp->GetElementStride();

	// Scalars past the end still arrive (as zero); drop them.
	if ( iElement >= pExtra->m_Ops.count( pStruct, pExtra->m_Offset ) )
		return;

	// Call through to the proxy they passed in, making pStruct=the vector.
	pExtra->m_ProxyFn( pData, pOut, (char*)pExtra->m_Ops.base( pStruct, pExtra->m_Offset ) + iElement*pExtra->m_ElementStride );
}

void RecvProxy_UtlVectorElement_DataTable( const RecvProp *pProp, void **pOut, void *pData, int objectID )
{
	CRecvPropExtra_UtlVector *pExtra = (CRecvPropExtra_UtlVector*)pProp->GetExtraData();

	int iElement = pProp->GetElementStride();
	Assert( iElement < pExtra->m_nMaxElements );

	// Null past the end: the decoder skips the slot (stale slots persist in merged states).
	if ( iElement >= pExtra->m_Ops.count( pData, pExtra->m_Offset ) )
	{
		*pOut = nullptr;
		return;
	}

	// Call through to the proxy they passed in, making pStruct=the vector and forcing iElement to 0.
	pExtra->m_DataTableProxyFn( pProp, pOut, (char*)pExtra->m_Ops.base( pData, pExtra->m_Offset ) + iElement*pExtra->m_ElementStride, objectID );
}


void DataTableRecvProxy_LengthProxy( const RecvProp *pProp, void **pOut, void *pData, int objectID )
{
	*pOut = pData;
}


RecvProp RecvPropUtlVector(
	const char *pVarName,
	int offset,			// Used to generate pData in the function specified in varProxy.
	int sizeofVar,		// The size of each element in the vector.
	const WireVecOps &ops,
	int nMaxElements,	// Max # of elements in the array. Keep this as low as possible.
	RecvProp pArrayProp
	)
{
	RecvProp ret;

	Assert( nMaxElements <= MAX_ARRAY_ELEMENTS );
	Assert( ops.resize && ops.base && ops.count );

	ret.m_RecvType = DPT_DataTable;
	ret.m_pVarName = pVarName;
	ret.SetOffset( 0 );
	ret.SetDataTableProxyFn( DataTableRecvProxy_StaticDataTable );
	
	RecvProp *pProps = new RecvProp[nMaxElements+1]; // TODO free that again

	
	// Extra data bound to each of the properties.
	CRecvPropExtra_UtlVector *pExtraData = new CRecvPropExtra_UtlVector;
	
	pExtraData->m_nMaxElements = nMaxElements;
	pExtraData->m_ElementStride = sizeofVar;
	pExtraData->m_Ops = ops;
	pExtraData->m_Offset = offset;
	
	if ( pArrayProp.m_RecvType == DPT_DataTable )
		pExtraData->m_DataTableProxyFn = pArrayProp.GetDataTableProxyFn();
	else
		pExtraData->m_ProxyFn = pArrayProp.GetProxyFn();


	// Length subtable; must match SendPropUtlVector's layout and names.
	RecvProp *pLengthProp = new RecvProp;
	*pLengthProp = RecvPropInt( AllocateStringHelper( "lengthprop%d", nMaxElements ), 0, 0, 0, RecvProxy_UtlVectorLength );
	pLengthProp->SetExtraData( pExtraData );
	pLengthProp->SetVectorLength();

	char *pLengthProxyTableName = AllocateUniqueDataTableName( false, "_LPT_%s_%d", pVarName, nMaxElements );
	RecvTable *pLengthTable = new RecvTable( pLengthProp, 1, pLengthProxyTableName );
	pProps[0] = RecvPropDataTable( "lengthproxy", 0, 0, pLengthTable, DataTableRecvProxy_LengthProxy );
	pProps[0].SetExtraData( pExtraData );

	// The first element is a sub-datatable.
	for ( int i = 1; i < nMaxElements+1; i++ )
	{
		pProps[i] = pArrayProp;	// copy array element property setting
		pProps[i].SetOffset( 0 ); // leave offset at 0 so pStructBase is always a pointer to the vector's owner
		pProps[i].m_pVarName = s_ClientElementNames[i-1];	// give unique name
		pProps[i].SetExtraData( pExtraData );
		pProps[i].SetElementStride( i-1 );	// Kind of lame overloading element stride to hold the element index,
											// but we can easily move it into its SetExtraData stuff if we need to.
		
		// We provide our own proxy here.
		if ( pArrayProp.m_RecvType == DPT_DataTable )
		{
			pProps[i].SetDataTableProxyFn( RecvProxy_UtlVectorElement_DataTable );
		}
		else
		{
			pProps[i].SetProxyFn( RecvProxy_UtlVectorElement );
		}
	}

	RecvTable *pTable = new RecvTable( 
		pProps, 
		nMaxElements+1, 
		AllocateUniqueDataTableName( false, "_ST_%s_%d", pVarName, nMaxElements )
		); // TODO free that again

	ret.SetDataTable( pTable );
	return ret;
}
