// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
#include "UnicodeFileHelpers.h"

//-----------------------------------------------------------------------------
// Purpose: Advances until non-whitespace hit
//-----------------------------------------------------------------------------
ucs2 *AdvanceOverWhitespace(ucs2 *Start)
{
	while (*Start != 0 && std::iswspace(*Start))
	{
		Start++;
	}

	return Start;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ucs2 *ReadUnicodeToken(ucs2 *start, ucs2 *token, int tokenBufferSize, bool &quoted)
{
	// skip over any whitespace
	start = AdvanceOverWhitespace(start);
	quoted = false;
	*token = 0;

	if (!*start)
	{
		return start;
	}

	// check to see if it's a quoted string
	if (*start == '\"')
	{
		quoted = true;
		// copy out the string until we hit an end quote
		start++;
		int count = 0;
		while (*start && *start != '\"' && count < tokenBufferSize-1)
		{
			// check for special characters
			if (*start == '\\' && *(start+1) == 'n')
			{
				start++;
				*token = '\n';
			}
			else if (*start == '\\' && *(start+1) == '\"')
			{
				start++;
				*token = '\"';
			}
			else
			{
				*token = *start;
			}

			start++;
			token++;
			count++;
		}

		if (*start == '\"')
		{
			start++;
		}
	}
	else
	{
		// copy out the string until we hit a whitespace
		int count = 0;
		while (*start && !std::iswspace(*start) && count < tokenBufferSize-1)
		{
			// no checking for special characters if it's not a quoted string
			*token = *start;

			start++;
			token++;
			count++;
		}
	}

	*token = 0;
	return start;
}

//-----------------------------------------------------------------------------
// Purpose: Same as above but no translation of \n
//-----------------------------------------------------------------------------
ucs2 *ReadUnicodeTokenNoSpecial(ucs2 *start, ucs2 *token, int tokenBufferSize, bool &quoted)
{
	// skip over any whitespace
	start = AdvanceOverWhitespace(start);
	quoted = false;
	*token = 0;

	if (!*start)
	{
		return start;
	}

	// check to see if it's a quoted string
	if (*start == '\"')
	{
		quoted = true;
		// copy out the string until we hit an end quote
		start++;
		int count = 0;
		while (*start && *start != '\"' && count < tokenBufferSize-1)
		{
			// check for special characters
			/*
			if (*start == '\\' && *(start+1) == 'n')
			{
				start++;
				*token = '\n';
			}
			else
			*/
			if (*start == '\\' && *(start+1) == '\"')
			{
				start++;
				*token = '\"';
			}
			else
			{
				*token = *start;
			}

			start++;
			token++;
			count++;
		}

		if (*start == '\"')
		{
			start++;
		}
	}
	else
	{
		// copy out the string until we hit a whitespace
		int count = 0;
		while (*start && !std::iswspace(*start) && count < tokenBufferSize-1)
		{
			// no checking for special characters if it's not a quoted string
			*token = *start;

			start++;
			token++;
			count++;
		}
	}

	*token = 0;
	return start;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the first character after the next EOL characters
//-----------------------------------------------------------------------------
ucs2 *ReadToEndOfLine(ucs2 *start)
{
	if (!*start)
		return start;

	while (*start)
	{
		if (*start == 0x0D || *start== 0x0A)
			break;
		start++;
	}

	while (*start == 0x0D || *start== 0x0A)
		start++;

	return start;
}

//-----------------------------------------------------------------------------
// Purpose: file writing
//-----------------------------------------------------------------------------
void WriteUnicodeString(ks::byte_writer &buf, const wchar_t *string, bool addQuotes)
{
	if (addQuotes)
	{
		buf.write< unsigned short >( '\"' );
	}

	for (const wchar_t *ws = string; *ws != 0; ws++)
	{
		// handle special characters
		if (addQuotes && *ws == '\"')
		{
			buf.write< unsigned short >( '\\' );
		}
		// write the character
		buf.write< unsigned short >( *ws );
	}

	if (addQuotes)
	{
		buf.write< unsigned short >( '\"' );
	}
}

//-----------------------------------------------------------------------------
// Purpose: file writing
//-----------------------------------------------------------------------------
void WriteAsciiStringAsUnicode(ks::byte_writer &buf, const char *string, bool addQuotes)
{
	if (addQuotes)
	{
		buf.write< unsigned short >( '\"' );
	}

	for (const char *sz = string; *sz != 0; sz++)
	{
		// handle special characters
		if (addQuotes && *sz == '\"')
		{
			buf.write< unsigned short >( '\\' );
		}
		buf.write< unsigned short >( *sz );
	}

	if (addQuotes)
	{
		buf.write< unsigned short >( '\"' );
	}
}
