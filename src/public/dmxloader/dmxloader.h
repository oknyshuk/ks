#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

import std;
import ks.core.vprof;
import ks.tier1.byte_reader;
import ks.tier1.byte_writer;
import ks.tier1.commandbuffer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.strtools;
import ks.tier1.text_reader;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace ks {
class byte_reader;
class byte_writer;
class text_reader;
class text_writer;
} // namespace ks
class CDmxElement;

//-----------------------------------------------------------------------------
// Serialization/Unserialization
//-----------------------------------------------------------------------------
bool SerializeDMX(ks::byte_writer &buf, CDmxElement *pRoot,
                  const char *pFileName = nullptr);
bool SerializeDMX(ks::text_writer &buf, CDmxElement *pRoot,
                  const char *pFileName = nullptr);
bool SerializeDMX(const char *pFileName, const char *pPathID, bool bTextMode,
                  CDmxElement *pRoot);

bool UnserializeDMX(ks::byte_reader &buf, CDmxElement **ppRoot,
                    const char *pFileName = nullptr);
bool UnserializeDMX(ks::text_reader &buf, CDmxElement **ppRoot,
                    const char *pFileName = nullptr);
bool UnserializeDMX(const char *pFileName, const char *pPathID, bool bTextMode,
                    CDmxElement **ppRoot);

//-----------------------------------------------------------------------------
// DMX elements/attributes can only be accessed inside a dmx context
//-----------------------------------------------------------------------------
void BeginDMXContext();
void EndDMXContext(bool bDecommitMemory);
void DecommitDMXMemory();

//-----------------------------------------------------------------------------
// Helper macro
//-----------------------------------------------------------------------------
class CDMXContextHelper {
public:
  CDMXContextHelper(bool bDecommitMemory) {
    m_bDecommitMemory = bDecommitMemory;
    BeginDMXContext();
  }
  ~CDMXContextHelper() { EndDMXContext(m_bDecommitMemory); }

private:
  bool m_bDecommitMemory;
};

#define DECLARE_DMX_CONTEXT() CDMXContextHelper __dmxContextHelper(true);
#define DECLARE_DMX_CONTEXT_NODECOMMIT()                                       \
  CDMXContextHelper __dmxContextHelper(false);
#define DECLARE_DMX_CONTEXT_DECOMMIT(_decommit)                                \
  CDMXContextHelper __dmxContextHelper(_decommit);

//-----------------------------------------------------------------------------
// Used for allocation. All will be freed when we leave the DMX context
//-----------------------------------------------------------------------------
void *DMXAlloc(std::size_t size);

//-----------------------------------------------------------------------------
// Helper macro
//-----------------------------------------------------------------------------
#define DECLARE_DMX_ALLOCATOR()                                                \
public:                                                                        \
  inline void *operator new(std::size_t size) {                                     \
    return DMXAlloc(size);                                                     \
  }                                                                            \
  inline void *operator new(std::size_t size, int nBlockUse, const char *pFileName, \
                            int nLine) {                                       \
    return DMXAlloc(size);                                                     \
  }                                                                            \
  inline void operator delete(void *p) {}                                      \
  inline void operator delete(void *p, int nBlockUse, const char *pFileName,   \
                              int nLine) {}
