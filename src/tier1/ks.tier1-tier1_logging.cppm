// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1.tier1_logging;

import std;
import ks.core.logging;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
export import ks.tier1.byte_writer;

export extern "C++" {

class CBufferedLoggingListener : public ILoggingListener
{
public:
	CBufferedLoggingListener();

	void Log( const LoggingContext_t *pContext, const tchar *pMessage );

	void EmitBufferedSpew();

private:

	ks::byte_writer m_StoredSpew;
};

}
