#pragma once

// Stub header for D3D9-only builds (shader cache not needed)

#include <string>

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#include "dxvk_shader_ir.h"

namespace dxvk {

  // Empty stub - D3D9 doesn't use shader cache
  class DxvkShaderCache : public RcObject {
  public:
    static Rc<DxvkShaderCache> getInstance() { return nullptr; }

    Rc<DxvkIrShader> lookupShader(const std::string&, const DxvkIrShaderCreateInfo&) { return nullptr; }
    void addShader(Rc<DxvkIrShader>) { }
  };

}
