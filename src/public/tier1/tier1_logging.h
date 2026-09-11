//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Tier1 logging helpers.
//
//===============================================================================

#ifndef TIER1_LOGGING_H
#define TIER1_LOGGING_H


#include "logging.h"
#include "utlbuffer.h"
#include "color.h"

class CBufferedLoggingListener : public ILoggingListener
{
public:
	CBufferedLoggingListener();

	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage );

	void EmitBufferedSpew();

private:

	CUtlBuffer m_StoredSpew;
};

#endif // TIER1_LOGGING_H
