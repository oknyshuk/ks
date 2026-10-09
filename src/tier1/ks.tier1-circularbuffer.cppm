// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:circularbuffer;

import std;
import ks.math;

export extern "C++" {

class CCircularBuffer
{
public:
						CCircularBuffer();
						CCircularBuffer( int size );
	void				SetSize( int nSize );

protected:
	inline void			AssertValid()
	{
	}

public:

	void				Flush();
	int					GetSize();					// Get the size of the buffer (how much can you write without reading
													// before losing data.
	
	int					GetWriteAvailable();		// Get the amount available to write without overflowing.
													// Note: you can write however much you want, but it may overflow,
													// in which case the newest data is kept and the oldest is discarded.
	
	int					GetReadAvailable();			// Get the amount available to read.
	
	int					Peek(char *pchDest, int nCount);
	int					Advance(int nCount);
	int					Read(void *pchDest, int nCount);
	int					Write(void *pchData, int nCount);

public:
	int		m_nCount;			// Space between the read and write pointers (how much data we can read).
	
	int		m_nRead;			// Read index into circular buffer
	int		m_nWrite;			// Write index into circular buffer
	
	int		m_nSize;			// Size of circular buffer in bytes (how much data it can hold).
	char	m_chData[1];		// Circular buffer holding data
};

// Use this to instantiate a CircularBuffer.
template< int size >
class CSizedCircularBuffer : public CCircularBuffer
{
public:
	CSizedCircularBuffer() : CCircularBuffer(size) {}

private:
	char	myData[size-1];
};

CCircularBuffer *AllocateCircularBuffer( int nSize );
void FreeCircularBuffer( CCircularBuffer *pCircularBuffer );

}
