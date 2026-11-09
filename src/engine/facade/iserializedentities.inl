// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISerializedEntities class to CSerializedEntities. Included by serializedentity.cpp.

SerializedEntityHandle_t ISerializedEntities::AllocateSerializedEntity( char const *pFile, int nLine )
{ return static_cast<CSerializedEntities*>(this)->AllocateSerializedEntity( pFile, nLine ); }
void ISerializedEntities::ReleaseSerializedEntity( SerializedEntityHandle_t handle )
{ static_cast<CSerializedEntities*>(this)->ReleaseSerializedEntity( handle ); }
SerializedEntityHandle_t ISerializedEntities::CopySerializedEntity( SerializedEntityHandle_t handle, char const *pFile, int nLine )
{ return static_cast<CSerializedEntities*>(this)->CopySerializedEntity( handle, pFile, nLine ); }

