// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// IMatRenderContext and IMatRenderContextInternal call the render context implementation the context
// was created as.

#include "cmatrendercontext.h"
#include "cmatqueuedrendercontext.h"

#include "facade/imatrendercontext.inl"
#include "facade/imatrendercontextinternal.inl"
import ks.tier1.exprevaluator;
import ks.tier1.refcount;

int CMatRenderContextBase::Release()
{
	int result = CRefMT::Decrement( &m_iRefs );
	if ( result )
		return result;

	switch ( Kind() )
	{
	case MatRenderContextKind::Hardware:	delete static_cast<CMatRenderContext *>( this ); return 0;
	case MatRenderContextKind::Queued:		delete static_cast<CMatQueuedRenderContext *>( this ); return 0;
	}
	std::unreachable();
}
