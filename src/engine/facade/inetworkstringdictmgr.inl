// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the INetworkStringTableDictionaryMananger class to CNetworkStringTableDictionaryManager. Included by networkstringtable.cpp.

bool INetworkStringTableDictionaryMananger::OnLevelLoadStart( char const *pchMapName, CRC32_t *pStringTableCRC )
{ return static_cast<CNetworkStringTableDictionaryManager*>(this)->OnLevelLoadStart( pchMapName, pStringTableCRC ); }
void INetworkStringTableDictionaryMananger::OnBSPFullyUnloaded()
{ static_cast<CNetworkStringTableDictionaryManager*>(this)->OnBSPFullyUnloaded(); }
CRC32_t INetworkStringTableDictionaryMananger::GetCRC()
{ return static_cast<CNetworkStringTableDictionaryManager*>(this)->GetCRC(); }

