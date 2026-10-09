// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Arbitrary length bit string
//				** NOTE: This class does NOT override the bitwise operators
//						 as doing so would require overriding the operators
//						 to allocate memory for the returned bitstring.  This method
//						 would be prone to memory leaks as the calling party
//						 would have to remember to delete the memory.  Funtions
//						 are used instead to require the calling party to allocate
//						 and destroy their own memory

import std;
import ks.core;


#include "bitstring.h"
