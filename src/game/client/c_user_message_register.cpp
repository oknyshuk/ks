// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "c_user_message_register.h"

CUserMessageRegisterBase *CUserMessageRegisterBase::s_pHead = nullptr;

CUserMessageRegisterBase::CUserMessageRegisterBase()
{	
	// Link it in.
	m_pNext = s_pHead;
	s_pHead = this;
}


void CUserMessageRegisterBase::RegisterAll()
{
	for ( CUserMessageRegisterBase *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		pCur->Register();
	}
}
