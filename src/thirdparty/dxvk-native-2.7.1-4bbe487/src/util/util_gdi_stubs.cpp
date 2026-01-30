// Stub implementations for Windows GDI/KMT functions (not available on Linux)

#include "util_gdi.h"
#include "com/com_guid.h"
#include <ostream>

namespace dxvk {

  // KMT function stubs - these are Windows-only APIs
  NTSTATUS WINAPI D3DKMTAcquireKeyedMutex(D3DKMT_ACQUIREKEYEDMUTEX*) { return -1; }
  NTSTATUS WINAPI D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTCreateDCFromMemory(D3DKMT_CREATEDCFROMMEMORY*) { return -1; }
  NTSTATUS WINAPI D3DKMTCreateDevice(D3DKMT_CREATEDEVICE*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTCreateKeyedMutex2(D3DKMT_CREATEKEYEDMUTEX2*) { return -1; }
  NTSTATUS WINAPI D3DKMTDestroyAllocation(const D3DKMT_DESTROYALLOCATION*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTDestroyDCFromMemory(const D3DKMT_DESTROYDCFROMMEMORY*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTDestroyKeyedMutex(const D3DKMT_DESTROYKEYEDMUTEX*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTDestroySynchronizationObject(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTEscape(const D3DKMT_ESCAPE*) { return STATUS_SUCCESS; }
  NTSTATUS WINAPI D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID*) { return -1; }
  NTSTATUS WINAPI D3DKMTOpenResource2(D3DKMT_OPENRESOURCE*) { return -1; }
  NTSTATUS WINAPI D3DKMTQueryResourceInfo(D3DKMT_QUERYRESOURCEINFO*) { return -1; }
  NTSTATUS WINAPI D3DKMTReleaseKeyedMutex(D3DKMT_RELEASEKEYEDMUTEX*) { return -1; }

  // Shared resource stubs
  HANDLE openKmtHandle(HANDLE) { return nullptr; }

  void setSharedMetadata(HANDLE, void*, uint32_t) { }

  // GUID logging
  bool logQueryInterfaceError(REFIID, REFIID) { return false; }

}

std::ostream& operator << (std::ostream& os, const GUID&) {
  return os << "{GUID}";
}
