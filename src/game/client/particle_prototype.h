#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// This defines things that allow particle effects to hook into the particle app.

class CParticleMgr;
class RecvTable;


// Command-line args can be passed in to set state or options in the effects.
class IPrototypeArgAccess
{
public:
	virtual const char*	FindArg(const char *pName, const char *pDefault=0)=0;
};



// You must implement this interface for the prototype app to be able to run your effect.
class IPrototypeAppEffect
{
public:
	virtual				~IPrototypeAppEffect()	{}
	
	// Start the effect. You can get command-line args with pArgs.
	virtual void		Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs)=0;
	
	// Return false if you don't allow properties to be edited in the prototype app.
	virtual bool		GetPropEditInfo(RecvTable **ppTable, void **ppObj) {return false;}
};



// Used internally.
typedef IPrototypeAppEffect* (*PrototypeEffectCreateFn)();
class PrototypeEffectLink
{
public:
							PrototypeEffectLink(PrototypeEffectCreateFn fn, const char *pName);								

	PrototypeEffectCreateFn	m_CreateFn;
	const char				*m_pEffectName;
	PrototypeEffectLink		*m_pNext;
};
