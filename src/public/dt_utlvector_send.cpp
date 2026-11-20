// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "dt_utlvector_send.h"
import ks.core.basetypes;

extern char const *s_ElementNames[MAX_ARRAY_ELEMENTS];

// This gets associated with SendProps inside a utlvector and stores extra data needed to make it work.
class CSendPropExtra_UtlVector
{
public:
	SendTableProxyFn m_DataTableProxyFn = nullptr;	// If it's a datatable, then this is the proxy they specified.
	SendVarProxyFn m_ProxyFn = nullptr;				// If it's a non-datatable, then this is the proxy they specified.
	WireVecOps m_Ops;
	int m_ElementStride = 0;						// Distance between each element in the array.
	int m_Offset = 0;								// # bytes from the parent structure to its vector.
	int m_nMaxElements = 0;
};


void SendProxy_UtlVectorElement( 
	const SendProp *pProp, 
	const void *pStruct, 
	const void *pData, 
	DVariant *pOut, 
	int iElement, 
	int objectID )
{
	CSendPropExtra_UtlVector *pExtra = (CSendPropExtra_UtlVector*)pProp->GetExtraData();
	contract_assert( pExtra );

	// Kind of lame overloading element stride to hold the element index,
	// but we can easily move it into its SetExtraData stuff if we need to.
	iElement = pProp->GetElementStride();

	// Shares the vector table's base, so it can't be absent; zero past the end.
	if ( iElement >= pExtra->m_Ops.count( pStruct, pExtra->m_Offset ) )
	{
		std::memset( pOut, 0, sizeof( *pOut ) );
	}
	else
	{
		// Call through to the proxy they passed in, making pStruct=the vector and forcing iElement to 0.
		pExtra->m_ProxyFn( pProp, pData, (char*)pExtra->m_Ops.base( pStruct, pExtra->m_Offset ) + iElement*pExtra->m_ElementStride, pOut, 0, objectID );
	}
}

void* SendProxy_UtlVectorElement_DataTable( 
	const SendProp *pProp,
	const void *pStructBase, 
	const void *pData, 
	CSendProxyRecipients *pRecipients, 
	int objectID )
{
	CSendPropExtra_UtlVector *pExtra = (CSendPropExtra_UtlVector*)pProp->GetExtraData();

	int iElement = pProp->m_ElementStride;
	contract_assert( iElement < pExtra->m_nMaxElements );

	// Null past the end: the encoder omits the slot.
	if ( iElement >= pExtra->m_Ops.count( pStructBase, pExtra->m_Offset ) )
		return nullptr;

	// Call through to the proxy they passed in, making pStruct=the vector and forcing iElement to 0.
	return pExtra->m_DataTableProxyFn( pProp, pData, (char*)pExtra->m_Ops.base( pStructBase, pExtra->m_Offset ) + iElement*pExtra->m_ElementStride, pRecipients, objectID );
}

void SendProxy_UtlVectorLength( 
	const SendProp *pProp, 
	const void *pStruct, 
	const void *pData, 
	DVariant *pOut, 
	int iElement, 
	int objectID )
{
	CSendPropExtra_UtlVector *pExtra = (CSendPropExtra_UtlVector*)pProp->GetExtraData();
	
	// Don't let them overflow the buffer because they might expect that to get transmitted to the client.
	pOut->m_Int = pExtra->m_Ops.count( pStruct, pExtra->m_Offset );
	if ( pOut->m_Int > pExtra->m_nMaxElements )
	{
		contract_assert( false );
		pOut->m_Int = pExtra->m_nMaxElements;
	}
}


// Not SendProxy_DataTableToDataTable: that would put the (storage-less) length into the
// listen-server change-offset map (BuildPropOffsetToIndexMap).
void* SendProxy_LengthTable( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
{
	return (void*)pData;
}


// Note: your pArrayProp will NOT get iElement set to anything other than 0, because this function installs its
// own proxy in front of yours. pStruct will point at the vector and pData will point at the element in it.
//
// Note: you have to be DILIGENT about calling NetworkStateChanged whenever an element in your vector changes
// since the vector doesn't do this automatically.
SendProp SendPropUtlVector(
	const char *pVarName,
	int offset,			// Used to generate pData in the function specified in varProxy.
	int sizeofVar,		// The size of each element in the vector.
	const WireVecOps &ops,
	int nMaxElements,			// Max # of elements in the array. Keep this as low as possible.
	SendProp pArrayProp,		// Describe the data inside of each element in the array.
	SendTableProxyFn varProxy	// This can be overridden to control who the array is sent to.
	)
{
	SendProp ret;

	contract_assert( nMaxElements <= MAX_ARRAY_ELEMENTS );
	contract_assert( ops.base && ops.count );

	ret.m_Type = DPT_DataTable;
	ret.m_pVarName = pVarName;
	ret.SetOffset( 0 );
	ret.SetDataTableProxyFn( varProxy );
	
	// Handle special proxy types where they always let all clients get the results.
	if ( varProxy == SendProxy_DataTableToDataTable || varProxy == SendProxy_DataTablePtrToDataTable )
	{
		ret.SetFlags( SPROP_PROXY_ALWAYS_YES );
	}

	
	// Extra data bound to each of the properties.
	CSendPropExtra_UtlVector *pExtraData = new CSendPropExtra_UtlVector;
	
	pExtraData->m_nMaxElements = nMaxElements;
	pExtraData->m_ElementStride = sizeofVar;
	pExtraData->m_Ops = ops;
	pExtraData->m_Offset = offset;

	if ( pArrayProp.m_Type == DPT_DataTable )
		pExtraData->m_DataTableProxyFn = pArrayProp.GetDataTableProxyFn();
	else
		pExtraData->m_ProxyFn = pArrayProp.GetProxyFn();


	SendProp *pProps = new SendProp[nMaxElements+1]; // TODO free that again

	// Length subtable. Layout and names are wire/demo format; keep them.
	SendProp *pLengthProp = new SendProp;
	*pLengthProp = SendPropInt( AllocateStringHelper( "lengthprop%d", nMaxElements ), 0, 0, NumBitsForCount( nMaxElements ), SPROP_UNSIGNED, SendProxy_UtlVectorLength );
	pLengthProp->SetExtraData( pExtraData );

	char *pLengthProxyTableName = AllocateUniqueDataTableName( true, "_LPT_%s_%d", pVarName, nMaxElements );
	SendTable *pLengthTable = new SendTable( pLengthProp, 1, pLengthProxyTableName );
	pProps[0] = SendPropDataTable( "lengthproxy", 0, pLengthTable, SendProxy_LengthTable );
	pProps[0].SetExtraData( pExtraData );


	// The first element is a sub-datatable.
	for ( int i = 1; i < nMaxElements+1; i++ )
	{
		pProps[i] = pArrayProp;	// copy array element property setting
		pProps[i].SetOffset( 0 ); // leave offset at 0 so pStructBase is always a pointer to the vector's owner
		pProps[i].m_pVarName = s_ElementNames[i-1];	// give unique name
		pProps[i].SetExtraData( pExtraData );
		pProps[i].m_ElementStride = i-1;	// Kind of lame overloading element stride to hold the element index,
											// but we can easily move it into its SetExtraData stuff if we need to.
		
		// We provide our own proxy here.
		if ( pArrayProp.m_Type == DPT_DataTable )
		{
			pProps[i].SetDataTableProxyFn( SendProxy_UtlVectorElement_DataTable );
			pProps[i].SetFlags( SPROP_PROXY_ALWAYS_YES );
		}
		else
		{
			pProps[i].SetProxyFn( SendProxy_UtlVectorElement );
		}
	}

	SendTable *pTable = new SendTable( 
		pProps, 
		nMaxElements+1, 
		AllocateUniqueDataTableName( true, "_ST_%s_%d", pVarName, nMaxElements )
		);

	ret.SetDataTable( pTable );
	return ret;
}
