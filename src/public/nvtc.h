/*
 *   Copyright (c) 1997-8  S3 Inc.  All Rights Reserved.
 *
 *   Module Name:  s3_intrf.h
 *
 *   Purpose:  Constant, structure, and prototype definitions for S3TC
 *			   interface to DX surface
 *
 *   Author:  Dan Hung, Martin Hoffesommer
 *
 *   Revision History:
 *	version Beta 1.00.00-98-03-26
 */

// Highlevel interface

#ifndef NVTC_H
#define NVTC_H


// RGB encoding types
#define S3TC_ENCODE_RGB_FULL    		0x0
#define S3TC_ENCODE_RGB_COLOR_KEY		0x1
#define S3TC_ENCODE_RGB_ALPHA_COMPARE	0x2
#define _S3TC_ENCODE_RGB_MASK			0xff

// alpha encoding types
#define S3TC_ENCODE_ALPHA_NONE			0x000
#define S3TC_ENCODE_ALPHA_EXPLICIT		0x100
#define S3TC_ENCODE_ALPHA_INTERPOLATED	0x200
#define _S3TC_ENCODE_ALPHA_MASK			0xff00



#endif // NVTC_H
