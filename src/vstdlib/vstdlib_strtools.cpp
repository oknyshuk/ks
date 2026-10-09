// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: C runtime and standard library wrappers / equivalents / upgrades.
// Allows centralization of CRT and CRT-like code and consistent behavior across platforms.

import std;
import ks.tier1;

// Prints out a memory dump where stuff that's ascii is human readable, etc.
void V_LogMultiline(bool input, char const *label, const char *data, std::size_t len, std::string &output);

// If the string above is really long, you can't pass it into varargs print routines, so this splits everything out into smaller chunks
void V_LogMultilineToArray( bool input, char const *label, const char *data, std::size_t len, std::vector< std::string > &output )
{
	static const char HEX[] = "0123456789abcdef";
	const char * direction = (input ? " << " : " >> ");
	const std::size_t LINE_SIZE = 24;
	char hex_line[LINE_SIZE * 9 / 4 + 2], asc_line[LINE_SIZE + 1];
	while (len > 0) 
	{
		V_memset(asc_line, ' ', sizeof(asc_line));
		V_memset(hex_line, ' ', sizeof(hex_line));
		std::size_t line_len = std::min(len, LINE_SIZE);
		for (std::size_t i=0; i<line_len; ++i) {
			unsigned char ch = static_cast<unsigned char>(data[i]);
			asc_line[i] = ( V_isprint(ch) && !V_iscntrl(ch) ) ? data[i] : '.';
			hex_line[i*2 + i/4] = HEX[ch >> 4];
			hex_line[i*2 + i/4 + 1] = HEX[ch & 0xf];
		}
		asc_line[sizeof(asc_line)-1] = 0;
		hex_line[sizeof(hex_line)-1] = 0;
		std::string s;
		s = ks::format_printf( "%s %s %s %s\n", label, direction, asc_line, hex_line );
		output.push_back( s );
		data += line_len;
		len -= line_len;
	}
}



void V_SplitString2InPlace( char *pString, const char **pSeparators, int nSeparators, std::vector< const char * > &outStrings )
{
	// The pieces point into pString, which is cut in place.
	outStrings.clear();
	char *pCurPos = pString;
	while ( 1 )
	{
		int iFirstSeparator = -1;
		char *pFirstSeparator = 0;
		for ( int i=0; i < nSeparators; i++ )
		{
			char *pTest = V_stristr( pCurPos, pSeparators[i] );
			if ( pTest && (!pFirstSeparator || pTest < pFirstSeparator) )
			{
				iFirstSeparator = i;
				pFirstSeparator = pTest;
			}
		}

		if ( pFirstSeparator )
		{
			// Split on this separator and continue on.
			int separatorLen = V_strlen( pSeparators[iFirstSeparator] );
			V_memset( pFirstSeparator, 0, separatorLen );
			outStrings.push_back( pCurPos );

			pCurPos = pFirstSeparator + separatorLen;
		}
		else
		{
			// Copy the rest of the string
			if ( *pCurPos != '\0' )
			{
				outStrings.push_back( pCurPos );
			}
			return;
		}
	}
}

void V_SplitStringInPlace( char *pString, const char *pSeparator, std::vector< const char * > &outStrings )
{
	V_SplitString2InPlace( pString, &pSeparator, 1, outStrings );
}
