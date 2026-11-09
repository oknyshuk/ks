// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IInputSystem class to CInputSystem. Included by inputsystem.cpp.

void IInputSystem::AttachToWindow( void* hWnd )
{ static_cast<CInputSystem*>(this)->AttachToWindow( hWnd ); }
void IInputSystem::DetachFromWindow()
{ static_cast<CInputSystem*>(this)->DetachFromWindow(); }
void IInputSystem::EnableInput( bool bEnable )
{ static_cast<CInputSystem*>(this)->EnableInput( bEnable ); }
void IInputSystem::EnableMessagePump( bool bEnable )
{ static_cast<CInputSystem*>(this)->EnableMessagePump( bEnable ); }
void IInputSystem::PollInputState( bool bIsInGame )
{ static_cast<CInputSystem*>(this)->PollInputState( bIsInGame ); }
int IInputSystem::GetPollTick() const
{ return static_cast<const CInputSystem*>(this)->GetPollTick(); }
bool IInputSystem::IsButtonDown( ButtonCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->IsButtonDown( code ); }
int IInputSystem::GetButtonPressedTick( ButtonCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->GetButtonPressedTick( code ); }
int IInputSystem::GetButtonReleasedTick( ButtonCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->GetButtonReleasedTick( code ); }
int IInputSystem::GetAnalogValue( AnalogCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->GetAnalogValue( code ); }
int IInputSystem::GetAnalogDelta( AnalogCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->GetAnalogDelta( code ); }
int IInputSystem::GetEventCount() const
{ return static_cast<const CInputSystem*>(this)->GetEventCount(); }
const InputEvent_t* IInputSystem::GetEventData() const
{ return static_cast<const CInputSystem*>(this)->GetEventData(); }
bool IInputSystem::MotionControllerActive() const
{ return static_cast<const CInputSystem*>(this)->MotionControllerActive(); }
Quaternion IInputSystem::GetMotionControllerOrientation() const
{ return static_cast<const CInputSystem*>(this)->GetMotionControllerOrientation(); }
float IInputSystem::GetMotionControllerPosX() const
{ return static_cast<const CInputSystem*>(this)->GetMotionControllerPosX(); }
float IInputSystem::GetMotionControllerPosY() const
{ return static_cast<const CInputSystem*>(this)->GetMotionControllerPosY(); }
int IInputSystem::GetMotionControllerDeviceStatus() const
{ return static_cast<const CInputSystem*>(this)->GetMotionControllerDeviceStatus(); }
uint64 IInputSystem::GetMotionControllerDeviceStatusFlags() const
{ return static_cast<const CInputSystem*>(this)->GetMotionControllerDeviceStatusFlags(); }
void IInputSystem::SetMotionControllerDeviceStatus( int nStatus )
{ static_cast<CInputSystem*>(this)->SetMotionControllerDeviceStatus( nStatus ); }
void IInputSystem::SetMotionControllerCalibrationInvalid( void )
{ static_cast<CInputSystem*>(this)->SetMotionControllerCalibrationInvalid(); }
void IInputSystem::StepMotionControllerCalibration( void )
{ static_cast<CInputSystem*>(this)->StepMotionControllerCalibration(); }
void IInputSystem::ResetMotionControllerScreenCalibration( void )
{ static_cast<CInputSystem*>(this)->ResetMotionControllerScreenCalibration(); }
void IInputSystem::PostUserEvent( const InputEvent_t &event )
{ static_cast<CInputSystem*>(this)->PostUserEvent( event ); }
int IInputSystem::GetJoystickCount() const
{ return static_cast<const CInputSystem*>(this)->GetJoystickCount(); }
void IInputSystem::EnableJoystickInput( int nJoystick, bool bEnable )
{ static_cast<CInputSystem*>(this)->EnableJoystickInput( nJoystick, bEnable ); }
void IInputSystem::EnableJoystickDiagonalPOV( int nJoystick, bool bEnable )
{ static_cast<CInputSystem*>(this)->EnableJoystickDiagonalPOV( nJoystick, bEnable ); }
void IInputSystem::SampleDevices( void )
{ static_cast<CInputSystem*>(this)->SampleDevices(); }
void IInputSystem::SetRumble( float fLeftMotor, float fRightMotor, int userId )
{ static_cast<CInputSystem*>(this)->SetRumble( fLeftMotor, fRightMotor, userId ); }
void IInputSystem::StopRumble( int userId )
{ static_cast<CInputSystem*>(this)->StopRumble( userId ); }
void IInputSystem::ResetInputState()
{ static_cast<CInputSystem*>(this)->ResetInputState(); }
const char * IInputSystem::ButtonCodeToString( ButtonCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->ButtonCodeToString( code ); }
const char * IInputSystem::AnalogCodeToString( AnalogCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->AnalogCodeToString( code ); }
ButtonCode_t IInputSystem::StringToButtonCode( const char *pString ) const
{ return static_cast<const CInputSystem*>(this)->StringToButtonCode( pString ); }
AnalogCode_t IInputSystem::StringToAnalogCode( const char *pString ) const
{ return static_cast<const CInputSystem*>(this)->StringToAnalogCode( pString ); }
void IInputSystem::SleepUntilInput( int nMaxSleepTimeMS )
{ static_cast<CInputSystem*>(this)->SleepUntilInput( nMaxSleepTimeMS ); }
ButtonCode_t IInputSystem::VirtualKeyToButtonCode( int nVirtualKey ) const
{ return static_cast<const CInputSystem*>(this)->VirtualKeyToButtonCode( nVirtualKey ); }
int IInputSystem::ButtonCodeToVirtualKey( ButtonCode_t code ) const
{ return static_cast<const CInputSystem*>(this)->ButtonCodeToVirtualKey( code ); }
ButtonCode_t IInputSystem::ScanCodeToButtonCode( int lParam ) const
{ return static_cast<const CInputSystem*>(this)->ScanCodeToButtonCode( lParam ); }
int IInputSystem::GetPollCount() const
{ return static_cast<const CInputSystem*>(this)->GetPollCount(); }
void IInputSystem::SetCursorPosition( int x, int y )
{ static_cast<CInputSystem*>(this)->SetCursorPosition( x, y ); }
void IInputSystem::AddUIEventListener()
{ static_cast<CInputSystem*>(this)->AddUIEventListener(); }
void IInputSystem::RemoveUIEventListener()
{ static_cast<CInputSystem*>(this)->RemoveUIEventListener(); }
PlatWindow_t IInputSystem::GetAttachedWindow() const
{ return static_cast<const CInputSystem*>(this)->GetAttachedWindow(); }
InputCursorHandle_t IInputSystem::GetStandardCursor( InputStandardCursor_t id )
{ return static_cast<CInputSystem*>(this)->GetStandardCursor( id ); }
InputCursorHandle_t IInputSystem::LoadCursorFromFile( const char *pFileName, const char *pPathID )
{ return static_cast<CInputSystem*>(this)->LoadCursorFromFile( pFileName, pPathID ); }
void IInputSystem::SetCursorIcon( InputCursorHandle_t hCursor )
{ static_cast<CInputSystem*>(this)->SetCursorIcon( hCursor ); }
void IInputSystem::GetCursorPosition( int *pX, int *pY )
{ static_cast<CInputSystem*>(this)->GetCursorPosition( pX, pY ); }
void IInputSystem::EnableMouseCapture( PlatWindow_t hWnd )
{ static_cast<CInputSystem*>(this)->EnableMouseCapture( hWnd ); }
void IInputSystem::DisableMouseCapture()
{ static_cast<CInputSystem*>(this)->DisableMouseCapture(); }
void IInputSystem::SetMouseCursorVisible( bool bVisible )
{ static_cast<CInputSystem*>(this)->SetMouseCursorVisible( bVisible ); }
void IInputSystem::DisableHardwareCursor( void )
{ static_cast<CInputSystem*>(this)->DisableHardwareCursor(); }
void IInputSystem::EnableHardwareCursor( void )
{ static_cast<CInputSystem*>(this)->EnableHardwareCursor(); }
void IInputSystem::ResetCursorIcon()
{ static_cast<CInputSystem*>(this)->ResetCursorIcon(); }
void IInputSystem::GetRawMouseAccumulators( float& accumX, float& accumY )
{ static_cast<CInputSystem*>(this)->GetRawMouseAccumulators( accumX, accumY ); }
InputDevice_t IInputSystem::GetConnectedInputDevices( void )
{ return static_cast<CInputSystem*>(this)->GetConnectedInputDevices(); }
bool IInputSystem::IsInputDeviceConnected( InputDevice_t device )
{ return static_cast<CInputSystem*>(this)->IsInputDeviceConnected( device ); }
void IInputSystem::SetInputDeviceConnected( InputDevice_t device, bool connected )
{ static_cast<CInputSystem*>(this)->SetInputDeviceConnected( device, connected ); }
InputDevice_t IInputSystem::IsOnlySingleDeviceConnected( void )
{ return static_cast<CInputSystem*>(this)->IsOnlySingleDeviceConnected(); }
InputDevice_t IInputSystem::GetCurrentInputDevice( void )
{ return static_cast<CInputSystem*>(this)->GetCurrentInputDevice(); }
bool IInputSystem::IsDeviceReadingInput( InputDevice_t device ) const
{ return static_cast<const CInputSystem*>(this)->IsDeviceReadingInput( device ); }
void IInputSystem::SetCurrentInputDevice( InputDevice_t device )
{ static_cast<CInputSystem*>(this)->SetCurrentInputDevice( device ); }
void IInputSystem::ResetCurrentInputDevice( void )
{ static_cast<CInputSystem*>(this)->ResetCurrentInputDevice(); }
void IInputSystem::SampleInputToFindCurrentDevice( bool a0 )
{ static_cast<CInputSystem*>(this)->SampleInputToFindCurrentDevice( a0 ); }
bool IInputSystem::IsSamplingForCurrentDevice( void )
{ return static_cast<CInputSystem*>(this)->IsSamplingForCurrentDevice(); }
bool IInputSystem::Connect( CreateInterfaceFn factory )
{ return static_cast<CInputSystem*>(this)->Connect( factory ); }
void IInputSystem::Disconnect()
{ static_cast<CInputSystem*>(this)->Disconnect(); }
void *IInputSystem::QueryInterface( const char *pInterfaceName )
{ return static_cast<CInputSystem*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t IInputSystem::Init()
{ return static_cast<CInputSystem*>(this)->Init(); }
void IInputSystem::Shutdown()
{ static_cast<CInputSystem*>(this)->Shutdown(); }

