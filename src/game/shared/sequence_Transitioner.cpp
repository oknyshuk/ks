// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
import std;
import ks.core;
#if defined(CLIENT_DLL)
import ks.tier1;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "engine/ivmodelinfo.h"
#elif defined(GAME_DLL)
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#endif
#include "sequence_Transitioner.h"

#include "studio.h"

// -----------------------------------------------------------------------------
// CSequenceTransitioner implementation.
// -----------------------------------------------------------------------------

void CSequenceTransitioner::CheckForSequenceChange( 
	CStudioHdr *hdr,
	int nCurSequence, 
	bool bForceNewSequence,
	bool bInterpolate )
{
	// sequence may be set before model is initialized
	if ( hdr == nullptr)
		return;

	// FIXME?: this should detect that what's been asked to be drawn isn't what was expected
	// due to not only sequence change, by frame index, rate, or whatever.  When that happens, 
	// it should insert the previous rules.

	if (m_animationQueue.empty())
	{
		m_animationQueue.emplace_back();
#ifdef CLIENT_DLL
		m_animationQueue[0].SetOwner( nullptr );
#endif
	}

	CAnimationLayer *currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];

	if (currentblend->m_flLayerAnimtime && 
		(currentblend->GetSequence() != nCurSequence || bForceNewSequence ))
	{
		if ( nCurSequence < 0 || nCurSequence >= hdr->GetNumSeq() )
		{
			// remove all entries
			m_animationQueue.clear();
		}
		else
		{
			mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( nCurSequence );
			// sequence changed
			if ((seqdesc.flags & STUDIO_SNAP) || !bInterpolate )
			{
				// remove all entries
				m_animationQueue.clear();
			}
			else
			{
				mstudioseqdesc_t &prevseqdesc = hdr->pSeqdesc( currentblend->GetSequence() );
				currentblend->m_flLayerFadeOuttime = std::min( prevseqdesc.fadeouttime, seqdesc.fadeintime );
				/*
				// clip blends to time remaining
				if ( !IsSequenceLooping(hdr, currentblend->GetSequence()) )
				{
					float length = Studio_Duration( hdr, currentblend->GetSequence(), flPoseParameter ) / currentblend->GetPlaybackRate();
					float timeLeft = (1.0 - currentblend->GetCycle()) * length;
					if (timeLeft < currentblend->m_flLayerFadeOuttime)
						currentblend->m_flLayerFadeOuttime = timeLeft;
				}
				*/
			}
			if ( std::ssize( m_animationQueue ) > 2 )
				m_animationQueue.erase( m_animationQueue.begin(), m_animationQueue.begin() + 1 );
		}
		// push previously set sequence
		m_animationQueue.emplace_back();
		currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];
#ifdef CLIENT_DLL
		currentblend->SetOwner( nullptr );
#endif
	}

	currentblend->SetSequence( -1 );
	currentblend->m_flLayerAnimtime = 0.0;
	currentblend->m_flLayerFadeOuttime = 0.0;
}


void CSequenceTransitioner::UpdateCurrent( 
	CStudioHdr *hdr,
	int nCurSequence, 
	float flCurCycle,
	float flCurPlaybackRate,
	float flCurTime )
{
	// sequence may be set before model is initialized
	if ( hdr == nullptr)
		return;

	if (m_animationQueue.empty())
	{
		m_animationQueue.emplace_back();
#ifdef CLIENT_DLL
		m_animationQueue[0].SetOwner( nullptr );
#endif
	}

	CAnimationLayer *currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];

	// keep track of current sequence
	currentblend->SetSequence( nCurSequence );
	currentblend->m_flLayerAnimtime = flCurTime;
	currentblend->SetCycle( flCurCycle );
	currentblend->SetPlaybackRate( flCurPlaybackRate );

	// calc blending weights for previous sequences
	int i;
	for (i = 0; i < std::ssize( m_animationQueue ) - 1;)
	{
 		float s = m_animationQueue[i].GetFadeout( flCurTime );

		if (s > 0)
		{
			m_animationQueue[i].SetWeight( s );
			i++;
		}
		else
		{
			m_animationQueue.erase( m_animationQueue.begin() + i );
		}
	}
}
