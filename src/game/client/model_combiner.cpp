//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "model_combiner.h"
#include <memory>

struct model_combine_t;
enum combine_result_t
{
	COMBINE_NOT_STARTED,
	COMBINE_IN_PROCESS,
	COMBINE_SUCCEEDED,
	COMBINE_FAILED,
};

static const char *g_pszCombineResults[] =
{
	"not started",		// COMBINE_NOT_STARTED,
	"in process",		// COMBINE_IN_PROCESS,
	"succeeded",		// COMBINE_SUCCEEDED,
	"failed"			// COMBINE_FAILED,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

// Class that manages combining multiple models into single models
class CModelCombiner : public CAutoGameSystem
{
public:
	CModelCombiner();

	// Request a combined model to be created for the base model and the attached additional models.
	// The requester it must derive from IModelCombinerRequesterInterface. The ModelCombineFinished
	// function will be called with the result of the combine request.
	bool	CombineModel( IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels );
	bool	CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );

	// Call this when you're being destroyed. It'll remove you from any pending callback lists.
	void	AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester );


public:
	model_combine_t	*FindCombineRequestForIndex( int iCombineIndex );
	virtual void	LevelShutdownPostEntity( void );
	void	DumpStats( void );

private:
	void	FillOutCombineRequest( model_combine_t *pRequest, IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels );
	void	FillOutCombineRequest( model_combine_t *pRequest, IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine );
	bool	CombineRequestExists( model_combine_t *pRequest, combine_result_t *iResult, int *iIndex );
	void	AddRequesterToCallbackList( int iIndex, IModelCombinerRequesterInterface *pRequester );

private:
	std::vector< std::unique_ptr< model_combine_t > >	m_vecCombines;
	int									m_nNextCombineIndex;
};

CModelCombiner *ModelCombiner( void );


// Struct that contains the state of a single model combine request
struct model_combine_t
{
	model_combine_t& operator=( const model_combine_t& src )
	{
		if ( this == &src )
		{
			return *this;
		}

		nCombinedModelIndex = src.nCombinedModelIndex;
		vecModelsToCombine = src.vecModelsToCombine;
		pRequesterCallbackList = src.pRequesterCallbackList;
		iCombineRequestIndex = src.iCombineRequestIndex;
		iCombineResult = src.iCombineResult;

		return *this;
	}

	bool operator==( const model_combine_t &val ) const 
	{ 
		if ( std::ssize( vecModelsToCombine ) != std::ssize( val.vecModelsToCombine ) )
		{
			return false;
		}
		for ( int i = 0; i < std::ssize( vecModelsToCombine ); i++ )
		{
			if ( ( vecModelsToCombine[ i ].m_iszModelName != val.vecModelsToCombine[i].m_iszModelName ) ||
				 ( vecModelsToCombine[ i ].m_nBodyGroupSubModelSelection != val.vecModelsToCombine[i].m_nBodyGroupSubModelSelection ) )
			{
				return false;
			}
			for ( int j = 0; j < COMBINER_MAX_MATERIALS_PER_INPUT_MODEL; j++ )
			{
				for ( int k = 0; k < COMBINER_MAX_TEXTURES_PER_MATERIAL; k++ )
				{
					if ( ( vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_iszMaterialParam != val.vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_iszMaterialParam ) ||
						 ( vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_pVTFTexture != val.vecModelsToCombine[ i ].m_textureSubstitutes[ j ][ k ].m_pVTFTexture ) )
					{
						return false;
					}
				}
			}
		}

		return true; 
	}

	int														nCombinedModelIndex;
	std::vector< SCombinerModelInput_t >				vecModelsToCombine;
	std::vector< IModelCombinerRequesterInterface* >	pRequesterCallbackList;
	int														iCombineRequestIndex;
	combine_result_t										iCombineResult;
};

CModelCombiner g_ModelCombiner;
CModelCombiner* ModelCombiner() { return &g_ModelCombiner; }

// Request a combined model to be created for the base model and the attached additional models.
// The requester it must derive from IModelCombinerRequesterInterface. The ModelCombineFinished
// function will be called with the result of the combine request.
bool ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const char *pszBaseModel, std::vector< const char * > *vecAdditionalModels )
{
	std::vector< SCombinerModelInput_t > vecModelsToCombine;
	string_t iszBaseModel = AllocPooledString( pszBaseModel );

	vecModelsToCombine.push_back( SCombinerModelInput_t( iszBaseModel ) );
	for ( const char *pszAdditionalModel : *vecAdditionalModels )
	{
		string_t iszAdditionalModel = AllocPooledString( pszAdditionalModel );
		vecModelsToCombine.push_back( SCombinerModelInput_t( iszAdditionalModel ) );
	}
	return ModelCombiner()->CombineModel( pRequester, vecModelsToCombine );
}

bool ModelCombiner_CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	return ModelCombiner()->CombineModel( pRequester, vecModelsToCombine );
}

// Call this when you're being destroyed. It'll remove you from any pending callback lists.
void ModelCombiner_AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester )
{
	ModelCombiner()->AbortCombineModelFor( pRequester );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CModelCombiner::CModelCombiner()
{
	m_nNextCombineIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::FillOutCombineRequest( model_combine_t *pRequest, IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	for ( const SCombinerModelInput_t &modelInput : vecModelsToCombine )
	{
		pRequest->vecModelsToCombine.push_back( modelInput );
	}

	pRequest->pRequesterCallbackList.push_back( pRequester );
	pRequest->iCombineRequestIndex = -1;
	pRequest->nCombinedModelIndex = 0;
	pRequest->iCombineResult = COMBINE_NOT_STARTED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CModelCombiner::CombineRequestExists( model_combine_t *pRequest, combine_result_t *iResult, int *iIndex )
{
	*iResult = COMBINE_NOT_STARTED;
	*iIndex = -1;

	for ( int i = 0; i < std::ssize( m_vecCombines ); i++ )
	{
		if ( *m_vecCombines[i] == *pRequest )
		{
			*iResult = m_vecCombines[i]->iCombineResult;
			*iIndex = i;
			break;
		}
	}

	return (*iIndex != -1);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::AddRequesterToCallbackList( int iIndex, IModelCombinerRequesterInterface *pRequester )
{
	for ( IModelCombinerRequesterInterface *pCallbackRequester : m_vecCombines[iIndex]->pRequesterCallbackList )
	{
		if ( pCallbackRequester == pRequester )
			return;
	}

	m_vecCombines[iIndex]->pRequesterCallbackList.push_back( pRequester );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
model_combine_t	*CModelCombiner::FindCombineRequestForIndex( int iCombineIndex )
{
	for ( auto &pCombine : m_vecCombines )
	{
		if ( pCombine->iCombineRequestIndex == iCombineIndex )
			return pCombine.get();
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::LevelShutdownPostEntity( void )
{
	// Release all the combined models.
	for ( auto &pCombine : m_vecCombines )
	{
		modelinfo->ReleaseDynamicModel( pCombine->nCombinedModelIndex );
	}

	m_vecCombines.clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ModelCombineFinished( void *pUserData, MDLHandle_t OldHandle, MDLHandle_t NewHandle, TCombinedResults &CombinedResults )
{
	int *pCombineIndex = (int *)pUserData;
	if ( !pCombineIndex )
		return;

	model_combine_t	*pCombine = ModelCombiner()->FindCombineRequestForIndex( *pCombineIndex );
	if ( !pCombine )
	{
		Warning("MODELCOMBINER: Received a ModelCombineFinished call with no matching combine request (%d)\n", *pCombineIndex );
		return;
	}

	bool bSucceeded = ( CombinedResults.m_nCombinedResults == COMBINE_RESULT_FLAG_OK );
	if ( bSucceeded )
	{
		DevMsg("MODELCOMBINER: Finishing building combined model for %s.\n", STRING( pCombine->vecModelsToCombine[ 0 ].m_iszModelName ) );
		pCombine->iCombineResult = COMBINE_SUCCEEDED;
		modelinfo->UpdateCombinedDynamicModel( pCombine->nCombinedModelIndex, NewHandle );
	}
	else
	{
		DevMsg("MODELCOMBINER: Failed to build combined model for %s: Error %s (%d)\n", STRING( pCombine->vecModelsToCombine[ 0 ].m_iszModelName ), CombinedResults.m_szErrorMessage, CombinedResults.m_nCombinedResults );
		pCombine->iCombineResult = COMBINE_FAILED;
		modelinfo->ReleaseDynamicModel( pCombine->nCombinedModelIndex );
	}

	// Fire off the callbacks for every requester that wanted to use their combined model
	for ( int i = 0; i < std::ssize( pCombine->pRequesterCallbackList ); i++ )
	{
		pCombine->pRequesterCallbackList[i]->ModelCombineFinished( bSucceeded, pCombine->nCombinedModelIndex );
	}

	pCombine->pRequesterCallbackList.clear(); pCombine->pRequesterCallbackList.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CModelCombiner::CombineModel( IModelCombinerRequesterInterface *pRequester, const std::vector< SCombinerModelInput_t > &vecModelsToCombine )
{
	Assert( std::ssize( vecModelsToCombine ) > 1 );

	auto pOwnedCombine = std::make_unique< model_combine_t >();
	model_combine_t	*pCombine = pOwnedCombine.get();
	FillOutCombineRequest( pCombine, pRequester, vecModelsToCombine );

	DevMsg("MODELCOMBINER: Received combine request for:\n");
	for ( const SCombinerModelInput_t &modelInput : vecModelsToCombine )
	{
		DevMsg("   -> %s\n", STRING( modelInput.m_iszModelName ) );
	}
	DevMsg("  %d models.\n", (int)std::ssize( vecModelsToCombine ) );

	// Have we already tried combining this set of models?
	int iExistingIndex;
	combine_result_t iExistingState;
	if ( CombineRequestExists( pCombine, &iExistingState, &iExistingIndex ) )
	{
		DevMsg("  FOUND existing combine in state: %d\n", iExistingState );

		if ( iExistingState == COMBINE_IN_PROCESS )
		{
			// We're still combining this set. Add ourselves to the callback list.
			AddRequesterToCallbackList( iExistingIndex, pRequester );

			DevMsg("  ADDING to the in-progress callback list.\n" );
		}
		else if ( iExistingState == COMBINE_SUCCEEDED )
		{
			// Tell the requester the combined model is already available.
			pRequester->ModelCombineFinished( true, m_vecCombines[iExistingIndex]->nCombinedModelIndex );
			DevMsg("  RE-USING existing combined model (%d).\n", m_vecCombines[iExistingIndex]->nCombinedModelIndex );
		}

		return ( ( iExistingState == COMBINE_SUCCEEDED ) || ( iExistingState == COMBINE_IN_PROCESS ) );
	}

	// New combine. Add it to our list.
	m_vecCombines.push_back( std::move( pOwnedCombine ) );
	pCombine->iCombineRequestIndex = m_nNextCombineIndex++;
	pCombine->iCombineResult = COMBINE_FAILED;

	// Build our unique combined model name.
	char szCombinedModelName[ 256 ];
	V_sprintf_safe( szCombinedModelName, "%s_c_%d", STRING( vecModelsToCombine[ 0 ].m_iszModelName ), pCombine->iCombineRequestIndex );

	pCombine->nCombinedModelIndex = modelinfo->BeginCombinedModel( szCombinedModelName, true );
	if ( pCombine->nCombinedModelIndex == -1 )
	{
		AssertMsg1( false, "Failed to combine model %s", STRING( vecModelsToCombine[ 0 ].m_iszModelName ) );
		return false;
	}

	if ( !modelinfo->SetCombineModels( pCombine->nCombinedModelIndex, vecModelsToCombine ) )
	{	
		// we failed - invalid handle?
		AssertMsg( "Failed to set combined models for %s!", STRING( vecModelsToCombine[ 0 ].m_iszModelName ) );
		return false;
	}

	DevMsg("  STARTED Combining: final model index will be %d\n", pCombine->nCombinedModelIndex );

	pCombine->iCombineResult = COMBINE_IN_PROCESS;
	modelinfo->FinishCombinedModel( pCombine->nCombinedModelIndex, &ModelCombineFinished, &pCombine->iCombineRequestIndex );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::AbortCombineModelFor( IModelCombinerRequesterInterface *pRequester )
{
	for ( auto &pCombine : m_vecCombines )
	{
		for ( int j = (int)std::ssize( pCombine->pRequesterCallbackList ) - 1; j >= 0; j-- )
		{
			if ( pCombine->pRequesterCallbackList[ j ]  == pRequester )
			{
				pCombine->pRequesterCallbackList.erase( pCombine->pRequesterCallbackList.begin() + j );

				DevMsg("MODELCOMBINER: Removing requester from callback for %s\n", STRING( pCombine->vecModelsToCombine[ 0 ].m_iszModelName ) );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelCombiner::DumpStats( void )
{
	Msg("Model Combiner State\n  %d combined models.\n", (int)std::ssize( m_vecCombines ));
	for ( int i = 0; i < std::ssize( m_vecCombines ); i++ )
	{
		Msg("  %d: Base model %s  (%d attached models)\n", i, STRING( m_vecCombines[ i ]->vecModelsToCombine[ 0 ].m_iszModelName ), (int)std::ssize( m_vecCombines[ i ]->vecModelsToCombine ) - 1 );

		for ( int j = 0; j < std::ssize( m_vecCombines[ i ]->vecModelsToCombine ); j++ )
		{
			if ( j > 0 )
			{
				Msg("       -> %s\n", STRING( m_vecCombines[ i ]->vecModelsToCombine[ j ].m_iszModelName ) );
			}
		}

		Msg("     Combine result:        %s\n", g_pszCombineResults[ m_vecCombines[ i ]->iCombineResult ] );
		Msg("     Combine model index:   %d\n", m_vecCombines[ i ]->nCombinedModelIndex );
		Msg("     Combine request index: %d\n", m_vecCombines[ i ]->iCombineRequestIndex );
		Msg("     Number of requesters:  %d\n", (int)std::ssize( m_vecCombines[ i ]->pRequesterCallbackList ) );
	}
}
