import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IDemoActionManager class to CDemoActionManager. Included by cl_demoactionmanager.cpp.

void IDemoActionManager::Init( void )
{ static_cast<CDemoActionManager*>(this)->Init(); }
void IDemoActionManager::Shutdown( void )
{ static_cast<CDemoActionManager*>(this)->Shutdown(); }
void IDemoActionManager::StartPlaying( char const *demfilename )
{ static_cast<CDemoActionManager*>(this)->StartPlaying( demfilename ); }
void IDemoActionManager::StopPlaying()
{ static_cast<CDemoActionManager*>(this)->StopPlaying(); }
void IDemoActionManager::Update( bool newframe, int demoframe, float demotime )
{ static_cast<CDemoActionManager*>(this)->Update( newframe, demoframe, demotime ); }
void IDemoActionManager::SaveToBuffer( ks::text_writer& buf )
{ static_cast<CDemoActionManager*>(this)->SaveToBuffer( buf ); }
void IDemoActionManager::SaveToFile( void )
{ static_cast<CDemoActionManager*>(this)->SaveToFile(); }
char const * IDemoActionManager::GetCurrentDemoFile( void )
{ return static_cast<CDemoActionManager*>(this)->GetCurrentDemoFile(); }
int IDemoActionManager::GetActionCount( void )
{ return static_cast<CDemoActionManager*>(this)->GetActionCount(); }
CBaseDemoAction * IDemoActionManager::GetAction( int index )
{ return static_cast<CDemoActionManager*>(this)->GetAction( index ); }
void IDemoActionManager::AddAction( CBaseDemoAction *action )
{ static_cast<CDemoActionManager*>(this)->AddAction( action ); }
void IDemoActionManager::RemoveAction( CBaseDemoAction *action )
{ static_cast<CDemoActionManager*>(this)->RemoveAction( action ); }
bool IDemoActionManager::IsDirty( void ) const
{ return static_cast<const CDemoActionManager*>(this)->IsDirty(); }
void IDemoActionManager::SetDirty( bool dirty )
{ static_cast<CDemoActionManager*>(this)->SetDirty( dirty ); }
void IDemoActionManager::ReloadFromDisk( void )
{ static_cast<CDemoActionManager*>(this)->ReloadFromDisk(); }
void IDemoActionManager::DispatchEvents()
{ static_cast<CDemoActionManager*>(this)->DispatchEvents(); }
void IDemoActionManager::InsertFireEvent( CBaseDemoAction *action )
{ static_cast<CDemoActionManager*>(this)->InsertFireEvent( action ); }
bool IDemoActionManager::OverrideView( democmdinfo_t& info, int tick )
{ return static_cast<CDemoActionManager*>(this)->OverrideView( info, tick ); }

