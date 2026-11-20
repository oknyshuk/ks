#pragma once
import ks.math.ssequaternion;
import ks.tier1.convar;
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: IObjectContainer.h: interface for the ObjectContainer class.

class IObjectContainer  
{
public:
	virtual ~IObjectContainer() {};
	
	virtual void		Init() = 0;

	virtual bool		Add(void * newObject) = 0;
	virtual bool		Remove(void * object) = 0;
	virtual void		Clear(bool freeElementsMemory) = 0;
	
	virtual void *		GetFirst() = 0;
	virtual void *		GetNext() = 0;

	virtual int			CountElements() = 0;;
	virtual bool		Contains(void * object) = 0;
	virtual bool		IsEmpty() = 0;
};
