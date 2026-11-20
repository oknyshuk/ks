#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.platwindow;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.iappsystem;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
#include "inputsystem/InputEnums.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
import std;
#include "input_device.h"

///-----------------------------------------------------------------------------
/// A handle to a cursor icon
///-----------------------------------------------------------------------------
struct InputCursorHandle_t__ { int unused; };
using InputCursorHandle_t = InputCursorHandle_t__ *;
inline constexpr auto INPUT_CURSOR_HANDLE_INVALID = ( (InputCursorHandle_t)0 );


///-----------------------------------------------------------------------------
/// An enumeration describing well-known cursor icons
///-----------------------------------------------------------------------------
enum InputStandardCursor_t
{
	INPUT_CURSOR_NONE	= 0,
	INPUT_CURSOR_ARROW,
	INPUT_CURSOR_IBEAM,   
	INPUT_CURSOR_HOURGLASS,
	INPUT_CURSOR_CROSSHAIR,
	INPUT_CURSOR_WAITARROW,
	INPUT_CURSOR_UP,
	INPUT_CURSOR_SIZE_NW_SE,
	INPUT_CURSOR_SIZE_NE_SW,
	INPUT_CURSOR_SIZE_W_E,
	INPUT_CURSOR_SIZE_N_S,
	INPUT_CURSOR_SIZE_ALL,
	INPUT_CURSOR_NO,
	INPUT_CURSOR_HAND,

	INPUT_CURSOR_COUNT
};



///-----------------------------------------------------------------------------
/// Main interface for input. This is a low-level interface, creating an
/// OS-independent queue of low-level input events which were sampled since
/// the last call to PollInputState. It also contains facilities for cursor
/// control and creation.
///-----------------------------------------------------------------------------
class IInputSystem
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

	/// Attach, detach input system from a particular window
	/// This window should be the root window for the application
	/// Only 1 window should be attached at any given time.
	void AttachToWindow( void* hWnd );
	void DetachFromWindow( );

	/// Enables/disables input. PollInputState will not update current 
	/// button/analog states when it is called if the system is disabled.
	void EnableInput( bool bEnable );

	/// Enables/disables the windows message pump. PollInputState will not
	/// Peek/Dispatch messages if this is disabled
	void EnableMessagePump( bool bEnable );

	/// Polls the current input state
	void PollInputState( bool bIsInGame = false );

	/// Gets the time of the last polling in ms
	int GetPollTick() const;

	/// Is a button down? "Buttons" are binary-state input devices (mouse buttons, keyboard keys)
	bool IsButtonDown( ButtonCode_t code ) const;

	/// Returns the tick at which the button was pressed and released
	int GetButtonPressedTick( ButtonCode_t code ) const;
	int GetButtonReleasedTick( ButtonCode_t code ) const;

	/// Gets the value of an analog input device this frame
	/// Includes joysticks, mousewheel, mouse
	int GetAnalogValue( AnalogCode_t code ) const;

	/// Gets the change in a particular analog input device this frame
	/// Includes joysticks, mousewheel, mouse
	int GetAnalogDelta( AnalogCode_t code ) const;

	/// Returns the input events since the last poll
	int GetEventCount() const;
	const InputEvent_t* GetEventData( ) const;

	// Motion Controller status
	bool	   MotionControllerActive() const;
	Quaternion GetMotionControllerOrientation() const; // Pointer direction
	float	   GetMotionControllerPosX() const;
	float	   GetMotionControllerPosY() const;
	int		   GetMotionControllerDeviceStatus() const;
	uint64		   GetMotionControllerDeviceStatusFlags() const;
	void	   SetMotionControllerDeviceStatus( int nStatus );
	void	   SetMotionControllerCalibrationInvalid( void );
	void	   StepMotionControllerCalibration( void );
	void	   ResetMotionControllerScreenCalibration( void );

	/// Posts a user-defined event into the event queue; this is expected
	/// to be called in overridden wndprocs connected to the root panel.
	void PostUserEvent( const InputEvent_t &event );

	/// Returns the number of joysticks
	int GetJoystickCount() const;

	/// Enable/disable joystick, it has perf costs
	void EnableJoystickInput( int nJoystick, bool bEnable );

	/// Enable/disable diagonal joystick POV (simultaneous POV buttons down)
	void EnableJoystickDiagonalPOV( int nJoystick, bool bEnable );

	/// Sample the joystick and append events to the input queue
	void SampleDevices( void );

	// FIXME: Currently force-feedback is only supported on the Xbox 360
	void SetRumble( float fLeftMotor, float fRightMotor, int userId = INVALID_USER_ID );
	void StopRumble( int userId = INVALID_USER_ID );

	/// Resets the input state
	void ResetInputState();

	/// Convert back + forth between ButtonCode/AnalogCode + strings
	const char *ButtonCodeToString( ButtonCode_t code ) const;
	const char *AnalogCodeToString( AnalogCode_t code ) const;
	ButtonCode_t StringToButtonCode( const char *pString ) const;
	AnalogCode_t StringToAnalogCode( const char *pString ) const;

	/// Sleeps until input happens. Pass a negative number to sleep infinitely
	void SleepUntilInput( int nMaxSleepTimeMS = -1 );

	/// Convert back + forth between virtual codes + button codes
	// FIXME: This is a temporary piece of code
	ButtonCode_t VirtualKeyToButtonCode( int nVirtualKey ) const;
	int ButtonCodeToVirtualKey( ButtonCode_t code ) const;
	ButtonCode_t ScanCodeToButtonCode( int lParam ) const;

	/// How many times have we called PollInputState?
	int GetPollCount() const;

	/// Sets the cursor position
	void SetCursorPosition( int x, int y );

	/// Tells the input system to generate UI-related events, defined
	/// in inputsystem/inputenums.h (see IE_FirstUIEvent)
	/// We could have multiple clients that care about UI-related events
	/// so we refcount the clients with an Add/Remove strategy. If there
	/// are no interested clients, the UI events are not generated
	void AddUIEventListener();
	void RemoveUIEventListener();

	/// Returns the currently attached window
	PlatWindow_t GetAttachedWindow() const;

	/// Creates a cursor using one of the well-known cursor icons
	InputCursorHandle_t GetStandardCursor( InputStandardCursor_t id );

	/// Loads a cursor defined in a file
	InputCursorHandle_t LoadCursorFromFile( const char *pFileName, const char *pPathID = nullptr );

	/// Sets the cursor icon
	void SetCursorIcon( InputCursorHandle_t hCursor );

	/// Gets the cursor position
	void GetCursorPosition( int *pX, int *pY );

	/// Mouse capture
	void EnableMouseCapture( PlatWindow_t hWnd );
	void DisableMouseCapture();
	
	// Mouse visibility, tell inputsystem when we hide stuff rather than querying the OS which is expensive on OSX
	void SetMouseCursorVisible( bool bVisible );


	void DisableHardwareCursor( void );
	void EnableHardwareCursor( void );
	
	/// Reset the current cursor icon.  Used to reset the icon in the case of alt+tabs where the cursor has been forced to a different
	/// icon because it was outside of the client rect during the reload.
	void ResetCursorIcon();

	// read and clear accumulated raw input values
	void GetRawMouseAccumulators( float& accumX, float& accumY );

	// ========================================================================
	// Platform Input Device Interface
	//
	// This section provides a way to determine what Input/controller setup(s) are available on a
	//  given platform (PC, MAC, PS3, XBox) and what input device is currently selected/in use on
	//  the local client
	//
	//  Modules outside of the inputsystem need this information for tasks such as statistics, 
	//  achievements, and player rankings which take into account what input controller setup the 
	//  player is using on a per-platform basis.
	//
	//  The platform can be specified because a dedicate server may be running on a different 
	//    platform than the clients connected to it.
	// 
	// The master list of input devices and platforms used here is located in src\common\input_device.h 
	//
	// The methods here allow the user to write platform agnostic code to iterate through and
	//  and process the list of input devices specific to the current (or specified) platform 
	//  without seeing devices not applicable to that platform.
	// 
	// Terminology:
	//   Connected Device    = Input setup is connected and available for use, can be more than one
	//   Current Device		 = Input setup being actively used
	// 

	// ================================================================
	// Input Device Functions specific to the local client and hardware
	// ================================================================

	// Manage the list of input devices that are connected
	InputDevice_t GetConnectedInputDevices( void );				// returns the bitfield of all connected devices
	bool IsInputDeviceConnected( InputDevice_t device );
	void SetInputDeviceConnected( InputDevice_t device, bool connected = true );
	InputDevice_t IsOnlySingleDeviceConnected( void );

	// Access the currently selected Input device
	InputDevice_t GetCurrentInputDevice( void );				// returns the enum referring to the one currently selected device
	bool IsDeviceReadingInput( InputDevice_t device ) const;			// returns whether the passed in device is the current device.  Returns true if no current device is defined.
	void SetCurrentInputDevice( InputDevice_t device );
	void ResetCurrentInputDevice( void );						// sets the input device to the platform default

	void SampleInputToFindCurrentDevice( bool );  // looks for the next 'significant' button press to determine and set the current input device
	bool IsSamplingForCurrentDevice( void );
};

extern IInputSystem *g_pInputSystem;;
