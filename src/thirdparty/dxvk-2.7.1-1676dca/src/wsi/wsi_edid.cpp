// [CS_PATCH] Stub - libdisplay-info not needed for D3D9/CS builds.
// Caller (d3d9_swapchain) handles nullopt with NormalizeDisplayMetadata defaults.
#include "wsi_edid.h"

namespace dxvk::wsi {

  std::optional<WsiDisplayMetadata> parseColorimetryInfo(
    const WsiEdidData& edidData) {
    return std::nullopt;
  }

}
