#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
import std;


//-----------------------------------------------------------------------------
// Purpose: Handles localization of text
//			looks up string names and returns the localized unicode text
//-----------------------------------------------------------------------------
// direct references to localized strings
typedef uint32 LocalizeStringIndex_t;
const uint32 LOCALIZE_INVALID_STRING_INDEX = (LocalizeStringIndex_t)-1;

class ILocalize
{
public:
	//--------------------------------------------------------
	// App system protocol. Not an IAppSystem: CAppSystemGroup drives it by kind (see AppSystemRef).
	//--------------------------------------------------------
	bool Connect( CreateInterfaceFn factory );
	void Disconnect();
	void *QueryInterface( const char *pInterfaceName );
	InitReturnVal_t Init();
	void Shutdown();
	const AppSystemInfo_t *GetDependencies() { return nullptr; }
	void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { ReconnectInterface( factory, pInterfaceName ); }

	// adds the contents of a file to the localization table
	bool AddFile( const char *fileName, const char *pPathID = nullptr, bool bIncludeFallbackSearchPaths = false );

	// Remove all strings from the table
	void RemoveAll();

	// Finds the localized text for tokenName. Returns NULL if none is found.
	wchar_t *Find(const char *tokenName);

	// Like Find(), but as a failsafe, returns an error message instead of NULL if the string isn't found.  
	const wchar_t *FindSafe(const char *tokenName);

	// converts an english string to unicode
	// returns the number of wchar_t in resulting string, including null terminator
	int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes);

	// converts an unicode string to an english string
	// unrepresentable characters are converted to system default
	// returns the number of characters in resulting string, including null terminator
	int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize);

	// finds the index of a token by token name, INVALID_STRING_INDEX if not found
	LocalizeStringIndex_t FindIndex(const char *tokenName);

	// gets the values by the string index
	const char *GetNameByIndex(LocalizeStringIndex_t index);
	wchar_t *GetValueByIndex(LocalizeStringIndex_t index);

	///////////////////////////////////////////////////////////////////
	// the following functions should only be used by localization editors

	// iteration functions
	LocalizeStringIndex_t GetFirstStringIndex();
	// returns the next index, or INVALID_STRING_INDEX if no more strings available
	LocalizeStringIndex_t GetNextStringIndex(LocalizeStringIndex_t index);

	// adds a single name/unicode string pair to the table
	void AddString( const char *tokenName, wchar_t *unicodeString, const char *fileName );

	// changes the value of a string
	void SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue);

	// saves the entire contents of the token tree to the file
	bool SaveToFile( const char *fileName );

	// iterates the filenames
	int GetLocalizationFileCount();
	const char *GetLocalizationFileName(int index);

	// returns the name of the file the specified localized string is stored in
	const char *GetFileNameByIndex(LocalizeStringIndex_t index);

	// for development only, reloads localization files
	void ReloadLocalizationFiles( );

	// need to replace the existing ConstructString with this
	void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables);
	void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables);

	wchar_t* GetAsianFrequencySequence( const char * pLanguage );

	const char *FindAsUTF8( const char *pchTokenName );

	// builds a localized formatted string
	// uses the format strings first: %s1, %s2, ...  unicode strings (wchar_t *)
	template < typename T >
	void ConstructString(T *unicodeOuput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, ...)
	{
		std::va_list argList;
		__builtin_va_start( argList, numFormatParameters );

		ConstructStringVArgsInternal( unicodeOuput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );

		__builtin_va_end( argList );
	}

	template < typename T >
	void ConstructStringVArgs(T *unicodeOuput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, std::va_list argList)
	{
		ConstructStringVArgsInternal( unicodeOuput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
	}

	template < typename T >
	void ConstructString(T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables)
	{
		ConstructStringKeyValuesInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
	}

protected:
	// internal "interface" (forwarded to the implementation, which makes these public)
	void ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, std::va_list argList);
	void ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, std::va_list argList);

	void ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables);
	void ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables);
};
