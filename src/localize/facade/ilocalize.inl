// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ILocalize class to CLocalize. Included by localize.cpp.

bool ILocalize::AddFile( const char *fileName, const char *pPathID, bool bIncludeFallbackSearchPaths )
{ return static_cast<CLocalize*>(this)->AddFile( fileName, pPathID, bIncludeFallbackSearchPaths ); }
void ILocalize::RemoveAll()
{ static_cast<CLocalize*>(this)->RemoveAll(); }
wchar_t * ILocalize::Find( const char *tokenName )
{ return static_cast<CLocalize*>(this)->Find( tokenName ); }
const wchar_t * ILocalize::FindSafe( const char *tokenName )
{ return static_cast<CLocalize*>(this)->FindSafe( tokenName ); }
int ILocalize::ConvertANSIToUnicode( const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes )
{ return static_cast<CLocalize*>(this)->ConvertANSIToUnicode( ansi, unicode, unicodeBufferSizeInBytes ); }
int ILocalize::ConvertUnicodeToANSI( const wchar_t *unicode, char *ansi, int ansiBufferSize )
{ return static_cast<CLocalize*>(this)->ConvertUnicodeToANSI( unicode, ansi, ansiBufferSize ); }
LocalizeStringIndex_t ILocalize::FindIndex( const char *tokenName )
{ return static_cast<CLocalize*>(this)->FindIndex( tokenName ); }
const char * ILocalize::GetNameByIndex( LocalizeStringIndex_t index )
{ return static_cast<CLocalize*>(this)->GetNameByIndex( index ); }
wchar_t * ILocalize::GetValueByIndex( LocalizeStringIndex_t index )
{ return static_cast<CLocalize*>(this)->GetValueByIndex( index ); }
LocalizeStringIndex_t ILocalize::GetFirstStringIndex()
{ return static_cast<CLocalize*>(this)->GetFirstStringIndex(); }
LocalizeStringIndex_t ILocalize::GetNextStringIndex( LocalizeStringIndex_t index )
{ return static_cast<CLocalize*>(this)->GetNextStringIndex( index ); }
void ILocalize::AddString( const char *tokenName, wchar_t *unicodeString, const char *fileName )
{ static_cast<CLocalize*>(this)->AddString( tokenName, unicodeString, fileName ); }
void ILocalize::SetValueByIndex( LocalizeStringIndex_t index, wchar_t *newValue )
{ static_cast<CLocalize*>(this)->SetValueByIndex( index, newValue ); }
bool ILocalize::SaveToFile( const char *fileName )
{ return static_cast<CLocalize*>(this)->SaveToFile( fileName ); }
int ILocalize::GetLocalizationFileCount()
{ return static_cast<CLocalize*>(this)->GetLocalizationFileCount(); }
const char * ILocalize::GetLocalizationFileName( int index )
{ return static_cast<CLocalize*>(this)->GetLocalizationFileName( index ); }
const char * ILocalize::GetFileNameByIndex( LocalizeStringIndex_t index )
{ return static_cast<CLocalize*>(this)->GetFileNameByIndex( index ); }
void ILocalize::ReloadLocalizationFiles()
{ static_cast<CLocalize*>(this)->ReloadLocalizationFiles(); }
void ILocalize::ConstructString( wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables )
{ static_cast<CLocalize*>(this)->ConstructString( unicodeOutput, unicodeBufferSizeInBytes, tokenName, localizationVariables ); }
void ILocalize::ConstructString( wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables )
{ static_cast<CLocalize*>(this)->ConstructString( unicodeOutput, unicodeBufferSizeInBytes, unlocalizedTextSymbol, localizationVariables ); }
wchar_t* ILocalize::GetAsianFrequencySequence( const char * pLanguage )
{ return static_cast<CLocalize*>(this)->GetAsianFrequencySequence( pLanguage ); }
const char * ILocalize::FindAsUTF8( const char *pchTokenName )
{ return static_cast<CLocalize*>(this)->FindAsUTF8( pchTokenName ); }
void ILocalize::ConstructStringVArgsInternal( char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, std::va_list argList )
{ static_cast<CLocalize*>(this)->ConstructStringVArgsInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList ); }
void ILocalize::ConstructStringVArgsInternal( wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, std::va_list argList )
{ static_cast<CLocalize*>(this)->ConstructStringVArgsInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList ); }
void ILocalize::ConstructStringKeyValuesInternal( char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables )
{ static_cast<CLocalize*>(this)->ConstructStringKeyValuesInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables ); }
void ILocalize::ConstructStringKeyValuesInternal( wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables )
{ static_cast<CLocalize*>(this)->ConstructStringKeyValuesInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables ); }
bool ILocalize::Connect( CreateInterfaceFn factory )
{ return static_cast<CLocalize*>(this)->Connect( factory ); }
void ILocalize::Disconnect()
{ static_cast<CLocalize*>(this)->Disconnect(); }
void *ILocalize::QueryInterface( const char *pInterfaceName )
{ return static_cast<CLocalize*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t ILocalize::Init()
{ return static_cast<CLocalize*>(this)->Init(); }
void ILocalize::Shutdown()
{ static_cast<CLocalize*>(this)->Shutdown(); }

