
import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.strtools;
import ks.vstdlib.vstrtools;




//-----------------------------------------------------------------------------
// Purpose: Converts a UTF8 string into a unicode string
//-----------------------------------------------------------------------------
int V_UTF8ToUnicode( const char *pUTF8, wchar_t *pwchDest, int cubDestSizeInBytes )
{
	if ( !pUTF8 )
		return 0;

	AssertValidStringPtr(pUTF8);
	AssertValidWritePtr(pwchDest);

	pwchDest[0] = 0;
	int cchResult = -1;
	unsigned long nLenUnicde = cubDestSizeInBytes;
	unsigned long nMaxUTF8 = std::strlen(pUTF8) + 1;
	char *pIn = (char *)pUTF8;
	char *pOut = (char *)pwchDest;
	const unsigned long nInputCharCount = nMaxUTF8;
	const ks::platform::convert_result eConvert = ks::platform::convert_text( "UTF-32LE", "UTF-8", pIn, nMaxUTF8, pOut, nLenUnicde );
	if ( eConvert != ks::platform::convert_result::no_converter )
	{
		cchResult = ( eConvert == ks::platform::convert_result::ok ) ? (int)( nInputCharCount - nMaxUTF8 ) : 0;
	}
	pwchDest[(cubDestSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return cchResult;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a unicode string into a UTF8 (standard) string
//-----------------------------------------------------------------------------
int V_UnicodeToUTF8( const wchar_t *pUnicode, char *pUTF8, int cubDestSizeInBytes )
{
	AssertValidStringPtr(pUTF8, cubDestSizeInBytes);
	AssertValidReadPtr(pUnicode);

	if ( cubDestSizeInBytes > 0 )
	{
		pUTF8[0] = 0;
	}

	int cchResult = 0;
	if ( pUnicode && pUTF8 )
	{
		unsigned long nLenUnicde = ( std::wcslen(pUnicode) + 1 ) * sizeof(wchar_t); // 4 bytes per wchar vs. 1 byte for utf8 for simple english
		unsigned long nMaxUTF8 = cubDestSizeInBytes;
		char *pIn = (char *)pUnicode;
		char *pOut = (char *)pUTF8;
		const ks::platform::convert_result eConvert = ks::platform::convert_text( "UTF-8", "UTF-32LE", pIn, nLenUnicde, pOut, nMaxUTF8 );
		if ( eConvert != ks::platform::convert_result::no_converter )
		{
			cchResult = ( eConvert == ks::platform::convert_result::ok ) ? (int)nMaxUTF8 : 0;
		}
	}

	if ( cubDestSizeInBytes > 0 )
	{
		pUTF8[cubDestSizeInBytes - 1] = 0;
	}

	return cchResult;
}


//-----------------------------------------------------------------------------
// Purpose: Converts a ucs2 string to a unicode (wchar_t) one, no-op on win32
//-----------------------------------------------------------------------------
int V_UCS2ToUnicode( const ucs2 *pUCS2, wchar_t *pUnicode, int cubDestSizeInBytes )
{
	AssertValidWritePtr(pUnicode);
	AssertValidReadPtr(pUCS2);
	
	pUnicode[0] = 0;
	int cchResult = -1;
	unsigned long nLenUnicde = cubDestSizeInBytes;
	unsigned long nMaxUTF8 = cubDestSizeInBytes;
	char *pIn = (char *)pUCS2;
	char *pOut = (char *)pUnicode;
	const ks::platform::convert_result eConvert = ks::platform::convert_text( "UCS-4LE", "UCS-2LE", pIn, nLenUnicde, pOut, nMaxUTF8 );
	if ( eConvert != ks::platform::convert_result::no_converter )
	{
		cchResult = ( eConvert == ks::platform::convert_result::ok ) ? nMaxUTF8 : 0;
	}
	pUnicode[(cubDestSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return cchResult;	

}


//-----------------------------------------------------------------------------
// Purpose: Converts a wchar_t string into a UCS2 string -noop on windows
//-----------------------------------------------------------------------------
int V_UnicodeToUCS2( const wchar_t *pUnicode, int cubSrcInBytes, char *pUCS2, int cubDestSizeInBytes )
{
	 // TODO: MACMERGE: Figure out how to convert from 2-byte Win32 wchars to platform wchar_t type that can be 4 bytes
	std::size_t cchResult = -1;
	unsigned long nLenUnicde = cubSrcInBytes;
	unsigned long nMaxUCS2 = cubDestSizeInBytes;
	char *pIn = (char*)pUnicode;
	char *pOut = pUCS2;
	const ks::platform::convert_result eConvert = ks::platform::convert_text( "UCS-2LE", "UTF-32LE", pIn, nLenUnicde, pOut, nMaxUCS2 );
	if ( eConvert != ks::platform::convert_result::no_converter )
	{
		cchResult = ( eConvert == ks::platform::convert_result::ok ) ? cubSrcInBytes / sizeof( wchar_t ) : 0;
	}
	return cchResult;	
}

// UTF-8 encodes each character (code point) in 1 to 4 octets (8-bit bytes).
// The first 128 characters of the Unicode character set (which correspond directly to the ASCII) use a single octet with the same binary value as in ASCII.
// url:http://en.wikipedia.org/wiki/UTF-8
constexpr auto MAX_UTF8_CHARACTER_BYTES = 4;


//-----------------------------------------------------------------------------
// Purpose: Converts a ucs-2 (windows wchar_t) string into a UTF8 (standard) string
//-----------------------------------------------------------------------------
int V_UCS2ToUTF8( const ucs2 *pUCS2, char *pUTF8, int cubDestSizeInBytes )
{
	AssertValidStringPtr(pUTF8, cubDestSizeInBytes);
	AssertValidReadPtr(pUCS2);
	contract_assert( cubDestSizeInBytes >= 1 ); // must have at least 1 byte to write the terminator character
	
	pUTF8[0] = '\0';
	std::size_t cchResult = -1;
	unsigned long nLenUnicde = cubDestSizeInBytes;
	unsigned long nMaxUTF8 = cubDestSizeInBytes;
	char *pIn = (char *)pUCS2;
	char *pOut = (char *)pUTF8;
	const ks::platform::convert_result eConvert = ks::platform::convert_text( "UTF-8", "UCS-2LE", pIn, nLenUnicde, pOut, nMaxUTF8 );
	if ( eConvert != ks::platform::convert_result::no_converter )
	{
		cchResult = ( eConvert == ks::platform::convert_result::ok ) ? nMaxUTF8 : 0;
	}
	pUTF8[cubDestSizeInBytes - 1] = '\0';
	return cchResult;	
}


//-----------------------------------------------------------------------------
// Purpose: Converts a UTF8 to ucs-2 (windows wchar_t)
//-----------------------------------------------------------------------------
int V_UTF8ToUCS2( const char *pUTF8, int cubSrcInBytes, ucs2 *pUCS2, int cubDestSizeInBytes )
{
	AssertValidStringPtr(pUTF8, cubDestSizeInBytes);
	AssertValidReadPtr(pUCS2);

	pUCS2[0] = 0;
	std::size_t cchResult = -1;
	unsigned long nLenUnicde = cubSrcInBytes;
	unsigned long nMaxUTF8 = cubDestSizeInBytes;
	char *pIn = (char *)pUTF8;
	char *pOut = (char *)pUCS2;
	const ks::platform::convert_result eConvert = ks::platform::convert_text( "UCS-2LE", "UTF-8", pIn, nLenUnicde, pOut, nMaxUTF8 );
	if ( eConvert != ks::platform::convert_result::no_converter )
	{
		cchResult = ( eConvert == ks::platform::convert_result::ok ) ? cubSrcInBytes : 0;
	}
	pUCS2[ (cubDestSizeInBytes/sizeof(ucs2)) - 1] = 0;
	return cchResult;	
}

//-----------------------------------------------------------------------------
// Purpose: copies at most nMaxBytes of the UTF-8 input data into the destination,
// ensuring that a trailing multi-byte sequence isn't truncated.
//-----------------------------------------------------------------------------
void * V_UTF8_strncpy( char *pDest, const char *pSrc, std::size_t nMaxBytes )
{
	std::strncpy( pDest, pSrc, nMaxBytes );

	// http://en.wikipedia.org/wiki/UTF-8
	int end = nMaxBytes-1;
	pDest[end] = 0;

	// walk backwards, ignoring nulls
	while ( end > 0 && pDest[end] == 0 )
		--end;

	// found a non-null - see if it's part of a multi-byte sequence
	int nBytesSeen = 0;
	while ( end >= 0 && ( pDest[end] & 0xC0 ) == 0x80 )		// utf8 multi-byte trailing characters begin with 10xxxxxx
	{
		nBytesSeen++;
		--end;
	}

	int nBytesExpected = 0;
	if ( ( pDest[end] & 0xC0 ) == 0xC0 )		// utf8 multi-byte character sequences begin with 11xxxxxx
	{
		for ( int i = 6; i > 1; --i )
		{
			if ( (char)( pDest[end] >> i ) & 0x1 )
				++nBytesExpected;
			else
				break;
		}
	}

	if ( nBytesExpected != nBytesSeen )
		pDest[end] = 0;

	return pDest;
}
