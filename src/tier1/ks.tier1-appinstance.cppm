// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2009 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.appinstance;

import std;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;

export extern "C++" {

// check if handle is defined rather than inlcude another header
typedef void *HANDLE;

class CSingleAppInstance
{
	public:
		explicit CSingleAppInstance( tchar* InstanceName, bool exitOnNotUnique = false, bool displayMsgIfNotUnique = false );
				~CSingleAppInstance();

		bool	CheckForOtherRunningInstances( bool exitOnNotUnique = false, bool displayMsgIfNotUnique = true );
		
		static bool CheckForRunningInstance( tchar* InstanceName );

		bool	IsUniqueInstance()	{ return m_isUniqueInstance; }
		HANDLE	GetHandle()			{ return reinterpret_cast< HANDLE >( (intp) m_hMutex ); }

	private:
		CSingleAppInstance();		// Hidden for a reason.  You must specify the instance name

		HANDLE	m_hMutex;
		bool	m_isUniqueInstance;
};

}
