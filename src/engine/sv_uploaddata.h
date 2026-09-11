//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SV_UPLOADDATA_H
#define SV_UPLOADDATA_H


class KeyValues;

bool UploadData( char const *cserIP, char const *tablename, KeyValues *fields );

#endif // SV_UPLOADDATA_H
