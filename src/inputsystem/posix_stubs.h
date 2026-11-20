#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import std;
import ks.core.basetypes;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.platform;
import ks.simd;
import ks.tier1.appinstance;

typedef int32 LRESULT;
typedef void* HWND;
typedef uint32 UINT;
typedef uintp WPARAM;
typedef uintp LPARAM;

typedef uint8 BYTE;
typedef int16 SHORT;

typedef void* WNDPROC;
typedef void* HANDLE;

//typedef char xKey_t;

#define XUSER_MAX_COUNT 2

inline constexpr auto XK_MAX_KEYS = 20;

typedef struct joyinfoex_tag 
{ 
    DWORD dwSize; 
    DWORD dwFlags; 
    DWORD dwXpos; 
    DWORD dwYpos; 
    DWORD dwZpos; 
    DWORD dwRpos; 
    DWORD dwUpos; 
    DWORD dwVpos; 
    DWORD dwButtons; 
    DWORD dwButtonNumber; 
    DWORD dwPOV; 
    DWORD dwReserved1; 
    DWORD dwReserved2; 
} JOYINFOEX, *LPJOYINFOEX; 


typedef struct _XINPUT_GAMEPAD
{
    WORD                                wButtons;
    BYTE                                bLeftTrigger;
    BYTE                                bRightTrigger;
    SHORT                               sThumbLX;
    SHORT                               sThumbLY;
    SHORT                               sThumbRX;
    SHORT                               sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
    DWORD                               dwPacketNumber;
    XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION
{
    WORD                                wLeftMotorSpeed;
    WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;
