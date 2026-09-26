//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_STACK_H
#define DATATABLE_STACK_H


#include <span>
#include "dt.h"
#include "dt_recv_decoder.h"


class CSendNode;
static 	CSendProxyRecipients s_Recipients; // avoid calling constructor each time


// ----------------------------------------------------------------------------- //
//
// CDatatableStack
//
// CDatatableStack is used to walk through a datatable's tree, calling proxies
// along the way to update the current data pointer.
//
// ----------------------------------------------------------------------------- //

abstract_class CDatatableStack
{
public:
	
							CDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID );

	// This must be called before accessing properties.
	void Init( bool bExplicitRoutes, bool bLocalNetworkBackDoor );

	// The stack is meant to be used by calling SeekToProp with increasing property
	// numbers.
	void			SeekToProp( int iProp );

	bool			IsCurProxyValid() const;
	bool			IsPropProxyValid(int iProp ) const;
	int				GetCurPropIndex() const;
	
	unsigned char*	GetCurStructBase() const;
	
	int				GetObjectID() const;

	// Derived classes must implement this. The server gets one and the client gets one.
	// It calls the proxy to move to the next datatable's data.
	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase ) = 0;


public:
	CSendTablePrecalc *m_pPrecalc;
	
	enum
	{
		MAX_PROXY_RESULTS = 64
	};

	// These point at the various values that the proxies returned. They are setup once, then 
	// the properties index them.
	unsigned char *m_pProxies[MAX_PROXY_RESULTS];
	unsigned char *m_pStructBase;
	int m_iCurProp;

protected:

	const SendProp *m_pCurProp;
	
	int m_ObjectID;

	bool m_bInitted;
	bool m_bLocalNetworkBackDoor;
};

inline bool CDatatableStack::IsPropProxyValid(int iProp ) const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[iProp]] != 0;
}

inline bool CDatatableStack::IsCurProxyValid() const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[m_iCurProp]] != 0;
}

inline int CDatatableStack::GetCurPropIndex() const
{
	return m_iCurProp;
}

inline unsigned char* CDatatableStack::GetCurStructBase() const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[m_iCurProp]]; 
}

inline void CDatatableStack::SeekToProp( int iProp )
{
	Assert( m_bInitted );
	
	m_iCurProp = iProp;
	m_pCurProp = m_pPrecalc->GetProp( iProp );
}

inline int CDatatableStack::GetObjectID() const
{
	return m_ObjectID;
}


// "Not routed yet" marker for explicit routes; must match Init's fill.
inline unsigned char *UnroutedProxy()
{
	return reinterpret_cast< unsigned char * >( ~uintptr_t{ 0 } );
}

// For Init( true, ... ): proxies are called lazily, along the current prop's path only.
template< class DTStack, class ProxyCaller >
inline unsigned char* UpdateRoutesExplicit_Template( DTStack *pStack, ProxyCaller *caller )
{
	unsigned short iPropProxyIndex = pStack->m_pPrecalc->m_PropProxyIndices[pStack->m_iCurProp];
	unsigned char **pTest = &pStack->m_pProxies[iPropProxyIndex];
	if ( *pTest != UnroutedProxy() )
		return *pTest;
	
	unsigned char *pStructBase = pStack->m_pStructBase;
	
	CSendTablePrecalc::CProxyPath &proxyPath = pStack->m_pPrecalc->m_ProxyPaths[iPropProxyIndex];
	for ( unsigned short i=0; i < proxyPath.m_nEntries && pStructBase; i++ )
	{
		CSendTablePrecalc::CProxyPathEntry *pEntry = &pStack->m_pPrecalc->m_ProxyPathEntries[proxyPath.m_iFirstEntry + i];
		unsigned char *&slot = pStack->m_pProxies[pEntry->m_iProxy];
		if ( slot == UnroutedProxy() )
			slot = ProxyCaller::CallProxy( pStack, pStructBase, pEntry->m_iDatatableProp );
		pStructBase = slot;
	}
	
	*pTest = pStructBase;
	return pStructBase;
}


// ------------------------------------------------------------------------------------ //
// The datatable stack for a RecvTable.
// ------------------------------------------------------------------------------------ //
class CClientDatatableStack : public CDatatableStack
{
public:
						CClientDatatableStack( CRecvDecoder *pDecoder, unsigned char *pStructBase, int objectID ) :
							CDatatableStack( &pDecoder->m_Precalc, pStructBase, objectID )
						{
							m_pDecoder = pDecoder;
						}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		const RecvProp *pProp = m_pDecoder->GetDatatableProp( iProp );

		void *pVal = nullptr;

		if ( !pProp )
			return nullptr;

		pProp->GetDataTableProxyFn()( 
			pProp,
			&pVal,
			pStructBase + pProp->GetOffset(), 
			GetObjectID()
			);

		return (unsigned char*)pVal;
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[pNode->GetRecursiveProxyIndex()] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = nullptr;
			if ( pStructBase )
			{
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );
			}

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

	class CRecvProxyCaller
	{
	public:
		static inline unsigned char* CallProxy( CClientDatatableStack *pStack, unsigned char *pStructBase, unsigned short iDatatableProp )
		{
			const RecvProp *pProp = pStack->m_pDecoder->GetDatatableProp( iDatatableProp );
			if ( !pProp )
				return nullptr;	// unmatched (demo) table

			void *pVal = nullptr;
			pProp->GetDataTableProxyFn()( 
				pProp,
				&pVal, 
				pStructBase + pProp->GetOffset(), 
				pStack->m_ObjectID
				);
				
			return (unsigned char*)pVal;
		}
	};
	
	inline unsigned char* UpdateRoutesExplicit()
	{
		return UpdateRoutesExplicit_Template( this, (CRecvProxyCaller*)nullptr );
	}
			

public:
	
	CRecvDecoder	*m_pDecoder;
};


class CServerDatatableStack : public CDatatableStack
{
public:
						CServerDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID, std::span< CSendProxyRecipients > recipients = {} ) :
							CDatatableStack( pPrecalc, pStructBase, objectID )
						{
							m_pPrecalc = pPrecalc;
							m_Recipients = recipients;
						}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		const SendProp *pProp = m_pPrecalc->GetDatatableProp( iProp );

		CSendProxyRecipients *pRecipients;

		if ( !m_Recipients.empty() && pNode->GetDataTableProxyIndex() != DATATABLE_PROXY_INDEX_NOPROXY )
		{
			// set recipients pointer and all clients by default
			pRecipients = &m_Recipients[ pNode->GetDataTableProxyIndex() ];
			pRecipients->SetAllRecipients();
		}
		else
		{
			// we don't care about recipients, just provide a valid pointer
			pRecipients = &s_Recipients; 
		}

		unsigned char *pRet = (unsigned char*)pProp->GetDataTableProxyFn()( 
			pProp,
			pStructBase, 
			pStructBase + pProp->GetOffset(), 
			pRecipients,
			GetObjectID()
			);

		if ( m_bLocalNetworkBackDoor && (pRecipients != &s_Recipients) && !pRecipients->m_Bits[0] )
			return nullptr;
	
		return pRet;
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[pNode->GetRecursiveProxyIndex()] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = nullptr;
			if ( pStructBase )
			{
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );
			}

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

	// This can be used IF you called Init() with true for bExplicitRoutes.
	// It is faster to use this route if you only are going to ask for a couple props.
	// If you're going to ask for all the props, then you shouldn't use the "explicit" route.
	class CSendProxyCaller
	{
	public:
		static inline unsigned char* CallProxy( CServerDatatableStack *pStack, unsigned char *pStructBase, unsigned short iDatatableProp )
		{
			const SendProp *pProp = pStack->m_pPrecalc->GetDatatableProp( iDatatableProp );

			CSendProxyRecipients *pRecipients;

			if ( pStack->m_bLocalNetworkBackDoor && !pStack->m_Recipients.empty() )
			{
				// set recipients pointer and all clients by default
				pRecipients = &pStack->m_Recipients[ 0 ];//pNode->GetDataTableProxyIndex() );
				pRecipients->SetAllRecipients();
			}
			else
			{
				// we don't care about recipients, just provide a valid pointer
				pRecipients = &s_Recipients; 
			}
			
			unsigned char *pRet = (unsigned char*)pProp->GetDataTableProxyFn()( 
				pProp,
				pStructBase, 
				pStructBase + pProp->GetOffset(), 
				pRecipients,
				pStack->GetObjectID()
				);

			if ( pStack->m_bLocalNetworkBackDoor && (pRecipients != &s_Recipients) && !pRecipients->m_Bits[0] )
				return nullptr;

			return pRet;
		}
	};
	
	inline unsigned char* UpdateRoutesExplicit()
	{
		return UpdateRoutesExplicit_Template( this, (CSendProxyCaller*)nullptr );
	}

	
	const SendProp*	GetCurProp() const;


public:
	
	CSendTablePrecalc					*m_pPrecalc;
	std::span<CSendProxyRecipients>	m_Recipients;
};


inline const SendProp* CServerDatatableStack::GetCurProp() const
{
	return m_pPrecalc->GetProp( GetCurPropIndex() );
}


#endif // DATATABLE_STACK_H
