#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2010 Valve Corporation

// Anyone requesting model combines needs to derive from this to get completion callbacks.

#include "datacache/imdlcache.h"

class IModelCombinerRequesterInterface
{
public:
	// Called when a requested model combine has finished.
	virtual void	ModelCombineFinished( bool bSucceeded, int nCombinedModelIndex ) = 0;

	// The CModelCombiner will never call this. It's here to remind you that 
	// you need to call CModelCombiner::AbortCombineModelFor() when you're deleted, so it
	// removes you from any pending callbacks.
	virtual void	ModelCombineAbort( void ) = 0;
};



// Request a combined model to be created for the base model and the attached additional models.
// The requester it must derive from IModelCombinerRequesterInterface. The ModelCombineFinished
// function will be called with the result of the combine request.
bool	ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels );
bool	ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );

// Call this when you're being destroyed. It'll remove you from any pending callback lists.
void	ModelCombiner_AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester );
