#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;

// helper functions for parsing unicode file buffers
ucs2 *AdvanceOverWhitespace(ucs2 *start);
ucs2 *ReadUnicodeToken(ucs2 *start, ucs2 *token, int tokenBufferSize, bool &quoted);
ucs2 *ReadUnicodeTokenNoSpecial(ucs2 *start, ucs2 *token, int tokenBufferSize, bool &quoted);
ucs2 *ReadToEndOfLine(ucs2 *start);

// writing UTF-16 code units to unicode files
import ks.tier1;
void WriteUnicodeString(ks::byte_writer &buffer, const wchar_t *string, bool addQuotes = false);
void WriteAsciiStringAsUnicode(ks::byte_writer &buffer, const char *string, bool addQuotes = false);
