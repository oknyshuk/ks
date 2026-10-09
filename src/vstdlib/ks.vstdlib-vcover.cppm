// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.vstdlib:vcover;

import std;
import ks.tier1;

export extern "C++" {

class CVCoverage
{
public:
	CVCoverage() :
	  m_bActive( false ),
	  m_depth( 0 ),
	  m_token( 1 )
	{
	}

	bool IsActive() const
	{
		return m_bActive;
	}

	void SetActive( bool bActive )
	{
		contract_assert( bActive != m_bActive );
		m_bActive = bActive;
		if ( bActive )
			++m_token;
	}

	void Begin()
	{
		++m_depth;
	}

	void End()
	{
		--m_depth;
	}

	void Reset()
	{
		m_locations.clear();
	}

	bool ShouldCover( unsigned token ) const
	{
		return ( m_bActive && m_depth > 0 && token != m_token );
	}

	unsigned Cover( const char *pszFile, int line )
	{
		Location_t location = { pszFile, line };

		m_locations.insert( location );

		return m_token;
	}

	void Report()
	{
		for ( const Location_t &location : m_locations )
		{
			Msg( "%s(%d) :\n", location.pszFile, location.line );
		}
	}

private:
	struct Location_t
	{
		const char *pszFile;
		int line;

	};

	class CLocationLess
	{
	public:
		bool operator()(  const Location_t &lhs, const Location_t &rhs ) const
		{
			if ( lhs.line < rhs.line )
			{
				return true;
			}

			return CaselessStringLessThan( lhs.pszFile, rhs.pszFile );
		}
	};

	bool m_bActive;
	int m_depth;
	unsigned m_token;

	std::multiset< Location_t, CLocationLess > m_locations;
};

extern CVCoverage g_VCoverage;

}
