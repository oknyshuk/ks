// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IScriptVM class to its one implementation, CSquirrelVM. Included by vsquirrel.cpp.

bool IScriptVM::Init()
{ return static_cast<CSquirrelVM*>(this)->Init(); }
void IScriptVM::Shutdown()
{ static_cast<CSquirrelVM*>(this)->Shutdown(); }
bool IScriptVM::ConnectDebugger()
{ return static_cast<CSquirrelVM*>(this)->ConnectDebugger(); }
void IScriptVM::DisconnectDebugger()
{ static_cast<CSquirrelVM*>(this)->DisconnectDebugger(); }
ScriptLanguage_t IScriptVM::GetLanguage()
{ return static_cast<CSquirrelVM*>(this)->GetLanguage(); }
const char * IScriptVM::GetLanguageName()
{ return static_cast<CSquirrelVM*>(this)->GetLanguageName(); }
void IScriptVM::AddSearchPath( const char *pszSearchPath )
{ static_cast<CSquirrelVM*>(this)->AddSearchPath( pszSearchPath ); }
bool IScriptVM::Frame( float simTime )
{ return static_cast<CSquirrelVM*>(this)->Frame( simTime ); }
ScriptStatus_t IScriptVM::Run( const char *pszScript, bool bWait )
{ return static_cast<CSquirrelVM*>(this)->Run( pszScript, bWait ); }
HSCRIPT IScriptVM::CompileScript( const char *pszScript, const char *pszId )
{ return static_cast<CSquirrelVM*>(this)->CompileScript( pszScript, pszId ); }
void IScriptVM::ReleaseScript( HSCRIPT hScript )
{ static_cast<CSquirrelVM*>(this)->ReleaseScript( hScript ); }
ScriptStatus_t IScriptVM::Run( HSCRIPT hScript, HSCRIPT hScope, bool bWait )
{ return static_cast<CSquirrelVM*>(this)->Run( hScript, hScope, bWait ); }
ScriptStatus_t IScriptVM::Run( HSCRIPT hScript, bool bWait )
{ return static_cast<CSquirrelVM*>(this)->Run( hScript, bWait ); }
HSCRIPT IScriptVM::CreateScope( const char *pszScope, HSCRIPT hParent )
{ return static_cast<CSquirrelVM*>(this)->CreateScope( pszScope, hParent ); }
void IScriptVM::ReleaseScope( HSCRIPT hScript )
{ static_cast<CSquirrelVM*>(this)->ReleaseScope( hScript ); }
HSCRIPT IScriptVM::LookupFunction( const char *pszFunction, HSCRIPT hScope )
{ return static_cast<CSquirrelVM*>(this)->LookupFunction( pszFunction, hScope ); }
void IScriptVM::ReleaseFunction( HSCRIPT hScript )
{ static_cast<CSquirrelVM*>(this)->ReleaseFunction( hScript ); }
ScriptStatus_t IScriptVM::ExecuteFunction( HSCRIPT hFunction, ScriptVariant_t *pArgs, int nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope, bool bWait )
{ return static_cast<CSquirrelVM*>(this)->ExecuteFunction( hFunction, pArgs, nArgs, pReturn, hScope, bWait ); }
void IScriptVM::RegisterFunction( ScriptFunctionBinding_t *pScriptFunction )
{ static_cast<CSquirrelVM*>(this)->RegisterFunction( pScriptFunction ); }
bool IScriptVM::RegisterClass( ScriptClassDesc_t *pClassDesc )
{ return static_cast<CSquirrelVM*>(this)->RegisterClass( pClassDesc ); }
HSCRIPT IScriptVM::RegisterInstance( ScriptClassDesc_t *pDesc, void *pInstance )
{ return static_cast<CSquirrelVM*>(this)->RegisterInstance( pDesc, pInstance ); }
void IScriptVM::SetInstanceUniqeId( HSCRIPT hInstance, const char *pszId )
{ static_cast<CSquirrelVM*>(this)->SetInstanceUniqeId( hInstance, pszId ); }
void IScriptVM::RemoveInstance( HSCRIPT hInstance )
{ static_cast<CSquirrelVM*>(this)->RemoveInstance( hInstance ); }
void * IScriptVM::GetInstanceValue( HSCRIPT hInstance, ScriptClassDesc_t *pExpectedType )
{ return static_cast<CSquirrelVM*>(this)->GetInstanceValue( hInstance, pExpectedType ); }
bool IScriptVM::GenerateUniqueKey( const char *pszRoot, char *pBuf, int nBufSize )
{ return static_cast<CSquirrelVM*>(this)->GenerateUniqueKey( pszRoot, pBuf, nBufSize ); }
bool IScriptVM::ValueExists( HSCRIPT hScope, const char *pszKey )
{ return static_cast<CSquirrelVM*>(this)->ValueExists( hScope, pszKey ); }
bool IScriptVM::SetValue( HSCRIPT hScope, const char *pszKey, const char *pszValue )
{ return static_cast<CSquirrelVM*>(this)->SetValue( hScope, pszKey, pszValue ); }
bool IScriptVM::SetValue( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value )
{ return static_cast<CSquirrelVM*>(this)->SetValue( hScope, pszKey, value ); }
void IScriptVM::CreateTable( ScriptVariant_t &Table )
{ static_cast<CSquirrelVM*>(this)->CreateTable( Table ); }
int IScriptVM::GetNumTableEntries( HSCRIPT hScope )
{ return static_cast<CSquirrelVM*>(this)->GetNumTableEntries( hScope ); }
int IScriptVM::GetKeyValue( HSCRIPT hScope, int nIterator, ScriptVariant_t *pKey, ScriptVariant_t *pValue )
{ return static_cast<CSquirrelVM*>(this)->GetKeyValue( hScope, nIterator, pKey, pValue ); }
bool IScriptVM::GetValue( HSCRIPT hScope, const char *pszKey, ScriptVariant_t *pValue )
{ return static_cast<CSquirrelVM*>(this)->GetValue( hScope, pszKey, pValue ); }
void IScriptVM::ReleaseValue( ScriptVariant_t &value )
{ static_cast<CSquirrelVM*>(this)->ReleaseValue( value ); }
bool IScriptVM::ClearValue( HSCRIPT hScope, const char *pszKey )
{ return static_cast<CSquirrelVM*>(this)->ClearValue( hScope, pszKey ); }
void IScriptVM::WriteState( ks::byte_writer *pBuffer )
{ static_cast<CSquirrelVM*>(this)->WriteState( pBuffer ); }
void IScriptVM::ReadState( ks::byte_reader *pBuffer )
{ static_cast<CSquirrelVM*>(this)->ReadState( pBuffer ); }
void IScriptVM::RemoveOrphanInstances()
{ static_cast<CSquirrelVM*>(this)->RemoveOrphanInstances(); }
void IScriptVM::DumpState()
{ static_cast<CSquirrelVM*>(this)->DumpState(); }
void IScriptVM::SetOutputCallback( ScriptOutputFunc_t pFunc )
{ static_cast<CSquirrelVM*>(this)->SetOutputCallback( pFunc ); }
void IScriptVM::SetErrorCallback( ScriptErrorFunc_t pFunc )
{ static_cast<CSquirrelVM*>(this)->SetErrorCallback( pFunc ); }
bool IScriptVM::RaiseException( const char *pszExceptionText )
{ return static_cast<CSquirrelVM*>(this)->RaiseException( pszExceptionText ); }
