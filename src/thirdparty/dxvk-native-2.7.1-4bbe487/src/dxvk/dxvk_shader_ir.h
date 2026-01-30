#pragma once

// Stub header for D3D9-only builds (dxbc-spirv not needed)

#include <atomic>
#include <string>
#include <vector>

#include "dxvk_shader.h"
#include "../util/thread.h"

namespace dxvk {

  // Empty stubs - D3D9 doesn't use IR shaders
  struct DxvkIrShaderCreateInfo {
    DxvkShaderOptions options;
    uint32_t flatShadingInputs = 0u;
    int32_t rasterizedStream = 0;

    size_t hash() const { return 0; }
    bool eq(const DxvkIrShaderCreateInfo&) const { return false; }
  };

  class DxvkIrShaderConverter : public RcObject {
  public:
    virtual ~DxvkIrShaderConverter() { }
  };

  class DxvkIrShader : public DxvkShader {
  public:
    DxvkIrShader(const DxvkIrShaderCreateInfo&, Rc<DxvkIrShaderConverter>) { }

    DxvkPipelineLayoutBuilder getLayout() override { return DxvkPipelineLayoutBuilder(); }
    DxvkShaderMetadata getShaderMetadata() override { return DxvkShaderMetadata(); }
    void compile() override { }
    SpirvCodeBuffer getCode(const DxvkShaderBindingMap*, const DxvkShaderLinkage*) override { return SpirvCodeBuffer(); }
    void dump(std::ostream&) override { }
    std::string debugName() override { return ""; }
  };

}
