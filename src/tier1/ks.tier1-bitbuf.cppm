// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.tier1:bitbuf;

import std;
import ks.math;

export extern "C++" {

//-----------------------------------------------------------------------------
// Forward declarations.
//-----------------------------------------------------------------------------

class Vector;
class QAngle;

//-----------------------------------------------------------------------------
// You can define a handler function that will be called in case of 
// out-of-range values and overruns here.
//
// NOTE: the handler is only called in debug mode.
//
// Call SetBitBufErrorHandler to install a handler.
//-----------------------------------------------------------------------------

typedef enum
{
	BITBUFERROR_VALUE_OUT_OF_RANGE=0,		// Tried to write a value with too few bits.
	BITBUFERROR_BUFFER_OVERRUN,				// Was about to overrun a buffer.

	BITBUFERROR_NUM_ERRORS
} BitBufErrorType;

typedef void (*BitBufErrorHandler)( BitBufErrorType errorType, const char *pDebugName );

// Use this to install the error handler. Call with NULL to uninstall your error handler.
void SetBitBufErrorHandler( BitBufErrorHandler fn );

//-----------------------------------------------------------------------------
// Helpers.
//-----------------------------------------------------------------------------

inline int BitByte( int bits )
{
	// return PAD_NUMBER( bits, 8 ) >> 3;
	return (bits + 7) >> 3;
}

//-----------------------------------------------------------------------------
enum EBitCoordType
{
	kCW_None,
	kCW_LowPrecision,
	kCW_Integral
};

//-----------------------------------------------------------------------------
// namespaced helpers
//-----------------------------------------------------------------------------
namespace bitbuf
{
	// ZigZag Transform:  Encodes signed integers so that they can be
	// effectively used with varint encoding.
	//
	// varint operates on unsigned integers, encoding smaller numbers into
	// fewer bytes.  If you try to use it on a signed integer, it will treat
	// this number as a very large unsigned integer, which means that even
	// small signed numbers like -1 will take the maximum number of bytes
	// (10) to encode.  ZigZagEncode() maps signed integers to unsigned
	// in such a way that those with a small absolute value will have smaller
	// encoded values, making them appropriate for encoding using varint.
	//
	//       int32 ->     uint32
	// -------------------------
	//           0 ->          0
	//          -1 ->          1
	//           1 ->          2
	//          -2 ->          3
	//         ... ->        ...
	//  2147483647 -> 4294967294
	// -2147483648 -> 4294967295
	//
	//        >> encode >>
	//        << decode <<

	inline uint32 ZigZagEncode32(int32 n) 
	{
		// Note:  the right-shift must be arithmetic
		return(n << 1) ^ (n >> 31);
	}

	inline int32 ZigZagDecode32(uint32 n) 
	{
		return(n >> 1) ^ -static_cast<int32>(n & 1);
	}

	inline uint64 ZigZagEncode64(int64 n) 
	{
		// Note:  the right-shift must be arithmetic
		return(n << 1) ^ (n >> 63);
	}

	inline int64 ZigZagDecode64(uint64 n) 
	{
		return(n >> 1) ^ -static_cast<int64>(n & 1);
	}

	inline const int kMaxVarintBytes = 10;
	inline const int kMaxVarint32Bytes = 5;
}

//-----------------------------------------------------------------------------
// Used for serialization
//-----------------------------------------------------------------------------

class bf_write
{
public:
	bf_write();
					
					// nMaxBits can be used as the number of bits in the buffer. 
					// It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
	bf_write( void *pData, int nBytes, int nMaxBits = -1 );
	bf_write( const char *pDebugName, void *pData, int nBytes, int nMaxBits = -1 );

	// Start writing to the specified buffer.
	// nMaxBits can be used as the number of bits in the buffer. 
	// It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
	void			StartWriting( void *pData, int nBytes, int iStartBit = 0, int nMaxBits = -1 );

	// Restart buffer writing.
	void			Reset();

	// Get the base pointer.
	unsigned char*	GetBasePointer() { return m_pData; }

	// Enable or disable assertion on overflow. 99% of the time, it's a bug that we need to catch,
	// but there may be the occasional buffer that is allowed to overflow gracefully.
	void			SetAssertOnOverflow( bool bAssert );

	// This can be set to assign a name that gets output if the buffer overflows.
	const char*		GetDebugName();
	void			SetDebugName( const char *pDebugName );

// Seek to a specific position.
public:
	
	void			SeekToBit( int bitPos );

// Bit functions.
public:

	void			WriteOneBit(int nValue);
	void			WriteOneBitNoCheck(int nValue);
	void			WriteOneBitAt( int iBit, int nValue );
	
	// Write signed or unsigned. Range is only checked in debug.
	void			WriteUBitLong( unsigned int data, int numbits, bool bCheckRange=true );
	void			WriteSBitLong( int data, int numbits );
	
	// Tell it whether or not the data is unsigned. If it's signed,
	// cast to unsigned before passing in (it will cast back inside).
	void			WriteBitLong(unsigned int data, int numbits, bool bSigned);

	// Write a list of bits in.
	bool			WriteBits(const void *pIn, int nBits);

	// writes an unsigned integer with variable bit length
	void			WriteUBitVar( unsigned int data );

	// writes a varint encoded integer
	void			WriteVarInt32( uint32 data );
	void			WriteVarInt64( uint64 data );
	void			WriteSignedVarInt32( int32 data );
	void			WriteSignedVarInt64( int64 data );
	int				ByteSizeVarInt32( uint32 data );
	int				ByteSizeVarInt64( uint64 data );
	int				ByteSizeSignedVarInt32( int32 data );
	int				ByteSizeSignedVarInt64( int64 data );

	// Copy the bits straight out of pIn. This seeks pIn forward by nBits.
	// Returns an error if this buffer or the read buffer overflows.
	bool			WriteBitsFromBuffer( class bf_read *pIn, int nBits );
	
	void			WriteBitAngle( float fAngle, int numbits );
	void			WriteBitCoord (const float f);
	void			WriteBitCoordMP( const float f, EBitCoordType coordType );
	void 			WriteBitCellCoord( const float f, int bits, EBitCoordType coordType );
	void			WriteBitFloat(float val);
	void			WriteBitVec3Coord( const Vector& fa );
	void			WriteBitNormal( float f );
	void			WriteBitVec3Normal( const Vector& fa );
	void			WriteBitAngles( const QAngle& fa );

// Byte functions.
public:

	void			WriteChar(int val);
	void			WriteByte( unsigned int val );
	void			WriteShort(int val);
	void			WriteWord( unsigned int val );
	void			WriteLong(int32 val);
	void			WriteLongLong(int64 val);
	void			WriteFloat(float val);
	bool			WriteBytes( const void *pBuf, int nBytes );

	// Returns false if it overflows the buffer.
	bool			WriteString(const char *pStr);
	bool			WriteString(const wchar_t *pStr);

// Status.
public:

	// How many bytes are filled in?
	int				GetNumBytesWritten() const;
	int				GetNumBitsWritten() const;
	int				GetMaxNumBits() const;
	int				GetNumBitsLeft() const;
	int				GetNumBytesLeft() const;
	unsigned char*	GetData();
	const unsigned char*	GetData() const;

	// Has the buffer overflowed?
	bool			CheckForOverflow(int nBits);
	inline bool		IsOverflowed() const {return m_bOverflow;}

	inline void		SetOverflowFlag();

public:
	// The current buffer.
	unsigned char*	m_pData;
	int				m_nDataBytes;
	int				m_nDataBits;
	
	// Where we are in the buffer.
	int				m_iCurBit;
	
private:

	// Errors?
	bool			m_bOverflow;

	bool			m_bAssertOnOverflow;
	const char		*m_pDebugName;
};

//-----------------------------------------------------------------------------
// Inlined methods
//-----------------------------------------------------------------------------

// How many bytes are filled in?
inline int bf_write::GetNumBytesWritten() const
{
	return BitByte(m_iCurBit);
}

inline int bf_write::GetNumBitsWritten() const
{
	return m_iCurBit;
}

inline int bf_write::GetMaxNumBits() const
{
	return m_nDataBits;
}

inline int bf_write::GetNumBitsLeft() const
{
	return m_nDataBits - m_iCurBit;
}

inline int bf_write::GetNumBytesLeft() const
{
	return GetNumBitsLeft() >> 3;
}

inline unsigned char* bf_write::GetData()			
{
	return m_pData;
}

inline const unsigned char* bf_write::GetData() const
{
	return m_pData;
}

inline bool bf_write::CheckForOverflow(int nBits)
{
	if ( m_iCurBit + nBits > m_nDataBits )
	{
		SetOverflowFlag();
	}
	
	return m_bOverflow;
}

inline void bf_write::SetOverflowFlag()
{
	if ( m_bAssertOnOverflow )
	{
		contract_assert( false );
	}

	m_bOverflow = true;
}

inline void bf_write::WriteOneBitNoCheck(int nValue)
{
	if(nValue)
		m_pData[m_iCurBit >> 3] |= (1 << (m_iCurBit & 7));
	else
		m_pData[m_iCurBit >> 3] &= ~(1 << (m_iCurBit & 7));

	++m_iCurBit;
}

inline void bf_write::WriteOneBit(int nValue)
{
	if( !CheckForOverflow(1) )
		WriteOneBitNoCheck( nValue );
}

inline void	bf_write::WriteOneBitAt( int iBit, int nValue )
{
	if( iBit+1 > m_nDataBits )
	{
		SetOverflowFlag();
		return;
	}

	if( nValue )
		m_pData[iBit >> 3] |= (1 << (iBit & 7));
	else
		m_pData[iBit >> 3] &= ~(1 << (iBit & 7));
}

inline void bf_write::WriteUBitLong( unsigned int curData, int numbits, bool bCheckRange )
{

	extern uint32 g_BitWriteMasks[32][33];

	// Bounds checking..
	if ((m_iCurBit+numbits) > m_nDataBits)
	{
		m_iCurBit = m_nDataBits;
		SetOverflowFlag();
		return;
	}

	int nBitsLeft = numbits;
	int iCurBit = m_iCurBit;

	// Mask in a dword.
	unsigned int iDWord = iCurBit >> 5;
	contract_assert( (iDWord*4 + sizeof(int32)) <= (unsigned int)m_nDataBytes );

	uint32 iCurBitMasked = iCurBit & 31;

	uint32 dword = LoadLittleDWord( (uint32*)m_pData, iDWord );

	dword &= g_BitWriteMasks[iCurBitMasked][nBitsLeft];
	dword |= curData << iCurBitMasked;

	// write to stream (lsb to msb ) properly
	StoreLittleDWord( (uint32*)m_pData, iDWord, dword );

	// Did it span a dword?
	int nBitsWritten = 32 - iCurBitMasked;
	if ( nBitsWritten < nBitsLeft )
	{
		nBitsLeft -= nBitsWritten;
		curData >>= nBitsWritten;

		// read from stream (lsb to msb) properly 
		dword = LoadLittleDWord( (uint32*)m_pData, iDWord+1 );

		dword &= g_BitWriteMasks[0][nBitsLeft];
		dword |= curData;

		// write to stream (lsb to msb) properly 
		StoreLittleDWord( (uint32*)m_pData, iDWord+1, dword );
	}

	m_iCurBit += numbits;
}

//-----------------------------------------------------------------------------
// This is useful if you just want a buffer to write into on the stack.
//-----------------------------------------------------------------------------

template<int SIZE>
class old_bf_write_static : public bf_write
{
public:
	inline old_bf_write_static() : bf_write(m_StaticData, SIZE) {}

	char	m_StaticData[SIZE];
};

//-----------------------------------------------------------------------------
// Used for unserialization
//-----------------------------------------------------------------------------

class old_bf_read
{
public:
	old_bf_read();

	// nMaxBits can be used as the number of bits in the buffer. 
	// It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
	old_bf_read( const void *pData, int nBytes, int nBits = -1 );
	old_bf_read( const char *pDebugName, const void *pData, int nBytes, int nBits = -1 );

	// Start reading from the specified buffer.
	// pData's start address must be dword-aligned.
	// nMaxBits can be used as the number of bits in the buffer. 
	// It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
	void			StartReading( const void *pData, int nBytes, int iStartBit = 0, int nBits = -1 );

	// Restart buffer reading.
	void			Reset();

	// Enable or disable assertion on overflow. 99% of the time, it's a bug that we need to catch,
	// but there may be the occasional buffer that is allowed to overflow gracefully.
	void			SetAssertOnOverflow( bool bAssert );

	// This can be set to assign a name that gets output if the buffer overflows.
	const char*		GetDebugName();
	void			SetDebugName( const char *pName );

	void			ExciseBits( int startbit, int bitstoremove );

// Bit functions.
public:
	
	// Returns 0 or 1.
	int				ReadOneBit();

protected:

	unsigned int	CheckReadUBitLong(int numbits);		// For debugging.
	int				ReadOneBitNoCheck();				// Faster version, doesn't check bounds and is inlined.
	bool			CheckForOverflow(int nBits);

public:

	// Get the base pointer.
	const unsigned char*	GetBasePointer() { return m_pData; }

	[[gnu::always_inline]] inline int TotalBytesAvailable( void ) const
	{
		return m_nDataBytes;
	}

	// Read a list of bits in..
	void            ReadBits(void *pOut, int nBits);
	
	float			ReadBitAngle( int numbits );

	unsigned int	ReadUBitLong( int numbits );
	unsigned int	PeekUBitLong( int numbits );
	int				ReadSBitLong( int numbits );

	// reads an unsigned integer with variable bit length
	unsigned int	ReadUBitVar();
	
	// reads a varint encoded integer
	uint32			ReadVarInt32();
	uint64			ReadVarInt64();
	int32			ReadSignedVarInt32();
	int64			ReadSignedVarInt64();

	// You can read signed or unsigned data with this, just cast to 
	// a signed int if necessary.
	unsigned int	ReadBitLong(int numbits, bool bSigned);
	
	float			ReadBitCoord();
	float			ReadBitCoordMP( EBitCoordType coordType );
	float 			ReadBitCellCoord( int bits, EBitCoordType coordType );
	float			ReadBitFloat();
	float			ReadBitNormal();
	void			ReadBitVec3Coord( Vector& fa );
	void			ReadBitVec3Normal( Vector& fa );
	void			ReadBitAngles( QAngle& fa );

// Byte functions (these still read data in bit-by-bit).
public:
	
	int				ReadChar();
	int				ReadByte();
	int				ReadShort();
	int				ReadWord();
	int32			ReadLong();
	int64			ReadLongLong();
	float			ReadFloat();
	bool			ReadBytes(void *pOut, int nBytes);

	// Returns false if bufLen isn't large enough to hold the
	// string in the buffer.
	//
	// Always reads to the end of the string (so you can read the
	// next piece of data waiting).
	//
	// If bLine is true, it stops when it reaches a '\n' or a null-terminator.
	//
	// pStr is always null-terminated (unless bufLen is 0).
	//
	// pOutNumChars is set to the number of characters left in pStr when the routine is 
	// complete (this will never exceed bufLen-1).
	//
	bool			ReadString( char *pStr, int bufLen, bool bLine=false, int *pOutNumChars=nullptr );
	bool			ReadWString( wchar_t *pStr, int bufLen, bool bLine=false, int *pOutNumChars=nullptr );

	// Reads a string and allocates memory for it. If the string in the buffer
	// is > 2048 bytes, then pOverflow is set to true (if it's not NULL).
	char*			ReadAndAllocateString( bool *pOverflow = 0 );

// Status.
public:
	int				GetNumBytesLeft();
	int				GetNumBytesRead();
	int				GetNumBitsLeft();
	int				GetNumBitsRead() const;

	// Has the buffer overflowed?
	inline bool		IsOverflowed() const {return m_bOverflow;}

	inline bool		Seek(int iBit);					// Seek to a specific bit.
	inline bool		SeekRelative(int iBitDelta);	// Seek to an offset from the current position.

	// Called when the buffer is overflowed.
	inline void		SetOverflowFlag();

public:

	// The current buffer.
	const unsigned char*	m_pData;
	int						m_nDataBytes;
	int						m_nDataBits;
	
	// Where we are in the buffer.
	int				m_iCurBit;

private:	
	// used by varbit reads internally
	inline int CountRunOfZeros();

	// Errors?
	bool			m_bOverflow;

	// For debugging..
	bool			m_bAssertOnOverflow;

	const char		*m_pDebugName;
};

//-----------------------------------------------------------------------------
// Inlines.
//-----------------------------------------------------------------------------

inline int old_bf_read::GetNumBytesRead()	
{
	return BitByte(m_iCurBit);
}

inline int old_bf_read::GetNumBitsLeft()	
{
	return m_nDataBits - m_iCurBit;
}

inline int old_bf_read::GetNumBytesLeft()	
{
	return GetNumBitsLeft() >> 3;
}

inline int old_bf_read::GetNumBitsRead() const
{
	return m_iCurBit;
}

inline void old_bf_read::SetOverflowFlag()
{
	if ( m_bAssertOnOverflow )
	{
		contract_assert( false );
	}

	m_bOverflow = true;
}

inline bool old_bf_read::Seek(int iBit)
{
	if(iBit < 0 || iBit > m_nDataBits)
	{
		SetOverflowFlag();
		m_iCurBit = m_nDataBits;
		return false;
	}
	else
	{
		m_iCurBit = iBit;
		return true;
	}
}

// Seek to an offset from the current position.
inline bool	old_bf_read::SeekRelative(int iBitDelta)		
{
	return Seek(m_iCurBit+iBitDelta);
}	

inline bool old_bf_read::CheckForOverflow(int nBits)
{
	if( m_iCurBit + nBits > m_nDataBits )
	{
		SetOverflowFlag();
	}

	return m_bOverflow;
}

inline int old_bf_read::ReadOneBitNoCheck()
{
	int value = m_pData[m_iCurBit >> 3] & (1 << (m_iCurBit & 7));
	++m_iCurBit;
	return !!value;
}

inline int old_bf_read::ReadOneBit()
{
	return (!CheckForOverflow(1)) ?	ReadOneBitNoCheck() : 0;
}

inline float old_bf_read::ReadBitFloat()
{
	int32 val;

	contract_assert( sizeof(float) == sizeof(int32) );
	contract_assert( sizeof(float) == 4 );

	if(CheckForOverflow(32))
		return 0.0f;

	int bit = m_iCurBit & 0x7;
	int byte = m_iCurBit >> 3;
	val = m_pData[byte] >> bit;
	val |= ((int)m_pData[byte + 1]) << (8 - bit);
	val |= ((int)m_pData[byte + 2]) << (16 - bit);
	val |= ((int)m_pData[byte + 3]) << (24 - bit);
	if (bit != 0)
		val |= ((int)m_pData[byte + 4]) << (32 - bit);
	m_iCurBit += 32;
	return *((float*)&val);
}

inline unsigned int old_bf_read::ReadUBitLong( int numbits )
{
	extern uint32 g_ExtraMasks[32];

	if ( (m_iCurBit+numbits) > m_nDataBits )
	{
		m_iCurBit = m_nDataBits;
		SetOverflowFlag();
		return 0;
	}

	contract_assert( numbits > 0 && numbits <= 32 );

	// Read the current dword.
	int idword1 = m_iCurBit >> 5;
	unsigned int dword1 = LoadLittleDWord( (uint32*)m_pData, idword1 );

	dword1 >>= (m_iCurBit & 31); // Get the bits we're interested in.

	m_iCurBit += numbits;
	unsigned int ret = dword1;

	// Does it span this dword?
	if ( (m_iCurBit-1) >> 5 == idword1 )
	{
		if (numbits != 32)
			ret &= g_ExtraMasks[numbits];
	}
	else
	{
		int nExtraBits = m_iCurBit & 31;
		unsigned int dword2 = LoadLittleDWord( (uint32*)m_pData, idword1+1 );

		dword2 &= g_ExtraMasks[nExtraBits];

		// No need to mask since we hit the end of the dword.
		// Shift the second dword's part into the high bits.
		ret |= (dword2 << (numbits - nExtraBits));
	}

	return ret;
}

class CBitBuffer
{
public:
	char const * m_pDebugName;
	bool m_bOverflow;
	int m_nDataBits;
	std::size_t m_nDataBytes;

	void SetDebugName( char const *pName )
	{
		m_pDebugName = pName;
	}

	CBitBuffer( void )
	{
		m_bOverflow = false;
		m_pDebugName = nullptr;
		m_nDataBits = -1;
		m_nDataBytes = 0;
	}

	[[gnu::always_inline]] inline void SetOverflowFlag( void )
	{
		m_bOverflow = true;
	}

	[[gnu::always_inline]] inline bool IsOverflowed( void ) const
	{
		return m_bOverflow;
	}

	static const uint32 s_nMaskTable[33];							// 0 1 3 7 15 ..

};

class CBitWrite : public CBitBuffer
{
	uint32 m_nOutBufWord;
	int m_nOutBitsAvail;
	uint32 *m_pDataOut;
	uint32 *m_pBufferEnd;
	uint32 *m_pData;
	bool m_bFlushed;

public:
	void StartWriting( void *pData, int nBytes, int iStartBit = 0, int nMaxBits = -1 );

	CBitWrite( void *pData, int nBytes, int nBits = -1 )
	{
		m_bFlushed = false;
		StartWriting( pData, nBytes, 0, nBits );
	}
	
	CBitWrite( const char *pDebugName, void *pData, int nBytes, int nBits = -1 )
	{
		m_bFlushed = false;
		SetDebugName( pDebugName );
		StartWriting( pData, nBytes, 0, nBits );
	}

	CBitWrite( void )
	{
		m_bFlushed = false;
	}

	~CBitWrite( void )
	{
		TempFlush();
		contract_assert( (! m_pData ) || m_bFlushed );
	}
	[[gnu::always_inline]] inline int GetNumBitsLeft( void ) const
	{
		return m_nOutBitsAvail + ( 32 * ( m_pBufferEnd - m_pDataOut -1 ) );
	}

	[[gnu::always_inline]] inline void Reset( void )
	{
		m_bOverflow = false;
		m_nOutBitsAvail = 32;
		m_pDataOut = m_pData;
		m_nOutBufWord = 0;
		
	}

	[[gnu::always_inline]] inline void TempFlush( void )
	{
		// someone wants to know how much data we have written, or the pointer to it, so we'd better make
		// sure we write our data
		if ( m_nOutBitsAvail != 32 )
		{
			if ( m_pDataOut == m_pBufferEnd )
			{
				SetOverflowFlag();
			}
			else
			{
				StoreLittleDWord( m_pDataOut, 0, LoadLittleDWord(m_pDataOut,0) & ~s_nMaskTable[ 32 - m_nOutBitsAvail ] | m_nOutBufWord );
			}
		}
		m_bFlushed = true;
	}

	[[gnu::always_inline]] inline unsigned char *GetBasePointer()
	{
		TempFlush();
		return reinterpret_cast< unsigned char *>( m_pData );
	}

	[[gnu::always_inline]] inline unsigned char *GetData()
	{
		return GetBasePointer();
	}

	[[gnu::always_inline]] inline void Finish();
	[[gnu::always_inline]] inline void Flush();
	[[gnu::always_inline]] inline void FlushNoCheck();
	[[gnu::always_inline]] inline void WriteOneBit(int nValue);
	[[gnu::always_inline]] inline void WriteOneBitNoCheck(int nValue);
	[[gnu::always_inline]] inline void WriteUBitLong( unsigned int data, int numbits, bool bCheckRange=true );
	[[gnu::always_inline]] inline void WriteSBitLong( int data, int numbits );
	[[gnu::always_inline]] inline void WriteUBitVar( unsigned int data );
	[[gnu::always_inline]] inline void WriteBitFloat( float flValue );
	[[gnu::always_inline]] inline void WriteFloat( float flValue );
	bool WriteBits(const void *pInData, int nBits);
	void WriteBytes( const void *pBuf, int nBytes );
	void SeekToBit( int nSeekPos );

	[[gnu::always_inline]] inline int GetNumBitsWritten( void ) const
	{
		return ( 32 - m_nOutBitsAvail ) + ( 32 * ( m_pDataOut - m_pData ) );
	}

	[[gnu::always_inline]] inline int GetNumBytesWritten( void ) const
	{
		return ( GetNumBitsWritten() + 7 ) >> 3;
	}

	[[gnu::always_inline]] inline void WriteLong(int32 val)
	{
		WriteSBitLong( val, 32 );
	}

	[[gnu::always_inline]] inline void WriteChar( int val )
	{
		WriteSBitLong(val, sizeof(char) << 3 );
	}

	[[gnu::always_inline]] inline void WriteByte( int val )
	{
		WriteUBitLong(val, sizeof(unsigned char) << 3, false );
	}

	[[gnu::always_inline]] inline void WriteShort(int val)
	{
		WriteSBitLong(val, sizeof(short) << 3);
	}

	[[gnu::always_inline]] inline void WriteWord(int val)
	{
		WriteUBitLong(val, sizeof(unsigned short) << 3);
	}
	
	bool WriteString( const char *pStr );
	bool WriteString( const wchar_t *pStr );

	void WriteLongLong( int64 val );

	void WriteBitAngle( float fAngle, int numbits );
	void WriteBitCoord (const float f);
	void WriteBitCoordMP( const float f, EBitCoordType coordType );
	void WriteBitCellCoord( const float f, int bits, EBitCoordType coordType );
	void WriteBitVec3Coord( const Vector& fa );
	void WriteBitNormal( float f );
	void WriteBitVec3Normal( const Vector& fa );
	void WriteBitAngles( const QAngle& fa );

	// Copy the bits straight out of pIn. This seeks pIn forward by nBits.
	// Returns an error if this buffer or the read buffer overflows.
	bool WriteBitsFromBuffer( class bf_read *pIn, int nBits );
	
};

void CBitWrite::Finish( void )
{
	if ( m_nOutBitsAvail != 32 )
	{
		if ( m_pDataOut == m_pBufferEnd )
		{
			SetOverflowFlag();
		}
		StoreLittleDWord( m_pDataOut, 0, m_nOutBufWord );
	}
}

void CBitWrite::FlushNoCheck( void )
{
	StoreLittleDWord( m_pDataOut++, 0, m_nOutBufWord );
	m_nOutBitsAvail = 32;
	m_nOutBufWord = 0;										// ugh - I need this because of 32 bit writes. a<<=32 is a nop
	
}
void CBitWrite::Flush( void )
{
	if ( m_pDataOut == m_pBufferEnd )
	{
		SetOverflowFlag();
	}
	else
	{
		StoreLittleDWord( m_pDataOut++, 0, m_nOutBufWord );
	}
	m_nOutBufWord = 0;										// ugh - I need this because of 32 bit writes. a<<=32 is a nop
	m_nOutBitsAvail = 32;
	
}
void CBitWrite::WriteOneBitNoCheck( int nValue )
{
	m_nOutBufWord |= ( nValue & 1 ) << ( 32 - m_nOutBitsAvail );
	if ( --m_nOutBitsAvail == 0 )
	{
		FlushNoCheck();
	}
}

void CBitWrite::WriteOneBit( int nValue )
{
	m_nOutBufWord |= ( nValue & 1 ) << ( 32 - m_nOutBitsAvail );
	if ( --m_nOutBitsAvail == 0 )
	{
		Flush();
	}
}

[[gnu::always_inline]] inline void CBitWrite::WriteUBitLong( unsigned int nData, int nNumBits, bool bCheckRange )
{

	if ( nNumBits <= m_nOutBitsAvail )
	{
		if ( bCheckRange )
			m_nOutBufWord |= ( nData ) << ( 32 - m_nOutBitsAvail );
		else
			m_nOutBufWord |= ( nData & s_nMaskTable[ nNumBits] ) << ( 32 - m_nOutBitsAvail );
		m_nOutBitsAvail -= nNumBits;
		if ( m_nOutBitsAvail == 0 )
		{
			Flush();
		}
	}
	else
	{
		// split dwords case
		int nOverflowBits = ( nNumBits - m_nOutBitsAvail );
		m_nOutBufWord |= ( nData & s_nMaskTable[m_nOutBitsAvail] ) << ( 32 - m_nOutBitsAvail );
		Flush();
		m_nOutBufWord = ( nData >> ( nNumBits - nOverflowBits ) );
		m_nOutBitsAvail = 32 - nOverflowBits;
	}
}

[[gnu::always_inline]] inline void CBitWrite::WriteSBitLong( int nData, int nNumBits )
{
	WriteUBitLong( ( uint32 ) nData, nNumBits, false );
}

[[gnu::always_inline]] inline void CBitWrite::WriteUBitVar( unsigned int n )
{
	if ( n < 16 )
		WriteUBitLong( n, 6 );
	else
		if ( n < 256 )
			WriteUBitLong( ( n & 15 ) | 16 | ( ( n & ( 128 | 64 | 32 | 16 ) ) << 2 ), 10 );
		else
			if ( n < 4096 )
				WriteUBitLong( ( n & 15 ) | 32 | ( ( n & ( 2048 | 1024 | 512 | 256 | 128 | 64 | 32 | 16 ) ) << 2 ), 14 );
			else
			{
				WriteUBitLong( ( n & 15 ) | 48, 6 );
				WriteUBitLong( ( n >> 4 ), 32 - 4 );
			}
}

[[gnu::always_inline]] inline void CBitWrite::WriteBitFloat( float flValue )
{
	WriteUBitLong( *((uint32 *) &flValue ), 32 );
}

[[gnu::always_inline]] inline void CBitWrite::WriteFloat( float flValue )
{
	// Pre-swap the float, since WriteBits writes raw data
	LittleFloat( &flValue, &flValue );
	WriteUBitLong( *((uint32 *) &flValue ), 32 );
}

class CBitRead : public CBitBuffer
{
	uint32 m_nInBufWord;
	int m_nBitsAvail;
	uint32 const *m_pDataIn;
	uint32 const *m_pBufferEnd;
	uint32 const *m_pData;

public:
	CBitRead( const void *pData, int nBytes, int nBits = -1 )
	{
		StartReading( pData, nBytes, 0, nBits );
	}
	
	CBitRead( const char *pDebugName, const void *pData, int nBytes, int nBits = -1 )
	{
		SetDebugName( pDebugName );
		StartReading( pData, nBytes, 0, nBits );
	}

	CBitRead( void ) : CBitBuffer()
	{
	}

	[[gnu::always_inline]] inline int Tell( void ) const
	{
		return GetNumBitsRead();
	}
	
	[[gnu::always_inline]] inline std::size_t TotalBytesAvailable( void ) const
	{
		return m_nDataBytes;
	}

	[[gnu::always_inline]] inline int GetNumBitsLeft() const 
	{
		return m_nDataBits - Tell();
	}

	[[gnu::always_inline]] inline int GetNumBytesLeft() const
	{
		return GetNumBitsLeft() >> 3;
	}

	bool Seek( int nPosition );

	[[gnu::always_inline]] inline bool SeekRelative( int nOffset )
	{
		return Seek( GetNumBitsRead() + nOffset );
	}

	[[gnu::always_inline]] inline unsigned char const * GetBasePointer()
	{
		return reinterpret_cast< unsigned char const *>( m_pData );
	}

	void StartReading( const void *pData, int nBytes, int iStartBit = 0, int nBits = -1 );

	[[gnu::always_inline]] inline int GetNumBitsRead( void ) const;
	[[gnu::always_inline]] inline int GetNumBytesRead( void ) const;

	[[gnu::always_inline]] inline void GrabNextDWord( bool bOverFlowImmediately = false );
	[[gnu::always_inline]] inline void FetchNext( void );
	[[gnu::always_inline]] inline unsigned int ReadUBitLong( int numbits );
	[[gnu::always_inline]] inline int ReadSBitLong( int numbits );
	[[gnu::always_inline]] inline unsigned int ReadUBitVar( void );
	[[gnu::always_inline]] inline unsigned int PeekUBitLong( int numbits );
	[[gnu::always_inline]] inline float ReadBitFloat( void );
	float ReadBitCoord();
	float ReadBitCoordMP( EBitCoordType coordType );
	float ReadBitCellCoord( int bits, EBitCoordType coordType );
	float ReadBitNormal();
	void ReadBitVec3Coord( Vector& fa );
	void ReadBitVec3Normal( Vector& fa );
	void ReadBitAngles( QAngle& fa );
	bool ReadBytes(void *pOut, int nBytes);
	float ReadBitAngle( int numbits );

	// Returns 0 or 1.
	[[gnu::always_inline]] inline int	ReadOneBit( void );
	[[gnu::always_inline]] inline int ReadLong( void );
	[[gnu::always_inline]] inline int ReadChar( void );
	[[gnu::always_inline]] inline int ReadByte( void );
	[[gnu::always_inline]] inline int ReadShort( void );
	[[gnu::always_inline]] inline int ReadWord( void );
	[[gnu::always_inline]] inline float ReadFloat( void );
	void ReadBits(void *pOut, int nBits);

	// Returns false if bufLen isn't large enough to hold the
	// string in the buffer.
	//
	// Always reads to the end of the string (so you can read the
	// next piece of data waiting).
	//
	// If bLine is true, it stops when it reaches a '\n' or a null-terminator.
	//
	// pStr is always null-terminated (unless bufLen is 0).
	//
	// pOutN<umChars is set to the number of characters left in pStr when the routine is 
	// complete (this will never exceed bufLen-1).
	//
	bool ReadString( char *pStr, int bufLen, bool bLine=false, int *pOutNumChars=nullptr );
	bool ReadWString( wchar_t *pStr, int maxLenInChars, bool bLine=false, int *pOutNumChars=nullptr );
	char* ReadAndAllocateString( bool *pOverflow = 0 );

	int64 ReadLongLong( void );

	// reads a varint encoded integer
	uint32			ReadVarInt32();
	uint64			ReadVarInt64();
	int32			ReadSignedVarInt32() { return bitbuf::ZigZagDecode32( ReadVarInt32() ); }
	int64			ReadSignedVarInt64() { return bitbuf::ZigZagDecode64( ReadVarInt64() ); }
};

[[gnu::always_inline]] inline int CBitRead::GetNumBitsRead( void ) const
{
	if ( ! m_pData )									   // pesky null ptr bitbufs. these happen.
		return 0;

	int nCurOfs = int(((intp(m_pDataIn) - intp(m_pData))/4)-1);
	nCurOfs  *= 32;
	nCurOfs += ( 32 - m_nBitsAvail );
	int nAdjust = 8 * ( m_nDataBytes & 3 );
	return std::min( nCurOfs + nAdjust, m_nDataBits );
}

[[gnu::always_inline]] inline int CBitRead::GetNumBytesRead( void ) const
{
	return ( (GetNumBitsRead()+7) >> 3 );
}

[[gnu::always_inline]] inline void CBitRead::GrabNextDWord( bool bOverFlowImmediately )
{
	if ( m_pDataIn == m_pBufferEnd )
	{
		m_nBitsAvail = 1;									// so that next read will run out of words
		m_nInBufWord = 0;
		m_pDataIn++;										// so seek count increments like old
		if ( bOverFlowImmediately )
			SetOverflowFlag();
	}
	else
		if ( m_pDataIn > m_pBufferEnd )
		{
			SetOverflowFlag();
			m_nInBufWord = 0;
		}
		else
		{
			contract_assert( reinterpret_cast<intp>(m_pDataIn) + 3 < reinterpret_cast<intp>(m_pBufferEnd) );
			m_nInBufWord = ( *( m_pDataIn++ ) );
		}
}

[[gnu::always_inline]] inline void CBitRead::FetchNext( void )
{
	m_nBitsAvail = 32;
	GrabNextDWord( false );
}

int CBitRead::ReadOneBit( void )
{
	int nRet = m_nInBufWord & 1;
	if ( --m_nBitsAvail == 0 )
	{
		FetchNext();
	}
	else
		m_nInBufWord >>= 1;
	return nRet;
}

unsigned int CBitRead::ReadUBitLong( int numbits )
{
	if ( m_nBitsAvail >= numbits )
	{
		unsigned int nRet = m_nInBufWord & s_nMaskTable[ numbits ];
		m_nBitsAvail -= numbits;
		if ( m_nBitsAvail )
		{
			m_nInBufWord >>= numbits;
		}
		else
		{
			FetchNext();
		}
		return nRet;
	}
	else
	{
		// need to merge words
		unsigned int nRet = m_nInBufWord;
		numbits -= m_nBitsAvail;
		GrabNextDWord( true );
		if ( m_bOverflow )
			return 0;
		nRet |= ( ( m_nInBufWord & s_nMaskTable[numbits] ) << m_nBitsAvail );
		m_nBitsAvail = 32 - numbits;
		m_nInBufWord >>= numbits;
		return nRet;
	}
}

[[gnu::always_inline]] inline unsigned int CBitRead::PeekUBitLong( int numbits )
{
	int nSaveBA = m_nBitsAvail;
	int nSaveW = m_nInBufWord;
	uint32 const *pSaveP = m_pDataIn;
	unsigned int nRet = ReadUBitLong( numbits );
	m_nBitsAvail = nSaveBA;
	m_nInBufWord = nSaveW;
	m_pDataIn = pSaveP;
	return nRet;
}

[[gnu::always_inline]] inline int CBitRead::ReadSBitLong( int numbits )
{
	int nRet = ReadUBitLong( numbits );
	// sign extend
	return ( nRet << ( 32 - numbits ) ) >> ( 32 - numbits );
}

[[gnu::always_inline]] inline int CBitRead::ReadLong( void )
{
	return ( int ) ReadUBitLong( sizeof(int32) << 3 );
}

[[gnu::always_inline]] inline float CBitRead::ReadFloat( void )
{
	uint32 nUval = ReadUBitLong( sizeof(int32) << 3 );
	return * ( ( float * ) &nUval );
}

[[gnu::always_inline]] inline unsigned int CBitRead::ReadUBitVar( void )
{
	unsigned int ret = ReadUBitLong( 6 );
	switch( ret & ( 16 | 32 ) )
	{
		case 16:
			ret = ( ret & 15 ) | ( ReadUBitLong( 4 ) << 4 );
			contract_assert( ret >= 16 );
			break;
				
		case 32:
			ret = ( ret & 15 ) | ( ReadUBitLong( 8 ) << 4 );
			contract_assert( ret >= 256 );
			break;
		case 48:
			ret = ( ret & 15 ) | ( ReadUBitLong( 32 - 4 ) << 4 );
			contract_assert( ret >= 4096 );
			break;
	}
	return ret;
}

[[gnu::always_inline]] inline float CBitRead::ReadBitFloat( void )
{
	uint32 nvalue = ReadUBitLong( 32 );
	return *( ( float * ) &nvalue );
}

int CBitRead::ReadChar( void )
{
	return ReadSBitLong(sizeof(char) << 3);
}

int CBitRead::ReadByte( void )
{
	return ReadUBitLong(sizeof(unsigned char) << 3);
}

int CBitRead::ReadShort( void )
{
	return ReadSBitLong(sizeof(short) << 3);
}

int CBitRead::ReadWord( void )
{
	return ReadUBitLong(sizeof(unsigned short) << 3);
}

class bf_read : public CBitRead
{
public:
	[[gnu::always_inline]] inline bf_read( void ) : CBitRead()
	{
	}

	[[gnu::always_inline]] inline bf_read( const void *pData, int nBytes, int nBits = -1 ) : CBitRead( pData, nBytes, nBits )
	{
	}

	[[gnu::always_inline]] inline bf_read( const char *pDebugName, const void *pData, int nBytes, int nBits = -1 ) : CBitRead( pDebugName, pData, nBytes, nBits )
	{
	}
};

}
