#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.dbg;
import ks.core.platform;
import ks.platform;
import ks.simd;
import std;

typedef enum
{
	TOKENSTRINGTOOLONG = -4,
	TOKENERROR = -3,
	TOKENNONE = -2,
	TOKENEOF = -1,
	OPERATOR,
	INTEGER,
	STRING,
	IDENT
} trtoken_t;


template <typename T1, typename T2> [[gnu::always_inline]] inline auto IsToken( T1 s1, T2 s2 ) { return !strcmpi(s1, s2); }

inline constexpr auto MAX_TOKEN = 128 + 1;
inline constexpr auto MAX_IDENT = 64 + 1;
inline constexpr auto MAX_STRING = 128 + 1;


class TokenReader
{
public:

	TokenReader();

	bool Open(const char *pszFilename);
	trtoken_t NextToken(char *pszStore, int nSize);
	trtoken_t NextTokenDynamic(char **ppszStore);
	void Close();

	void IgnoreTill(trtoken_t ttype, const char *pszToken);
	void Stuff(trtoken_t ttype, const char *pszToken);
	bool Expecting(trtoken_t ttype, const char *pszToken);
	const char *Error(char *error, ...);
	trtoken_t PeekTokenType(char* = nullptr, int maxlen = 0);

	inline int GetErrorCount(void);

private:

	trtoken_t GetString(char *pszStore, int nSize);
	bool SkipWhiteSpace(void);

	bool AtEnd() const { return m_nPos >= m_file.size(); }
	char PeekChar() const { return AtEnd() ? '\0' : static_cast< char >( m_file[m_nPos] ); }
	char PeekChar( std::size_t nOffset ) const { return m_nPos + nOffset < m_file.size() ? static_cast< char >( m_file[m_nPos + nOffset] ) : '\0'; }
	char GetChar() { return AtEnd() ? '\0' : static_cast< char >( m_file[m_nPos++] ); }

	std::vector< std::byte > m_file;
	std::size_t m_nPos;
	bool m_bOpen;
	int m_nLine;
	int m_nErrorCount;

	char m_szFilename[128];
	char m_szStuffed[128];
	bool m_bStuffed;
	trtoken_t m_eStuffed;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the total number of parsing errors since this file was opened.
//-----------------------------------------------------------------------------
int TokenReader::GetErrorCount(void)
{
	return(m_nErrorCount);
}
