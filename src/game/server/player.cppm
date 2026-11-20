// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) 2026 oknyshuk
// Copyright (c) Valve Corporation

module;
#include <time.h>
#include <ihltv.h>
#include <ihltvdirector.h>
#include "../../public/responserules/response_types.h"
#include "ai_criteria.h"
#include "responserules/rr_speechconcept.h"
#include "edict.h"
#include "responserules/response_types.h"
#include "declare_class.h"
#include "irecipientfilter.h"
#include "ai_speech.h"
#include "iservernetworkable.h"
#include "reflect_annotations.h"
#include "soundflags.h"
#include "string_t.h"
#include "const.h"
#include "vscript/ivscript.h"
#include "baseentity_shared.h"
#include "vscript_game.h"
#include "shattersurfacetypes.h"
#include "igameevents.h"
#include "eiface.h"
#include "game/server/iplayerinfo.h"
#include "input_device.h"
#include "networkvar.h"
#include "server_class.h"
#include "vphysics/player_controller.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "shareddefs.h"
#include "vscript_shared.h"
#include "studio.h"
#include "dt_recv.h"
#include "predictable_entity.h"
module ks.server:player;

import :baseentity;
import std;
import ks.math.ssequaternion;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import :vscript;
import ks.tier1.map_ops;
import ks.tier1.convar;
import ks.game.shareddefs;
import ks.core.numeric;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.core.basetypes;
import ks.tier1.exprevaluator;
import ks.tier1.keyvalues;
import :sharedvar;
import ks.tier1.icvar;
import ks.game.igamemovement;
import ks.core.threadtools;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.vector2d;
import ks.tier1.playerid;
import ks.game.ai_activity;
import ks.game.entitycore;
import :fish;
import ks.tier1.envelope;
import ks.tier1.strtools;
import :cstrike15.cs_playeranimstate;

using ResponseRules::ResponseType_t;

// Defined by the response rules library, which is shared with the other side.

#ifndef AI_CriteriaSet
#endif

typedef ResponseRules::ResponseParams	AI_ResponseParams ;
typedef ResponseRules::CRR_Response		AI_Response;

/*
// An AI response that is dynamically new'ed up and returned from SpeakFindResponse.
class AI_ResponseReturnValue : AI_Response
{

};
*/

// Purpose: Class data for an AI Concept, an atom of response-driven dialog.

class CAI_Concept : public ResponseRules::CRR_Concept
{
public:
	CAI_Concept() {};
	// construct concept from a string.
	CAI_Concept(const char *fromString) : CRR_Concept(fromString) {} ;

	// get/set BS
	inline EHANDLE GetSpeaker() const { return m_hSpeaker; }
	inline void SetSpeaker(EHANDLE val) { m_hSpeaker = val; }

	/*
	inline EHANDLE GetTarget() const { return m_hTarget; }
	inline void SetTarget(EHANDLE val) { m_hTarget = val; }
	inline EHANDLE GetTopic() const { return m_hTopic; }
	inline void SetTopic(EHANDLE val) { m_hTopic = val; }
	*/

protected:
	EHANDLE m_hSpeaker;

	/*
	EHANDLE m_hTarget;
	EHANDLE m_hTopic;
	*/
};

typedef CAI_Concept AIConcept_t;

class CBaseEntity;

using namespace ::ks::reflect;


// Purpose: An event queue of AI concepts that dispatches them to appropriate characters.

inline constexpr auto AI_RESPONSE_QUEUE_SIZE = 64;

enum DeferredResponseTarget_t // possible targets for a deferred response
{
	kDRT_ANY, // best matching respondent within range -- except for the one in the m_hTarget handle
	kDRT_ALL, // send to everyone in range -- except for the one in the m_hTarget handle
	kDRT_SPECIFIC, // a specific entity is targeted

	kDRT_MAX, // high water mark
};

// Allows you to postpone AI speech concepts to a later time, or to direct them to 
// a specific character, or all of them.
class CResponseQueue
{
	//////////////////// Local types ////////////////////
public: 

	// We pack up contexts to send along with the concept.
	// For now I'll just copy criteria sets, but it will be better to do something
	// more efficient in the future.
	typedef AI_CriteriaSet DeferredContexts_t; 

	struct CFollowupTargetSpec_t ///< to whom a followup is directed. Can be a specific entity or something more exotic.
	{
		DeferredResponseTarget_t m_iTargetType; ///< ANY, ALL, or SPECIFIC. If specific, pass through a handle to:
		EHANDLE	m_hHandle; ///< a specific target for the message, or a specific character to OMIT. 
		inline bool IsValid( void ) const;

		// constructors/destructors
		explicit CFollowupTargetSpec_t(const DeferredResponseTarget_t &targetType, const EHANDLE &handle)
			: m_iTargetType(targetType), m_hHandle(handle) 
		{};
		explicit CFollowupTargetSpec_t(const EHANDLE &handle)
			: m_iTargetType(kDRT_SPECIFIC), m_hHandle(handle) 
		{};
		CFollowupTargetSpec_t(DeferredResponseTarget_t target) // eg, ANY, ALL, etc. 
			: m_iTargetType(target)  
		{	
			contract_assert( ( m_iTargetType != kDRT_SPECIFIC ) && "Response rule followup tried to specify an entity target, but didn't provide the target.\n" ); 
		}
		CFollowupTargetSpec_t(void) // default: invalid
			: m_iTargetType(kDRT_MAX)
		{};
	};

	/// A single deferred response.
	struct CDeferredResponse
	{
		AIConcept_t m_concept;
		DeferredContexts_t m_contexts; ///< contexts to send along with the concept
		float m_fDispatchTime;
		EHANDLE m_hIssuer; ///< an entity, if issued by an entity
		/*
		DeferredResponseTarget_t m_iTargetType;
		EHANDLE m_hTarget; // May be invalid.
		*/
		CFollowupTargetSpec_t m_Target;

		inline void Init( const AIConcept_t &conc, const AI_CriteriaSet * __restrict__ contexts, float dtime, const CFollowupTargetSpec_t &target, CBaseEntity *pIssuer );
		inline bool IsQuashed() { return !m_Target.IsValid(); }
		void Quash(); ///< make this response invalid.
	};
	/// write
	static void DeferContextsFromCriteriaSet( DeferredContexts_t &contextsOut, const AI_CriteriaSet *criteriaIn );

	//////////////////// Methods ////////////////////
public:
	CResponseQueue( int queueSize );

	/// Add a deferred response.
	void Add( const AIConcept_t &conc,  ///< concept to dispatch
			  const AI_CriteriaSet * __restrict__ contexts, ///< the contexts that come with it (may be NULL)
			  float time,					 ///< when to dispatch it. You can specify a time of zero to mean "immediately."
			  const CFollowupTargetSpec_t &targetspec, /// All information necessary to target this response
			  CBaseEntity *pIssuer = nullptr ///< the entity who should not respond if this is a ANY or ALL rule. (eg, don't let people talk to themselves.)
			  );

	/// Remove all deferred responses matching the concept and issuer.
	void Remove( const AIConcept_t &conc,  ///< concept to dispatch
		CBaseEntity * const pIssuer = nullptr ///< the entity issuing the response, if one exists.
		) __restrict__;

	/// Remove all deferred responses queued to be spoken by given character
	void RemoveSpeechQueuedFor( const CBaseEntity *pSpeaker	);
	
    /// Empty out all pending events
	void Evacuate();

	/// Go through and dispatch any deferred responses.
	void PerFrameDispatch();

	/// Add an expressor owner to this queue.
	void AddExpresserHost(CBaseEntity *host);

	/// Remove an expresser host from this queue.
	void RemoveExpresserHost(CBaseEntity *host);

	/// Iterate over potential expressers for this queue
	inline int GetNumExpresserTargets() const;
	inline CBaseEntity *GetExpresserHost(int which) const;

protected:
	/// Actually send off one response to a consumer
	/// Return true if dispatch succeeded
	bool DispatchOneResponse( CDeferredResponse &response );

private:
	/// Helper function for one case in DispatchOneResponse
	/// (for better organization)
	bool DispatchOneResponse_ThenANY( CDeferredResponse &response, AI_CriteriaSet * __restrict__ pDeferredCriteria, CBaseEntity *  const __restrict__ pIssuer, float followupMaxDistSq );

	//////////////////// Data ////////////////////
protected:
	typedef std::list< CDeferredResponse > QueueType_t;
	QueueType_t m_Queue; // the queue of deferred responses, will eventually be sorted
	/// Note  about the queue type: if you move to replace it with a sorted priority queue,
	/// make sure it is a type such that an iterator is not invalidated by inserts and deletes.
	/// CResponseQueue::PerFrameDispatch() iterates over the queue calling DispatchOneResponse
	/// on each in turn, and those responses may very easily add new events to the queue. 
	/// A crash will result if the iterator used in CResponseQueue::PerFrameDispatch()'s loop
	/// becomes invalid.

	std::vector< EHANDLE > m_ExpresserTargets; // a list of legitimate expresser targets
};

void CResponseQueue::CDeferredResponse::Init(const AIConcept_t &conc, const AI_CriteriaSet * __restrict__ contexts, float dtime, const CFollowupTargetSpec_t &target, CBaseEntity *pIssuer )
{
	m_concept = conc;
	m_fDispatchTime = dtime;
	/*
	m_iTargetType = targetType;
	m_hTarget =  handle ;
	*/
	m_Target = target;
	m_hIssuer = pIssuer;
	DeferContextsFromCriteriaSet(m_contexts, contexts);
}

int CResponseQueue::GetNumExpresserTargets() const
{
	return std::ssize( m_ExpresserTargets );
}

CBaseEntity *CResponseQueue::GetExpresserHost(int which) const
{
	return m_ExpresserTargets[which];
}

// The wrapper game system that contains a response queue, and ticks it each frame.

class CResponseQueueManager : public CAutoGameSystemPerFrame
{
public:
	CResponseQueueManager(char const *name) : CAutoGameSystemPerFrame( name )
	{
		m_pQueue = nullptr;
	}
	virtual ~CResponseQueueManager(void);
	virtual void Shutdown();
	virtual void FrameUpdatePostEntityThink( void );
	virtual void LevelInitPreEntity( void );

	inline CResponseQueue *GetQueue(void) { contract_assert( m_pQueue ); return m_pQueue; }

protected:
	CResponseQueue *m_pQueue;
};

// Valid if the target type enum is within bounds. Furthermore if it 
// specifies a specific entity, that handle must be valid.
bool CResponseQueue::CFollowupTargetSpec_t::IsValid( void ) const
{
	if (m_iTargetType >= kDRT_MAX) 
		return false;
	if (m_iTargetType < 0) 
		return false;
	if (m_iTargetType == kDRT_SPECIFIC && !m_hHandle.IsValid())
		return false;

	return true;
}

extern CResponseQueueManager g_ResponseQueueManager;

// Handy global helper funcs

/// Automatically queue up speech to happen immediately -- calls straight through to response rules add
inline void QueueSpeak( const AIConcept_t &conc,					///< concept name to say
					    const CResponseQueue::CFollowupTargetSpec_t& targetspec,	///< kDRT_ANY, kDRT_ALL, etc
						CBaseEntity *pIssuer = nullptr					///< if specifying ANY or ALL, use this to specify the one you *don't* want to speak
						)
{
	return g_ResponseQueueManager.GetQueue()->Add( conc, nullptr, 0.0f, targetspec, pIssuer );
}

/// Automatically queue up speech to happen immediately -- calls straight through to response rules add
inline void QueueSpeak( const AIConcept_t &conc,					///< concept name to say
					    const CResponseQueue::CFollowupTargetSpec_t& targetspec,	///< kDRT_ANY, kDRT_ALL, etc
						const AI_CriteriaSet &criteria,				///< criteria to pass in
					    CBaseEntity *pIssuer = nullptr					///< if specifying ANY or ALL, use this to specify the one you *don't* want to speak
					   )
{
	return g_ResponseQueueManager.GetQueue()->Add( conc, &criteria, 0.0f, targetspec, pIssuer );
}

/// Automatically queue up speech to happen immediately -- calls straight through to response rules add
inline void QueueSpeak( const AIConcept_t &conc,					///< concept name to say
					   const EHANDLE &target,						///< which entity shall speak
					   float delay,									///< how far in the future to speak
					   const AI_CriteriaSet &criteria,				///< criteria to pass in
					   CBaseEntity *pIssuer = nullptr )
{
	return g_ResponseQueueManager.GetQueue()->Add( conc, &criteria, gpGlobals->curtime + delay,
		CResponseQueue::CFollowupTargetSpec_t(target), pIssuer );
}

class CAI_Expresser;

class CBaseEntity;
class CBaseFlex;

using namespace ::ks::reflect;


using ResponseRules::ResponseType_t;
using ResponseRules::AI_ResponseFollowup;

//-----------------------------------------------------------------------------
// Purpose: Used to share a global resource or prevent a system stepping on
//			own toes.
//-----------------------------------------------------------------------------

class CAI_TimedSemaphore
{
public:
	CAI_TimedSemaphore()
	 :	m_ReleaseTime( 0 )
	{
		m_hCurrentTalker = nullptr;
	}

	void Acquire( float time, CBaseEntity *pTalker )		{ m_ReleaseTime = gpGlobals->curtime + time; m_hCurrentTalker = pTalker; }
	void Release()					{ m_ReleaseTime = 0; m_hCurrentTalker = nullptr; }

	// Current owner of the semaphore is always allowed to talk
	bool IsAvailable( CBaseEntity *pTalker ) const		{ return ((gpGlobals->curtime > m_ReleaseTime) || (m_hCurrentTalker == pTalker)); }
	float GetReleaseTime() const 	{ return m_ReleaseTime; }

	CBaseEntity *GetOwner()	{ return m_hCurrentTalker; }

private:
	float		m_ReleaseTime;
	EHANDLE		m_hCurrentTalker;
};

//-----------------------------------------------------------------------------

extern CAI_TimedSemaphore g_AIFriendliesTalkSemaphore;
extern CAI_TimedSemaphore g_AIFoesTalkSemaphore;

//-----------------------------------------------------------------------------
// Basic speech system types
//-----------------------------------------------------------------------------

//-------------------------------------
// Constants

const float AIS_NO_DELAY  		= 0;
const soundlevel_t AIS_DEF_SNDLVL 	 	= SNDLVL_TALKING;
inline constexpr auto AI_NULL_CONCEPT = nullptr;

inline constexpr auto AI_NULL_SENTENCE = nullptr;

// Sentence prefix constants
inline constexpr auto AI_SP_SPECIFIC_SENTENCE = '!';
inline constexpr auto AI_SP_WAVFILE = '^';
inline constexpr auto AI_SP_SCENE_GROUP = '=';
inline constexpr auto AI_SP_SPECIFIC_SCENE = '?';

// @Note (toml 09-12-02): as scene groups are not currently implemented, the string is a semi-colon delimited list

// Designer overriding modifiers

//-------------------------------------

//-------------------------------------
// An id that represents the core meaning of a spoken phrase,
// eventually to be mapped to a sentence group or scene

inline bool CompareConcepts( AIConcept_t c1, AIConcept_t c2 )
{
	return c1.m_iConcept == c2.m_iConcept;
}

//-----------------------------------------------------------------------------
// CAI_Expresser
//
// Purpose: Provides the functionality of going from abstract concept ("hello")
//			to specific sentence/scene/wave
//

//-------------------------------------
// Sink supports behavior control and receives notifications of internal events

class CAI_ExpresserSink
{
public:
	virtual void OnSpokeConcept( AIConcept_t conc, AI_Response *response )	{};
	virtual void OnStartSpeaking()						{}
	virtual bool UseSemaphore()							{ return true; }
};

struct ConceptHistory_t
{
	DECLARE_SIMPLE_DATADESC();

	ConceptHistory_t(float timeSpoken = -1 )
	 : timeSpoken( timeSpoken ), m_response( )
	{
	}

	ConceptHistory_t( const ConceptHistory_t& src );
	ConceptHistory_t& operator = ( const ConceptHistory_t& src );

	~ConceptHistory_t();

	float		timeSpoken;
	AI_Response m_response;
};
//-------------------------------------

class CAI_Expresser : public ResponseRules::IResponseFilter
{
public:
	CAI_Expresser( CBaseFlex *pOuter = nullptr );
	~CAI_Expresser();

	// --------------------------------

	bool Connect( CAI_ExpresserSink *pSink )		{ m_pSink = pSink; return true; }
	bool Disconnect( CAI_ExpresserSink *pSink )	{ m_pSink = nullptr; return true;}

	void TestAllResponses();

	// --------------------------------

	bool Speak( AIConcept_t &conc, const char *modifiers = nullptr, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );
	bool Speak( AIConcept_t &conc, AI_CriteriaSet *criteria, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );

	// Given modifiers (which are colon-delimited strings), fill out a criteria set including this
	// character's contexts and the ones in the modifier. This lets us hang on to them after a call
	// to SpeakFindResponse.
	void GatherCriteria( AI_CriteriaSet *outputCritera, const AIConcept_t &conc, const char *modifiers );
	// These two methods allow looking up a response and dispatching it to be two different steps
	// AI_Response *SpeakFindResponse( AIConcept_t conc, const char *modifiers = NULL );
	// AI_Response *SpeakFindResponse( AIConcept_t &conc, AI_CriteriaSet *criteria );
	// Find the appropriate response for the given concept. Return false if none found.
	// Fills out the response object that you provide.
	bool FindResponse( AI_Response &outResponse, AIConcept_t &conc, AI_CriteriaSet *modifiers = nullptr );
	virtual bool SpeakDispatchResponse( AIConcept_t &conc, AI_Response *response, AI_CriteriaSet *criteria, IRecipientFilter *filter = nullptr );
	float GetResponseDuration( AI_Response *response );

	virtual int SpeakRawSentence( const char *pszSentence, float delay, float volume = VOL_NORM, soundlevel_t soundlevel = SNDLVL_TALKING, CBaseEntity *pListener = nullptr );

	bool SemaphoreIsAvailable( CBaseEntity *pTalker );
	float GetSemaphoreAvailableTime( CBaseEntity *pTalker );

	virtual void OnSpeechFinished() {};

	// This function can be overriden by games to suppress speech altogether during glue screens, etc
	static bool IsSpeechGloballySuppressed();

	// --------------------------------

	virtual bool IsSpeaking();
	bool CanSpeak();
	bool CanSpeakAfterMyself();
	float GetTimeSpeechComplete() const 	{ return m_flStopTalkTime; }
	void  BlockSpeechUntil( float time );

	// --------------------------------

	bool CanSpeakConcept( AIConcept_t conc );
	bool SpokeConcept( AIConcept_t conc );
	float GetTimeSpokeConcept( AIConcept_t conc ); // returns -1 if never
	void SetSpokeConcept( AIConcept_t conc, AI_Response *response, bool bCallback = true );
	void ClearSpokeConcept( AIConcept_t conc );

	// --------------------------------

	void SetVoicePitch( int voicePitch )	{ m_voicePitch = voicePitch; }
	int GetVoicePitch() const;

	void NoteSpeaking( float duration, float delay = 0 );

	// Force the NPC to release the semaphore & clear next speech time
	void ForceNotSpeaking( void );

	// helper used in dealing with RESPONSE_ENTITYIO
	// response is the output of AI_Response::GetName
	// note: the response string will get stomped on (by strtok)
	// returns false on failure (eg, couldn't match parse contents)
	static bool FireEntIOFromResponse( char *response, CBaseEntity *pInitiator );

protected:
	CAI_TimedSemaphore *GetMySpeechSemaphore( CBaseEntity *pNpc );

	bool SpeakRawScene( const char *pszScene, float delay, AI_Response *response, IRecipientFilter *filter = nullptr );
	// This will create a fake .vcd/CChoreoScene to wrap the sound to be played
	bool SpeakAutoGeneratedScene( char const *soundname, float delay );

	void DumpHistories();

	void SpeechMsg( CBaseEntity *pFlex, const char *pszFormat, ... );

	// --------------------------------

	CAI_ExpresserSink *GetSink() { return m_pSink; }

private:
	// --------------------------------

	virtual bool IsValidResponse( ResponseType_t type, const char *pszValue );

	// --------------------------------

	CAI_ExpresserSink *m_pSink;

	// --------------------------------
	//
	// Speech concept data structures
	//

	ks::idict< ConceptHistory_t > m_ConceptHistories;

	// --------------------------------
	//
	// Speaking states
	//

	float				m_flStopTalkTime;				// when in the future that I'll be done saying this sentence.
	float				m_flStopTalkTimeWithoutDelay;	// same as the above, but minus the delay before other people can speak
	float				m_flBlockedTalkTime;
	int					m_voicePitch;					// pitch of voice for this head
	float				m_flLastTimeAcceptedSpeak;		// because speech may not be blocked until NoteSpeaking called by scene ent, this handles in-think blocking

	DECLARE_SIMPLE_DATADESC();

	// --------------------------------
	//
public:
	void SetOuter( CBaseFlex *pOuter );

	CBaseFlex *		GetOuter() 			{ return m_pOuter; }
	const CBaseFlex *	GetOuter() const 	{ return m_pOuter; }

private:
	CHandle<CBaseFlex>	m_pOuter;
};

//-----------------------------------------------------------------------------
//
// An NPC base class to assist a branch of the inheritance graph
// in utilizing CAI_Expresser
//

template <class BASE_NPC>
class CAI_ExpresserHost : public BASE_NPC, protected CAI_ExpresserSink
{
	DECLARE_CLASS_NOFRIEND( CAI_ExpresserHost, BASE_NPC );

public:
	virtual void	NoteSpeaking( float duration, float delay );

	virtual bool 	Speak( AIConcept_t conc, const char *modifiers = nullptr, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );
	virtual bool 	Speak( AIConcept_t conc, AI_CriteriaSet *pCriteria, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );

	void GatherCriteria( AI_CriteriaSet *outputCritera, const AIConcept_t &conc, const char *modifiers );
	// These two methods allow looking up a response and dispatching it to be two different steps
	// AI_Response *	SpeakFindResponse( AIConcept_t conc, const char *modifiers = NULL );
	// AI_Response *SpeakFindResponse( AIConcept_t conc, AI_CriteriaSet *criteria );
	// AI_Response *SpeakFindResponse( AIConcept_t conc );
	// Find the appropriate response for the given concept. Return false if none found.
	// Fills out the response object that you provide.
	bool FindResponse( AI_Response &outResponse, AIConcept_t &conc, AI_CriteriaSet *criteria = nullptr );

	bool 			SpeakDispatchResponse( AIConcept_t conc, AI_Response *response,  AI_CriteriaSet *criteria = nullptr );
	virtual void	PostSpeakDispatchResponse( AIConcept_t conc, AI_Response *response ) { return; }
	float 			GetResponseDuration( AI_Response *response );

	float GetTimeSpeechComplete() const 	{ return this->GetExpresser()->GetTimeSpeechComplete(); }

	bool IsSpeaking()				{ return this->GetExpresser()->IsSpeaking(); }
	bool CanSpeak()					{ return this->GetExpresser()->CanSpeak(); }
	bool CanSpeakAfterMyself()		{ return this->GetExpresser()->CanSpeakAfterMyself(); }

	void SetSpokeConcept( AIConcept_t conc, AI_Response *response, bool bCallback = true ) 		{ this->GetExpresser()->SetSpokeConcept( conc, response, bCallback ); }
	float GetTimeSpokeConcept( AIConcept_t conc )												{ return this->GetExpresser()->GetTimeSpokeConcept( conc ); }
	bool SpokeConcept( AIConcept_t conc )														{ return this->GetExpresser()->SpokeConcept( conc ); }

protected:
	int 			PlaySentence( const char *pszSentence, float delay, float volume = VOL_NORM, soundlevel_t soundlevel = SNDLVL_TALKING, CBaseEntity *pListener = nullptr );
	public: void	ModifyOrAppendCriteria_Impl( AI_CriteriaSet& set );
	protected:

	public: ResponseRules::IResponseSystem *GetResponseSystem_Impl();
	protected:
	// Override of base entity response input handler
	public: void	DispatchResponse_Impl( const char *conceptName );
	protected:
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline void CAI_ExpresserHost<BASE_NPC>::NoteSpeaking( float duration, float delay )
{
	this->GetExpresser()->NoteSpeaking( duration, delay );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline bool CAI_ExpresserHost<BASE_NPC>::Speak( AIConcept_t conc, const char *modifiers /*= NULL*/, char *pszOutResponseChosen /*=NULL*/, std::size_t bufsize /* = 0 */, IRecipientFilter *filter /* = NULL */ )
{
	contract_assert( this->GetExpresser()->GetOuter() == this );
	return this->GetExpresser()->Speak( conc, modifiers, pszOutResponseChosen, bufsize, filter );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline bool CAI_ExpresserHost<BASE_NPC>::Speak( AIConcept_t conc, AI_CriteriaSet *pCriteria, char *pszOutResponseChosen /*=NULL*/, std::size_t bufsize /* = 0 */, IRecipientFilter *filter /* = NULL */ )
{
	contract_assert( this->GetExpresser()->GetOuter() == this );
	CAI_Expresser * const __restrict__ pExpresser = this->GetExpresser();
	conc.SetSpeaker( this );
	// add in any local criteria to the one passed on the command line.
	pExpresser->GatherCriteria( pCriteria, conc, nullptr );
	// call the "I have aleady gathered criteria" version of Expresser::Speak
	return pExpresser->Speak( conc, pCriteria, pszOutResponseChosen, bufsize, filter );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline int CAI_ExpresserHost<BASE_NPC>::PlaySentence( const char *pszSentence, float delay, float volume, soundlevel_t soundlevel, CBaseEntity *pListener )
{
	return this->GetExpresser()->SpeakRawSentence( pszSentence, delay, volume, soundlevel, pListener );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template <class BASE_NPC>
inline void CAI_ExpresserHost<BASE_NPC>::ModifyOrAppendCriteria_Impl( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria_Impl( criteriaSet );

	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
extern ResponseRules::IResponseSystem *g_pResponseSystem;
template <class BASE_NPC>
inline ResponseRules::IResponseSystem *CAI_ExpresserHost<BASE_NPC>::GetResponseSystem_Impl()
{
	// Expressive NPC's use the general response system
	return g_pResponseSystem;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline void CAI_ExpresserHost<BASE_NPC>::GatherCriteria( AI_CriteriaSet *outputCriteria, const AIConcept_t &conc, const char *modifiers )
{
	return this->GetExpresser()->GatherCriteria( outputCriteria, conc, modifiers );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline bool CAI_ExpresserHost<BASE_NPC>::FindResponse( AI_Response &outResponse, AIConcept_t &conc, AI_CriteriaSet *criteria )
{
	return this->GetExpresser()->FindResponse( outResponse, conc, criteria );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline bool CAI_ExpresserHost<BASE_NPC>::SpeakDispatchResponse( AIConcept_t conc, AI_Response *response,  AI_CriteriaSet *criteria )
{
	if ( this->GetExpresser()->SpeakDispatchResponse( conc, response, criteria ) )
	{
		PostSpeakDispatchResponse( conc, response );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline float CAI_ExpresserHost<BASE_NPC>::GetResponseDuration( AI_Response *response )
{
	return this->GetExpresser()->GetResponseDuration( response );
}

//-----------------------------------------------------------------------------
// Override of base entity response input handler
//-----------------------------------------------------------------------------
template <class BASE_NPC>
inline void CAI_ExpresserHost<BASE_NPC>::DispatchResponse_Impl( const char *conceptName )
	{
		Speak( (AIConcept_t)conceptName );
	}

//-----------------------------------------------------------------------------

/// A shim under CAI_ExpresserHost you can use when deriving a new expresser
/// host type under CAI_BaseNPC. This does the extra step of declaring an m_pExpresser
/// member and initializing it from CreateComponents(). If your BASE_NPC class isn't
/// actually an NPC, then CreateComponents() never gets called and you won't have
/// an expresser created.
/// Note: you still need to add m_pExpresser to the Datadesc for your derived type.
/// This is because I couldn't figure out how to make a templatized datadesc declaration
/// that works generically on the template type.
template <class BASE_NPC, class EXPRESSER_TYPE>
class CAI_ExpresserHostWithData : public CAI_ExpresserHost<BASE_NPC>
{
	DECLARE_CLASS_NOFRIEND( CAI_ExpresserHostWithData, CAI_ExpresserHost<BASE_NPC> );

public:
	CAI_ExpresserHostWithData( ) : m_pExpresser(nullptr) {};

	virtual CAI_Expresser *GetExpresser() { return m_pExpresser; }
	const CAI_Expresser *GetExpresser() const { return m_pExpresser; }

	virtual bool 			CreateComponents()
	{
		return BaseClass::CreateComponents() &&	( CreateExpresser() != nullptr );
	}

protected:
	EXPRESSER_TYPE *CreateExpresser( void )
	{
		contract_assert( ( m_pExpresser == nullptr ) && "Tried to double-initialize expresser in %s\n" );
		m_pExpresser = new EXPRESSER_TYPE(this);
		if ( !m_pExpresser)
		{
			contract_assert( false && "Creating an expresser failed in %s\n" );
			return nullptr;
		}

		m_pExpresser->Connect(this);
		return m_pExpresser;
	}

	virtual ~CAI_ExpresserHostWithData( void )
	{
		delete m_pExpresser;
		m_pExpresser = nullptr;
	}

	EXPRESSER_TYPE *m_pExpresser;
};

/// response rules
namespace RR
{
	/// some applycontext clauses have operators preceding them,
	/// like ++1 which means "take the current value and increment it
	/// by one". These classes detect these cases and do the appropriate
	/// thing.
	class CApplyContextOperator
	{
	public:
		inline CApplyContextOperator( int nSkipChars ) : m_nSkipChars(nSkipChars) {};

		/// perform whatever this operator does upon the given context value.
		/// Default op is simply to copy old to new.
		/// pOldValue should be the currently set value of the context. May be NULL meaning no prior value.
		/// pOperator the value that applycontext says to set
		/// pNewValue a pointer to a buffer where the real new value will be writ.
		/// returns true on success; false on failure (eg, tried to increment a
		/// non-numeric value).
		virtual bool Apply( const char *pOldValue, const char *pOperator, char *pNewValue, int pNewValBufSize );

		/// This is the function that should be called from outside,
		/// fed the input string, it'll select the right operator
		/// to apply.
		static CApplyContextOperator *FindOperator( const char *pContextString );

	protected:
		int m_nSkipChars; // how many chars to "skip" in the value string to get past the op specifier to the actual value
						  // eg, "++3" has a m_nSkipChars of 2, because the op string "++" is two characters.
	};

	class CIncrementOperator : public CApplyContextOperator
	{
	public:
		inline CIncrementOperator( int nSkipChars ) : CApplyContextOperator(nSkipChars) {};
		virtual bool Apply( const char *pOldValue, const char *pOperator, char *pNewValue, int pNewValBufSize );
	};

	class CDecrementOperator : public CApplyContextOperator
	{
	public:
		inline CDecrementOperator( int nSkipChars ) : CApplyContextOperator(nSkipChars) {};
		virtual bool Apply( const char *pOldValue, const char *pOperator, char *pNewValue, int pNewValBufSize );
	};

	class CToggleOperator : public CApplyContextOperator
	{
	public:
		inline CToggleOperator( int nSkipChars ) : CApplyContextOperator(nSkipChars) {};
		virtual bool Apply( const char *pOldValue, const char *pOperator, char *pNewValue, int pNewValBufSize );
	};

	// the singleton operators
	extern CApplyContextOperator sm_OpCopy;
	extern CIncrementOperator sm_OpIncrement;
	extern CDecrementOperator sm_OpDecrement;
	extern CToggleOperator	  sm_OpToggle;
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// A kind of AI Expresser that can dispatch a follow-up speech event when it
// finishes speaking.
//-----------------------------------------------------------------------------
class CAI_ExpresserWithFollowup : public CAI_Expresser
{
public:
	CAI_ExpresserWithFollowup( CBaseFlex *pOuter = nullptr ) : CAI_Expresser(pOuter),
		m_pPostponedFollowup(nullptr)
		{};
	virtual bool Speak( AIConcept_t &conc, const char *modifiers = nullptr, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );
	virtual bool SpeakDispatchResponse( AIConcept_t &conc, AI_Response *response,  AI_CriteriaSet *criteria, IRecipientFilter *filter = nullptr );
	virtual void SpeakDispatchFollowup( AI_ResponseFollowup &followup );

	virtual void OnSpeechFinished();

	typedef CAI_Expresser BaseClass;
protected:
	static void DispatchFollowupThroughQueue( const AIConcept_t &conc,
		const char *criteriaStr,
		const CResponseQueue::CFollowupTargetSpec_t &target,
		float delay,
		CBaseEntity * __restrict__ pOuter		);

	AI_ResponseFollowup *m_pPostponedFollowup; // TODO: save/restore
	CResponseQueue::CFollowupTargetSpec_t	m_followupTarget;
};

class CMultiplayer_Expresser : public CAI_ExpresserWithFollowup
{
public:
	CMultiplayer_Expresser( CBaseFlex *pOuter = nullptr );
	//~CMultiplayer_Expresser();

	virtual bool IsSpeaking();

	void AllowMultipleScenes();
	void DisallowMultipleScenes();

private:
	bool m_bAllowMultipleScenes;

};

struct inputdata_t;

using namespace ::ks::reflect;

// runtime pitch shift and volume fadein/out structure

// NOTE: IF YOU CHANGE THIS STRUCT YOU MUST CHANGE THE SAVE/RESTORE VERSION NUMBER
// SEE BELOW (in the typedescription for the class)
typedef struct dynpitchvol
{
	// NOTE: do not change the order of these parameters 
	// NOTE: unless you also change order of rgdpvpreset array elements!
	int preset;

	int pitchrun;		// pitch shift % when sound is running 0 - 255
	int pitchstart;		// pitch shift % when sound stops or starts 0 - 255
	int spinup;			// spinup time 0 - 100
	int spindown;		// spindown time 0 - 100

	int volrun;			// volume change % when sound is running 0 - 10
	int volstart;		// volume change % when sound stops or starts 0 - 10
	int fadein;			// volume fade in time 0 - 100
	int fadeout;		// volume fade out time 0 - 100

	// Low Frequency Oscillator
	int	lfotype;		// 0) off 1) square 2) triangle 3) random
	int lforate;		// 0 - 1000, how fast lfo osciallates

	int lfomodpitch;	// 0-100 mod of current pitch. 0 is off.
	int lfomodvol;		// 0-100 mod of current volume. 0 is off.

	int cspinup;		// each trigger hit increments counter and spinup pitch

	int	cspincount;

	int pitch;			
	int spinupsav;
	int spindownsav;
	int pitchfrac;

	int vol;
	int fadeinsav;
	int fadeoutsav;
	int volfrac;

	int	lfofrac;
	int	lfomult;

} dynpitchvol_t;

inline constexpr auto SF_AMBIENT_SOUND_EVERYWHERE = 1;
inline constexpr auto SF_AMBIENT_SOUND_START_SILENT = 16;
inline constexpr auto SF_AMBIENT_SOUND_NOT_LOOPING = 32;

class CAmbientGeneric : public CPointEntity
{
public:
	DECLARE_CLASS( CAmbientGeneric, CPointEntity );

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	void Spawn_Impl( void );
	void Precache_Impl( void );
	void Activate_Impl( void );
	void RampThink( void );
	void InitModulationParms(void);
	void ComputeMaxAudibleDistance( );

	// Rules about which entities need to transmit along with me
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );
	void UpdateOnRemove_Impl( void );

	virtual void ToggleSound();
	virtual void SendSound( SoundFlags_t flags );

	// Input handlers
	[[= ks::reflect::Input{ .name = "PlaySound", .type = FIELD_VOID } ]] void InputPlaySound( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopSound", .type = FIELD_VOID } ]] void InputStopSound( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleSound", .type = FIELD_VOID } ]] void InputToggleSound( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Pitch", .type = FIELD_FLOAT } ]] void InputPitch( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Volume", .type = FIELD_FLOAT } ]] void InputVolume( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeIn", .type = FIELD_FLOAT } ]] void InputFadeIn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeOut", .type = FIELD_FLOAT } ]] void InputFadeOut( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "radius" } ]] float m_radius;
	float m_flMaxRadius;
	soundlevel_t m_iSoundLevel;		// dB value
	dynpitchvol_t m_dpv;	

	bool m_fActive;		// only true when the entity is playing a looping sound
	bool m_fLooping;		// true when the sound played will loop

	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "message" } ]] string_t m_iszSound;			// Path/filename of WAV file to play.
	[[= ks::reflect::Key{ .name = "SourceEntityName" } ]] string_t m_sSourceEntName;
	EHANDLE m_hSoundSource;	// entity from which the sound comes
	int		m_nSoundSourceEntIndex; // In case the entity goes away before we finish stopping the sound...
};

class CAmmoDef;

class CBaseCombatCharacter;

// Purpose: Holds defintion for game ammo types


struct Ammo_t 
{
	char 				*pName;
	int					nDamageType;
	int					eTracerType;
	int					nMinSplashSize;
	int					nMaxSplashSize;

	int					nFlags;

	// Values for player/NPC damage and carrying capability
	// If the integers are set, they override the CVars
	int					pPlrDmg;					// CVar for player damage amount
	int					pNPCDmg;					// CVar for NPC damage amount
	int					pMaxCarry;					// CVar for maximum number can carry
	int					pPhysicsForceImpulse;		// CVar for the physics impulse
	const ConVar*		pPlrDmgCVar;				// CVar for player damage amount
	const ConVar*		pNPCDmgCVar;				// CVar for NPC damage amount
	const ConVar*		pMaxCarryCVar;				// CVar for maximum number can carry
	const ConVar*		pPhysicsForceImpulseCVar;	// CVar for maximum number can carry
};

// Used to tell AmmoDef to use the cvars, not the integers
inline constexpr auto USE_CVAR = -1;
// Ammo is infinite
inline constexpr auto INFINITE_AMMO = -2;

enum AmmoTracer_t
{
	TRACER_NONE,
	TRACER_LINE,
	TRACER_RAIL,
	TRACER_BEAM,
	TRACER_LINE_AND_WHIZ,
};

enum AmmoFlags_t
{
	AMMO_FORCE_DROP_IF_CARRIED = 0x1,
	AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER = 0x2,
};

//=============================================================================
//	>> CAmmoDef
//=============================================================================
class CAmmoDef
{

public:
	int					m_nAmmoIndex;

	Ammo_t				m_AmmoType[MAX_AMMO_TYPES];

	Ammo_t				*GetAmmoOfIndex(int nAmmoIndex);
	int					Index(const char *psz);
	int					PlrDamage(int nAmmoIndex);
	int					NPCDamage(int nAmmoIndex);
	int					MaxCarry(int nAmmoIndex, const CBaseCombatCharacter *owner);
	bool				CanCarryInfiniteAmmo(int nAmmoIndex);
	int					DamageType(int nAmmoIndex);
	int					TracerType(int nAmmoIndex);
	float				DamageForce(int nAmmoIndex);
	int					MinSplashSize(int nAmmoIndex);
	int					MaxSplashSize(int nAmmoIndex);
	int					Flags(int nAmmoIndex);

	void				AddAmmoType(char const* name, int damageType, int tracerType, int plr_dmg, int npc_dmg, int carry, int impulse, int nFlags, int minSplashSize = 4, int maxSplashSize = 8 );
	void				AddAmmoType(char const* name, int damageType, int tracerType, char const* plr_cvar, char const* npc_var, char const* carry_cvar, char const* impulse_cvar, int nFlags, int minSplashSize = 4, int maxSplashSize = 8 );
	int					NumAmmoTypes() { return m_nAmmoIndex; }

	CAmmoDef(void);
	virtual ~CAmmoDef( void );

private:
	bool				AddAmmoType(char const* name, int damageType, int tracerType, int nFlags, int minSplashSize, int maxSplashSize );
};

// Get the global ammodef object. This is usually implemented in each mod's game rules file somewhere,
// so the mod can setup custom ammo types.
CAmmoDef* GetAmmoDef();

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

extern ConVar hl2_episodic;

// Simple shared header file for common base entities

// How many bits are used to transmit parent attachment indices?

// Maximum number of vphysics objects per entity
inline constexpr auto VPHYSICS_MAX_OBJECT_LIST_COUNT = 1024;

inline constexpr auto DEFAULT_LOOK_AT_USE_ANGLE = 0.8f;

// CBaseEntity inlines
bool CBaseEntity::IsPlayerSimulated( void ) const
{
	return m_bIsPlayerSimulated;
}

inline HSCRIPT ToHScript( CBaseEntity *pEnt )
{
	return ( pEnt ) ? pEnt->GetScriptInstance() : nullptr;
}

template <> ScriptClassDesc_t *GetScriptDesc<CBaseEntity>( CBaseEntity * );
CBaseEntity *ToEnt( HSCRIPT hScript )
{

	return ( hScript ) ? (CBaseEntity *)g_pScriptVM->GetInstanceValue( hScript, GetScriptDescForClass(CBaseEntity) ) : nullptr;
}

// Shared EntityMessage between game and client .dlls
inline constexpr auto BASEENTITY_MSG_REMOVE_DECALS = 1;

inline bool IsPushableMoveType( int nMoveType )
{
	if ( nMoveType == MOVETYPE_PUSH || nMoveType == MOVETYPE_NONE || 
		nMoveType == MOVETYPE_VPHYSICS || nMoveType == MOVETYPE_NOCLIP )
		return false;
	return true;
}

extern float k_flMaxEntityPosCoord;
extern float k_flMaxEntityEulerAngle;
extern float k_flMaxEntitySpeed;
extern float k_flMaxEntitySpinRate;

inline bool IsEntityCoordinateReasonable( const vec_t c )
{
	float r = k_flMaxEntityPosCoord;
	return c > -r && c < r;
}

inline bool IsEntityPositionReasonable( const Vector &v )
{
	float r = k_flMaxEntityPosCoord;
	return
		v.x > -r && v.x < r &&
		v.y > -r && v.y < r &&
		v.z > -r && v.z < r;
}

// Returns:
//   -1 - velocity is really, REALLY bad and probably should be rejected.
//   0  - velocity was suspicious and clamped.
//   1  - velocity was OK and not modified
extern int CheckEntityVelocity( Vector &v );

inline bool IsEntityQAngleReasonable( const QAngle &q )
{
	float r = k_flMaxEntityEulerAngle;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Angular velocity in exponential map form
inline bool IsEntityAngularVelocityReasonable( const Vector &q )
{
	float r = k_flMaxEntitySpinRate;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Angular velocity of each Euler angle.
inline bool IsEntityQAngleVelReasonable( const QAngle &q )
{
	float r = k_flMaxEntitySpinRate;
	return
		q.x > -r && q.x < r &&
		q.y > -r && q.y < r &&
		q.z > -r && q.z < r;
}

// Should we emit physics spew into the log or not?
extern bool CheckEmitReasonablePhysicsSpew();

Vector	CBaseEntity::Forward() const __restrict__  ///< get my forward (+x) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][0], mat[1][0], mat[2][0] );
}

Vector	CBaseEntity::Left() const __restrict__     ///< get my left    (+y) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][1], mat[1][1], mat[2][1] );
}

Vector	CBaseEntity::Up() const  __restrict__      ///< get my up      (+z) vector
{
	const matrix3x4_t &mat = EntityToWorldTransform();
	return Vector( mat[0][2], mat[1][2], mat[2][2] );
}

class CBaseEntity;
class CPointCamera;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CreateInfoCameraLink( CBaseEntity *pTarget, CPointCamera *pCamera );

//-----------------------------------------------------------------------------
// Sets up visibility 
//-----------------------------------------------------------------------------
void PointCameraSetupVisibility( CBaseEntity *pPlayer, int area, unsigned char *pvs, int pvssize );

using namespace ::ks::reflect;

// Purpose: Definition of the info_remarkable entity.
//			This entity is a quick and dirty hack to provide writers
//			with some kind of object that characters can remark upon.
//			It is not performant, because it relies upon each character
//			polling over each of these for visibility.  A better approach
//			will be an object that is notified by the engine when it is within
//			a character's view.

class CInfoRemarkable : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoRemarkable, CPointEntity );
	DECLARE_DATADESC();
	typedef std::vector< CInfoRemarkable * > tRemarkableList;
	static tRemarkableList *GetListOfAllThatIsRemarkable( void );

	~CInfoRemarkable();
	void	Spawn_Impl( void );
	inline void ResetCount( void ) { m_iTimesRemarkedUpon = 0; } // reset the number of times commented upon (eg at map restart)
	
	inline const char *GetRemarkContext( void ) const
		{ return m_szRemarkContext.ToCStr();	}

	int m_iTimesRemarkedUpon;

//FIX protected:
	[[= ks::reflect::Key{ .name = "contextsubject" } ]] string_t	m_szRemarkContext;
};

struct factorylist_t
{
	CreateInterfaceFn engineFactory;
	CreateInterfaceFn physicsFactory;
	CreateInterfaceFn fileSystemFactory;
};

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData );

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData );

class CBaseEntity;

using namespace ::ks::reflect;


//-----------------------------------------------------------------------------
// Purpose: Interfaces derived from this are able to filter out the local player
//  when doing prediction on the client, this includes not sending network data to
//  the local player from the server if needed.
//-----------------------------------------------------------------------------
class IPredictionSystem
{
public:
	IPredictionSystem()
	{
		m_pNextSystem = g_pPredictionSystems;
		g_pPredictionSystems = this;

		m_bSuppressEvent = false;
		m_pSuppressHost = nullptr;

		m_nStatusPushed = 0;
	};

	virtual ~IPredictionSystem() {};

	IPredictionSystem *GetNext()
	{
		return m_pNextSystem;
	}

	void SetSuppressEvent( bool state )
	{
		m_bSuppressEvent = state;
	}

	void SetSuppressHost( CBaseEntity *host )
	{
		m_pSuppressHost = host;
	}

	CBaseEntity const *GetSuppressHost( void )
	{
		if ( DisableFiltering() )
		{
			return nullptr;
		}

		return m_pSuppressHost;
	}

	bool CanPredict( void ) const
	{
		if ( DisableFiltering() )
		{
			return false;
		}

		return !m_bSuppressEvent;
	}

	static IPredictionSystem *g_pPredictionSystems;

	static void SuppressEvents( bool state )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->SetSuppressEvent( state );
			sys = sys->GetNext();
		}
	}

	static void SuppressHostEvents( CBaseEntity *host )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->SetSuppressHost( host );
			sys = sys->GetNext();
		}
	}

private:

	static void Push( void )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->_Push();
			sys = sys->GetNext();
		}
	}

	static void Pop( void )
	{
		IPredictionSystem *sys = g_pPredictionSystems;
		while ( sys )
		{
			sys->_Pop();
			sys = sys->GetNext();
		}
	}

	void _Push( void )
	{
		++m_nStatusPushed;
	}
	void _Pop( void )
	{
		--m_nStatusPushed;
	}

	bool DisableFiltering( void ) const
	{
		return ( m_nStatusPushed > 0  ) ? true : false;
	}

	IPredictionSystem	*m_pNextSystem;
	bool				m_bSuppressEvent;
	CBaseEntity			*m_pSuppressHost;

	int					m_nStatusPushed;

	friend class CDisablePredictionFiltering;
};

class CDisablePredictionFiltering
{
public:
	CDisablePredictionFiltering( bool disable = true )
	{
		m_bDisabled = disable;
		if ( m_bDisabled )
		{
			IPredictionSystem::Push();
		}
	}

	~CDisablePredictionFiltering( void )
	{
		if ( m_bDisabled )
		{
			IPredictionSystem::Pop();
		}
	}
private:
	bool	m_bDisabled;
};

class CBasePlayer;



// Purpose: Interface that entities can use to redirect scoring to other entities.
//			i.e. A rocket redirects scoring to the player that fired it.

//-----------------------------------------------------------------------------
// Purpose: Interface that entities can use to redirect scoring to other entities.
//			i.e. A rocket redirects scoring to the player that fired it.
//-----------------------------------------------------------------------------
class IScorer
{
public:
	// Return the entity that should receive the score
	virtual CBasePlayer *GetScorer( void ) = 0;
	// Return the entity that should get assistance credit
	virtual CBasePlayer *GetAssistant( void ) = 0;
};

class CEconItemDefinition;

// Purpose: Stub for item inventory system (econ system removed)

// Type definitions
typedef uint64 globalindex_t;
inline constexpr auto INVALID_GLOBAL_INDEX = ((globalindex_t)0);

enum EItemRequestResult
{
	k_EItemRequestResultOK = 0,
	k_EItemRequestResultPending = 1,
	k_EItemRequestResultFailed = 2,
};

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Stub for script-created items
//-----------------------------------------------------------------------------
class CScriptCreatedItem
{
public:
	CScriptCreatedItem() {}
	virtual ~CScriptCreatedItem() {}

	CEconItemDefinition *GetItemDefinition() const { return nullptr; }
};

//-----------------------------------------------------------------------------
// Purpose: Base class for player inventories
//-----------------------------------------------------------------------------
class CPlayerInventory
{
public:
	CPlayerInventory() {}
	virtual ~CPlayerInventory() {}

	virtual bool ItemShouldBeIncluded( int iItemPosition ) { return true; }
	virtual void InventoryReceived( EItemRequestResult eResult, int iItems ) {}
	virtual void DumpInventoryToConsole( bool bRoot ) {}

};

//-----------------------------------------------------------------------------
// Purpose: Base class for inventory managers
//-----------------------------------------------------------------------------
class CInventoryManager
{
public:
	CInventoryManager() {}
	virtual ~CInventoryManager() {}

	virtual void PostInit( void ) {}
};

class CBaseEntity;

using namespace ::ks::reflect;



//-----------------------------------------------------------------------------
// Purpose:  Shared interface to temp entities
//-----------------------------------------------------------------------------
class ITempEntsSystem : public IPredictionSystem
{
public:
	virtual bool SuppressTE( IRecipientFilter& filter ) = 0;
	virtual void ArmorRicochet( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir ) = 0;
	virtual void BeamEntPoint( IRecipientFilter& filer, float delay,
		int	nStartEntity, const Vector *start, int nEndEntity, const Vector* end, 
		int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamEnts( IRecipientFilter& filer, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamFollow( IRecipientFilter& filter, float delay,
		int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth, 
		float fadeLength, float r, float g, float b, float a ) = 0;
	virtual void BeamPoints( IRecipientFilter& filer, float delay,
		const Vector* start, const Vector* end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, 
		int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamLaser( IRecipientFilter& filer, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, float endWidth, int fadeLength, float amplitude, int r, int g, int b, int a, int speed ) = 0;
	virtual void BeamRing( IRecipientFilter& filer, float delay,
		int	start, int end, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 ) = 0;
	virtual void BeamRingPoint( IRecipientFilter& filer, float delay,
		const Vector& center, float start_radius, float end_radius, int modelindex, int haloindex, int startframe, int framerate,
		float life, float width, int spread, float amplitude, int r, int g, int b, int a, int speed, int flags = 0 ) = 0;
	virtual void BeamSpline( IRecipientFilter& filer, float delay,
		int points, Vector* rgPoints ) = 0;
	virtual void BloodStream( IRecipientFilter& filer, float delay,
		const Vector* org, const Vector* dir, int r, int g, int b, int a, int amount ) = 0;
	virtual void BloodSprite( IRecipientFilter& filer, float delay,
		const Vector* org, const Vector *dir, int r, int g, int b, int a, int size ) = 0;
	virtual void BreakModel( IRecipientFilter& filer, float delay,
		const Vector& pos, const QAngle &angle, const Vector& size, const Vector& vel, 
		int modelindex, int randomization, int count, float time, int flags ) = 0;
	virtual void BSPDecal( IRecipientFilter& filer, float delay,
		const Vector* pos, int entity, int index ) = 0;
	virtual void ProjectDecal( IRecipientFilter& filter, float delay,
		const Vector* pos, const QAngle *angles, float distance, int index ) = 0;
	virtual void Bubbles( IRecipientFilter& filer, float delay,
		const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed ) = 0;
	virtual void BubbleTrail( IRecipientFilter& filer, float delay,
		const Vector* mins, const Vector* maxs, float height, int modelindex, int count, float speed ) = 0;
	virtual void Decal( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* start, int entity, int hitbox, int index ) = 0;
	virtual void DynamicLight( IRecipientFilter& filer, float delay,
		const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay ) = 0;
	virtual void Explosion( IRecipientFilter& filer, float delay,
		const Vector& pos, int modelindex, float scale, int framerate, int flags, int radius, int magnitude, const Vector* normal = nullptr, unsigned char materialType = 'C' ) = 0;
	virtual void ShatterSurface( IRecipientFilter& filer, float delay,
		const Vector* pos, const QAngle* angle, const Vector* vForce, const Vector* vForcePos, 
		float width, float height, float shardsize, ShatterSurface_t surfacetype,
		int front_r, int front_g, int front_b, int back_r, int back_g, int back_b) = 0;
	virtual void GlowSprite( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, float life, float size, int brightness ) = 0;
	virtual void FootprintDecal( IRecipientFilter& filer, float delay, const Vector *origin, const Vector* right, 
		int entity, int index, unsigned char materialType ) = 0;
	virtual void Fizz( IRecipientFilter& filer, float delay,
		const CBaseEntity *ed, int modelindex, int density, int current ) = 0;
	virtual void KillPlayerAttachments( IRecipientFilter& filer, float delay,
		int player ) = 0;
	virtual void LargeFunnel( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, int reversed ) = 0;
	virtual void MetalSparks( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir ) = 0;
	virtual void EnergySplash( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir, bool bExplosive ) = 0;
	virtual void PlayerDecal( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox ) = 0;
	virtual void ShowLine( IRecipientFilter& filer, float delay,
		const Vector* start, const Vector* end ) = 0;
	virtual void Smoke( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, float scale, int framerate ) = 0;
	virtual void Sparks( IRecipientFilter& filer, float delay,
		const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir ) = 0;
	virtual void Sprite( IRecipientFilter& filer, float delay,
		const Vector* pos, int modelindex, float size, int brightness ) = 0;
	virtual void SpriteSpray( IRecipientFilter& filer, float delay,
		const Vector* pos, const Vector* dir, int modelindex, int speed, float noise, int count ) = 0;
	virtual void WorldDecal( IRecipientFilter& filer, float delay,
		const Vector* pos, int index ) = 0;
	virtual void MuzzleFlash( IRecipientFilter& filer, float delay,
		const Vector &start, const QAngle &angles, float scale, int type ) = 0;
	virtual void Dust( IRecipientFilter& filer, float delay,
				 const Vector &pos, const Vector &dir, float size, float speed ) = 0;
	virtual void GaussExplosion( IRecipientFilter& filer, float delay,
				const Vector &pos, const Vector &dir, int type ) = 0;
	virtual void PhysicsProp( IRecipientFilter& filter, float delay, int modelindex, int skin, 
		const Vector& pos, const QAngle &angles, const Vector& vel, int flags, int effects, color24 renderColor ) = 0;

	// For playback from external tools
	virtual void TriggerTempEntity( KeyValues *pKeyValues ) = 0;

	virtual void ClientProjectile( IRecipientFilter& filter, float delay,
		const Vector* vecOrigin, const Vector* vecVelocity, int modelindex, int lifetime, CBaseEntity *pOwner ) = 0;
};

extern ITempEntsSystem *te;

class CBasePlayer;
class CHintSystem;

using namespace ::ks::reflect;



//--------------------------------------------------------------------------------------------------------------
class CHintMessage
{
public:
	CHintMessage( const char * hintString, std::vector< const char * > * args, float duration );
	~CHintMessage();

	float GetDuration() const { return m_duration; }
	void Send( CBasePlayer *client );

	bool IsEquivalent( const char *hintString, std::vector< const char * > * args ) const;

private:
	const char * m_hintString;					///< hintString is a pointer to a string that should never be deleted.
	std::vector< char * > m_args;				///< list of arguments.  The memory for these strings is internal to the CHintMessage.
	float m_duration;							///< time until the next message can be displayed
};

//--------------------------------------------------------------------------------------------------------------
class CHintMessageQueue
{
public:
	CHintMessageQueue( CBasePlayer *pPlayer );
	void		Reset();
	void		Update();
	bool		AddMessage( const char* message, float duration = 6.0f, std::vector< const char * > * args = nullptr );
	inline bool IsEmpty() { return m_messages.empty(); }

private:
	float		m_tmMessageEnd;
	std::vector< CHintMessage * > m_messages;
	CBasePlayer *m_pPlayer;
};

//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Timers that manage hint messages that should be displayed after some time.
class CHintMessageTimers
{
public:
	CHintMessageTimers( void );
	CHintMessageTimers( CHintSystem *pSystem, CHintMessageQueue *pQueue );

	void		Reset();
	void		Update();

	// Add / Register timers that will be started/stopped during play
	void		AddTimer( int iHintID, float timer_duration, float message_duration = 6.0f, std::vector< const char * > * args = nullptr );
	void		RemoveTimer( int iHintID );

	// Start / Stop timers that were previously registered via AddTimer()
	void		StartTimer( int iHintID );
	void		StopTimer( int iHintID );

private:
	int			GetTimerIndex( int iHintID );

private:
	struct hintmessagetime_t
	{
		hintmessagetime_t( float flTimerDuration ) :
			timer(flTimerDuration)
		{
			iHintID = 0;
			flMessageDuration = 6.0;
		}

		~hintmessagetime_t()
		{
			for ( int i=0; i<std::ssize( args ); ++i )
			{
				delete[] args[i];
			}
			args.clear();
		}

		int						iHintID;
		CStopwatch				timer;
		float					flMessageDuration;
		std::vector< char * >	args;
	};

	std::vector< hintmessagetime_t* >  m_Timers;
	CHintMessageQueue				  *m_pQueue;
	CHintSystem						  *m_pHintSystem;
};

class CBasePlayer;
class CHintMessageQueue;
class CHintMessageTimers;

// Purpose: A class embedded in players to provide hints to that player



typedef bool (*HintTimerCallback)( CBasePlayer *pOnPlayer );

//-----------------------------------------------------------------------------
// Purpose: A class embedded in players to provide hints to that player
//-----------------------------------------------------------------------------
class CHintSystem
{
	DECLARE_CLASS_NOBASE( CHintSystem );

public:
	CHintSystem();
	~CHintSystem();

	//-----------------------------------------------------
	// Call this from your player constructor
	void Init( CBasePlayer *pPlayer, int iMaxHintTypes, const char **pszHintStrings );

	//-----------------------------------------------------
	// CBasePlayer calls these for you, if you fall back to its 
	// versions of Spawn(), Event_Killed(), and PreThink().
	// Call this when your player respawns
	void				ResetHints( void );

	// Call this when your player dies
	void				ResetHintTimers( void );

	// Call this when in your player PreThink()
	void				Update( void );

	//-----------------------------------------------------
	// Hint addition
	// Call these to add a hint directly onscreen
	bool 				HintMessage( int hint, bool bForce = false, bool bOnlyIfClear = false );
	void 				HintMessage( const char *pMessage );

	// Call this to add a hint timer. It'll be reset for you automatically 
	// everytime ResetHintTimers() is called.
	void				RegisterHintTimer( int iHintID, float flTimerDuration, bool bOnlyIfClear = false, HintTimerCallback pfnCallback = nullptr );

	// Call these to start & stop registered hint timers
	void				StartHintTimer( int iHintID );
	void				StopHintTimer( int iHintID );
	void				RemoveHintTimer( int iHintID );
	bool				TimerShouldFire( int iHintID );

	// Set whether a player should see any hints at all
	void				SetShowHints( bool bShowHints ) { m_bShowHints = bShowHints; }
	void				SetHintPlayed( int iHintID );
	bool				ShouldShowHints( void );

	// Returns true if the hint has been played already
	bool				HasPlayedHint( int iHintID );
	void				PlayedAHint( void );
	void				ClearHintHistory( void ) { std::ranges::fill( m_HintHistory, false ); }

	// Not really an optimal solution, but saves us querying the hud element,
	// which wouldn't be easy with derived versions in different mods.
	bool				HintIsCurrentlyVisible( void ) { return (gpGlobals->curtime - m_flLastHintPlayedAt < 11 ); }

private:
	void				ReAddHintTimerIfNotDisplayed( int iHintID, float flTimerDuration );

private:
	CBasePlayer			*m_pPlayer;

	float				m_flLastHintPlayedAt;
	bool				m_bShowHints;
	std::vector< bool >	m_HintHistory;
	const char			**m_pszHintMessages;
	CHintMessageQueue	*m_pHintMessageQueue;
	CHintMessageTimers	*m_pHintMessageTimers;

	struct onresethints_t
	{
		int					iHintID;
		float				flTimer;
		bool				bOnlyIfClear;
		HintTimerCallback	pfnCallback;
	};
	std::vector< onresethints_t >	m_RegisteredResetHints;
};

class CBaseEntity;
class CBasePlayer;
class IGameSystem;

using namespace ::ks::reflect;


inline constexpr auto HLTV_MIN_DIRECTOR_DELAY = 3; // minimum delay if director is enabled
inline constexpr auto HLTV_MAX_DELAY = 120; // maximum delay

inline constexpr auto MAX_NUM_CAMERAS = 64; // support up to 64 fixed cameras per level

inline constexpr auto MIN_SHOT_LENGTH = 4.0f; // minimum time of a cut (seconds)
inline constexpr auto MAX_SHOT_LENGTH = 8.0f; // maximum time of a cut (seconds)
inline constexpr auto DEF_SHOT_LENGTH = 6.0f; // average time of a cut (seconds)

class CHLTVGameEvent
{
public:
		int			m_Tick;		// tick of this command
		int			m_Priority; // game event priority
		IGameEvent	*m_Event;	// IGameEvent
};

// Orders events by tick; transparent so the history can be searched by tick.
struct CHLTVGameEventLess
{
	using is_transparent = void;
	bool operator()( const CHLTVGameEvent &e1, const CHLTVGameEvent &e2 ) const { return e1.m_Tick < e2.m_Tick; }
	bool operator()( const CHLTVGameEvent &e, int tick ) const { return e.m_Tick < tick; }
	bool operator()( int tick, const CHLTVGameEvent &e ) const { return tick < e.m_Tick; }
};

class CHLTVDirector : public CGameEventListener, public CBaseGameSystemPerFrame, public IHLTVDirector
{
public:
	DECLARE_CLASS_NOBASE( CHLTVDirector );

	virtual char const *Name() { return "CHLTVDirector"; }

	CHLTVDirector();
	virtual ~CHLTVDirector();

	virtual void AddHLTVServer( IHLTVServer *hltv )override; // give the director an HLTV interface 
	virtual void RemoveHLTVServer( IHLTVServer *hltv ) override;
	virtual IHLTVServer* GetHLTVServer( int nIndex ) override { return m_HltvServers[ nIndex ].m_pHLTVServer; }
	virtual int GetHLTVServerCount() override { return std::ssize( m_HltvServers ); }
	int		GetDirectorTick( void );	// get current broadcast tick from director
	int		GetPVSEntity( void ); // get current view entity (PVS)
	Vector	GetPVSOrigin( void ); // get current PVS origin, if PVS entity is 0
	float	GetDelay( void ); // returns current delay in seconds
	bool	IsActive( void );

	virtual const char** GetModEvents(); // returns list of event names forwarded to HLTV clients

	void	BuildCameraList( void );

	// Starts automatic recording of the current session
	void	StartAutoRecording( void );
		
public: // game event handling
	void	FireGameEvent( IGameEvent * event );
	
public: // CBaseGameSystem overrides

	virtual bool	Init();
	virtual void	Shutdown();
	virtual void	FrameUpdatePostEntityThink();
	virtual void	LevelInitPostEntity();
	virtual const char	*GetFixedCameraEntityName( void ) { return "point_viewcontrol"; }

			bool	SetCameraMan( int iPlayerIndex );
			int		GetCameraMan() { return m_iCameraManIndex; }

protected:

	virtual void	StartNewShot();	
	virtual void	StartRandomShot();
	virtual void	StartDelayMessage();
	virtual void	StartBestFixedCameraShot(bool bForce);
	virtual void	StartBestPlayerCameraShot();
	virtual void	StartFixedCameraShot(int iCamera, int iTarget);
	virtual void	StartChaseCameraShot(int iTarget1, int iTarget2, int distance, int phi, int theta, bool bInEye);
	virtual void	UpdateSettings();
	virtual void	AnalyzePlayers();
	virtual void 	AnalyzeCameras();
	virtual bool	StartCameraManShot();
	virtual void	StartInstantBroadcastShot();
	virtual void	FinishCameraManShot();
	virtual void	BuildActivePlayerList();
	virtual const CHLTVGameEvent *FindBestGameEvent();
	virtual void	CreateShotFromEvent( const CHLTVGameEvent *ge );

	// events with equal ticks stay in arrival order
	using EventHistory_t = std::multiset<CHLTVGameEvent, CHLTVGameEventLess>;

	EventHistory_t::const_iterator FindFirstEvent( int tick ) const; // finds first event >= tick
	void	CheckHistory();
	void	RemoveEventsFromHistory(int tick); // removes all commands < tick, or all if tick -1
	
	struct HltvServerRecord_t
	{
		IHLTVServer		*m_pHLTVServer;	// interface to servers HLTV object
		CBasePlayer		*m_pHLTVClient; // the HLTV fake client
	};
	std::vector< HltvServerRecord_t > m_HltvServers;

	float			m_fDelay;	// hltv delay in seconds
	int				m_nBroadcastTick; // world time that is currently "on the air"
	int				m_iPVSEntity;	// entity for PVS center
	Vector			m_vPVSOrigin;	// PVS origin if PVS entity is 0
	int				m_iCameraMan;	//  >0 if current view entity is a cameraman
	int				m_nNextShotTick;	// time for the next scene cut
	int				m_iLastPlayer;		// last player in random rotation

	int				m_nNextAnalyzeTick;	
		
	int				m_nNumFixedCameras;	//number of cameras in current map
	CBaseEntity		*m_pFixedCameras[MAX_NUM_CAMERAS]; // fixed cameras (point_viewcontrol)
	
	int				m_nNumActivePlayers;	//number of cameras in current map
	CBasePlayer		*m_pActivePlayers[MAX_PLAYERS]; // fixed cameras (point_viewcontrol)
	int				m_iCameraManIndex;		// entity index of current camera man or 0
	
	EventHistory_t	m_EventHistory;
};

extern IGameSystem* HLTVDirectorSystem();
extern CHLTVDirector* HLTVDirector();

using namespace ::ks::reflect;

// Purpose: Client-server neutral effects interface

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Client-server neutral effects interface
//-----------------------------------------------------------------------------
inline constexpr const char *IEFFECTS_INTERFACE_VERSION = "IEffects001";
class IEffects : public IPredictionSystem
{
public:
	//
	// Particle effects
	//
	virtual void Beam( const Vector &Start, const Vector &End, int nModelIndex, 
		int nHaloIndex, unsigned char frameStart, unsigned char frameRate,
		float flLife, unsigned char width, unsigned char endWidth, unsigned char fadeLength, 
		unsigned char noise, unsigned char red, unsigned char green,
		unsigned char blue, unsigned char brightness, unsigned char speed) = 0;

	//-----------------------------------------------------------------------------
	// Purpose: Emits smoke sprites.
	// Input  : origin - Where to emit the sprites.
	//			scale - Sprite scale * 10.
	//			framerate - Framerate at which to animate the smoke sprites.
	//-----------------------------------------------------------------------------
	virtual void Smoke( const Vector &origin, int modelIndex, float scale, float framerate ) = 0;

	virtual void Sparks( const Vector &position, int nMagnitude = 1, int nTrailLength = 1, const Vector *pvecDir = nullptr ) = 0;

	virtual void Dust( const Vector &pos, const Vector &dir, float size, float speed ) = 0;

	virtual void MuzzleFlash( const Vector &vecOrigin, const QAngle &vecAngles, float flScale, int iType ) = 0;

	// like ricochet, but no sound
	virtual void MetalSparks( const Vector &position, const Vector &direction ) = 0; 

	virtual void EnergySplash( const Vector &position, const Vector &direction, bool bExplosive = false ) = 0;

	virtual void Ricochet( const Vector &position, const Vector &direction ) = 0;

	// FIXME: Should these methods remain in this interface? Or go in some 
	// other client-server neutral interface?
	virtual float Time() = 0;
	virtual bool IsServer() = 0;

	// Used by the playback system to suppress sounds
	virtual void SuppressEffectsSounds( bool bSuppress ) = 0;
};

//-----------------------------------------------------------------------------
// Client-server neutral effects interface accessor
//-----------------------------------------------------------------------------
extern IEffects *g_pEffects;

class CBasePlayer;

class CAI_Expresser;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CBaseEntity;
class CBaseViewModel;
class CBotManager;
class CColorCorrection;
class CEconItemView;
class CFogController;
class CHintSystem;
class CLogicPlayerProxy;
class CNavArea;
class CPlayerMove;
class CPostProcessController;
class CTakeDamageInfo;
class CTonemapTrigger;
class CUserCmd;
class CVoteController;
struct animevent_t;
struct gamevcollisionevent_t;
struct impactdamagetable_t;
struct inputdata_t;

using namespace ::ks::reflect;

// For queuing and processing usercmds
class CCommandContext
{
public:
	std::vector< CUserCmd > cmds;

	int				numcmds;
	int				totalcmds;
	int				dropped_packets;
	bool			paused;
};

// Info about last 20 or so updates to the
class CPlayerCmdInfo
{
public:
	CPlayerCmdInfo() : 
	  m_flTime( 0.0f ), m_nNumCmds( 0 ), m_nDroppedPackets( 0 )
	{
	}

	// realtime of sample
	float		m_flTime;
	// # of CUserCmds in this update
	int			m_nNumCmds;
	// # of dropped packets on the link
	int			m_nDroppedPackets;
};

class CPlayerSimInfo
{
public:
	CPlayerSimInfo() : 
	  m_flTime( 0.0f ), m_nNumCmds( 0 ), m_nTicksCorrected( 0 ), m_flFinalSimulationTime( 0.0f ), m_flGameSimulationTime( 0.0f ), m_flServerFrameTime( 0.0f ), m_vecAbsOrigin( 0, 0, 0 )
	{
	}

	// realtime of sample
	float		m_flTime;
	// # of CUserCmds in this update
	int			m_nNumCmds;
	// If clock needed correction, # of ticks added/removed
	int			m_nTicksCorrected; // +ve or -ve
	// player's m_flSimulationTime at end of frame
	float		m_flFinalSimulationTime;
	float		m_flGameSimulationTime;
	// estimate of server perf
	float		m_flServerFrameTime;  
	Vector		m_vecAbsOrigin;
};
//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------

// for step sounds

// !!!set this bit on guns and stuff that should never respawn.
inline constexpr auto SF_NORESPAWN = ( 1 << 30 );

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
inline constexpr auto CSUITPLAYLIST = 4; // max of 4 suit sentences queued up at any time
inline constexpr auto SUIT_REPEAT_OK = 0;

inline constexpr auto SUIT_NEXT_IN_30SEC = 30;
inline constexpr auto SUIT_NEXT_IN_1MIN = 60;
inline constexpr auto SUIT_NEXT_IN_5MIN = 300;
inline constexpr auto SUIT_NEXT_IN_10MIN = 600;
inline constexpr auto SUIT_NEXT_IN_30MIN = 1800;
inline constexpr auto SUIT_NEXT_IN_1HOUR = 3600;

inline constexpr auto CSUITNOREPEAT = 32;

inline constexpr auto TEAM_NAME_LENGTH = 16;

// constant items
inline constexpr auto ITEM_HEALTHKIT = 1;
inline constexpr auto ITEM_BATTERY = 4;

enum PlayerConnectedState
{
	PlayerConnected,
	PlayerDisconnecting,
	PlayerDisconnected,
};

enum AimResults
{
	AIMRESULTS_NONE,
	AIMRESULTS_ONTARGET,
	AIMRESULTS_ASSISTED,
};

extern bool gInitHUD;
extern ConVar *sv_cheats;

class CPlayerInfo : public IBotController, public IPlayerInfo
{
public:
	CPlayerInfo () { m_pParent = nullptr; } 
	~CPlayerInfo () {}
	void SetParent( CBasePlayer *parent ) { m_pParent = parent; } 

	// IPlayerInfo interface
	virtual const char *GetName();
	virtual int			GetUserID();
	virtual const char *GetNetworkIDString();
	virtual int			GetTeamIndex();
	virtual void		ChangeTeam( int iTeamNum );
	virtual int			GetFragCount();
	virtual int			GetAssistsCount();
	virtual int			GetDeathCount();
	virtual bool		IsConnected();
	virtual int			GetArmorValue();

	virtual bool IsHLTV();
	virtual bool IsPlayer();
	virtual bool IsFakeClient();
	virtual bool IsDead();
	virtual bool IsInAVehicle();
	virtual bool IsObserver();
	virtual const Vector GetAbsOrigin();
	virtual const QAngle GetAbsAngles();
	virtual const Vector GetPlayerMins();
	virtual const Vector GetPlayerMaxs();
	virtual const char *GetWeaponName();
	virtual const char *GetModelName();
	virtual const int GetHealth();
	virtual const int GetMaxHealth();

	// bot specific functions	
	virtual void SetAbsOrigin( Vector & vec );
	virtual void SetAbsAngles( QAngle & ang );
	virtual void RemoveAllItems( bool removeSuit );
	virtual void SetActiveWeapon( const char *WeaponName );
	virtual void SetLocalOrigin( const Vector& origin );
	virtual const Vector GetLocalOrigin( void );
	virtual void SetLocalAngles( const QAngle& angles );
	virtual const QAngle GetLocalAngles( void );
	virtual void PostClientMessagesSent( void );
	virtual bool IsEFlagSet( int nEFlagMask );

	virtual void RunPlayerMove( CBotCmd *ucmd );
	virtual void SetLastUserCommand( const CBotCmd &cmd );

	virtual CBotCmd GetLastUserCommand();

private:
	CBasePlayer *m_pParent; 
};

// DT_LocalPlayerExclusive is named as the "localdata" prop of DT_BasePlayer, and its proxy decides
// which clients receive it, so both have to be visible where CBasePlayer is declared.
namespace DT_LocalPlayerExclusive { extern SendTable g_SendTable; }
void* SendProxy_SendLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData,
    CSendProxyRecipients *pRecipients, int objectID );
void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct,
    const void *pVarData, DVariant *pOut, int iElement, int objectID );

class CBasePlayer : public CBaseCombatCharacter, public CPlayerMoveCore
{
	[[= ks::reflect::NetTable{ .name = "DT_BasePlayer" } ]]
	[[= ks::reflect::NetTable{ .name = "DT_LocalPlayerExclusive", .base = false } ]]
	[[= ks::reflect::From<"m_iHealth", ks::reflect::Net{ .bits = 16 }>{} ]]
	[[= ks::reflect::From<"m_lifeState", ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_iAmmo", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_fFlags",
	ks::reflect::Net{ .bits = PLAYER_FLAG_BITS, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN },
	SendProxy_CropFlagsToPlayerFlagBitsLength>{} ]]
	[[= ks::reflect::From<"m_hGroundEntity", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_PlayerFog.m_hCtrl", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::SubTable<"localdata", &DT_LocalPlayerExclusive::g_SendTable,
	SendProxy_SendLocalDataTable>{} ]]
	[[= ks::reflect::From<"m_vecViewOffset", ks::reflect::Net{ .bits = 8, .low = -32.0, .high = 32.0f, .flags = SPROP_ROUNDDOWN, .table = "DT_LocalPlayerExclusive", .index = 0 }>{} ]]
	[[= ks::reflect::From<"m_vecViewOffset", ks::reflect::Net{ .bits = 8, .low = -32.0, .high = 32.0f, .flags = SPROP_ROUNDDOWN, .table = "DT_LocalPlayerExclusive", .index = 1 }>{} ]]
	[[= ks::reflect::From<"m_vecViewOffset", ks::reflect::Net{ .bits = 10, .low = 0.0f, .high = 128.0f, .flags = SPROP_CHANGES_OFTEN, .table = "DT_LocalPlayerExclusive", .index = 2 }>{} ]]
	[[= ks::reflect::From<"m_flFriction", ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 4.0f, .flags = SPROP_ROUNDDOWN, .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_nNextThinkTick", ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive", .index = 0, .priority = SENDPROP_PLAYER_VELOCITY_XY_PRIORITY }>{} ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive", .index = 1, .priority = SENDPROP_PLAYER_VELOCITY_XY_PRIORITY }>{} ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive", .index = 2, .priority = SENDPROP_PLAYER_VELOCITY_Z_PRIORITY }>{} ]]
	[[= ks::reflect::From<"m_vecBaseVelocity", ks::reflect::Net{ .bits = 20, .low = -1000, .high = 1000, .flags = 0, .table = "DT_LocalPlayerExclusive" }>{} ]]
	[[= ks::reflect::From<"m_nWaterLevel", ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED, .table = "DT_LocalPlayerExclusive" }>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBasePlayer, CBaseCombatCharacter );

	// Names CPlayerMoveCore shares with the entity chain mean the entity's here.
	using CBaseCombatCharacter::GetFlags;
	using CBaseCombatCharacter::AddFlag;
	using CBaseCombatCharacter::RemoveFlag;
	using CBaseCombatCharacter::GetMoveType;
	using CBaseCombatCharacter::GetMoveCollide;
	using CBaseCombatCharacter::SetMoveCollide;
	using CBaseCombatCharacter::GetBaseVelocity;
	using CBaseCombatCharacter::SetBaseVelocity;
	using CBaseCombatCharacter::GetWaterLevel;
	using CBaseCombatCharacter::SetWaterLevel;
	using CBaseCombatCharacter::GetWaterType;
	using CBaseCombatCharacter::SetWaterType;
	using CBaseCombatCharacter::GetGravity;
	using CBaseCombatCharacter::SetGravity;
	using CBaseCombatCharacter::GetAbsVelocity;
	using CBaseCombatCharacter::entindex;
	using CBaseCombatCharacter::IsServer;
	using CBaseCombatCharacter::GetGroundEntity;
	using CBaseCombatCharacter::SetGroundEntity;
	using CBaseCombatCharacter::SetMoveType;
	using CBaseCombatCharacter::GetViewOffset;
	using CBaseCombatCharacter::SetViewOffset;
	using CBaseCombatCharacter::GetHealth;
protected:
	// HACK FOR BOTS
	friend class CBotManager;
	static edict_t *s_PlayerEdict; // must be set before calling constructor
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	// script description
	DECLARE_ENT_SCRIPTDESC();
	
	CBasePlayer();
	~CBasePlayer();

	void					StartUserMessageThrottling( char const *pchMessageNames[], int nNumMessageNames );
	void					FinishUserMessageThrottling();

	bool					ShouldThrottleUserMessage( char const *pchMessageName );

	// IPlayerInfo passthrough (because we can't do multiple inheritance)
	IPlayerInfo *GetPlayerInfo() { return &m_PlayerInfo; }
	IBotController *GetBotController() { return &m_PlayerInfo; }

	void			SetModel_Impl( const char *szModelName );
	void					SetBodyPitch( float flPitch );

	void			UpdateOnRemove_Impl( void );

	static CBasePlayer		*CreatePlayer( const char *className, edict_t *ed );

	virtual void			CreateViewModel( int viewmodelindex = 0 );
	CBaseViewModel			*GetViewModel( int viewmodelindex = 0 );
	void					HideViewModels( void );
	void					DestroyViewModels( void );

	CPlayerState			*PlayerData( void ) { return pl.Get(); }
	const CPlayerState		*PlayerData( void ) const { return pl.Get(); }
	
	int						RequiredEdictIndex_Impl( void ) { return ENTINDEX(edict()); } 

	void					LockPlayerInPlace( void );
	void					UnlockPlayer( void );

	void			DrawDebugGeometryOverlays_Impl(void);
	
	// Networking is about to update this entity, let it override and specify it's own pvs
	virtual void			SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );
	int				UpdateTransmitState_Impl();
	int				ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo );
	virtual bool			ShouldCheckOcclusion( CBasePlayer *pOtherPlayer ) { return false;  }

	// Returns true if this player wants pPlayer to be moved back in time when this player runs usercmds.
	// Saves a lot of overhead on the server if we can cull out entities that don't need to lag compensate
	// (like team members, entities out of our PVS, etc).
	virtual bool			WantsLagCompensationOnEntity( const CBaseEntity *entity, const CUserCmd *pCmd, const std::bitset<MAX_EDICTS> *pEntityTransmitBits ) const;

	void			Spawn_Impl( void );
	void			Activate_Impl( void );
	virtual void			SharedSpawn(); // Shared between client and server.
	virtual void			ForceRespawn( void );
	virtual void			PostSpawnPointSelection( void );

	virtual void			InitialSpawn( void );
	virtual void			InitHUD( void ) {}
	virtual void			ShowViewPortPanel( const char * name, bool bShow = true, KeyValues *data = nullptr );

	virtual const char *	GetPlayerModelName( void );

	virtual void			PlayerDeathThink( void );
	virtual void			PlayerForceTeamThink( void );

	virtual void			Jump( void );
	virtual void			Duck( void );

	const char				*GetTracerType_Impl( void );
	void					MakeTracer_Impl( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType );
	void					DoImpactEffect_Impl( trace_t &tr, int nDamageType );

	// If a map clean up has been done after a respawn, this function will reassign the entity pointers to those map entities that were deleted.
	void					UpdateMapEntityPointers( void );

	void					AddToPlayerSimulationList( CBaseEntity *other );
	void					RemoveFromPlayerSimulationList( CBaseEntity *other );
	void					SimulatePlayerSimulatedEntities( void );
	void					ClearPlayerSimulationList( void );

	// Physics simulation (player executes it's usercmd's here)
	void			PhysicsSimulate_Impl( void );

	// Forces processing of usercmds (e.g., even if game is paused, etc.)
	void					ForceSimulation();

	unsigned int	PhysicsSolidMaskForEntity_Impl( void ) const;

	// Move us a bit to ensure we're spawning somewhere we can actually move around.
	void					EnsureValidSpawnLocation();

	virtual void			PreThink( void );
	virtual void			PostThink( void );
	int				TakeHealth_Impl( float flHealth, int bitsDamageType );
	void			TraceAttack_Impl( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	bool					ShouldTakeDamageInCommentaryMode( const CTakeDamageInfo &inputInfo );
	int				OnTakeDamage_Impl( const CTakeDamageInfo &info );
	virtual void			DamageEffect(float flDamage, int fDamageType);

	virtual void			OnSwitchWeapons( CBaseCombatWeapon* pWeapon ){}
	virtual void			OnDamagedByExplosion( const CTakeDamageInfo &info );

	virtual bool			CanKickFromTeam( int kickTeam ) { return GetTeamNumber() == kickTeam; }

	void					PauseBonusProgress( bool bPause = true );
	void					SetBonusProgress( int iBonusProgress );
	void					SetBonusChallenge( int iBonusChallenge );

	int						GetBonusProgress() const { return m_iBonusProgress; }
	int						GetBonusChallenge() const { return m_iBonusChallenge; }

	Vector			EyePosition_Impl( );			// position of eyes
	const QAngle			&EyeAngles_Impl( );
	void					EyePositionAndVectors( Vector *pPosition, Vector *pForward, Vector *pRight, Vector *pUp );
	const QAngle	&LocalEyeAngles_Impl();		// Direction of eyes
	void					EyeVectors( Vector *pForward, Vector *pRight = nullptr, Vector *pUp = nullptr );

	// Sets the view angles
	void					SnapEyeAngles( const QAngle &viewAngles );

	virtual QAngle			BodyAngles();
	Vector			BodyTarget_Impl( const Vector &posSrc, bool bNoisy);
	virtual bool			ShouldFadeOnDeath( void ) { return false; }
	
	virtual const impactdamagetable_t &GetPhysicsImpactDamageTable();
	virtual int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void			Event_Killed_Impl( const CTakeDamageInfo &info );
	// Notifier that I've killed some other entity. (called from Victim's Event_Killed).
	void			Event_KilledOther_Impl( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	void					Event_Dying( void );

	bool					IsHLTV( void ) const { return pl.hltv; }
	bool					IsReplay( void ) const { return pl.replay; }
	bool			IsPlayer_Impl( void ) const { return true; }			// Spectators return true for this, use IsObserver to seperate cases
	bool			IsNetClient_Impl( void ) const { return true; }		// Bots should return false for this, they can't receive NET messages
																			// Spectators should return true for this

	virtual bool			IsFakeClient( void ) const;

	// Get the client index (entindex-1).
	int						GetClientIndex()	{ return ENTINDEX( edict() ) - 1; }

	// returns the player name
	const char *			GetPlayerName_Impl() const { return m_szNetname; }
		
	void					SetPlayerName( const char *name );

	virtual const char *	GetCharacterDisplayName() { return GetPlayerName(); }

	int						GetUserID() const { return engine->GetPlayerUserId( edict() ); }
	const char *			GetNetworkIDString(); 
	virtual const Vector	GetPlayerMins( void ) const; // uses local player
	virtual const Vector	GetPlayerMaxs( void ) const; // uses local player

	virtual void			UpdateCollisionBounds( void );

	void					VelocityPunch( const Vector &vecForce );
	void					ViewPunch( const QAngle &angleOffset );
	void					ViewPunchReset( float tolerance = 0 );
	void					ShowViewModel( bool bShow );
	void					ShowCrosshair( bool bShow );

	bool					ScriptIsPlayerNoclipping( void );
	virtual void			NoClipStateChanged( void ) { };

	// View model prediction setup
	void					CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );

	// Handle view smoothing when going up stairs
	void					SmoothViewOnStairs( Vector& eyeOrigin );
	virtual float			CalcRoll (const QAngle& angles, const Vector& velocity, float rollangle, float rollspeed);
	void					CalcViewRoll( QAngle& eyeAngles );
	virtual void			CalcViewBob( Vector& eyeOrigin );
	virtual void			CalcAddViewmodelCameraAnimation( Vector& eyeOrigin, QAngle& eyeAngles );

	bool			ShouldSavePhysics_Impl();
	void			OnRestore_Impl( void );

	virtual void			PackDeadPlayerItems( void );
	virtual void			RemoveAllItems( bool removeSuit );
	bool					IsDead() const;
	virtual bool			IsRunning( void ) const	{ return false; } // bot support under cstrike (AR)

	bool					HasPhysicsFlag( unsigned int flag ) { return (m_afPhysicsFlags & flag) != 0; }

	virtual	CBaseCombatCharacter *ActivePlayerCombatCharacter( void ) { return this; }

	// Weapon stuff
	virtual Vector			Weapon_ShootPosition( );
	virtual bool			Weapon_CanUse( CBaseCombatWeapon *pWeapon );
	virtual void			Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual	void			Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ );
	virtual	bool			Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	virtual void			Weapon_SetLast( CBaseCombatWeapon *pWeapon );
	virtual bool			Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon ) { return true; }
	virtual bool			Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon );
	void					Weapon_DropSlot( int weaponSlot );
	CBaseCombatWeapon		*Weapon_GetLast( void ) { return m_hLastWeapon.Get(); }

	virtual bool			HasUnlockableWeapons( int iUnlockedableIndex ) { return false; }
	bool					HasUnlockedWpn( int iIndex ) { return false; }
	bool					HasAnyAmmoOfType( int nAmmoIndex );

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void			UpdateClientData( void );
	virtual void			UpdateBattery( void );
	virtual void			RumbleEffect( unsigned char index, unsigned char rumbleData, unsigned char rumbleFlags );
	
	// Player is moved across the transition by other means
	int				ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }
	void			Precache_Impl( void );
	bool					IsOnLadder( void );
	virtual void			ExitLadder() {}
	virtual surfacedata_t	*GetLadderSurface( const Vector &origin );

	virtual void			SetFlashlightEnabled( bool bState ) { };
	virtual int				FlashlightIsOn( void ) { return false; }
	virtual bool			FlashlightTurnOn( bool playSound = false ) { return false; };		// Skip sounds when not necessary
	virtual void			FlashlightTurnOff( bool playSound = false ) { };	// Skip sounds when not necessary
	virtual bool			IsIlluminatedByFlashlight( CBaseEntity *pEntity, float *flReturnDot ) {return false; }
	
	virtual void			UpdatePlayerSound ( void );
	virtual void			UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity );
	virtual void			PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void			GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void			SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking );
	virtual void			DeathSound( const CTakeDamageInfo &info );
	const Vector &			GetMovementCollisionNormal( void ) const;	// return the normal of the surface we last collided with
	const Vector &			GetGroundNormal( void ) const;

	virtual					void SetFogController( CFogController *pFogController );

	// return the entity used for soundscape radius checks
	virtual CBaseEntity		*GetSoundscapeListener();

	Class_T					Classify_Impl ( void );
	virtual void			SetAnimation( PLAYER_ANIM playerAnim );
	virtual void			OnMainActivityComplete( Activity newActivity, Activity oldActivity ) {}
	virtual void			OnMainActivityInterrupted( Activity newActivity, Activity oldActivity ) {}
	void					SetWeaponAnimType( const char *szExtention );

	// custom player functions
	virtual void			ImpulseCommands( void );
	virtual void			CheatImpulseCommands( int iImpulse );
	virtual bool			ClientCommand( const CCommand &args );

	static CBasePlayer*		GetPlayerByPlayerID( const CPlayerId &playerUID );

	void					NotifySinglePlayerGameEnding() { m_bSinglePlayerGameEnding = true; }
	bool					IsSinglePlayerGameEnding() { return m_bSinglePlayerGameEnding == true; }

	bool					HandleVoteCommands( const CCommand &args );
	IntervalTimer &			GetLastHeldVoteTimer(){ return m_lastHeldVoteTimer; }

	CVoteController *		GetTeamVoteController( void );	// returns one of the two team vote controllers, g_voteControllerT or g_voteControllerCT
	
	// Observer functions
	virtual bool			StartObserverMode(int mode); // true, if successful
	virtual void			StopObserverMode( void );	// stop spectator mode
	virtual bool			ModeWantsSpectatorGUI( int iMode ) { return true; }
	virtual bool			SetObserverMode(int mode); // sets new observer mode, returns true if successful
	virtual int				GetObserverMode( void ); // returns observer mode or OBS_NONE
	virtual bool			SetObserverTarget(CBaseEntity * target);
	virtual void			ObserverUse( bool bIsPressed ); // observer pressed use
	virtual CBaseEntity		*GetObserverTarget( void ); // returns players targer or NULL
	virtual CBaseEntity		*FindNextObserverTarget( bool bReverse ); // returns next/prev player to follow or NULL
	virtual int				GetNextObserverSearchStartPoint( bool bReverse ); // Where we should start looping the player list in a FindNextObserverTarget call
	virtual bool			PassesObserverFilter( const CBaseEntity *entity );	// returns true if the entity passes the specified filter
	virtual bool			IsValidObserverTarget(CBaseEntity * target); // true, if player is allowed to see this target
	virtual void			CheckObserverSettings(); // checks, if target still valid (didn't die etc)
	virtual void			JumptoPosition(const Vector &origin, const QAngle &angles);
	virtual void			SpecLerptoPosition(const Vector &origin, const QAngle &angles, float flTime);
	virtual void			ForceObserverMode(int mode); // sets a temporary mode, force because of invalid targets
	virtual void			ResetObserverMode(); // resets all observer related settings
	virtual void			ValidateCurrentObserverTarget( void ); // Checks the current observer target, and moves on if it's not valid anymore
	virtual void			AttemptToExitFreezeCam( void );

	virtual bool			StartReplayMode( float fDelay, float fDuration, int iEntity );
	virtual void			StopReplayMode();
	virtual int				GetDelayTicks();
	virtual int				GetReplayEntity();

	CLogicPlayerProxy		*GetPlayerProxy( void );
	void					FirePlayerProxyOutput( const char *pszOutputName, variant_t variant, CBaseEntity *pActivator, CBaseEntity *pCaller );

	virtual void			CreateCorpse( void ) { }
	virtual CBaseEntity		*EntSelectSpawnPoint( void );

	// Vehicles
	
	// override these for 
	
	void					AddPoints( int score, bool bAllowNegativeScore );
	void					AddPointsToTeam( int score, bool bAllowNegativeScore );
	virtual bool			BumpWeapon( CBaseCombatWeapon *pWeapon );
	bool					RemovePlayerItem( CBaseCombatWeapon *pItem );
	CBaseEntity				*HasNamedPlayerItem( const char *pszItemName );
	bool 					HasWeapons( void );// do I have ANY weapons?
	virtual void			SelectLastItem(void);
	virtual void 			SelectItem( const char *pstr, int iSubType = 0 );
	void					ItemPreFrame( void );
	virtual void			ItemPostFrame( void );

	virtual CBaseEntity		*GiveNamedItem( const char *pchName, int iSubType = 0, CEconItemView *pScriptItem = nullptr, bool bForce = false );

	void					EnableControl(bool fControl);
	virtual void			CheckTrainUpdate( void );
	void					AbortReload( void );

	void					SendAmmoUpdate(void);

	void					WaterMove( void );
	float					GetWaterJumpTime() const;
	void					SetWaterJumpTime( float flWaterJumpTime );
	float					GetSwimSoundTime( void ) const;
	void					SetSwimSoundTime( float flSwimSoundTime );

	virtual void			SetPlayerUnderwater( bool state );
	void					UpdateUnderwaterState( void );
	bool					IsPlayerUnderwater( void ) { return m_bPlayerUnderwater; }

	virtual bool			CanBreatheUnderwater() const { return false; }
	virtual bool			CanRecoverCurrentDrowningDamage( void ) const { return true; }// Are we allowed to later recover the drowning damage we are taking right now?  (Not, can we right now recover drowning damage.)
	virtual void			PlayerUse( void );
	virtual void			PlayUseDenySound() {}

	virtual CBaseEntity		*FindUseEntity( void );
	virtual bool			IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );
	bool					ClearUseEntity();
	CBaseEntity				*DoubleCheckUseNPC( CBaseEntity *pNPC, const Vector &vecSrc, const Vector &vecDir );

	// physics interactions
	// mass/size limit set to zero for none
	static bool				CanPickupObject( CBaseEntity *pObject, float massLimit, float sizeLimit );
	virtual void			PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize = true ) {}
	virtual void			ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldindThis = nullptr ) {}
	virtual float			GetHeldObjectMass( IPhysicsObject *pHeldObject );

	void					CheckSuitUpdate();
	void					SetSuitUpdate(const char *name, int fgroup, int iNoRepeat);
	virtual void			UpdateGeigerCounter( void );
	void					CheckTimeBasedDamage( void );

	void					ResetAutoaim( void );
	
	virtual Vector			GetAutoaimVector( float flScale );
	virtual Vector			GetAutoaimVector( float flScale, float flMaxDist );
	virtual Vector			GetAutoaimVector( float flScale, float flMaxDist, float flMaxDeflection, AimResults *pAimResults );
	virtual void			GetAutoaimVector( autoaim_params_t &params );

	virtual bool			ShouldAutoaim( void );

	void					SetViewEntity( CBaseEntity *pEntity, bool bShouldDrawPlayer = true );
	CBaseEntity				*GetViewEntity( void ) { return m_hViewEntity; }

	virtual void			ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.

	void					DeathMessage( CBaseEntity *pKiller );

	virtual void			ProcessUsercmds( CUserCmd *cmds, int numcmds, int totalcmds,
								int dropped_packets, bool paused );
	bool					HasQueuedUsercmds( void ) const;

	void					AvoidPhysicsProps( CUserCmd *pCmd );

	// Run a user command. The default implementation calls ::PlayerRunCommand. In TF, this controls a vehicle if
	// the player is in one.
	virtual void			PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper);
	void					RunNullCommand();

	// Team Handling
	void			ChangeTeam_Impl( int iTeamNum ) { ChangeTeam(iTeamNum,false, false); }
	using BaseClass::ChangeTeam;
	virtual void	ChangeTeam( int iTeamNum, bool bAutoTeam, bool bSilent );

	// say/sayteam allowed?
	virtual bool			CanHearAndReadChatFrom( CBasePlayer *pPlayer ) { return true; }
	virtual bool			CanSpeak( void ) { return true; }

	audioparams_t			&GetAudioParams() { return *m_Local.m_audio.Get(); }

	virtual void 			ModifyOrAppendPlayerCriteria( AI_CriteriaSet& set );

	virtual QAngle			GetViewPunchAngle();
	void					SetViewPunchAngle( const QAngle &punchAngle );
	void					SetViewPunchAngle( int axis, float value );
	virtual QAngle			GetAimPunchAngle();
	void					SetAimPunchAngle( const QAngle &angle );
	void					SetAimPunchAngleVelocity( const QAngle &punchAngleVelocity );

	// TrackIR
	const Vector&		GetEyeOffset() const;
	void				SetEyeOffset( const Vector& v );

	const QAngle &		GetEyeAngleOffset() const;
	void				SetEyeAngleOffset( const QAngle & qa );
	// TrackIR

	const Vector &		GetAimDirection() const;
	void				SetAimDirection( const Vector & v );

	bool				IsCoach( void ) const;
	int					GetCoachingTeam( void ) const;

	int					GetAssociatedTeamNumber( void ) const; // Returns coaching team if player is a coach. Otherwise returns GetTeamNumber.

	bool				IsSpectator( void ) const; // is TEAM_SPECTATOR and is not a coach.

	// Returns the eye or pointer angle plus the punch angle.
	QAngle					GetFinalAimAngle();

	void					PropagatePunchAnglesToObservers( void );

	virtual void			DoMuzzleFlash();

	CNavArea				*GetLastKnownArea( void ) const		{ return m_lastNavArea; }		// return the last nav area the player occupied - NULL if unknown
	const char				*GetLastKnownPlaceName( void ) const	{ return m_szLastPlaceName; }	// return the last nav place name the player occupied

	virtual void			CheckChatText( char *p, int bufsize ) {}

	virtual void			CreateRagdollEntity( void ) { return; }

	virtual void			HandleAnimEvent( animevent_t *pEvent );

	CBaseEntity				*GetTonemapController( void ) const
	{
		return m_hTonemapController.Get();
	}

	virtual bool			ShouldAnnounceAchievement( void ){ return true; }

	bool					IsSplitScreenPartner( CBasePlayer *pPlayer );
	void					SetSplitScreenPlayer( bool bSplitScreenPlayer, CBasePlayer *pOwner );
	bool					IsSplitScreenPlayer() const;
	CBasePlayer				*GetSplitScreenPlayerOwner();
	bool					IsSplitScreenUserOnEdict( edict_t *edict );
	int						GetSplitScreenPlayerSlot();

	void					AddSplitScreenPlayer( CBasePlayer *pOther );
	void					RemoveSplitScreenPlayer( CBasePlayer *pOther );
	std::vector< CHandle< CBasePlayer > >& GetSplitScreenPlayers();
	bool					HasAttachedSplitScreenPlayers() const;

	void					AddPictureInPicturePlayer( CBasePlayer *pOther );
	void					RemovePictureInPicturePlayer( CBasePlayer *pOther );
	std::vector< CHandle< CBasePlayer > >& GetSplitScreenAndPictureInPicturePlayers();
	std::vector< CHandle< CBasePlayer > >& GetPictureInPicturePlayers( void );

	void					SetCrossPlayPlatform( CrossPlayPlatform_t clientPlatform );
	CrossPlayPlatform_t		GetCrossPlayPlatform( void ) const;

	// Returns true if team was changed
	virtual bool			EnsureSplitScreenTeam();

	virtual void			ForceChangeTeam( int iTeamNum ) { }

	void					UpdateFXVolume( void );

	// ===============================================================
	// player's client platform and input device selection
	void					SetPlayerInputDevice( InputDevice_t controller )	{ m_PlayerInputDevice = controller; }
	InputDevice_t			GetPlayerInputDevice( void )						{ return m_PlayerInputDevice; }
	
	void					SetPlayerPlatform( InputDevicePlatform_t platform ) { m_PlayerPlatform = platform; }
	InputDevicePlatform_t	GetPlayerPlatform( void )							{ return m_PlayerPlatform; }

	void					SetLastRequestedClientInfoTime( float newTime )		{ m_lastRequestedClientInfoTime = newTime; }
	float					GetLastRequestedClientInfoTime( void )				{ return m_lastRequestedClientInfoTime; }

public:
	// Player Physics Shadow
	void					SetupVPhysicsShadow( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, CPhysCollide *pStandModel, const char *pStandHullName, CPhysCollide *pCrouchModel, const char *pCrouchHullName );
	IPhysicsPlayerController* GetPhysicsController() { return m_pPhysicsController; }
	void			VPhysicsCollision_Impl( int index, gamevcollisionevent_t *pEvent );
	void					VPhysicsUpdate_Impl( IPhysicsObject *pPhysics );
	void			VPhysicsShadowUpdate_Impl( IPhysicsObject *pPhysics );
	virtual bool			IsFollowingPhysics( void ) { return false; }
	bool					IsRideablePhysics( IPhysicsObject *pPhysics );
	IPhysicsObject			*GetGroundVPhysics();

	void			Touch_Impl( CBaseEntity *pOther );
	void					SetTouchedPhysics( bool bTouch );
	bool					TouchedPhysics( void );
	Vector					GetSmoothedVelocity_Impl( void );

	virtual void			InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );
	void			VPhysicsDestroyObject_Impl();
	void					SetVCollisionState( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity, int collisionState );
	void					PostThinkVPhysics( void );
	virtual void			UpdatePhysicsShadowToCurrentPosition();
	void					UpdatePhysicsShadowToPosition( const Vector &vecAbsOrigin );
	void					UpdateVPhysicsPosition( const Vector &position, const Vector &velocity, float secondsToArrival );

	// Hint system
	virtual CHintSystem		*Hints( void ) { return nullptr; }
	bool					ShouldShowHints( void ) { return Hints() ? Hints()->ShouldShowHints() : false; }
	void					SetShowHints( bool bShowHints ) { if (Hints()) Hints()->SetShowHints( bShowHints ); }
	bool 					HintMessage( int hint, bool bForce = false ) { return Hints() ? Hints()->HintMessage( hint, bForce ) : false; }
	void 					HintMessage( const char *pMessage ) { if (Hints()) Hints()->HintMessage( pMessage ); }
	void					StartHintTimer( int iHintID ) { if (Hints()) Hints()->StartHintTimer( iHintID ); }
	void					StopHintTimer( int iHintID ) { if (Hints()) Hints()->StopHintTimer( iHintID ); }
	void					RemoveHintTimer( int iHintID ) { if (Hints()) Hints()->RemoveHintTimer( iHintID ); }

	// Accessor methods
	int		FragCount() const		{ return m_iFrags; }
	int		AssistsCount() const	{ return m_iAssists; }
	int		DeathCount() const		{ return m_iDeaths;}
	bool	IsConnected() const		{ return m_iConnected != PlayerDisconnected; }
	bool	IsDisconnecting() const	{ return m_iConnected == PlayerDisconnecting; }
	bool	IsSuitEquipped() const	{ return m_Local.m_bWearingSuit; }
	int		ArmorValue() const		{ return m_ArmorValue; }
	bool	HUDNeedsRestart() const { return m_fInitHUD; }
	float	MaxSpeed() const		{ return m_flMaxspeed; }
	Activity GetActivity( ) const	{ return m_Activity; }
	inline void SetActivity( Activity eActivity ) { m_Activity = eActivity; }
	bool	IsPlayerLockedInPlace() const { return m_iPlayerLocked != 0; }
	bool	IsObserver() const		{ return (m_afPhysicsFlags & PFLAG_OBSERVER) != 0; }
	bool	IsOnTarget() const		{ return m_fOnTarget; }
	float	MuzzleFlashTime() const { return m_flFlashTime; }
	float	PlayerDrownTime() const	{ return m_AirFinished; }

	void	SetPlayerLocked( int nLock )					{ m_iPlayerLocked = nLock; }
	int		GetPlayerLocked( void )							{ return m_iPlayerLocked; }

	int		GetObserverMode() const	{ return m_iObserverMode; }
	CBaseEntity *GetObserverTarget() const	{ return m_hObserverTarget; }
	bool	IsActiveCameraMan() const { return m_bActiveCameraMan; }
	void	SetActiveCameraMan( bool bState ) { m_bActiveCameraMan = bState; }

	// Round gamerules
	virtual bool	IsReadyToPlay( void ) { return true; }
	virtual bool	IsReadyToSpawn( void ) { return true; }
	virtual bool	ShouldGainInstantSpawn( void ) { return false; }
	virtual void	ResetPerRoundStats( void ) { return; }
	void			AllowInstantSpawn( void ) { m_bAllowInstantSpawn = true; }

	virtual void	ResetScores( void ) { ResetFragCount(); ResetAssistsCount(); ResetDeathCount(); }
	void			ResetFragCount();
	virtual void	IncrementFragCount( int nCount, int nHeadshots = 0 );
	
	void			ResetAssistsCount();
	virtual void	IncrementAssistsCount( int nCount );

	void			ResetDeathCount();
	virtual void	IncrementDeathCount( int nCount );

	void	SetArmorValue( int value );
	void	IncrementArmorValue( int nCount, int nMaxValue = -1 );

	void	SetConnected( PlayerConnectedState iConnected ) { m_iConnected = iConnected; }
	virtual void EquipSuit( bool bPlayEffects = true );
	virtual void RemoveSuit( void );
	void	SetMaxSpeed( float flMaxSpeed ) { m_flMaxspeed = flMaxSpeed; }

	void	NotifyNearbyRadiationSource( float flRange );

	void	SetAnimationExtension( const char *pExtension );

	void	SetAdditionalPVSOrigin( const Vector &vecOrigin );
	void	SetCameraPVSOrigin( const Vector &vecOrigin );
	void	SetMuzzleFlashTime( float flTime );
	void	SetDropEnabled( bool bEnabled );
	void	SetDuckEnabled( bool bEnabled );
	void	SetUseEntity( CBaseEntity *pUseEntity );
	virtual CBaseEntity* GetUseEntity( void );
	virtual CBaseEntity* GetPotentialUseEntity( void );

	virtual float GetPlayerMaxSpeed();

	// Used to set private physics flags PFLAG_*
	void	SetPhysicsFlag( int nFlag, bool bSet );

	void	AllowImmediateDecalPainting();
	void	PushAwayDecalPaintingTime( float flTime );

	// Suicide...
	virtual void CommitSuicide( bool bExplode = false, bool bForce = false );
	virtual void CommitSuicide( const Vector &vecForce, bool bExplode = false, bool bForce = false );

	// For debugging...
	void	ForceOrigin( const Vector &vecOrigin );

	// Bot accessors...
	void	SetTimeBase( float flTimeBase );
	float	GetTimeBase() const;
	void	SetLastUserCommand( const CUserCmd &cmd );
	const CUserCmd *GetLastUserCommand( void );
	virtual bool IsBot() const;

	bool	IsPredictingWeapons( void ) const; 
	int		CurrentCommandNumber() const;
	const CUserCmd *GetCurrentUserCommand() const;
	int		GetLockViewanglesTickNumber() const { return m_iLockViewanglesTickNumber; }
	QAngle	GetLockViewanglesData() const { return m_qangLockViewangles; }

	int		GetFOV( void );														// Get the current FOV value
	int		GetDefaultFOV( void ) const;										// Default FOV if not specified otherwise
	int		GetFOVForNetworking( void );										// Get the current FOV used for network computations
	bool	SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate = 0.0f, int iZoomStart = 0 );	// Alters the base FOV of the player (must have a valid requester)
	void	SetDefaultFOV( int FOV );											// Sets the base FOV if nothing else is affecting it by zooming
	CBaseEntity *GetFOVOwner( void ) { return m_hZoomOwner; }
	float	GetFOVDistanceAdjustFactor(); // shared between client and server
	float	GetFOVDistanceAdjustFactorForNetworking();

	int		GetImpulse( void ) const { return m_nImpulse; }

	// Movement constraints
	void	ActivateMovementConstraint( CBaseEntity *pEntity, const Vector &vecCenter, float flRadius, float flConstraintWidth, float flSpeedFactor, bool constraintPastRadius = false );
	void	DeactivateMovementConstraint( );

	// talk control
	void	NotePlayerTalked() { m_fLastPlayerTalkTime = gpGlobals->curtime; }
	float	LastTimePlayerTalked() { return m_fLastPlayerTalkTime; }

	void	DisableButtons( int nButtons );
	void	EnableButtons( int nButtons );
	void	ForceButtons( int nButtons );
	void	UnforceButtons( int nButtons );

	//---------------------------------
	// Inputs
	//---------------------------------
	[[= ks::reflect::Input{ .name = "SetHealth", .type = FIELD_INTEGER } ]] void	InputSetHealth( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetHUDVisibility", .type = FIELD_BOOLEAN } ]] void	InputSetHUDVisibility( inputdata_t &inputdata );

	surfacedata_t *GetSurfaceData( void ) const { return m_pSurfaceData; }
	void SetLadderNormal( Vector vecLadderNormal ) { m_vecLadderNormal = vecLadderNormal; }
	const Vector &GetLadderNormal( void ) const { return m_vecLadderNormal; }
	// If a derived class extends ImpulseCommands() and changes an existing impulse, it'll need to clear out the impulse.
	void ClearImpulse( void ) { m_nImpulse = 0; }

	// Here so that derived classes can use the expresser
	virtual CAI_Expresser *GetExpresser() { return nullptr; };

	//----------------------------
	// Player ID handling
	bool		GetPlayerID( CPlayerId *pID, bool bRequireFullyAuthenticated = false );
	uint64		GetPlayerIDAsUInt64( void );

	void				IncrementEFNoInterpParity();
	int					GetEFNoInterpParity() const;

private:
	
	// For queueing up CUserCmds and running them from PhysicsSimulate
	int					GetCommandContextCount( void ) const;
	CCommandContext		*GetCommandContext( int index );
	CCommandContext		*AllocCommandContext( void );
	void				RemoveCommandContext( int index );
	void				RemoveAllCommandContexts( void );
	CCommandContext		*RemoveAllCommandContextsExceptNewest( void );
	void				ReplaceContextCommands( CCommandContext *ctx, CUserCmd *pCommands, int nCommands );

	int					DetermineSimulationTicks( void );
	void				AdjustPlayerTimeBase( int simulation_ticks );
	void				UpdateSplitScreenAndPictureInPicturePlayerList();

public:
	
	// Used by gamemovement to check if the entity is stuck.
	
	// FIXME: Make these protected or private!
	
	CNetworkVar( float, m_flDuckAmount, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_CHANGES_OFTEN } ]] );
	CNetworkVar( float, m_flDuckSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_CHANGES_OFTEN } ]] );
	Vector2D m_vecLastPositionAtFullCrouchSpeed;

	int m_nSuicides;

	// This player's data that should only be replicated to 
	//  the player and not to other players.
	CNetworkVarEmbedded( CPlayerLocalData, m_Local, [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] );

	CNetworkVarEmbedded( fogplayerparams_t, m_PlayerFog );
	void InitFogController( void );
	[[= ks::reflect::Input{ .name = "SetFogController", .type = FIELD_STRING } ]] void InputSetFogController( inputdata_t &inputdata );

	void OnTonemapTriggerStartTouch( CTonemapTrigger *pTonemapTrigger );
	void OnTonemapTriggerEndTouch( CTonemapTrigger *pTonemapTrigger );
	std::vector< CHandle< CTonemapTrigger > > m_hTriggerTonemapList;

	CNetworkHandle( CPostProcessController, m_hPostProcessCtrl, [[= ks::reflect::Net{} ]] );	// active postprocessing controller
	CNetworkHandle( CColorCorrection, m_hColorCorrectionCtrl, [[= ks::reflect::Net{} ]] );		// active FXVolume color correction
	void InitPostProcessController( void );
	void InputSetPostProcessController( inputdata_t &inputdata );
	void InitColorCorrectionController( void );
	void InputSetColorCorrectionController( inputdata_t &inputdata );

	// Used by env_soundscape_triggerable to manage when the player is touching multiple
	// soundscape triggers simultaneously.
	// The one at the HEAD of the list is always the current soundscape for the player.
	std::vector< EHANDLE > m_hTriggerSoundscapeList;

	// Player data that's sometimes needed by the engine
	CNetworkVarEmbedded( CPlayerState, pl, [[= ks::reflect::Net{} ]] );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecViewOffset );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_flFriction );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iAmmo );
	
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_hGroundEntity );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_lifeState );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecBaseVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nNextThinkTick );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nWaterLevel );

	CNetworkVar( int, m_iCoachingTeam, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] );	// When on TEAM_SPECTATOR, is this player restricted to a team, aka 'coaching' a team
	
	int						m_nButtons;
	int						m_afButtonPressed;
	int						m_afButtonReleased;
	int						m_afButtonLast;
	int						m_afButtonDisabled;	// A mask of input flags that are cleared automatically
	int						m_afButtonForced;	// These are forced onto the player's inputs

	CNetworkVar( bool, m_fOnTarget, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED, .table = "DT_LocalPlayerExclusive" } ]] );		//Is the crosshair on a target?

	char					m_szAnimExtension[32];

	int						m_nUpdateRate;		// user snapshot rate cl_updaterate
	float					m_fLerpTime;		// users cl_interp
	bool					m_bLagCompensation;	// user wants lag compenstation
	bool					m_bPredictWeapons; //  user has client side predicted weapons
	bool					m_bPredictionEnabled; // user has prediction enabled
	
	float		GetDeathTime( void ) { return m_flDeathTime; }

	void		ClearZoomOwner( void );

	void		SetPreviouslyPredictedOrigin( const Vector &vecAbsOrigin );
	const Vector &GetPreviouslyPredictedOrigin() const;
	float		GetFOVTime( void ){ return m_flFOVTime; }

	void		AdjustDrownDmg( int nAmount );

private:

	Activity				m_Activity;

protected:

	virtual void			CalcPlayerView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void					CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void					CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles);

	// FIXME: Make these private! (tf_player uses them)

	// Secondary point to derive PVS from when zoomed in with binoculars/sniper rifle.  The PVS is 
	//  a merge of the standing origin and this additional origin
	Vector					m_vecAdditionalPVSOrigin; 
	// Extra PVS origin if we are using a camera object
	Vector					m_vecCameraPVSOrigin;

	bool					m_bIsSpecLerping;
	float					m_flSpecLerpTime;
	float					m_flSpecLerpEndTime;
	Vector					m_vecSpecLerpIdealPos;
	QAngle					m_angSpecLerpIdealAng;
	Vector					m_vecSpecLerpOldPos;
	QAngle					m_angSpecLerpOldAng;

	bool					m_bDropEnabled;
	bool					m_bDuckEnabled;
	CNetworkHandle( CBaseEntity, m_hUseEntity, [[= ks::reflect::Net{} ]] );			// the player is currently controlling this entity because of +USE latched, NULL if no entity

	int						m_iTrain;				// Train control position

	float					m_iRespawnFrames;	// used in PlayerDeathThink() to make sure players can always respawn
 	CNetworkVar( unsigned int, m_afPhysicsFlags, [[= ks::reflect::Net{ .bits = 6, .flags = SPROP_UNSIGNED } ]] );	// physics flags - set when 'normal' physics should be revisited or overriden
	
	// Vehicles

	void					UpdateButtonState( int nUserCmdButtonMask );

	bool	m_bPauseBonusProgress;
	CNetworkVar( int, m_iBonusProgress, [[= ks::reflect::Net{ .bits = 15 } ]] );
	CNetworkVar( int, m_iBonusChallenge, [[= ks::reflect::Net{ .bits = 4 } ]] );

	float m_flTimeLastTouchedGround;

	int						m_lastDamageAmount;		// Last damage taken
	float					m_fTimeLastHurt;

	Vector					m_DmgOrigin;
	float					m_DmgTake;
	float					m_DmgSave;
	int						m_bitsDamageType;	// what types of damage has player taken
	int						m_bitsHUDDamage;	// Damage bits for the current fame. These get sent to the hud via gmsgDamage

	CNetworkVar( float, m_flDeathTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );		// the time at which the player died  (used in PlayerDeathThink())
	float					m_flDeathAnimTime;	// the time at which the player finished their death anim (used in PlayerDeathThink() and ShouldTransmit())

	CNetworkVar( float, m_fForceTeam, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );			// the time at which the player will be forced onto a team ( use in PlayerForceTeamThink())

	CNetworkVar( int, m_iObserverMode, [[= ks::reflect::Net{ .bits = 3, .flags = SPROP_UNSIGNED } ]] );	// if in spectator mode != 0
	CNetworkVar( bool, m_bActiveCameraMan, [[= ks::reflect::Net{} ]] );		// the player is an active cameraman for gotv viewers.
	CNetworkVar( bool, m_bCameraManXRay, [[= ks::reflect::Net{} ]] );			// XRay state for cameraman
	CNetworkVar( bool, m_bCameraManOverview, [[= ks::reflect::Net{} ]] );		// Overview state for cameraman
	CNetworkVar( bool, m_bCameraManScoreBoard, [[= ks::reflect::Net{} ]] );	// ScoreBoard state for cameraman
	CNetworkVar( uint8, m_uCameraManGraphs, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );		// Graphs state for cameraman
	CNetworkVar( int,	m_iFOV, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED|SPROP_CHANGES_OFTEN } ]] );			// field of view
	CNetworkVar( int,	m_iDefaultFOV, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );	// default field of view
	CNetworkVar( int,	m_iFOVStart, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );		// What our FOV started at
	CNetworkVar( float,	m_flFOVTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_CHANGES_OFTEN } ]] );		// Time our FOV change started
	
	int						m_iObserverLastMode; // last used observer mode
	CNetworkHandle( CBaseEntity, m_hObserverTarget, [[= ks::reflect::Net{} ]] );	// entity handle to m_iObserverTarget
	bool					m_bForcedObserverMode; // true, player was forced by invalid targets to switch mode
	
	CNetworkHandle( CBaseEntity, m_hZoomOwner, [[= ks::reflect::Net{} ]] );	//This is a pointer to the entity currently controlling the player's zoom
													//Only this entity can change the zoom state once it has ownership

	float					m_tbdPrev;				// Time-based damage timer
	int						m_idrowndmg;			// track drowning damage taken
	int						m_idrownrestored;		// track drowning damage restored
	int						m_nPoisonDmg;			// track recoverable poison damage taken
	int						m_nPoisonRestored;		// track poison damage restored
	// NOTE: bits damage type appears to only be used for time-based damage
	BYTE					m_rgbTimeBasedDamage[CDMG_TIMEBASED];

	// Player Physics Shadow
	CNetworkVar( int, m_vphysicsCollisionState, [[= ks::reflect::Net{} ]] );

	virtual int SpawnArmorValue( void ) const { return 0; }

	float					m_fNextSuicideTime; // the time after which the player can next use the suicide command
	int						m_iSuicideCustomKillFlags;

	// Replay mode	
	float					m_fDelay;			// replay delay in seconds
	float					m_fReplayEnd;		// time to stop replay mode
	int						m_iReplayEntity;	// follow this entity in replay

	virtual void UpdateTonemapController( void );
	CNetworkHandle( CBaseEntity, m_hTonemapController, [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] );

	bool m_bKilledByHeadshot;

public:
	CNetworkVar( int, m_iDeathPostEffect, [[= ks::reflect::Net{} ]] );		// which deathcam post effect to use

private:
	void HandleFuncTrain();

// DATA
private:
	std::vector< CCommandContext > m_CommandContext;
	// Player Physics Shadow

protected: //used to be private, but need access for portal mod (Dave Kircher)
	IPhysicsPlayerController	*m_pPhysicsController;
	IPhysicsObject				*m_pShadowStand;
	IPhysicsObject				*m_pShadowCrouch;
	Vector						m_oldOrigin;
	Vector						m_vecSmoothedVelocity;
	bool						m_bTouchedPhysObject;
	bool						m_bPhysicsWasFrozen;
	CNetworkVar( float, m_flNextDecalTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );	// next time this player can spray a decal
	bool			m_bNextDecalTimeExpedited;	// whether decal time was shortened already

private:

	int						m_iPlayerSound;// the index of the sound list slot reserved for this player
	int						m_iTargetVolume;// ideal sound volume. 
	
	int						m_rgItems[MAX_ITEMS];

	// Voting info
	IntervalTimer 			m_lastHeldVoteTimer;	///< How long since we last created a vote.  Prevents vote spam.

	// these are time-sensitive things that we keep track of
	float					m_flSwimTime;		// how long player has been underwater
	float					m_flDuckTime;		// how long we've been ducking
	float					m_flDuckJumpTime;	

	float					m_flSuitUpdate;					// when to play next suit update
	int						m_rgSuitPlayList[CSUITPLAYLIST];// next sentencenum to play for suit update
	int						m_iSuitPlayNext;				// next sentence slot for queue storage;
	int						m_rgiSuitNoRepeat[CSUITNOREPEAT];		// suit sentence no repeat list
	float					m_rgflSuitNoRepeatTime[CSUITNOREPEAT];	// how long to wait before allowing repeat

	float					m_flgeigerRange;		// range to nearest radiation source
	float					m_flgeigerDelay;		// delay per update of range msg to client
	int						m_igeigerRangePrev;

	bool					m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	bool					m_fGameHUDInitialized;
	bool					m_fWeapon;				// Set this to false to force a reset of the current weapon HUD info

	int						m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int						m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new

	// Autoaim data
	QAngle					m_vecAutoAim;

	// Team Handling
	// char					m_szTeamName[TEAM_NAME_LENGTH];

	// Multiplayer handling
	PlayerConnectedState	m_iConnected;

	// from edict_t
	// CBasePlayer doesn't send this but CCSPlayer does.
	CNetworkVarForDerived( int, m_ArmorValue );
	float					m_AirFinished;
	float					m_PainFinished;

	// player locking
	int						m_iPlayerLocked;

	CSimpleSimTimer			m_AutoaimTimer;

protected:
	int						m_iFrags;
	int						m_iAssists;
	int						m_iDeaths;

	// the player's personal view model
	typedef CHandle<CBaseViewModel> CBaseViewModelHandle;
	CNetworkArray( CBaseViewModelHandle, m_hViewModel, MAX_VIEWMODELS, [[= ks::reflect::Net{ .varlen = true } ]] );

	// Last received usercmd (in case we drop a lot of packets )
	CUserCmd				m_LastCmd;
	CUserCmd				*m_pCurrentCommand;
	int						m_iLockViewanglesTickNumber;
	QAngle					m_qangLockViewangles;


	bool					m_bAllowInstantSpawn;

	// Input device info
	InputDevice_t			m_PlayerInputDevice;
	InputDevicePlatform_t	m_PlayerPlatform;
	float					m_lastRequestedClientInfoTime;

private:

// Replicated to all clients
	CNetworkVar( float, m_flMaxspeed, [[= ks::reflect::Net{ .bits = 12, .low = 0.0f, .high = 2048.0f, .flags = SPROP_ROUNDDOWN } ]] );
	CNetworkVar( int, m_ladderSurfaceProps, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVector( m_vecLadderNormal, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NORMAL } ]] );	// Clients may need this for climbing anims
	
protected:
// Not transmitted
	int						m_nImpulse;
	
private:
	float					m_flFlashTime;
	int						m_nDrownDmgRate;		// Drowning damage in points per second without air.

	int						m_nNumCrouches;			// Number of times we've crouched (for hinting)
	bool					m_bDuckToggled;		// If true, the player is crouching via a toggle

public:
	bool					GetToggledDuckState( void ) { return m_bDuckToggled; }
	void					ToggleDuck( void );
	float					GetStickDist( void );

	float					m_flForwardMove;
	float					m_flSideMove;
	int						m_nNumCrateHudHints;

private:

	// Used in test code to teleport the player to random locations in the map.
	Vector					m_vForcedOrigin;
	bool					m_bForceOrigin;	

	// Clients try to run on their own realtime clock, this is this client's clock
	CNetworkVar( int, m_nTickBase, [[= ks::reflect::Net{ .bits = -1, .flags = 0, .table = "DT_LocalPlayerExclusive", .priority = SENDPROP_TICKBASE_PRIORITY } ]] );

	bool					m_bGamePaused;
	float					m_fLastPlayerTalkTime;
	
	CNetworkVar( CBaseCombatWeaponHandle, m_hLastWeapon, [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] );

	std::vector< CHandle< CBaseEntity > > m_SimulatedByThisPlayer;

	float					m_flOldPlayerZ;
	float					m_flOldPlayerViewOffsetZ;

	bool					m_bPlayerUnderwater;

	CNetworkHandle( CBaseEntity, m_hViewEntity, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bShouldDrawPlayerWhileUsingViewEntity, [[= ks::reflect::Net{} ]] );

	// Movement constraints
	CNetworkHandle( CBaseEntity, m_hConstraintEntity, [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] );
	CNetworkVector( m_vecConstraintCenter, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );
	CNetworkVar( float, m_flConstraintRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );
	CNetworkVar( float, m_flConstraintWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );
	CNetworkVar( float, m_flConstraintSpeedFactor, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );
	CNetworkVar( bool, m_bConstraintPastRadius, [[= ks::reflect::Net{ .table = "DT_LocalPlayerExclusive" } ]] );

	friend class CPlayerMove;
	friend class CPlayerClass;
	friend class CASW_PlayerMove;
	friend class CDOTAPlayerMove;
	friend class CPaintPlayerMove;

	// Player name
	char					m_szNetname[MAX_PLAYER_NAME_LENGTH];

protected:
	// HACK FOR TF2 Prediction
	friend class CTFGameMovementRecon;
	friend class CTFGameMovement;
	friend class CHL1GameMovement;
	friend class CCSGameMovement;	
	friend class CHL2GameMovement;
	friend class CPortalGameMovement;
	friend class CASW_MarineGameMovement;
	
	// Accessors for gamemovement
	bool IsDucked( void ) const { return m_Local.m_bDucked; }
	bool IsDucking( void ) const { return m_Local.m_bDucking; }
	float GetStepSize( void ) const { return m_Local.m_flStepSize; }

	CNetworkVar( float,  m_flLaggedMovementValue, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .table = "DT_LocalPlayerExclusive" } ]] );

	// These are generated while running usercmds, then given to UpdateVPhysicsPosition after running all queued commands.
	Vector m_vNewVPhysicsPosition;
	Vector m_vNewVPhysicsVelocity;
	

	Vector m_vecPreviouslyPredictedOrigin; // Used to determine if non-gamemovement game code has teleported, or tweaked the player's origin
	int		m_nBodyPitchPoseParam;

	CNetworkString( m_szLastPlaceName, MAX_PLACE_NAME_LENGTH, [[= ks::reflect::Net{} ]] );
	unsigned int    m_nTicksSinceLastPlaceUpdate;

	char m_szNetworkIDString[MAX_NETWORKID_LENGTH];
	CPlayerInfo m_PlayerInfo;

	// Texture names and surface data, used by CGameMovement

	bool			m_bSinglePlayerGameEnding;

	CNetworkVar( int, m_ubEFNoInterpParity, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );	// bits: NOINTERP_PARITY_MAX_BITS

	EHANDLE			m_hPlayerProxy;	// Handle to a player proxy entity for quicker reference

public:

	float  GetLaggedMovementValue( void ){ return m_flLaggedMovementValue;	}
	void   SetLaggedMovementValue( float flValue ) { m_flLaggedMovementValue = flValue;	}

	inline bool IsAutoKickDisabled( void ) const;
	inline void DisableAutoKick( bool disabled );

	void	DumpPerfToRecipient( CBasePlayer *pRecipient, int nMaxRecords );

	// picker debug utility functions
	virtual CBaseEntity*	FindEntityClassForward( char *classname );
	virtual CBaseEntity*	FindEntityForward( bool fHull );
	virtual CBaseEntity*	FindPickerEntityClass( char *classname );
	virtual CBaseEntity*	FindPickerEntity();

	void PrepareForFullUpdate( void );

	virtual void OnSpeak( CBasePlayer *actor, const char *sound, float duration ) {}

	// A voice packet from this client was received by the server
	virtual void OnVoiceTransmit( void ) {}

	float GetRemainingMovementTimeForUserCmdProcessing() const { return m_flMovementTimeForUserCmdProcessingRemaining; }
	float ConsumeMovementTimeForUserCmdProcessing( float flTimeNeeded );

private:
	// How much of a movement time buffer can we process from this user?
	float				m_flMovementTimeForUserCmdProcessingRemaining;

public:
	float				GetInitialSpawnTime() const { return m_flInitialSpawnTime; }
private:
	float				m_flInitialSpawnTime;

	bool m_autoKickDisabled;

	struct StepSoundCache_t
	{
		StepSoundCache_t() : m_usSoundNameIndex( 0 ) {}
		CSoundParameters	m_SoundParameters;
		unsigned short		m_usSoundNameIndex;
	};
	// One for left and one for right side of step
	StepSoundCache_t		m_StepSoundCache[ 2 ];

	std::deque< CPlayerSimInfo >  m_vecPlayerSimInfo;
	std::deque< CPlayerCmdInfo >  m_vecPlayerCmdInfo;

	friend class CMoveHelperServer;
	Vector m_movementCollisionNormal;
	Vector m_groundNormal;
	CHandle< CBaseCombatCharacter > m_stuckCharacter;

	// If true, the m_hSplitOwner points to who owns us
	bool			m_bSplitScreenPlayer;
	CHandle< CBasePlayer > m_hSplitOwner;
	// If we have any attached split users, this is the list of them
	std::vector< CHandle< CBasePlayer > > m_hSplitScreenPlayers;
	std::vector< CHandle< CBasePlayer > > m_hSplitScreenAndPipPlayers;
	std::vector< CHandle< CBasePlayer > > m_hPipPlayers;

	CrossPlayPlatform_t m_ClientPlatform;

public:
	float GetAirTime( void );

private:
	float	GetAutoaimScore( const Vector &eyePosition, const Vector &viewDir, const Vector &vecTarget, CBaseEntity *pTarget, float fScale, CBaseCombatWeapon *pActiveWeapon );
	QAngle	AutoaimDeflection( Vector &vecSrc, autoaim_params_t &params );

public:
	virtual unsigned int PlayerSolidMask( bool brushOnly = false ) const;	// returns the solid mask for the given player, so bots can have a more-restrictive set

private:
	// Eye/angle offsets - used for TrackIR and motion controllers
	Vector							m_vecEyeOffset;
	QAngle							m_EyeAngleOffset;
	Vector							m_AimDirection;
};

typedef CHandle<CBasePlayer> CBasePlayerHandle;

EXTERN_SEND_TABLE(DT_BasePlayer)

bool CBasePlayer::IsAutoKickDisabled( void ) const
{
	return m_autoKickDisabled;
}

void CBasePlayer::DisableAutoKick( bool disabled )
{
	m_autoKickDisabled = disabled;
}

//-----------------------------------------------------------------------------
// Converts an entity to a player
//-----------------------------------------------------------------------------
inline CBasePlayer *ToBasePlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	return static_cast<CBasePlayer *>( pEntity );
}

inline const CBasePlayer *ToBasePlayer( const CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;

	return static_cast< const CBasePlayer * >( pEntity );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use CollectPlayers() instead.
 * Iterate over all active players in the game, invoking functor on each.
 * If functor returns false, stop iteration and return false.
 */
template < typename Functor >
bool ForEachPlayer( Functor &func )
{
	CVProfScope _( "ForEachPlayer", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (FNullEnt( player->edict() ))
			continue;

		if (!player->IsPlayer())
			continue;

		if( !player->IsConnected() )
			continue;

		if (func( player ) == false)
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------------------------
/**
 * The interface for an iterative player functor
 */
class IPlayerFunctor
{
public:
	virtual void OnBeginIteration( void )						{ }		// invoked once before iteration begins
	
	virtual bool operator() ( CBasePlayer *player ) = 0;
	
	virtual void OnEndIteration( bool allElementsIterated )		{ }		// invoked once after iteration is complete whether successful or not
};

//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use CollectPlayers() instead.
 * Specialization of ForEachPlayer template for IPlayerFunctors
 */
template <>
inline bool ForEachPlayer( IPlayerFunctor &func )
{
	CVProfScope _( "ForEachPlayer", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	func.OnBeginIteration();
	
	bool isComplete = true;
	
	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == nullptr)
			continue;

		if (FNullEnt( player->edict() ))
			continue;

		if (!player->IsPlayer())
			continue;

		if( !player->IsConnected() )
			continue;

		if (func( player ) == false)
		{
			isComplete = false;
			break;
		}
	}
	
	func.OnEndIteration( isComplete );

	return isComplete;
}

//--------------------------------------------------------------------------------------------------------------
//
// Collect all valid, connected players into given vector.
// Returns number of players collected.
//
inline constexpr auto COLLECT_ONLY_LIVING_PLAYERS = true;
inline constexpr auto APPEND_PLAYERS = true;
template < typename T >
int CollectPlayers( std::vector< T * > *playerVector, int team = TEAM_ANY, bool isAlive = false, bool shouldAppend = false )
{
	if ( !shouldAppend )
	{
		playerVector->clear();
	}

	for( int i=1; i<=gpGlobals->maxClients; ++i )
	{
		T *player = static_cast< T * >( UTIL_PlayerByIndex( i ) );

		if ( player == nullptr )
			continue;

		if ( FNullEnt( player->edict() ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsConnected() )
			continue;

		if ( team != TEAM_ANY && player->GetTeamNumber() != team )
			continue;

		if ( isAlive && !player->IsAlive() )
			continue;

		playerVector->push_back( player );
	}

	return std::ssize( *playerVector );
}

enum
{
	VEHICLE_ANALOG_BIAS_NONE = 0,
	VEHICLE_ANALOG_BIAS_FORWARD,
	VEHICLE_ANALOG_BIAS_REVERSE,
};

// Used on player entities - only sends the data to the local player (objectID-1).
void* SendProxy_SendLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
void* SendProxy_SendNonLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );

// Helper class
class CAutoUserMessageThrottle
{
public:
	CAutoUserMessageThrottle( CBasePlayer *pPlayer, char const *pchMessages[], int nNumMessage );

	  ~CAutoUserMessageThrottle();
private:

	CBasePlayer	*m_pPlayer;
};

class CBasePlayer;
class CUserCmd;




//-----------------------------------------------------------------------------
// Purpose: Server side player movement
//-----------------------------------------------------------------------------
class CPlayerMove
{
public:
	DECLARE_CLASS_NOBASE( CPlayerMove );
	
	// Construction/destruction
					CPlayerMove( void );
	virtual			~CPlayerMove( void ) {}

	// Public interfaces:
	// Run a movement command from the player
	virtual void	RunCommand ( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *moveHelper );

protected:
	// Prepare for running movement
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );

	// Finish movement
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );

	// Called before and after any movement processing
	virtual void	StartCommand( CBasePlayer *player, CUserCmd *cmd );
	void			FinishCommand( CBasePlayer *player );

	// Helper to determine if the user is standing on ground
	void			CheckMovingGround( CBasePlayer *player, double frametime );

	// Helpers to call pre and post think for player, and to call think if a think function is set
	void			RunPreThink( CBasePlayer *player );
	void			RunThink (CBasePlayer *ent, double frametime );
	void			RunPostThink( CBasePlayer *player );
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CPlayerMove *PlayerMove();

class CBaseAnimatingOverlay;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


// Purpose: Manages the server moving things back in time to match up to where clients thought they were
//			when the client commited an action

inline constexpr int MAX_LAYER_RECORDS = CBaseAnimatingOverlay::MAX_OVERLAYS;

struct LayerRecord
{
	int m_sequence;
	float m_cycle;
	float m_weight;
	int m_order;

	LayerRecord()
	{
		Clear();
	}

	LayerRecord( const LayerRecord& src );

	void Clear()
	{
		m_sequence = 0;
		m_cycle = 0;
		m_weight = 0;
		m_order = 0;
	}
};

struct LagRecord
{
public:
	LagRecord()
	{
		Clear();
	}

	LagRecord( const LagRecord& src );

	void Clear()
	{
		m_fFlags = 0;
		m_vecOrigin.Init();
		m_vecAngles.Init();
		m_vecMins.Init();
		m_vecMaxs.Init();
		m_flSimulationTime = -1;
		m_masterSequence = 0;
		m_masterCycle = 0;
		for( int layerIndex = 0; layerIndex < MAX_LAYER_RECORDS; ++layerIndex )
		{
			m_layerRecords[layerIndex].Clear();
		}

		for( int i=0; i<MAXSTUDIOPOSEPARAM; i++ )
		{
			m_flPoseParameters[i] = 0;
		}
	}

	// Did player die this frame
	int						m_fFlags;

	// Player position, orientation and bbox
	Vector					m_vecOrigin;
	QAngle					m_vecAngles;
	Vector					m_vecMins;
	Vector					m_vecMaxs;

	float					m_flSimulationTime;	

	// Player animation details, so we can get the legs in the right spot.
	LayerRecord				m_layerRecords[MAX_LAYER_RECORDS];
	int						m_masterSequence;
	float					m_masterCycle;

	float					m_flPoseParameters[MAXSTUDIOPOSEPARAM];
};

typedef std::deque< LagRecord > LagRecordList;	// newest at front

//-----------------------------------------------------------------------------
class CLagCompensationManager : public CAutoGameSystemPerFrame, public ILagCompensationManager
{
public:
	CLagCompensationManager( char const *name );

	// IServerSystem stuff
	virtual void Shutdown()
	{
		ClearHistory();
	}

	virtual void LevelShutdownPostEntity()
	{
		ClearHistory();
	}

	// called after entities think
	virtual void FrameUpdatePostEntityThink();

	// ILagCompensationManager stuff

	// Called during player movement to set up/restore after lag compensation
	void			StartLagCompensation( CBasePlayer *player, LagCompensationType lagCompensationType, const Vector& weaponPos = vec3_origin, const QAngle &weaponAngles = vec3_angle, float weaponRange = 0.0f );
	void			FinishLagCompensation( CBasePlayer *player );

	// Mappers can flag certain additional entities to lag compensate, this handles them
	virtual void	AddAdditionalEntity( CBaseEntity *pEntity );
	virtual void	RemoveAdditionalEntity( CBaseEntity *pEntity );

	void RecordDataIntoTrack( CBaseEntity *entity, LagRecordList *track, bool wantsAnims );
	bool BacktrackEntity( CBaseEntity *entity, float flTargetTime, LagRecordList *track, LagRecord *restore, LagRecord *change, bool wantsAnims );
	void RestoreEntityFromRecords( CBaseEntity *entity, LagRecord *restore, LagRecord *change, bool wantsAnims );
private:

	void ClearHistory()
	{
		m_CompensatedEntities.clear();
	}

	struct EntityLagData
	{
		EntityLagData() : m_bRestoreEntity( false )
		{
		}

		// True if lag compensation altered entity data
		bool			m_bRestoreEntity;			   
		// keep a list of lag records for each player
		LagRecordList	m_LagRecords;				   

		// Entity data before we moved him back
		LagRecord		m_RestoreData;
		// Entity data where we moved him back
		LagRecord		m_ChangeData;
	};

	std::map< EHANDLE, std::unique_ptr< EntityLagData > > m_CompensatedEntities;

	// True if at least one entity was changed
	bool					m_bNeedToRestore;
	CBasePlayer				*m_pCurrentPlayer;	// The player we are doing lag compensation for

	LagCompensationType		m_lagCompensationType;
	Vector					m_weaponPos;
	QAngle					m_weaponAngles;
	float					m_weaponRange;
	bool					m_isCurrentlyDoingCompensation;	// Sentinel to prevent calling StartLagCompensation a second time before a Finish.

    // List of additional entities flagged by mappers for lag compensation (shouldn't be more than a few)
	std::set< EHANDLE >		m_AdditionalEntities;
};

class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseEntity;
class CTakeDamageInfo;

using namespace ::ks::reflect;


inline constexpr auto BASEGRENADE_EXPLOSION_VOLUME = 1024;

void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct,
    const void *pData, DVariant *pOut, int iElement, int objectID );

void RecvProxy_LocalVelocity( const CRecvProxyData *pData, void *pStruct, void *pOut );


class CBaseGrenade : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
	[[= ks::reflect::NetTable{ .name = "DT_BaseGrenade" } ]]
	[[= ks::reflect::From<"m_flDamage", ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 256.0f, .flags = SPROP_ROUNDDOWN }>{} ]]
	[[= ks::reflect::From<"m_DmgRadius", ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 1024.0f, .flags = SPROP_ROUNDDOWN }>{} ]]
	[[= ks::reflect::From<"m_bIsLive", ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED }>{} ]]
	[[= ks::reflect::From<"m_hThrower", ks::reflect::Net{}>{} ]]
	[[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector }>{} ]]
	[[= ks::reflect::From<"m_fFlags", ks::reflect::Net{ .bits = PLAYER_FLAG_BITS, .flags = SPROP_UNSIGNED }, SendProxy_CropFlagsToPlayerFlagBitsLength>{} ]]
	[[= ks::reflect::Exclude{ .table = "DT_AnimTimeMustBeFirst", .prop = "m_flAnimTime" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CBaseGrenade, CBaseAnimating );
public:

	CBaseGrenade(void);
	~CBaseGrenade(void);

	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	DECLARE_DATADESC();

	 void		Precache_Impl( void );

	virtual void		Explode( trace_t *pTrace, int bitsDamageType );
	void				Smoke( void );

	void				BounceTouch( CBaseEntity *pOther );
	void				SlideTouch( CBaseEntity *pOther );
	void				ExplodeTouch( CBaseEntity *pOther );
	void				DangerSoundThink( void );
	void				PreDetonate( void );
	virtual void		Detonate( void );
	void				DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void				TumbleThink( void );

	virtual Vector		GetBlastForce() { return vec3_origin; }

	virtual void		BounceSound( void );
	 int			BloodColor_Impl( void ) { return DONT_BLEED; }
	 void		Event_Killed_Impl( const CTakeDamageInfo &info );

	virtual float		GetShakeAmplitude( void ) { return 25.0; }
	virtual float		GetShakeRadius( void ) { return 750.0; }

	virtual const char *GetParticleSystemName( int pointContents, surfacedata_t *pdata = nullptr ) { return nullptr; }

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

	// Damage accessors.
	 float GetDamage_Impl()
	{
		return m_flDamage;
	}
	virtual float GetDamageRadius()
	{
		return m_DmgRadius;
	}

	 void SetDamage_Impl(float flDamage)
	{
		m_flDamage = flDamage;
	}

	virtual void SetDamageRadius(float flDamageRadius)
	{
		m_DmgRadius = flDamageRadius;
	}

	// Bounce sound accessors.
	void SetBounceSound( const char *pszBounceSound ) 
	{
		m_iszBounceSound = MAKE_STRING( pszBounceSound );
	}

	CBaseCombatCharacter *GetThrower( void );
	void				  SetThrower( CBaseCombatCharacter *pThrower );
	CBaseEntity *GetOriginalThrower() { return m_hOriginalThrower; }

	// Allow +USE pickup
	int ObjectCaps_Impl() 
	{ 
		return (BaseClass::ObjectCaps_Impl() | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS);
	}

	void				Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

public:
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );
	
	bool				m_bHasWarnedAI;				// whether or not this grenade has issued its DANGER sound to the world sound list yet.
	CNetworkVar( bool, m_bIsLive, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );					// Is this grenade live, or can it be picked up?
	CNetworkVar( float, m_DmgRadius, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );				// How far do I do damage?
	CNetworkVar( float, m_flNextAttack, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE, .tolerance = TD_MSECTOLERANCE } ]] );
	float				m_flDetonateTime;			// Time at which to detonate.
	float				m_flWarnAITime;				// Time at which to warn the AI

protected:

	CNetworkVar( float, m_flDamage, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );		// Damage to inflict.
	string_t m_iszBounceSound;	// The sound to make on bouncing.  If not NULL, overrides the BounceSound() function.

private:
	CNetworkHandle( CBaseEntity, m_hThrower, [[= ks::reflect::Pred{ .flags = FTYPEDESC_INSENDTABLE } ]] );					// Who threw this grenade
	EHANDLE			m_hOriginalThrower;							// Who was the original thrower of this grenade

	CBaseGrenade( const CBaseGrenade & ); // not defined, not accessible

};

class CAI_Expresser;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;


enum SpeechPriorityType
{
	SPEECH_PRIORITY_LOW,
	SPEECH_PRIORITY_NORMAL,
	SPEECH_PRIORITY_MANUAL,
	SPEECH_PRIORITY_UNINTERRUPTABLE,
};
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseMultiplayerPlayer : public CAI_ExpresserHost<CBasePlayer>
{

	DECLARE_CLASS( CBaseMultiplayerPlayer, CAI_ExpresserHost<CBasePlayer> );
	DECLARE_ENT_SCRIPTDESC();

public:

	CBaseMultiplayerPlayer();
	~CBaseMultiplayerPlayer();

	void		Spawn_Impl( void );

	void		PostConstructor_Impl( const char *szClassname );
	void		ModifyOrAppendCriteria_Impl( AI_CriteriaSet& criteriaSet );

	virtual bool		SpeakIfAllowed( AIConcept_t conc, SpeechPriorityType priority, const char *modifiers = nullptr, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );
	void				SpeakConcept( AI_Response &outresponse, int iConcept );
	virtual bool		SpeakConceptIfAllowed( int iConcept, const char *modifiers = nullptr, char *pszOutResponseChosen = nullptr, std::size_t bufsize = 0, IRecipientFilter *filter = nullptr );

	virtual bool		CanHearAndReadChatFrom( CBasePlayer *pPlayer );
	virtual bool		CanSpeak( void ) { return true; }

	void		Precache_Impl( void );

	virtual bool		ClientCommand( const CCommand &args );

	virtual bool		CanSpeakVoiceCommand( void ) { return true; }
	virtual bool		ShouldShowVoiceSubtitleToEnemy( void );
	virtual void		NoteSpokeVoiceCommand( const char *pszScenePlayed ) {}

	virtual void OnAchievementEarned( int iAchievement ) {}

	enum ChatIgnore
	{
		CHAT_IGNORE_NONE = 0,
		CHAT_IGNORE_BROADCAST,
		CHAT_IGNORE_BROADCAST_AND_TEAM,
		CHAT_IGNORE_EVERYTHING,
	};

	int m_iIgnoreGlobalChat;

	//---------------------------------
	// Speech support
	virtual CAI_Expresser *GetExpresser() { return m_pExpresser; }
	virtual CMultiplayer_Expresser *GetMultiplayerExpresser() { return m_pExpresser; }

	void SetLastForcedChangeTeamTimeToNow( void ) { m_flLastForcedChangeTeamTime = gpGlobals->curtime; }
	float GetLastForcedChangeTeamTime( void ) { return m_flLastForcedChangeTeamTime; }

	void SetTeamBalanceScore( int iScore ) { m_iBalanceScore = iScore; }
	int GetTeamBalanceScore( void ) { return m_iBalanceScore; }

	virtual int	CalculateTeamBalanceScore( void );

	virtual void AwardAchievement( int iAchievement, int iCount = 1 );
	int	GetPerLifeCounterKV( const char *name );
	void SetPerLifeCounterKV( const char *name, int value );
	void ResetPerLifeCounters( void );

	KeyValues *GetPerLifeCounterKeys( void ) { return m_pAchievementKV; }

	void EscortScoringThink( void );
	void StartScoringEscortPoints( float flRate );
	void StopScoringEscortPoints( void );
	float m_flAreaCaptureScoreAccumulator;
	float m_flCapPointScoreRate;

	float GetConnectionTime( void ) { return m_flConnectionTime; }

	//----------------------------
	// Player id handling
	bool		GetPlayerID( CPlayerId *pID ) const;
	uint64		GetPlayerIDAsUInt64( void ) const;

protected:
	virtual CAI_Expresser *CreateExpresser( void );

	int		m_iCurrentConcept;
private:
	//---------------------------------
	CMultiplayer_Expresser		*m_pExpresser;

	float m_flConnectionTime;
	float m_flLastForcedChangeTeamTime;

	int m_iBalanceScore;	// a score used to determine which players are switched to balance the teams

	KeyValues	*m_pAchievementKV;
};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CBaseMultiplayerPlayer *ToBaseMultiplayerPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return nullptr;
	return static_cast<CBaseMultiplayerPlayer *>( pEntity );
}
