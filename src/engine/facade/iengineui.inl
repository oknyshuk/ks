import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IEngineUI class to CEngineUI. Included by engineui.cpp.

bool IEngineUI::IsGameUIVisible()
{ return static_cast<CEngineUI*>(this)->IsGameUIVisible(); }
void IEngineUI::ActivateGameUI()
{ static_cast<CEngineUI*>(this)->ActivateGameUI(); }

void IEngineUIInternal::Init()
{ static_cast<CEngineUI*>(this)->Init(); }
void IEngineUIInternal::Connect()
{ static_cast<CEngineUI*>(this)->Connect(); }
void IEngineUIInternal::Shutdown()
{ static_cast<CEngineUI*>(this)->Shutdown(); }
bool IEngineUIInternal::SetVGUIDirectories()
{ return static_cast<CEngineUI*>(this)->SetVGUIDirectories(); }
bool IEngineUIInternal::IsInitialized() const
{ return static_cast<const CEngineUI*>(this)->IsInitialized(); }
CreateInterfaceFn IEngineUIInternal::GetGameUIFactory()
{ return static_cast<CEngineUI*>(this)->GetGameUIFactory(); }
bool IEngineUIInternal::Key_Event( const InputEvent_t &event )
{ return static_cast<CEngineUI*>(this)->Key_Event( event ); }
void IEngineUIInternal::UpdateButtonState( const InputEvent_t &event )
{ static_cast<CEngineUI*>(this)->UpdateButtonState( event ); }
void IEngineUIInternal::PostInit()
{ static_cast<CEngineUI*>(this)->PostInit(); }
void IEngineUIInternal::ActivateGameUI()
{ static_cast<CEngineUI*>(this)->ActivateGameUI(); }
bool IEngineUIInternal::HideGameUI()
{ return static_cast<CEngineUI*>(this)->HideGameUI(); }
bool IEngineUIInternal::IsGameUIVisible()
{ return static_cast<CEngineUI*>(this)->IsGameUIVisible(); }
void IEngineUIInternal::ShowConsole()
{ static_cast<CEngineUI*>(this)->ShowConsole(); }
void IEngineUIInternal::HideConsole()
{ static_cast<CEngineUI*>(this)->HideConsole(); }
bool IEngineUIInternal::IsConsoleVisible()
{ return static_cast<CEngineUI*>(this)->IsConsoleVisible(); }
void IEngineUIInternal::ClearConsole()
{ static_cast<CEngineUI*>(this)->ClearConsole(); }
void IEngineUIInternal::HideDebugSystem()
{ static_cast<CEngineUI*>(this)->HideDebugSystem(); }
void IEngineUIInternal::OnLevelLoadingStarted( const char *levelName, bool bLocalServer )
{ static_cast<CEngineUI*>(this)->OnLevelLoadingStarted( levelName, bLocalServer ); }
void IEngineUIInternal::OnLevelLoadingFinished()
{ static_cast<CEngineUI*>(this)->OnLevelLoadingFinished(); }
void IEngineUIInternal::NotifyOfServerConnect( const char *game, int IP, int connectionPort )
{ static_cast<CEngineUI*>(this)->NotifyOfServerConnect( game, IP, connectionPort ); }
void IEngineUIInternal::NotifyOfServerDisconnect()
{ static_cast<CEngineUI*>(this)->NotifyOfServerDisconnect(); }
void IEngineUIInternal::EnabledProgressBarForNextLoad()
{ static_cast<CEngineUI*>(this)->EnabledProgressBarForNextLoad(); }
void IEngineUIInternal::UpdateProgressBar( LevelLoadingProgress_e progress, bool showDialog )
{ static_cast<CEngineUI*>(this)->UpdateProgressBar( progress, showDialog ); }
void IEngineUIInternal::UpdateCustomProgressBar( float progress, const wchar_t *desc )
{ static_cast<CEngineUI*>(this)->UpdateCustomProgressBar( progress, desc ); }
void IEngineUIInternal::StartCustomProgress()
{ static_cast<CEngineUI*>(this)->StartCustomProgress(); }
void IEngineUIInternal::FinishCustomProgress()
{ static_cast<CEngineUI*>(this)->FinishCustomProgress(); }
void IEngineUIInternal::UpdateSecondaryProgressBarWithFile( float progress, const char *pDesc, int nBytesTotal )
{ static_cast<CEngineUI*>(this)->UpdateSecondaryProgressBarWithFile( progress, pDesc, nBytesTotal ); }
void IEngineUIInternal::UpdateSecondaryProgressBar( float progress, const wchar_t *desc )
{ static_cast<CEngineUI*>(this)->UpdateSecondaryProgressBar( progress, desc ); }
void IEngineUIInternal::ShowErrorMessage()
{ static_cast<CEngineUI*>(this)->ShowErrorMessage(); }
void IEngineUIInternal::HideLoadingPlaque()
{ static_cast<CEngineUI*>(this)->HideLoadingPlaque(); }
void IEngineUIInternal::StartLoadingScreenForCommand( const char* command )
{ static_cast<CEngineUI*>(this)->StartLoadingScreenForCommand( command ); }
void IEngineUIInternal::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{ static_cast<CEngineUI*>(this)->StartLoadingScreenForKeyValues( keyValues ); }
bool IEngineUIInternal::ShouldPause()
{ return static_cast<CEngineUI*>(this)->ShouldPause(); }
void IEngineUIInternal::SetGameDLLPanelsVisible( bool show )
{ static_cast<CEngineUI*>(this)->SetGameDLLPanelsVisible( show ); }
void IEngineUIInternal::SetProgressLevelName( const char *levelName )
{ static_cast<CEngineUI*>(this)->SetProgressLevelName( levelName ); }
void IEngineUIInternal::Simulate()
{ static_cast<CEngineUI*>(this)->Simulate(); }
void IEngineUIInternal::SetNotAllowedToHideGameUI( bool bNotAllowedToHide )
{ static_cast<CEngineUI*>(this)->SetNotAllowedToHideGameUI( bNotAllowedToHide ); }
void IEngineUIInternal::SetNotAllowedToShowGameUI( bool bNotAllowedToShow )
{ static_cast<CEngineUI*>(this)->SetNotAllowedToShowGameUI( bNotAllowedToShow ); }
void IEngineUIInternal::NeedConnectionProblemWaitScreen()
{ static_cast<CEngineUI*>(this)->NeedConnectionProblemWaitScreen(); }
void IEngineUIInternal::ShowPasswordUI( char const *pchCurrentPW )
{ static_cast<CEngineUI*>(this)->ShowPasswordUI( pchCurrentPW ); }
void IEngineUIInternal::OnToolModeChanged( bool bGameMode )
{ static_cast<CEngineUI*>(this)->OnToolModeChanged( bGameMode ); }
InputContextHandle_t IEngineUIInternal::GetGameUIInputContext()
{ return static_cast<CEngineUI*>(this)->GetGameUIInputContext(); }
bool IEngineUIInternal::IsPlayingFullScreenVideo()
{ return static_cast<CEngineUI*>(this)->IsPlayingFullScreenVideo(); }

