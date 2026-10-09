// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation



import std;

import ks.vstdlib;
#include "filesystem.h"

#include "localize/ilocalize.h"
#include "UnicodeFileHelpers.h"


constexpr auto MAX_LOCALIZED_CHARS = 4096;


//-----------------------------------------------------------------------------
// 
// Internal implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Maps token names to localized unicode strings
//-----------------------------------------------------------------------------
class CLocalize : public CTier2AppSystem< ILocalize >
{
	typedef CTier2AppSystem< ILocalize > BaseClass;

	// Methods of IAppSystem
public:
	virtual InitReturnVal_t Init();

	// ILocalize overrides
public:
	virtual bool AddFile( const char *fileName, const char *pPathID, bool bIncludeFallbackSearchPaths );
	virtual void RemoveAll();
	virtual wchar_t *Find(const char *pName);
	virtual const wchar_t *FindSafe(const char *tokenName);
	virtual int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes);
	virtual int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize);
	virtual LocalizeStringIndex_t FindIndex(const char *pName);
	virtual const char *GetNameByIndex(LocalizeStringIndex_t index);
	virtual wchar_t *GetValueByIndex(LocalizeStringIndex_t index);
	virtual LocalizeStringIndex_t GetFirstStringIndex();
	virtual LocalizeStringIndex_t GetNextStringIndex(LocalizeStringIndex_t index);
	virtual void AddString(const char *tokenName, wchar_t *unicodeString, const char *fileName);
	virtual void SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue);
	virtual bool SaveToFile( const char *fileName );
	virtual int GetLocalizationFileCount();
	virtual const char *GetLocalizationFileName(int index);
	virtual const char *GetFileNameByIndex(LocalizeStringIndex_t index);
	virtual void ReloadLocalizationFiles( );
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *dialogVariables);
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *dialogVariables);
	virtual void SetTextQuery( ILocalizeTextQuery *pQuery );
	virtual void InstallChangeCallback( ILocalizationChangeCallback *pCallback );
	virtual void RemoveChangeCallback( ILocalizationChangeCallback *pCallback );
	virtual const char *FindAsUTF8( const char *pchTokenName );
	virtual wchar_t* GetAsianFrequencySequence( const char * pLanguage );

protected:
	// internal "interface"
	virtual void ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, std::va_list argList);
	virtual void ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, std::va_list argList);

	virtual void ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables);
	virtual void ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables);

	// Other public methods
public:
	CLocalize();
	virtual ~CLocalize();

	// returns whether a file has already been loaded
	bool LocalizationFileIsLoaded( const char *name );

private:
	struct localizedstring_t
	{
		LocalizeStringIndex_t nameIndex;
		LocalizeStringIndex_t valueIndex;
		const char *filename;	// in ks::global_symbols(), so equal names share a pointer; null if none
	};

	struct LocalizationFileInfo_t
	{
		std::string	symName;
		std::string	symPathID;
		bool		bIncludeFallbacks;

		static bool LessFunc( const LocalizationFileInfo_t& lhs, const LocalizationFileInfo_t& rhs )
		{
			int iresult = V_stricmp( lhs.symPathID.c_str(), rhs.symPathID.c_str() );
			if ( iresult != 0 )
			{
				return iresult == -1;
			}

			return V_stricmp( lhs.symName.c_str(), rhs.symName.c_str() ) < 0;
		}
	};

	// Orders value indices by their text in m_Values.
	struct FastValueLess
	{
		using is_transparent = void;
		const std::vector< wchar_t > *m_pValues;
		const wchar_t *Str( int i ) const { return &( *m_pValues )[ i ]; }
		bool operator()( int a, int b ) const { return std::wcscmp( Str( a ), Str( b ) ) < 0; }
		bool operator()( int a, const wchar_t *b ) const { return std::wcscmp( Str( a ), b ) < 0; }
		bool operator()( const wchar_t *a, int b ) const { return std::wcscmp( a, Str( b ) ) < 0; }
	};

private:
	bool AddAllLanguageFiles( const char *baseFileName );
	void BuildFastValueLookup();
	void DiscardFastValueLookup();
	int FindExistingValueIndex( const wchar_t *value );
	bool ReadLocalizationFile( const char *pRelativePath, const char *pPathID );
	void InvokeChangeCallbacks( );
	virtual int ConvertANSIToUCS2(const char *ansi, ucs2 *unicode, int unicodeBufferSizeInBytes);
	virtual int ConvertUCS2ToANSI(const ucs2 *unicode, char *ansi, int ansiBufferSize);
	virtual void AddString(const char *tokenName, ucs2 *unicodeString, const char *fileName);
	char m_szLanguage[64];
	bool m_bUseOnlyLongestLanguageString;
	bool m_bSuppressChangeCallbacks;
	bool m_bQueuedChangeCallback;

	// Stores the symbol lookup. String indices are handles given out through ILocalize: stable
	// until RemoveAll, so they index this vector; m_LookupByName orders them by name. A multimap:
	// FindIndex strips '#' but AddString doesn't, so a '#' token can be added twice.
	std::vector< localizedstring_t > m_Lookup;
	std::multimap< std::string, LocalizeStringIndex_t, ks::iless > m_LookupByName;
	
	// stores the string data
	std::vector< char > m_Names;
	std::vector< wchar_t > m_Values;
	const char *m_CurrentFile = nullptr;	// in ks::global_symbols()
	std::vector< LocalizationFileInfo_t > m_LocalizationFiles;
	std::multiset< int, FastValueLess >	m_FastValueLookup{ FastValueLess{ &m_Values } };
	ILocalizeTextQuery *m_pQuery;
	static CLocalize *s_pTable;
	std::vector< ILocalizationChangeCallback* > m_ChangeCallbacks;

	std::vector< std::byte > m_AsianFrequencySequence;
	bool m_bAsianFrequencySequenceLoaded;
};

// global instance of table
static CLocalize s_Localize;

// expose the interface
static InterfaceReg s_Reg_CLocalize_ILocalize( []() -> void * { return static_cast<ILocalize *>( &s_Localize ); }, LOCALIZE_INTERFACE_VERSION );;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLocalize::CLocalize()
{
	m_bUseOnlyLongestLanguageString = false;
	m_bSuppressChangeCallbacks = false;
	m_bQueuedChangeCallback = false;
	m_pQuery = nullptr;
	m_bAsianFrequencySequenceLoaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalize::~CLocalize()
{
	m_Names.clear(); m_Names.shrink_to_fit();
	m_Values.clear(); m_Values.shrink_to_fit();
	m_LocalizationFiles.clear(); m_LocalizationFiles.shrink_to_fit();
}


//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
InitReturnVal_t CLocalize::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_bUseOnlyLongestLanguageString = ( CommandLine()->FindParm("-all_languages") > 0 );
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Sets the callback used to check length of a localization string
//-----------------------------------------------------------------------------
void CLocalize::SetTextQuery( ILocalizeTextQuery *pQuery )
{
	m_pQuery = pQuery;
}


//-----------------------------------------------------------------------------
// Add, remove, invoke localization string change callbacks
//-----------------------------------------------------------------------------
void CLocalize::InstallChangeCallback( ILocalizationChangeCallback *pCallback )
{
	if ( ks::find_index( m_ChangeCallbacks, pCallback ) != -1 )
	{
		Warning( "CLocalize::InstallChangeCallback: Attempted to add the same callback twice!\n" );
		return;
	}

	m_ChangeCallbacks.push_back( pCallback );
}

void CLocalize::RemoveChangeCallback( ILocalizationChangeCallback *pCallback )
{
	ks::erase_first( m_ChangeCallbacks, pCallback );
}


//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
const char *CLocalize::FindAsUTF8( const char *pchTokenName )
{
	wchar_t *pwch = Find( pchTokenName );
	if ( !pwch )
		return pchTokenName;

	static char rgchT[2048];
	V_UnicodeToUTF8( pwch, rgchT, sizeof( rgchT ) );
	return rgchT;
}


void CLocalize::InvokeChangeCallbacks( )
{
	// This is to prevent a ton of change callbacks while loading using -all_languages
	if ( m_bSuppressChangeCallbacks )
	{
		m_bQueuedChangeCallback = true;
		return;
	}

	int nCount = std::ssize( m_ChangeCallbacks );
	for ( int i = 0; i < nCount; ++i )
	{
		m_ChangeCallbacks[i]->OnLocalizationChanged();
	}
}


int DistanceToEndOfLine( ucs2 *start )
{
	int nResult = 0;

	if ( !*start )
	{
		return nResult;
	}

	while ( *start )
	{
		if ( *start == 0x0D || *start== 0x0A )
		{
			break;
		}

		start++;
		nResult++;
	}

	while ( *start == 0x0D || *start== 0x0A )
	{
		start++;
		nResult++;
	}

	return nResult;
}

//-----------------------------------------------------------------------------
// Purpose:Reads the contents of a file
//-----------------------------------------------------------------------------
bool CLocalize::ReadLocalizationFile( const char *pRelativePath, const char *pPathID )
{
	FileHandle_t file = g_pFullFileSystem->Open( pRelativePath, "rb", pPathID );
	if ( FILESYSTEM_INVALID_HANDLE == file )
		return false;

	// this is an optimization so that the filename string doesn't have to get converted to a symbol for each key/value
	m_CurrentFile = ks::global_symbols().intern( pRelativePath ).data();

	// read into a memory block
	int fileSize = g_pFullFileSystem->Size(file);
	int bufferSize = g_pFullFileSystem->GetOptimalReadSize( file, fileSize + sizeof(wchar_t) );
	ucs2 *memBlock = (ucs2 *)g_pFullFileSystem->AllocOptimalReadBuffer(file, bufferSize);
	bool bReadOK = ( g_pFullFileSystem->ReadEx(memBlock, bufferSize, fileSize, file) != 0 );

	// finished with file
	g_pFullFileSystem->Close(file);

	// null-terminate the stream
	memBlock[fileSize / sizeof(ucs2)] = 0x0000;

	// check the first character, make sure this a little-endian unicode file
	ucs2 *data = memBlock;
	ucs2 signature = ( data[0] );
	if ( !bReadOK || signature != 0xFEFF )
	{
		Msg( "Ignoring non-unicode close caption file %s\n", pRelativePath );
		g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
		m_CurrentFile = nullptr;
		return false;
	}

	// ensure little-endian unicode reads correctly on all platforms

	// skip past signature
	data++;

	// parse out a token at a time
	enum states_e
	{
		STATE_BASE,		// looking for base settings
		STATE_TOKENS,	// reading in unicode tokens
	};

	bool bQuoted;
	bool bEnglishFile = false;
	if ( V_stristr(pRelativePath, "_english.txt") )
	{
		bEnglishFile = true;
	}

	bool spew = false;
	if ( CommandLine()->FindParm( "-ccsyntax" ) )
	{
		spew = true;
	}

	BuildFastValueLookup();

	CExpressionEvaluator ExpressionHandler;

	states_e state = STATE_BASE;
	while (1)
	{
		// read the key and the value
		ucs2 keytoken[128];
		data = ReadUnicodeToken(data, keytoken, 128, bQuoted);
		if (!keytoken[0])
			break;	// we've hit the null terminator

		// convert the token to a string
		char key[128];
		ConvertUCS2ToANSI(keytoken, key, sizeof(key));

		// if we have a C++ style comment, read to end of line and continue
		if (!strnicmp(key, "//", 2))
		{
			data = ReadToEndOfLine(data);
			continue;
		}

		if ( spew )
		{
			Msg( "%s\n", key );
		}

		ucs2 valuetoken[ MAX_LOCALIZED_CHARS ];

		bool bEnoughCapacity = true;

		if ( DistanceToEndOfLine( data ) > ( MAX_LOCALIZED_CHARS - 1 ) )
		{
			Warning( "Error: Localization key value exceeds MAX_LOCALIZED_CHARS. Problem key: %s\n", key );
			bEnoughCapacity = false;
		}

		data = ReadUnicodeToken(data, valuetoken, MAX_LOCALIZED_CHARS, bQuoted);
		if (!valuetoken[0] && !bQuoted)
			break;	// we've hit the null terminator

		if (state == STATE_BASE)
		{
			if (!stricmp(key, "Language"))
			{
				// copy out our language setting
				char value[MAX_LOCALIZED_CHARS];
				ConvertUCS2ToANSI(valuetoken, value, sizeof(value));
				std::strncpy(m_szLanguage, value, sizeof(m_szLanguage) - 1);
			}
			else if (!stricmp(key, "Tokens"))
			{
				state = STATE_TOKENS;
			}
			else if (!stricmp(key, "}"))
			{
				// we've hit the end
				break;
			}
		}
		else if (state == STATE_TOKENS)
		{
			if (!stricmp(key, "}"))
			{
				// end of tokens
				state = STATE_BASE;
			}
			else
			{
				// skip our [english] beginnings (in non-english files)
				if ( (bEnglishFile) || (!bEnglishFile && strnicmp(key, "[english]", 9)))
				{
					// Check for a conditional tag
					bool bAccepted = true;
					ucs2 conditional[ MAX_LOCALIZED_CHARS ];
					ucs2 *tempData = ReadUnicodeToken(data, conditional, MAX_LOCALIZED_CHARS, bQuoted);
					char cond[MAX_LOCALIZED_CHARS];
 					V_UCS2ToUTF8( conditional, cond, sizeof(cond) );
					if ( !bQuoted && (std::strstr( cond, "[$" )||std::strstr( cond, "[!$" )) )
					{
						// Evaluate the conditional tag
						char cond[MAX_LOCALIZED_CHARS];
						ConvertUCS2ToANSI( conditional, cond, sizeof( cond ) );
						ExpressionHandler.Evaluate( bAccepted, cond );
						data = tempData;
					}
					if ( bAccepted && bEnoughCapacity )
					{
						// add the string to the table
						AddString(key, valuetoken, nullptr);
					}
				}
			}
		}
	}

	g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
	m_CurrentFile = nullptr;
	DiscardFastValueLookup();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Adds the contents of a file 
//-----------------------------------------------------------------------------
bool CLocalize::AddFile( const char *szFileName, const char *pPathID, bool bIncludeFallbackSearchPaths )
{
	// use the correct file based on the chosen language
	static const char *const LANGUAGE_STRING = "%language%";
	static const char *const ENGLISH_STRING = "english";
	static const int MAX_LANGUAGE_NAME_LENGTH = 64;
	int offs = 0;
	bool success = false;

	char language[MAX_LANGUAGE_NAME_LENGTH];
	std::memset( language, 0, sizeof(language) );

	if ( V_IsAbsolutePath( szFileName ) )
	{
		Warning( "Full paths not allowed in localization file specificaton %s\n", szFileName );
		return false;
	}

	const char *langptr = std::strstr(szFileName, LANGUAGE_STRING);
	if (langptr)
	{
		// LOAD THE ENGLISH FILE FIRST
		// always load the file to make sure we're not missing any strings
		// copy out the initial part of the string
		offs = langptr - szFileName;
		char fileName[MAX_PATH];
		std::strncpy(fileName, szFileName, offs);
		fileName[offs] = 0;

		if ( m_bUseOnlyLongestLanguageString )
		{
			return AddAllLanguageFiles( fileName );
		}

		// append "english" as our default language
		V_strncat(fileName, ENGLISH_STRING, sizeof( fileName ), COPY_ALL_CHARACTERS );

		// append the end of the initial string
		offs += std::strlen(LANGUAGE_STRING);
		V_strncat(fileName, szFileName + offs, sizeof( fileName ), COPY_ALL_CHARACTERS);

		success = AddFile( fileName, pPathID, bIncludeFallbackSearchPaths );

		bool bValid = true;
		if ( CommandLine()->CheckParm( "-language" ) )
		{
			V_strncpy( language, CommandLine()->ParmValue( "-language", "english" ), sizeof( language ) );
			bValid = true;
		}
		else
		{
			bValid = false;
		}
		if ( bValid && !V_stricmp( language, "unknown" ) )
		{
			// Fall back to english
			bValid = false;
		}

		// LOAD THE LOCALIZED FILE IF IT'S NOT ENGLISH
		// append the language
		if ( bValid )
		{
			if ( std::strlen(language) != 0 && stricmp(language, ENGLISH_STRING) != 0 )
			{
				// copy out the initial part of the string
				offs = langptr - szFileName;
				std::strncpy(fileName, szFileName, offs);
				fileName[offs] = 0;

				V_strncat(fileName, language, sizeof( fileName ), COPY_ALL_CHARACTERS);

				// append the end of the initial string
				offs += std::strlen(LANGUAGE_STRING);
				V_strncat(fileName, szFileName + offs, sizeof( fileName ), COPY_ALL_CHARACTERS );

				success &= AddFile( fileName, pPathID, bIncludeFallbackSearchPaths );
			}
		}
		return success;
	}

	// store the localization file name if it doesn't already exist
	LocalizationFileInfo_t search;
	search.symName = szFileName;
	search.symPathID = pPathID ? pPathID : "";
	search.bIncludeFallbacks = false;

	int lfc = std::ssize( m_LocalizationFiles );
	for ( int lf = 0; lf < lfc; ++lf )
	{
		LocalizationFileInfo_t& entry = m_LocalizationFiles[ lf ];
		if ( !V_stricmp( entry.symName.c_str(), szFileName ) )
		{
			m_LocalizationFiles.erase( m_LocalizationFiles.begin() + lf );
			break;
		}
	}

	m_LocalizationFiles.push_back( search );

	bool bOk = ReadLocalizationFile( szFileName, pPathID );
	if ( !bOk )
	{
		DevWarning( "ILocalize::AddFile() failed to load file \"%s\".\n", szFileName );
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Purpose: Load all the localized language strings, and uses the longest string from each language
//-----------------------------------------------------------------------------
bool CLocalize::AddAllLanguageFiles( const char *baseFileName )
{
	bool bSuccess = true;

	// Each new language load could potentially change the string value
	// This will suppress callbacks until we're done.
	m_bSuppressChangeCallbacks = true;

	{
		// work out the path the files are in
		char szFilePath[MAX_PATH];
		V_strncpy( szFilePath, baseFileName, sizeof(szFilePath) );
		char *pLastSlash = std::strrchr( szFilePath, '\\' );
		if ( !pLastSlash )
		{
			pLastSlash = std::strrchr( szFilePath, '/' );
		}
		if ( pLastSlash )
		{
			pLastSlash[1] = 0;
		}
		else
		{
			szFilePath[0] = 0;
		}

		// iterate through and add all the languages (for development)
		// the longest string out of all the languages will be used
		char szSearchPath[MAX_PATH];
		V_snprintf( szSearchPath, sizeof(szSearchPath), "%s*.txt", baseFileName );

		FileFindHandle_t hFind = 0;
		const char *file = g_pFullFileSystem->FindFirst( szSearchPath, &hFind );
		while ( file )
		{
			// re-add in the search path
			char szFile[MAX_PATH];
			V_snprintf( szFile, sizeof(szFile), "%s%s", szFilePath, file );

			// add the file
			bSuccess &= AddFile( szFile, nullptr, true );

			// next file
			file = g_pFullFileSystem->FindNext( hFind );
		}
		g_pFullFileSystem->FindClose( hFind );
	}

	m_bSuppressChangeCallbacks = false;
	if ( m_bQueuedChangeCallback )
	{
		m_bQueuedChangeCallback = false;
		InvokeChangeCallbacks();
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: saves the entire contents of the token tree to the file
//-----------------------------------------------------------------------------
bool CLocalize::SaveToFile( const char *szFileName )
{
	// parse out the file
	FileHandle_t file = g_pFullFileSystem->Open(szFileName, "wb");
	if (!file)
		return false;

	// only save the symbols relevant to this file
	const char *fileName = ks::global_symbols().intern( szFileName ).data();

	// write litte-endian unicode marker
	unsigned short marker = 0xFEFF;
	marker = ( marker );
	g_pFullFileSystem->Write(&marker, sizeof( marker ), file);

	const char *startStr = "\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n";
	const char *endStr = "}\r\n}\r\n";

	// write out the first string
	static ucs2 unicodeString[1024];
	int strLength = ConvertANSIToUCS2(startStr, unicodeString, sizeof(unicodeString));
	if (!strLength)
		return false;

	g_pFullFileSystem->Write(unicodeString, std::strlen(startStr) * sizeof(ucs2), file);

	// convert our spacing characters to unicode
//	wchar_t unicodeSpace = L' '; 
	ucs2 unicodeQuote = L'\"'; 
	ucs2 unicodeCR = L'\r'; 
	ucs2 unicodeNewline = L'\n'; 
	ucs2 unicodeTab = L'\t';

	// write out all the key/value pairs
	for (LocalizeStringIndex_t idx = GetFirstStringIndex(); idx != LOCALIZE_INVALID_STRING_INDEX; idx = GetNextStringIndex(idx))
	{
		// only write strings that belong in this file
		if (fileName != m_Lookup[idx].filename)
			continue;

		const char *name = GetNameByIndex(idx);
		wchar_t *value = GetValueByIndex(idx);

		// convert the name to a unicode string
		ConvertANSIToUCS2(name, unicodeString, sizeof(unicodeString));

		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);

		// write out
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);
		g_pFullFileSystem->Write(unicodeString, std::strlen(name) * sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);
		ucs2 ucs2Value[MAX_LOCALIZED_CHARS];
		V_UnicodeToUCS2( value, std::wcslen(value)*sizeof(wchar_t), (char *)ucs2Value, sizeof(ucs2Value) );
		g_pFullFileSystem->Write(ucs2Value, std::wcslen(value) * sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeCR, sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeNewline, sizeof(ucs2), file);
	}

	// write end string
	strLength = ConvertANSIToUCS2(endStr, unicodeString, sizeof(unicodeString));
	g_pFullFileSystem->Write(unicodeString, strLength * sizeof(ucs2), file);

	g_pFullFileSystem->Close(file);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: for development, reloads localization files
//-----------------------------------------------------------------------------
void CLocalize::ReloadLocalizationFiles( )
{
	// re-add all the localization files
	for (int i = 0; i < std::ssize( m_LocalizationFiles ); i++)
	{
		LocalizationFileInfo_t& entry = m_LocalizationFiles[ i ];
		AddFile
		(
			entry.symName.c_str(), 
			entry.symPathID.empty() ? nullptr : entry.symPathID.c_str(),
			entry.bIncludeFallbacks 
		);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
wchar_t *CLocalize::Find(const char *pName)
{	
	LocalizeStringIndex_t idx = FindIndex(pName);
	if (idx == LOCALIZE_INVALID_STRING_INDEX)
		return nullptr;

	return &m_Values[m_Lookup[idx].valueIndex];
}


// Like Find(), but as a failsafe, returns an error message instead of NULL if the string isn't found.  
const wchar_t *CLocalize::FindSafe(const char *pName)
{
	const wchar_t *failsafe = L"#FIXME_LOCALIZATION_FAIL_MISSING_STRING";

	const wchar_t *locstr = Find( pName );

	if ( !locstr )
	{
		DevMsg( "CLocalize::FindSafe failed to localize: %s\n", pName );
		return failsafe;
	}
	else
	{
		return locstr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: finds the index of a token by token name
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::FindIndex(const char *pName)
{
	if (!pName)
		return LOCALIZE_INVALID_STRING_INDEX;

	// strip the pound character (which is used elsewhere to indicate that it's a string that should be translated)
	if (pName[0] == '#')
	{
		pName++;
	}
	
	const LocalizeStringIndex_t *pIndex = ks::find_ptr( m_LookupByName, pName );
	return pIndex ? *pIndex : LOCALIZE_INVALID_STRING_INDEX;
}

void CLocalize::AddString(const char *pString, ucs2 *pUCS2Value, const char *fileName)
{
	if (!pString || !pUCS2Value ) 
		return;
	wchar_t pValue[2048];
	V_UCS2ToUnicode( pUCS2Value, pValue, sizeof(pValue) );

	AddString( pString, pValue, fileName );
}

//-----------------------------------------------------------------------------
// Finds and/or creates a symbol based on the string
//-----------------------------------------------------------------------------
void CLocalize::AddString(const char *pString, wchar_t *pValue, const char *fileName)
{
	if (!pString) 
		return;


	// see if the value is already in our string table
	int valueIndex = FindExistingValueIndex( pValue );
	if ( valueIndex == LOCALIZE_INVALID_STRING_INDEX )
	{
		int len = std::wcslen( pValue ) + 1;
		valueIndex = std::ssize( m_Values ); m_Values.resize( m_Values.size() + len );
		std::memcpy( &m_Values[valueIndex], pValue, len * sizeof(wchar_t) );
	}

	// see if the key is already in the table
	LocalizeStringIndex_t stridx = FindIndex( pString );
	localizedstring_t item;
	item.nameIndex = stridx;

	if ( stridx == LOCALIZE_INVALID_STRING_INDEX )
	{
		// didn't find, insert the string into the vector.
		int len = std::strlen(pString) + 1;
		stridx = std::ssize( m_Names ); m_Names.resize( m_Names.size() + len );
		std::memcpy( &m_Names[stridx], pString, len * sizeof(char) );

		item.nameIndex = stridx;
		item.valueIndex = valueIndex;
		item.filename = fileName ? ks::global_symbols().intern( fileName ).data() : m_CurrentFile;

		m_LookupByName.emplace( pString, (LocalizeStringIndex_t)m_Lookup.size() );
		m_Lookup.push_back( item );
	}
	else
	{
		// it's already in the table
		if ( m_bUseOnlyLongestLanguageString )
		{
			// check which string is longer
			wchar_t *newValue = pValue;
			wchar_t *oldValue = GetValueByIndex( stridx );

			// get the width of the string, using just the first font
			if ( m_pQuery )
			{
				int newWide = m_pQuery->ComputeTextWidth( newValue );
				int oldWide = m_pQuery->ComputeTextWidth( oldValue );
				
				// if the new one is shorter, don't let it be added
				if (newWide < oldWide)
					return;
			}
		}

		// replace the current item
		item.nameIndex = GetNameByIndex( stridx ) - &m_Names[ 0 ];
		item.valueIndex = valueIndex;
		item.filename = fileName ? ks::global_symbols().intern( fileName ).data() : m_CurrentFile;
		m_Lookup[ stridx ] = item;

		InvokeChangeCallbacks();
	}
}

//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------
void CLocalize::RemoveAll()
{
	m_Lookup.clear();
	m_LookupByName.clear();
	m_Names.clear();
	m_Values.clear();
	m_LocalizationFiles.clear();
}

//-----------------------------------------------------------------------------
// Purpose: iteration functions
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::GetFirstStringIndex()
{
	return m_LookupByName.empty() ? LOCALIZE_INVALID_STRING_INDEX : m_LookupByName.begin()->second;
}

//-----------------------------------------------------------------------------
// Purpose: returns the next index, or INVALID_STRING_INDEX if no more strings available
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::GetNextStringIndex(LocalizeStringIndex_t index)
{
	auto [it, itEnd] = m_LookupByName.equal_range( GetNameByIndex( index ) );
	while ( it != itEnd && it->second != index )
		++it;
	if ( it == itEnd || ++it == m_LookupByName.end() )
		return LOCALIZE_INVALID_STRING_INDEX;
	return it->second;
}

//-----------------------------------------------------------------------------
// Purpose: gets the name of the localization string by index
//-----------------------------------------------------------------------------
const char *CLocalize::GetNameByIndex(LocalizeStringIndex_t index)
{
	localizedstring_t &lstr = m_Lookup[index];
	return &m_Names[lstr.nameIndex];
}

//-----------------------------------------------------------------------------
// Purpose: gets the localized string value by index
//-----------------------------------------------------------------------------
wchar_t *CLocalize::GetValueByIndex(LocalizeStringIndex_t index)
{
	if (index == LOCALIZE_INVALID_STRING_INDEX)
		return nullptr;

	localizedstring_t &lstr = m_Lookup[index];
	return &m_Values[lstr.valueIndex];
}


CLocalize *CLocalize::s_pTable = nullptr;

void CLocalize::BuildFastValueLookup()
{
	m_FastValueLookup.clear();
	s_pTable = this;

	// Build it
	for ( const localizedstring_t &lstr : m_Lookup )
	{
		m_FastValueLookup.insert( (int)lstr.valueIndex );
	}
}

void CLocalize::DiscardFastValueLookup()
{
	m_FastValueLookup.clear();
	s_pTable = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLocalize::FindExistingValueIndex( const wchar_t *value )
{
	if ( !s_pTable )
		return (int)LOCALIZE_INVALID_STRING_INDEX;

	auto it = m_FastValueLookup.find( value );
	if ( it != m_FastValueLookup.end() )
	{
		return *it;
	}
	return (int)LOCALIZE_INVALID_STRING_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: returns which file a string was loaded from
//-----------------------------------------------------------------------------
const char *CLocalize::GetFileNameByIndex(LocalizeStringIndex_t index)
{
	localizedstring_t &lstr = m_Lookup[index];
	return ks::nz( lstr.filename );
}

//-----------------------------------------------------------------------------
// Purpose: sets the value in the index
//-----------------------------------------------------------------------------
void CLocalize::SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue)
{
	// get the existing string
	localizedstring_t &lstr = m_Lookup[index];
	wchar_t *wstr = &m_Values[lstr.valueIndex];

	// see if the new string will fit within the old memory
	int newLen = std::wcslen(newValue);
	int oldLen = std::wcslen(wstr);

	if (newLen > oldLen)
	{
		// it won't fit, so allocate new memory - this is wasteful, but only happens in edit mode
		lstr.valueIndex = std::ssize( m_Values ); m_Values.resize( m_Values.size() + ( newLen + 1 ) );
		std::memcpy(&m_Values[lstr.valueIndex], newValue, (newLen + 1) * sizeof(wchar_t));
	}
	else
	{
		// copy the string into the old position
		std::wcscpy(wstr, newValue);		
	}

	InvokeChangeCallbacks();
}

//-----------------------------------------------------------------------------
// Purpose: returns number of localization files currently loaded
//-----------------------------------------------------------------------------
int CLocalize::GetLocalizationFileCount()
{
	return std::ssize( m_LocalizationFiles );
}

//-----------------------------------------------------------------------------
// Purpose: returns localization filename by index
//-----------------------------------------------------------------------------
const char *CLocalize::GetLocalizationFileName(int index)
{
	return m_LocalizationFiles[index].symName.c_str();
}

//-----------------------------------------------------------------------------
// Purpose: returns whether a localization file has been loaded already
//-----------------------------------------------------------------------------
bool CLocalize::LocalizationFileIsLoaded(const char *name)
{
	int c = std::ssize( m_LocalizationFiles );
	for ( int i = 0; i < c; ++i )
	{
		if ( !V_stricmp( m_LocalizationFiles[ i ].symName.c_str(), name ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int CLocalize::ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes)
{
	return V_UTF8ToUnicode(ansi, unicode, unicodeBufferSizeInBytes);
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int CLocalize::ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize)
{
	return V_UnicodeToUTF8(unicode, ansi, ansiBufferSize);
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int CLocalize::ConvertANSIToUCS2(const char *ansi, ucs2 *unicode, int unicodeBufferSizeInBytes)
{
	return V_UTF8ToUCS2(ansi, std::strlen(ansi)*sizeof(char), unicode, unicodeBufferSizeInBytes);
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int CLocalize::ConvertUCS2ToANSI(const ucs2 *unicode, char *ansi, int ansiBufferSize)
{
	return V_UCS2ToUTF8(unicode, ansi, ansiBufferSize);
}



//-----------------------------------------------------------------------------
// Purpose: Constructs a string, inserting variables where necessary
//-----------------------------------------------------------------------------
void CLocalize::ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables)
{
	LocalizeStringIndex_t index = FindIndex(tokenName);

	if (index != LOCALIZE_INVALID_STRING_INDEX)
	{
		ConstructString(unicodeOutput, unicodeBufferSizeInBytes, index, localizationVariables);
	}
	else
	{
		// string not found, just return the token name
		ConvertANSIToUnicode(tokenName, unicodeOutput, unicodeBufferSizeInBytes);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a string, inserting variables where necessary
//-----------------------------------------------------------------------------
void CLocalize::ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables)
{
	if (unicodeBufferSizeInBytes < 1)
		return;

	unicodeOutput[0] = 0;
	const wchar_t *searchPos = GetValueByIndex(unlocalizedTextSymbol);
	if (!searchPos)
	{
		std::wcsncpy(unicodeOutput, L"[unknown string]", unicodeBufferSizeInBytes / sizeof(wchar_t));
		return;
	}

	wchar_t *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(wchar_t);

	while ( *searchPos != '\0' && unicodeBufferSize > 0 )
	{
		bool shouldAdvance = true;

		if ( *searchPos == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( searchPos[1] == 's' && searchPos[2] >= '0' && searchPos[2] <= '9' )
			{
				shouldAdvance = false;

				char variableName[3];
				variableName[0] = searchPos[1];
				variableName[1] = searchPos[2];
				variableName[2] = 0;

				// Handle this as a valid, fixed substitution string
				// look up the variable name
				const wchar_t *value = localizationVariables->GetWString( variableName, L"[unknown]" );

				int paramSize = std::wcslen(value);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = std::max( 0, unicodeBufferSize - 1 );
				}

				std::wcsncpy(outputPos, value, paramSize);

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;
				searchPos += 3;
			}
			else if ( searchPos[1] == '%' )
			{
				// just a '%' char, just write the second one
				searchPos++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const wchar_t *varStart = searchPos + 1;

				// first letter of a valid variable MUST be alphanumeric, otherwise this isn't a variable
				if ( std::iswalnum(*varStart) )
				{
					const wchar_t *varEnd = std::wcschr( varStart, '%' );

					if ( varEnd && *varEnd == '%' )
					{
						shouldAdvance = false;

						// assume variable names must be ascii, do a quick convert
						char variableName[32];
						char *vset = variableName;
						for ( const wchar_t *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
						{
							*vset = (char)*pws;
						}
						*vset = 0;

						// look up the variable name
						const wchar_t *value = localizationVariables->GetWString( variableName, L"[unknown]" );
					
						int paramSize = std::wcslen(value);
						if (paramSize >= unicodeBufferSize)
						{
							paramSize = std::max( 0, unicodeBufferSize - 1 );
						}

						std::wcsncpy(outputPos, value, paramSize);

						unicodeBufferSize -= paramSize;
						outputPos += paramSize;
						searchPos = varEnd + 1;
					}
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}


wchar_t* CLocalize::GetAsianFrequencySequence( const char * pLanguage )
{
	if( !m_bAsianFrequencySequenceLoaded )
	{
		m_bAsianFrequencySequenceLoaded = true;
		char szFileName[128];
		V_snprintf( szFileName, sizeof( szFileName ), "resource/%s_frequency.txt", pLanguage );
		if ( auto file = g_pFullFileSystem->ReadFile( szFileName, "GAME" ) )
			m_AsianFrequencySequence = std::move( *file );
		uint nSize = m_AsianFrequencySequence.size() / sizeof( wchar_t );
		m_AsianFrequencySequence.resize( m_AsianFrequencySequence.size() + sizeof( wchar_t ) ); // 0-terminate

		wchar_t * pAsianFrequencySequence = (wchar_t*) m_AsianFrequencySequence.data();
		// transcode from LT Unicode to GT Unicode
		if( pAsianFrequencySequence[0] == 0xFFFE )
		{
			// switch from little-endian
			for( uint i = 0; i < nSize; ++i )
			{
				wchar_t &refChar = pAsianFrequencySequence[i];
				refChar = ( refChar >> 8 ) | ( refChar << 8 );
			}
		}
	}	

	if( m_AsianFrequencySequence.size() > sizeof( wchar_t ) )
	{
		wchar_t * pAsianFrequencySequence = (wchar_t*) m_AsianFrequencySequence.data();

		if( pAsianFrequencySequence[0] == 0xFEFF )
		{
			return pAsianFrequencySequence + 1;
		}
		return pAsianFrequencySequence;
	}
	return nullptr;
}


#define _INTSIZEOF(n)   ((sizeof(n) + sizeof(intp) - 1) & ~(sizeof(intp) - 1)) 

#define va_argByIndex(ap,t,i)    ( *(t *)(ap + i * _INTSIZEOF(t)) )

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
void ConstructStringVArgsInternal_Impl(T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, std::va_list argList)
{
	ks::ScratchArena<> scratch;
	// Safety check
	if ( unicodeOutput == nullptr || unicodeBufferSizeInBytes < 1 )
	{
		return;
	}
	if (!formatString)
	{
		unicodeOutput[0] = 0;
		return;
	}

	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);
	const T *searchPos = formatString;
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int formatLength = StringFuncs<T>::Length( formatString );

	// On 64 bits, va_list does not just point to a contiguous blob of parameters
	// so extract into an array here.
	T** arguments = scratch.Alloc<T*>( numFormatParameters );
	for ( int i = 0; i < numFormatParameters; ++i )
	{
		arguments[i] = __builtin_va_arg( argList, T* );
	}

#ifdef _DEBUG
	int curArgIdx = 0;
#endif

	while ( searchPos[0] != '\0' && unicodeBufferSize > 1 )
	{
		if ( formatLength >= 3 && searchPos[0] == '%' && searchPos[1] == 's' )
		{
			//this is an escape sequence - %s1, %s2 etc, up to %s9

			int argindex = ( searchPos[2] ) - '0' - 1;

			if ( argindex < 0 || argindex > 9 )
			{
				Warning( "Bad format string in CLocalizeStringTable::ConstructString\n" );
				*outputPos = '\0';
				return;
			}

			if ( argindex < numFormatParameters )
			{
				T *param = nullptr;
				param = arguments[ argindex ];

				if (!param)
				{
					contract_assert( !("ConstructStringVArgsInternal_Impl() - Found a %s# escape sequence whose index was more than the number of args.") );
					*outputPos = '\0';
					return;
				}


				int paramSize = StringFuncs<T>::Length(param);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = unicodeBufferSize - 1;
				}

				std::memcpy(outputPos, param, paramSize * sizeof(T));

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;

				searchPos += 3;
				formatLength -= 3;
			}
			else
			{
				//copy it over, char by char
				*outputPos = *searchPos;

				outputPos++;
				unicodeBufferSize--;

				searchPos++;
				formatLength--;
			}
		}
		else
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
			formatLength--;
		}
	}

	// ensure null termination
	contract_assert( outputPos - unicodeOutput < unicodeBufferSizeInBytes/sizeof(T) );
	*outputPos = L'\0';
}

void CLocalize::ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, std::va_list argList)
{
	ConstructStringVArgsInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

void CLocalize::ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, std::va_list argList)
{
	ConstructStringVArgsInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
const T *GetTypedKeyValuesString( KeyValues *pKeyValues, const char *pKeyName );

template < >
const char *GetTypedKeyValuesString<char>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetString( pKeyName, "[unknown]" );
}

template < >
const wchar_t *GetTypedKeyValuesString<wchar_t>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetWString( pKeyName, L"[unknown]" );
}

template < typename T >
void ConstructStringKeyValuesInternal_Impl( T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables )
{
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);

	while ( *formatString != '\0' && unicodeBufferSize > 0 )
	{
		bool shouldAdvance = true;

		if ( *formatString == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( formatString[1] == 's' && formatString[2] >= '0' && formatString[2] <= '9' )
			{
				// old style escape sequence, ignore
			}
			else if ( formatString[1] == '%' )
			{
				// just a '%' char, just write the second one
				formatString++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const T *varStart = formatString + 1;
				const T *varEnd = StringFuncs<T>::FindChar( varStart, '%' );

				if ( varEnd && *varEnd == '%' )
				{
					shouldAdvance = false;

					// assume variable names must be ascii, do a quick convert
					char variableName[32];
					char *vset = variableName;
					for ( const T *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
					{
						*vset = (char)*pws;
					}
					*vset = 0;

					// look up the variable name
					const T *value = GetTypedKeyValuesString<T>( localizationVariables, variableName );

					int paramSize = StringFuncs<T>::Length( value );
					if (paramSize >= unicodeBufferSize)
					{
						paramSize = std::max( 0, unicodeBufferSize - 1 );
					}

					StringFuncs<T>::Copy( outputPos, value, paramSize );

					unicodeBufferSize -= paramSize;
					outputPos += paramSize;
					formatString = varEnd + 1;
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *formatString;

			outputPos++;
			unicodeBufferSize--;

			formatString++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}

void CLocalize::ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}

void CLocalize::ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}
