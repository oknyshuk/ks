// RocketUI Vulkan Renderer integrated with DXVK
// Uses DXVK's Vulkan interop to share VkInstance/VkDevice/VkQueue

// Undefine Source engine Assert to avoid conflict with RmlUi
#ifdef Assert
#undef Assert
#endif

// Use DXVK's bundled Vulkan headers (more up-to-date than system)
#include "../thirdparty/dxvk-native-2.7.1-4bbe487/include/vulkan/include/vulkan/vulkan.h"

#include "rocketrender.h"
#include <RmlUi/Core.h>

// D3D9 types needed for interface - from DXVK native
#include "d3d9_include.h"

// Forward declare the DXVK interop interfaces
MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
ID3D9VkInteropDevice : public IUnknown {
  virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance * pInstance,
                                                  VkPhysicalDevice * pPhysDev,
                                                  VkDevice * pDevice) = 0;

  virtual void STDMETHODCALLTYPE GetSubmissionQueue(
      VkQueue * pQueue, uint32_t *pQueueIndex, uint32_t *pQueueFamilyIndex) = 0;

  virtual void STDMETHODCALLTYPE TransitionTextureLayout(
      IUnknown * pTexture, const VkImageSubresourceRange *pSubresources,
      VkImageLayout OldLayout, VkImageLayout NewLayout) = 0;

  virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
  virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
  virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
  virtual void STDMETHODCALLTYPE LockDevice() = 0;
  virtual void STDMETHODCALLTYPE UnlockDevice() = 0;

  virtual bool STDMETHODCALLTYPE WaitForResource(IDirect3DResource9 * pResource,
                                                 DWORD MapFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateImage(
      const void *desc, IDirect3DResource9 **ppResult) = 0;
};

#ifndef _MSC_VER
__CRT_UUID_DECL(ID3D9VkInteropDevice, 0x2eaa4b89, 0x0107, 0x4bdb, 0x87, 0xf7,
                0x0f, 0x54, 0x1c, 0x49, 0x3c, 0xe0);
#endif

// ID3D9VkInteropTexture for getting VkImage from D3D9 textures
MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
ID3D9VkInteropTexture : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(
      VkImage * pHandle, VkImageLayout * pLayout,
      VkImageCreateInfo * pInfo) = 0;
};

#ifndef _MSC_VER
__CRT_UUID_DECL(ID3D9VkInteropTexture, 0xd56344f5, 0x8d35, 0x46fd, 0x80, 0x6d,
                0x94, 0xc3, 0x51, 0xb4, 0x72, 0xc1);
#endif

// VMA implementation
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "../thirdparty/RmlUi/Backends/RmlUi_Vulkan/vk_mem_alloc.h"

// SPIR-V shaders from RmlUi
#include "../thirdparty/RmlUi/Backends/RmlUi_Vulkan/ShadersCompiledSPV.h"

#include <string.h>

// Helper for alignment
template <typename T> static T AlignUp(T val, T alignment) {
  return (val + alignment - (T)1) & ~(alignment - (T)1);
}

RocketRenderDXVK RocketRenderDXVK::m_Instance;

RocketRenderDXVK::RocketRenderDXVK()
    : m_pD3D9Device(nullptr), m_pDxvkInterop(nullptr),
      m_vkInstance(VK_NULL_HANDLE), m_vkPhysicalDevice(VK_NULL_HANDLE),
      m_vkDevice(VK_NULL_HANDLE), m_vkQueue(VK_NULL_HANDLE),
      m_queueFamilyIndex(0), m_allocator(VK_NULL_HANDLE),
      m_renderPass(VK_NULL_HANDLE), m_pipelineLayout(VK_NULL_HANDLE),
      m_pipelineWithTextures(VK_NULL_HANDLE),
      m_pipelineWithoutTextures(VK_NULL_HANDLE),
      m_pipelineStencilRegion(VK_NULL_HANDLE),
      m_pipelineStencilWithTextures(VK_NULL_HANDLE),
      m_pipelineStencilWithoutTextures(VK_NULL_HANDLE),
      m_descriptorSetLayoutTransform(VK_NULL_HANDLE),
      m_descriptorSetLayoutTexture(VK_NULL_HANDLE),
      m_descriptorPool(VK_NULL_HANDLE), m_descriptorSet(VK_NULL_HANDLE),
      m_samplerLinear(VK_NULL_HANDLE), m_commandPool(VK_NULL_HANDLE),
      m_commandBuffer(VK_NULL_HANDLE), m_uploadFence(VK_NULL_HANDLE),
      m_renderFence(VK_NULL_HANDLE), m_depthStencilImage{},
      m_framebuffer(VK_NULL_HANDLE), m_geometryBuffer(VK_NULL_HANDLE),
      m_geometryBufferAlloc(VK_NULL_HANDLE),
      m_geometryVirtualBlock(VK_NULL_HANDLE), m_geometryBufferMapped(nullptr),
      m_geometryBufferSize(32 * 1024 * 1024) // 32MB - textarea generates geometry for ALL text
      ,
      m_minUniformAlignment(256), m_frameUniformOffset(0),
      m_uniformBufferStart(0), m_width(1920), m_height(1080),
      m_transformEnabled(false), m_scissorEnabled(false),
      m_useStencilPipeline(false), m_applyStencilToGeometry(false),
      m_initialized(false), m_frameActive(false), m_generation(0), m_scissor{},
      m_scissorOriginal{}, m_viewport{}, m_projection{},
      m_shaderUserData{} // Back buffer tracking
      ,
      m_backBufferImage(VK_NULL_HANDLE), m_backBufferImageView(VK_NULL_HANDLE),
      m_backBufferFormat(VK_FORMAT_B8G8R8A8_UNORM),
      m_renderPassFormat(VK_FORMAT_UNDEFINED),
      m_depthStencilFormat(VK_FORMAT_D32_SFLOAT_S8_UINT),
      m_backBufferCurrentLayout(VK_IMAGE_LAYOUT_UNDEFINED),
      m_pBackBufferInterop(nullptr), m_backBufferWidth(0),
      m_backBufferHeight(0) {}

RocketRenderDXVK::~RocketRenderDXVK() {
  if (m_initialized)
    Shutdown();
}

bool RocketRenderDXVK::Initialize(IDirect3DDevice9 *pD3D9Device) {
  if (m_initialized)
    return true;

  if (!pD3D9Device) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "No D3D9 device provided");
    return false;
  }

  m_pD3D9Device = pD3D9Device;

  // Query DXVK interop interface
  HRESULT hr = pD3D9Device->QueryInterface(__uuidof(ID3D9VkInteropDevice),
                                           (void **)&m_pDxvkInterop);
  if (FAILED(hr) || !m_pDxvkInterop) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "Failed to get DXVK interop interface");
    return false;
  }

  // Get Vulkan handles from DXVK
  m_pDxvkInterop->GetVulkanHandles(&m_vkInstance, &m_vkPhysicalDevice,
                                   &m_vkDevice);
  m_pDxvkInterop->GetSubmissionQueue(&m_vkQueue, nullptr, &m_queueFamilyIndex);

  if (!m_vkInstance || !m_vkDevice || !m_vkQueue) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "Invalid Vulkan handles from DXVK");
    return false;
  }

  // Get physical device properties for alignment requirements
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &props);
  m_minUniformAlignment = props.limits.minUniformBufferOffsetAlignment;

  // Find a supported depth/stencil format
  VkFormat depthFormats[] = {
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D32_SFLOAT,
  };
  m_depthStencilFormat = VK_FORMAT_UNDEFINED;
  for (VkFormat format : depthFormats) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(m_vkPhysicalDevice, format,
                                        &formatProps);
    if (formatProps.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      m_depthStencilFormat = format;
      break;
    }
  }
  if (m_depthStencilFormat == VK_FORMAT_UNDEFINED) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "No supported depth/stencil format found");
    return false;
  }

  // Initialize VMA allocator using DXVK's device
  VmaVulkanFunctions vulkanFunctions = {};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
  allocatorInfo.physicalDevice = m_vkPhysicalDevice;
  allocatorInfo.device = m_vkDevice;
  allocatorInfo.instance = m_vkInstance;
  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);
  if (result != VK_SUCCESS) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to create VMA allocator");
    return false;
  }

  // Create Vulkan resources
  CreateCommandPool();
  CreateSamplers();
  CreateDescriptorSetLayout();
  CreatePipelineLayout();
  CreateDescriptorPool();
  CreateRenderPass();
  CreatePipelines();

  // Create geometry buffer pool
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = m_geometryBufferSize;
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo allocResultInfo;
  result =
      vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &m_geometryBuffer,
                      &m_geometryBufferAlloc, &allocResultInfo);
  if (result != VK_SUCCESS) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to create geometry buffer");
    return false;
  }
  m_geometryBufferMapped = (char *)allocResultInfo.pMappedData;

  // Partition buffer: 7/8 for geometry, 1/8 for per-frame uniforms
  m_uniformBufferStart = (m_geometryBufferSize * 7) / 8;
  m_frameUniformOffset = m_geometryBufferSize;

  // Create virtual block for geometry allocations (limited to geometry portion)
  VmaVirtualBlockCreateInfo blockInfo = {};
  blockInfo.size = m_uniformBufferStart; // Use first 7/8 of buffer for geometry
  result = vmaCreateVirtualBlock(&blockInfo, &m_geometryVirtualBlock);
  if (result != VK_SUCCESS) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to create virtual block");
    return false;
  }

  // Allocate main descriptor set
  VkDescriptorSetAllocateInfo descAllocInfo = {};
  descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descAllocInfo.descriptorPool = m_descriptorPool;
  descAllocInfo.descriptorSetCount = 1;
  descAllocInfo.pSetLayouts = &m_descriptorSetLayoutTransform;
  vkAllocateDescriptorSets(m_vkDevice, &descAllocInfo, &m_descriptorSet);

  // Update descriptor set to point to geometry buffer (for uniform data)
  // For dynamic uniform buffers, range must cover the maximum possible
  // dynamic offset + data size. Use the full buffer size to be safe.
  VkDescriptorBufferInfo bufferDescInfo = {};
  bufferDescInfo.buffer = m_geometryBuffer;
  bufferDescInfo.offset = 0;
  bufferDescInfo.range = m_geometryBufferSize;

  VkWriteDescriptorSet writeDesc = {};
  writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDesc.dstSet = m_descriptorSet;
  writeDesc.dstBinding = 1; // Binding 1 for uniform buffer (matches shader)
  writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  writeDesc.descriptorCount = 1;
  writeDesc.pBufferInfo = &bufferDescInfo;
  vkUpdateDescriptorSets(m_vkDevice, 1, &writeDesc, 0, nullptr);

  m_initialized = true;
  return true;
}

void RocketRenderDXVK::Shutdown() {
  if (!m_initialized)
    return;

  // Wait for device idle before cleanup
  if (m_vkDevice)
    vkDeviceWaitIdle(m_vkDevice);

  // Process pending deletions BEFORE incrementing generation
  // (these were released while renderer was still valid, so free from current virtual block)
  for (auto *tex : m_pendingTextureDeletes) {
    if (tex->m_p_vk_descriptor_set)
      vkFreeDescriptorSets(m_vkDevice, m_descriptorPool, 1,
                           &tex->m_p_vk_descriptor_set);
    if (tex->m_p_vk_image_view)
      vkDestroyImageView(m_vkDevice, tex->m_p_vk_image_view, nullptr);
    if (tex->m_p_vk_image)
      vmaDestroyImage(m_allocator, tex->m_p_vk_image, tex->m_p_vma_allocation);
    delete tex;
  }
  m_pendingTextureDeletes.clear();

  for (auto *geom : m_pendingGeometryDeletes) {
    // Free from current virtual block (no generation check - these are all valid)
    if (geom->m_p_vertex_allocation)
      vmaVirtualFree(m_geometryVirtualBlock, geom->m_p_vertex_allocation);
    if (geom->m_p_index_allocation)
      vmaVirtualFree(m_geometryVirtualBlock, geom->m_p_index_allocation);
    delete geom;
  }
  m_pendingGeometryDeletes.clear();

  // NOW increment generation to invalidate any remaining geometry handles
  // (these will be released later and should NOT free from the new virtual block)
  m_generation++;

  // Clean up back buffer resources
  if (m_backBufferImageView) {
    vkDestroyImageView(m_vkDevice, m_backBufferImageView, nullptr);
    m_backBufferImageView = VK_NULL_HANDLE;
  }
  if (m_pBackBufferInterop) {
    m_pBackBufferInterop->Release();
    m_pBackBufferInterop = nullptr;
  }

  // Mark as uninitialized early to prevent use during cleanup
  m_initialized = false;

  // Destroy resources
  if (m_geometryVirtualBlock) {
    vmaDestroyVirtualBlock(m_geometryVirtualBlock);
    m_geometryVirtualBlock = VK_NULL_HANDLE;
  }
  if (m_geometryBuffer) {
    vmaDestroyBuffer(m_allocator, m_geometryBuffer, m_geometryBufferAlloc);
    m_geometryBuffer = VK_NULL_HANDLE;
    m_geometryBufferAlloc = VK_NULL_HANDLE;
    m_geometryBufferMapped = nullptr;
  }

  DestroyFramebuffer();
  DestroyDepthStencilImage();
  DestroyPipelines();
  DestroyRenderPass();
  DestroyDescriptorPool();
  DestroyPipelineLayout();
  DestroyDescriptorSetLayout();
  DestroySamplers();
  DestroyCommandPool();

  if (m_allocator)
    vmaDestroyAllocator(m_allocator);

  if (m_pDxvkInterop) {
    m_pDxvkInterop->Release();
    m_pDxvkInterop = nullptr;
  }

  // Reset ALL state for clean reinit
  m_allocator = VK_NULL_HANDLE;
  m_vkInstance = VK_NULL_HANDLE;
  m_vkPhysicalDevice = VK_NULL_HANDLE;
  m_vkDevice = VK_NULL_HANDLE;
  m_vkQueue = VK_NULL_HANDLE;
  m_queueFamilyIndex = 0;
  m_pD3D9Device = nullptr;

  // Reset back buffer state
  m_backBufferImage = VK_NULL_HANDLE;
  m_backBufferImageView = VK_NULL_HANDLE;
  m_backBufferFormat = VK_FORMAT_B8G8R8A8_UNORM;
  m_renderPassFormat = VK_FORMAT_UNDEFINED;
  m_backBufferCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_backBufferWidth = 0;
  m_backBufferHeight = 0;
  m_frameActive = false;
}

void RocketRenderDXVK::Reinitialize(IDirect3DDevice9 *pDevice) {
  Shutdown();
  Initialize(pDevice);
}

void RocketRenderDXVK::SetScreenSize(int width, int height) {
  if (width <= 0 || height <= 0)
    return;

  m_width = width;
  m_height = height;

  // Update projection matrix (orthographic)
  // RmlUi uses screen coordinates with Y=0 at top, Y=height at bottom
  // Use bottom=height, top=0 to match RmlUi's other backends
  m_projection = Rml::Matrix4f::ProjectOrtho(0, (float)m_width, (float)m_height,
                                             0, -10000, 10000);

  // Apply Vulkan coordinate system correction matrix
  // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
  // - Flip Y axis (Vulkan Y points down, OpenGL Y points up)
  // - Adjust depth from [-1,1] to [0,1] range
  Rml::Matrix4f correction_matrix;
  correction_matrix.SetColumns(
      Rml::Vector4f(1.0f, 0.0f, 0.0f, 0.0f),  // X unchanged
      Rml::Vector4f(0.0f, -1.0f, 0.0f, 0.0f), // Y negated
      Rml::Vector4f(0.0f, 0.0f, 0.5f, 0.0f),  // Z scaled
      Rml::Vector4f(0.0f, 0.0f, 0.5f, 1.0f)); // Z shifted
  m_projection = correction_matrix * m_projection;

  // Update viewport
  m_viewport.x = 0;
  m_viewport.y = 0;
  m_viewport.width = (float)m_width;
  m_viewport.height = (float)m_height;
  m_viewport.minDepth = 0.0f;
  m_viewport.maxDepth = 1.0f;

  // Update scissor
  m_scissorOriginal.offset = {0, 0};
  m_scissorOriginal.extent = {(uint32_t)m_width, (uint32_t)m_height};
  m_scissor = m_scissorOriginal;

  // Update shader user data
  m_shaderUserData.m_transform = m_projection;
}

void RocketRenderDXVK::ReleaseBackBuffer() {
  // Wait for ALL GPU work to finish
  if (m_vkDevice) {
    vkDeviceWaitIdle(m_vkDevice);
  }

  // Release back buffer resources to allow D3D9 device reset
  if (m_framebuffer) {
    vkDestroyFramebuffer(m_vkDevice, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }
  if (m_backBufferImageView) {
    vkDestroyImageView(m_vkDevice, m_backBufferImageView, nullptr);
    m_backBufferImageView = VK_NULL_HANDLE;
  }
  if (m_pBackBufferInterop) {
    m_pBackBufferInterop->Release();
    m_pBackBufferInterop = nullptr;
  }
  // Also release the DXVK interop device - it holds a D3D9 device reference
  if (m_pDxvkInterop) {
    m_pDxvkInterop->Release();
    m_pDxvkInterop = nullptr;
  }
  m_backBufferImage = VK_NULL_HANDLE;
  m_backBufferWidth = 0;
  m_backBufferHeight = 0;
  m_frameActive = false;
  m_pD3D9Device = nullptr; // Will be re-set on device reset
}

bool RocketRenderDXVK::AcquireBackBuffer() {
  if (!m_pD3D9Device)
    return false;

  // Re-acquire DXVK interop if it was released during device lost
  if (!m_pDxvkInterop) {
    HRESULT hr = m_pD3D9Device->QueryInterface(__uuidof(ID3D9VkInteropDevice),
                                               (void **)&m_pDxvkInterop);
    if (FAILED(hr) || !m_pDxvkInterop) {
      Rml::Log::Message(Rml::Log::LT_ERROR,
                        "Failed to re-acquire DXVK interop interface");
      return false;
    }
  }

  // Get the back buffer from D3D9
  IDirect3DSurface9 *pBackBuffer = nullptr;
  HRESULT hr =
      m_pD3D9Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
  if (FAILED(hr) || !pBackBuffer) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to get D3D9 back buffer");
    return false;
  }

  // Get the interop interface for the back buffer
  ID3D9VkInteropTexture *pInterop = nullptr;
  hr = pBackBuffer->QueryInterface(__uuidof(ID3D9VkInteropTexture),
                                   (void **)&pInterop);
  pBackBuffer->Release();

  if (FAILED(hr) || !pInterop) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "Failed to get interop for back buffer");
    return false;
  }

  // Get the VkImage from DXVK
  VkImage backBufferImage = VK_NULL_HANDLE;
  VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

  hr = pInterop->GetVulkanImageInfo(&backBufferImage, &currentLayout,
                                    &imageInfo);
  if (FAILED(hr) || !backBufferImage) {
    pInterop->Release();
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "Failed to get VkImage from back buffer");
    return false;
  }

  // Store the current layout for use in BeginFrame barrier
  m_backBufferCurrentLayout = currentLayout;

  // Check if we need to recreate the image view (new back buffer or size
  // changed)
  bool needRecreate = (m_backBufferImage != backBufferImage) ||
                      (m_backBufferWidth != imageInfo.extent.width) ||
                      (m_backBufferHeight != imageInfo.extent.height);

  // Check if format changed - need to recreate render pass and pipelines
  bool formatChanged = (m_renderPassFormat != VK_FORMAT_UNDEFINED) &&
                       (imageInfo.format != m_renderPassFormat);
  if (formatChanged) {
    RecreateRenderPassAndPipelines(imageInfo.format);
    needRecreate = true; // Force framebuffer recreation too
  }

  if (needRecreate) {
    // Clean up old resources
    if (m_backBufferImageView) {
      vkDestroyImageView(m_vkDevice, m_backBufferImageView, nullptr);
      m_backBufferImageView = VK_NULL_HANDLE;
    }
    if (m_framebuffer) {
      vkDestroyFramebuffer(m_vkDevice, m_framebuffer, nullptr);
      m_framebuffer = VK_NULL_HANDLE;
    }
    if (m_pBackBufferInterop) {
      m_pBackBufferInterop->Release();
      m_pBackBufferInterop = nullptr;
    }

    // Destroy old depth/stencil if size changed
    if (m_backBufferWidth != imageInfo.extent.width ||
        m_backBufferHeight != imageInfo.extent.height) {
      DestroyDepthStencilImage();
    }

    m_backBufferImage = backBufferImage;
    m_backBufferFormat = imageInfo.format;
    m_backBufferWidth = imageInfo.extent.width;
    m_backBufferHeight = imageInfo.extent.height;
    m_pBackBufferInterop = pInterop; // Keep reference

    // Create image view for the back buffer
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_backBufferImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_backBufferFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(m_vkDevice, &viewInfo, nullptr,
                                        &m_backBufferImageView);
    if (result != VK_SUCCESS) {
      Rml::Log::Message(Rml::Log::LT_ERROR,
                        "Failed to create back buffer image view");
      return false;
    }

    // Create depth/stencil if needed
    CreateDepthStencilImage();

    // Create framebuffer
    CreateFramebuffer();
  } else {
    // Same back buffer, just release the new interop reference
    pInterop->Release();
  }

  return true;
}

void RocketRenderDXVK::BeginFrame() {
  if (!m_initialized || !m_pD3D9Device)
    return;

  // Reset rendering state for new frame
  m_applyStencilToGeometry = false;
  m_useStencilPipeline = false;
  m_scissorEnabled = false;
  m_transformEnabled = false;
  m_frameActive = false;
  m_scissor = m_scissorOriginal;
  m_shaderUserData.m_transform = m_projection;

  // Try to acquire the back buffer first - this will re-acquire the DXVK
  // interop if it was released during device lost
  if (!AcquireBackBuffer()) {
    return;
  }

  // Now we have the interop, lock the submission queue
  m_pDxvkInterop->FlushRenderingCommands();
  m_pDxvkInterop->LockSubmissionQueue();

  // Process pending deletions from previous frames
  for (auto *tex : m_pendingTextureDeletes) {
    if (tex->m_p_vk_descriptor_set)
      vkFreeDescriptorSets(m_vkDevice, m_descriptorPool, 1,
                           &tex->m_p_vk_descriptor_set);
    if (tex->m_p_vk_image_view)
      vkDestroyImageView(m_vkDevice, tex->m_p_vk_image_view, nullptr);
    if (tex->m_p_vk_image)
      vmaDestroyImage(m_allocator, tex->m_p_vk_image, tex->m_p_vma_allocation);
    delete tex;
  }
  m_pendingTextureDeletes.clear();

  // Process pending geometry deletions - free their virtual allocations
  for (auto *geom : m_pendingGeometryDeletes) {
    // Only free from virtual block if this geometry is from current generation
    // (stale geometry from before reinit had its allocations in the old, now-destroyed block)
    if (geom->m_generation == m_generation) {
      if (geom->m_p_vertex_allocation)
        vmaVirtualFree(m_geometryVirtualBlock, geom->m_p_vertex_allocation);
      if (geom->m_p_index_allocation)
        vmaVirtualFree(m_geometryVirtualBlock, geom->m_p_index_allocation);
    }
    // Don't free shader allocation - it's managed by per-frame allocator
    delete geom;
  }
  m_pendingGeometryDeletes.clear();

  // Validate all critical resources before proceeding
  if (!m_framebuffer || !m_commandBuffer || !m_renderPass ||
      !m_pipelineWithTextures || !m_pipelineWithoutTextures ||
      !m_descriptorSet || !m_geometryBuffer || !m_backBufferImage) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "BeginFrame: missing resources (fb=%p cmd=%p rp=%p pipe=%p desc=%p geom=%p bb=%p)",
                      (void *)m_framebuffer, (void *)m_commandBuffer, (void *)m_renderPass,
                      (void *)m_pipelineWithTextures, (void *)m_descriptorSet,
                      (void *)m_geometryBuffer, (void *)m_backBufferImage);
    m_pDxvkInterop->ReleaseSubmissionQueue();
    return;
  }

  // Wait for previous frame's rendering to complete
  vkWaitForFences(m_vkDevice, 1, &m_renderFence, VK_TRUE, UINT64_MAX);
  vkResetFences(m_vkDevice, 1, &m_renderFence);

  // Reset per-frame uniform allocator (simple bump allocator)
  // Uniform data is allocated from the END of the geometry buffer
  m_frameUniformOffset = m_geometryBufferSize;

  // Begin command buffer
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkResetCommandBuffer(m_commandBuffer, 0);
  vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

  // Transition back buffer to color attachment optimal
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = m_backBufferCurrentLayout;
  barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_backBufferImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  // Set appropriate source access mask based on old layout
  if (m_backBufferCurrentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  } else if (m_backBufferCurrentLayout ==
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  } else {
    // UNDEFINED or GENERAL - no prior access to wait for
    barrier.srcAccessMask = 0;
  }
  barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  // Begin render pass
  VkRenderPassBeginInfo rpBeginInfo = {};
  rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpBeginInfo.renderPass = m_renderPass;
  rpBeginInfo.framebuffer = m_framebuffer;
  rpBeginInfo.renderArea.offset = {0, 0};
  rpBeginInfo.renderArea.extent = {m_backBufferWidth, m_backBufferHeight};

  // Clear values for depth/stencil only (we load color)
  VkClearValue clearValues[2] = {};
  clearValues[0].color = {
      {0.0f, 0.0f, 0.0f, 0.0f}}; // Not used (load op is LOAD)
  clearValues[1].depthStencil = {1.0f, 0};
  rpBeginInfo.clearValueCount = 2;
  rpBeginInfo.pClearValues = clearValues;

  vkCmdBeginRenderPass(m_commandBuffer, &rpBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Set viewport and scissor
  vkCmdSetViewport(m_commandBuffer, 0, 1, &m_viewport);
  vkCmdSetScissor(m_commandBuffer, 0, 1, &m_scissorOriginal);

  m_frameActive = true;
}

void RocketRenderDXVK::EndFrame() {
  if (!m_initialized || !m_frameActive) {
    if (m_initialized && m_pDxvkInterop)
      m_pDxvkInterop->ReleaseSubmissionQueue();
    return;
  }

  // End render pass
  vkCmdEndRenderPass(m_commandBuffer);

  // Transition back buffer back to present layout
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_backBufferImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

  vkCmdPipelineBarrier(m_commandBuffer,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // End command buffer
  vkEndCommandBuffer(m_commandBuffer);

  // Submit
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_commandBuffer;

  vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_renderFence);

  m_frameActive = false;

  // Release DXVK submission queue
  m_pDxvkInterop->ReleaseSubmissionQueue();
}

Rml::CompiledGeometryHandle
RocketRenderDXVK::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                  Rml::Span<const int> indices) {
  if (!m_initialized)
    return Rml::CompiledGeometryHandle(0);

  auto *handle = new geometry_handle_t{};
  handle->m_num_indices = (int)indices.size();
  handle->m_generation = m_generation;

  // Allocate vertex buffer
  VkDeviceSize vertexSize = vertices.size() * sizeof(Rml::Vertex);
  VmaVirtualAllocationCreateInfo allocInfo = {};
  allocInfo.size = vertexSize;
  allocInfo.alignment = 16;

  VkDeviceSize offset;
  if (vmaVirtualAllocate(m_geometryVirtualBlock, &allocInfo,
                         &handle->m_p_vertex_allocation,
                         &offset) != VK_SUCCESS) {
    VmaStatistics stats = {};
    vmaGetVirtualBlockStatistics(m_geometryVirtualBlock, &stats);
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "CompileGeometry: vertex alloc failed (need=%llu, used=%llu/%llu, count=%u)",
                      (unsigned long long)vertexSize, (unsigned long long)stats.allocationBytes,
                      (unsigned long long)m_uniformBufferStart, stats.allocationCount);
    delete handle;
    return Rml::CompiledGeometryHandle(0);
  }
  handle->m_p_vertex.buffer = m_geometryBuffer;
  handle->m_p_vertex.offset = offset;
  handle->m_p_vertex.range = vertexSize;
  memcpy(m_geometryBufferMapped + offset, vertices.data(), vertexSize);

  // Allocate index buffer
  VkDeviceSize indexSize = indices.size() * sizeof(int);
  allocInfo.size = indexSize;
  allocInfo.alignment = 4;

  if (vmaVirtualAllocate(m_geometryVirtualBlock, &allocInfo,
                         &handle->m_p_index_allocation,
                         &offset) != VK_SUCCESS) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "CompileGeometry: index allocation failed (size=%llu)",
                      (unsigned long long)indexSize);
    vmaVirtualFree(m_geometryVirtualBlock, handle->m_p_vertex_allocation);
    delete handle;
    return Rml::CompiledGeometryHandle(0);
  }
  handle->m_p_index.buffer = m_geometryBuffer;
  handle->m_p_index.offset = offset;
  handle->m_p_index.range = indexSize;
  memcpy(m_geometryBufferMapped + offset, indices.data(), indexSize);

  // Shader data will be allocated per-render
  handle->m_p_shader_allocation = VK_NULL_HANDLE;

  return Rml::CompiledGeometryHandle(handle);
}

void RocketRenderDXVK::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                      Rml::Vector2f translation,
                                      Rml::TextureHandle texture) {
  if (!m_initialized || !m_frameActive || !m_commandBuffer)
    return;

  geometry_handle_t *geom = reinterpret_cast<geometry_handle_t *>(geometry);
  if (!geom)
    return;

  // Check if this geometry handle is from a previous renderer generation
  // (stale handle after device reset/reinit)
  if (geom->m_generation != m_generation)
    return;

  // Check if geometry was released (buffer set to null in ReleaseGeometry)
  if (geom->m_p_vertex.buffer == VK_NULL_HANDLE)
    return;

  // Validate the geometry buffer matches our current buffer
  if (geom->m_p_vertex.buffer != m_geometryBuffer)
    return;

  texture_data_t *tex = reinterpret_cast<texture_data_t *>(texture);

  // Allocate and write shader uniform data
  m_shaderUserData.m_translate = translation;

  // Per-frame uniform data uses a simple bump allocator from the end of buffer
  // This avoids fragmenting the geometry virtual block
  VkDeviceSize uniformSize = AlignUp(
      (VkDeviceSize)sizeof(shader_vertex_user_data_t), m_minUniformAlignment);

  // Allocate from the end, growing downward (align the START of our allocation)
  VkDeviceSize alignedOffset =
      (m_frameUniformOffset - uniformSize) & ~(m_minUniformAlignment - 1);

  // Check we haven't run into the geometry region boundary
  if (alignedOffset < m_uniformBufferStart) {
    Rml::Log::Message(Rml::Log::LT_WARNING,
                      "Uniform buffer overflow - too many draw calls");
    return;
  }

  m_frameUniformOffset = alignedOffset;
  VkDeviceSize shaderOffset = alignedOffset;

  // Copy shader data
  memcpy(m_geometryBufferMapped + shaderOffset, &m_shaderUserData,
         sizeof(shader_vertex_user_data_t));

  // Check if texture descriptor set is stale (from before reinit)
  if (tex && tex->m_generation != m_generation) {
    // Descriptor set was allocated from old pool, now invalid - need to re-allocate
    tex->m_p_vk_descriptor_set = VK_NULL_HANDLE;
    tex->m_generation = m_generation;
  }

  // Create/update texture descriptor if needed
  if (tex && tex->m_p_vk_descriptor_set == VK_NULL_HANDLE) {
    VkDescriptorSetAllocateInfo descAllocInfo = {};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = m_descriptorPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &m_descriptorSetLayoutTexture;

    VkDescriptorSet texDescSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(m_vkDevice, &descAllocInfo, &texDescSet) ==
        VK_SUCCESS) {
      VkDescriptorImageInfo imageInfo = {};
      imageInfo.imageView = tex->m_p_vk_image_view;
      imageInfo.sampler = tex->m_p_vk_sampler;
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkWriteDescriptorSet writeDesc = {};
      writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDesc.dstSet = texDescSet;
      writeDesc.dstBinding = 2; // Binding 2 for texture sampler
      writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writeDesc.descriptorCount = 1;
      writeDesc.pImageInfo = &imageInfo;

      vkUpdateDescriptorSets(m_vkDevice, 1, &writeDesc, 0, nullptr);
      tex->m_p_vk_descriptor_set = texDescSet;
    }
  }

  // Bind descriptor sets
  uint32_t dynamicOffset = (uint32_t)shaderOffset;

  if (tex && tex->m_p_vk_descriptor_set) {
    VkDescriptorSet sets[] = {m_descriptorSet, tex->m_p_vk_descriptor_set};
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 2, sets, 1, &dynamicOffset);
  } else {
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &m_descriptorSet, 1,
                            &dynamicOffset);
  }

  // Bind appropriate pipeline
  VkPipeline pipeline =
      tex ? m_pipelineWithTextures : m_pipelineWithoutTextures;
  if (m_applyStencilToGeometry) {
    pipeline =
        tex ? m_pipelineStencilWithTextures : m_pipelineStencilWithoutTextures;
    if (!pipeline)
      pipeline = tex ? m_pipelineWithTextures : m_pipelineWithoutTextures;
  }
  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  // Set scissor
  vkCmdSetScissor(m_commandBuffer, 0, 1, &m_scissor);

  // Bind vertex and index buffers
  VkBuffer vertexBuffers[] = {geom->m_p_vertex.buffer};
  VkDeviceSize vertexOffsets[] = {geom->m_p_vertex.offset};
  vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, vertexBuffers, vertexOffsets);
  vkCmdBindIndexBuffer(m_commandBuffer, geom->m_p_index.buffer,
                       geom->m_p_index.offset, VK_INDEX_TYPE_UINT32);

  // Draw!
  vkCmdDrawIndexed(m_commandBuffer, geom->m_num_indices, 1, 0, 0, 0);
}

void RocketRenderDXVK::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
  if (!geometry)
    return;

  geometry_handle_t *geom = reinterpret_cast<geometry_handle_t *>(geometry);

  // Prevent double-free by checking if already released
  if (geom->m_p_vertex.buffer == VK_NULL_HANDLE)
    return;

  // Mark as released and queue for deletion
  geom->m_p_vertex.buffer = VK_NULL_HANDLE;
  m_pendingGeometryDeletes.push_back(geom);
}

Rml::TextureHandle
RocketRenderDXVK::LoadTexture(Rml::Vector2i &texture_dimensions,
                              const Rml::String &source) {
  Rml::FileInterface *file_interface = Rml::GetFileInterface();
  Rml::FileHandle file_handle = file_interface->Open(source);
  if (!file_handle)
    return Rml::TextureHandle(0);

  file_interface->Seek(file_handle, 0, SEEK_END);
  size_t buffer_size = file_interface->Tell(file_handle);
  file_interface->Seek(file_handle, 0, SEEK_SET);

// TGA header
#pragma pack(push, 1)
  struct TGAHeader {
    char idLength;
    char colourMapType;
    char dataType;
    short int colourMapOrigin;
    short int colourMapLength;
    char colourMapDepth;
    short int xOrigin;
    short int yOrigin;
    short int width;
    short int height;
    char bitsPerPixel;
    char imageDescriptor;
  };
#pragma pack(pop)

  if (buffer_size <= sizeof(TGAHeader)) {
    file_interface->Close(file_handle);
    return Rml::TextureHandle(0);
  }

  Rml::UniquePtr<Rml::byte[]> buffer(new Rml::byte[buffer_size]);
  file_interface->Read(buffer.get(), buffer_size, file_handle);
  file_interface->Close(file_handle);

  TGAHeader header;
  memcpy(&header, buffer.get(), sizeof(TGAHeader));

  int color_mode = header.bitsPerPixel / 8;
  if (header.dataType != 2 || color_mode < 3) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "Only 24/32bit uncompressed TGAs are supported");
    return Rml::TextureHandle(0);
  }

  const size_t image_size = header.width * header.height * 4;
  const Rml::byte *image_src = buffer.get() + sizeof(TGAHeader);
  Rml::UniquePtr<Rml::byte[]> image_dest(new Rml::byte[image_size]);

  bool top_to_bottom = ((header.imageDescriptor & 32) != 0);

  // Convert BGR(A) to RGBA with premultiplied alpha
  for (int y = 0; y < header.height; y++) {
    int read_index = y * header.width * color_mode;
    int write_index = top_to_bottom
                          ? (y * header.width * 4)
                          : ((header.height - y - 1) * header.width * 4);

    for (int x = 0; x < header.width; x++) {
      image_dest[write_index] = image_src[read_index + 2];
      image_dest[write_index + 1] = image_src[read_index + 1];
      image_dest[write_index + 2] = image_src[read_index];

      if (color_mode == 4) {
        Rml::byte alpha = image_src[read_index + 3];
        for (int j = 0; j < 3; j++)
          image_dest[write_index + j] =
              Rml::byte((image_dest[write_index + j] * alpha) / 255);
        image_dest[write_index + 3] = alpha;
      } else {
        image_dest[write_index + 3] = 255;
      }

      write_index += 4;
      read_index += color_mode;
    }
  }

  texture_dimensions.x = header.width;
  texture_dimensions.y = header.height;

  return GenerateTexture({image_dest.get(), image_size}, texture_dimensions);
}

Rml::TextureHandle
RocketRenderDXVK::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                  Rml::Vector2i source_dimensions) {
  if (!m_initialized || source_data.empty())
    return Rml::TextureHandle(0);

  auto *tex = new texture_data_t{};

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.extent.width = source_dimensions.x;
  imageInfo.extent.height = source_dimensions.y;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &tex->m_p_vk_image,
                     &tex->m_p_vma_allocation, nullptr) != VK_SUCCESS) {
    delete tex;
    return Rml::TextureHandle(0);
  }

  // Create staging buffer and upload
  VkDeviceSize imageSize = source_data.size();
  buffer_data_t staging =
      CreateStagingBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  void *mapped;
  vmaMapMemory(m_allocator, staging.m_p_vma_allocation, &mapped);
  memcpy(mapped, source_data.data(), imageSize);
  vmaUnmapMemory(m_allocator, staging.m_p_vma_allocation);

  // Upload to GPU
  UploadToGPU([&](VkCommandBuffer cmd) {
    // Transition to transfer dst
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex->m_p_vk_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {(uint32_t)source_dimensions.x,
                          (uint32_t)source_dimensions.y, 1};

    vkCmdCopyBufferToImage(cmd, staging.m_p_vk_buffer, tex->m_p_vk_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
  });

  DestroyStagingBuffer(staging);

  // Create image view
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = tex->m_p_vk_image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_vkDevice, &viewInfo, nullptr,
                        &tex->m_p_vk_image_view) != VK_SUCCESS) {
    vmaDestroyImage(m_allocator, tex->m_p_vk_image, tex->m_p_vma_allocation);
    delete tex;
    return Rml::TextureHandle(0);
  }

  tex->m_p_vk_sampler = m_samplerLinear;
  tex->m_p_vk_descriptor_set = VK_NULL_HANDLE; // Allocated on first use
  tex->m_generation = m_generation;

  return Rml::TextureHandle(tex);
}

void RocketRenderDXVK::ReleaseTexture(Rml::TextureHandle texture_handle) {
  if (!texture_handle)
    return;

  texture_data_t *tex = reinterpret_cast<texture_data_t *>(texture_handle);
  m_pendingTextureDeletes.push_back(tex);
}

void RocketRenderDXVK::EnableScissorRegion(bool enable) {
  m_scissorEnabled = enable;

  if (m_transformEnabled)
    m_applyStencilToGeometry = enable;

  if (!enable) {
    m_applyStencilToGeometry = false;
    m_scissor = m_scissorOriginal;
  }
}

void RocketRenderDXVK::SetScissorRegion(Rml::Rectanglei region) {
  if (!m_scissorEnabled)
    return;

  if (m_transformEnabled) {
    // For transformed scissor, we need to use stencil
    // This requires drawing a quad to the stencil buffer
    m_useStencilPipeline = true;
    m_applyStencilToGeometry = true;

    // TODO: Draw stencil quad
  } else {
    m_scissor.offset.x = Rml::Math::Clamp(region.Left(), 0, m_width);
    m_scissor.offset.y = Rml::Math::Clamp(region.Top(), 0, m_height);
    m_scissor.extent.width = region.Width();
    m_scissor.extent.height = region.Height();
  }
}

void RocketRenderDXVK::SetTransform(const Rml::Matrix4f *transform) {
  m_transformEnabled = (transform != nullptr);

  if (transform)
    m_shaderUserData.m_transform = m_projection * (*transform);
  else
    m_shaderUserData.m_transform = m_projection;
}

// Internal helper implementations

RocketRenderDXVK::buffer_data_t
RocketRenderDXVK::CreateStagingBuffer(VkDeviceSize size,
                                      VkBufferUsageFlags flags) {
  buffer_data_t data = {};

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = flags;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &data.m_p_vk_buffer,
                  &data.m_p_vma_allocation, nullptr);

  return data;
}

void RocketRenderDXVK::DestroyStagingBuffer(const buffer_data_t &data) {
  if (data.m_p_vk_buffer)
    vmaDestroyBuffer(m_allocator, data.m_p_vk_buffer, data.m_p_vma_allocation);
}

void RocketRenderDXVK::UploadToGPU(
    std::function<void(VkCommandBuffer)> commands) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &cmd);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmd, &beginInfo);
  commands(cmd);
  vkEndCommandBuffer(cmd);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_uploadFence);
  vkWaitForFences(m_vkDevice, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
  vkResetFences(m_vkDevice, 1, &m_uploadFence);

  vkFreeCommandBuffers(m_vkDevice, m_commandPool, 1, &cmd);
}

void RocketRenderDXVK::CreateCommandPool() {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = m_queueFamilyIndex;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  vkCreateCommandPool(m_vkDevice, &poolInfo, nullptr, &m_commandPool);

  // Allocate the main command buffer for rendering
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &m_commandBuffer);

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &m_uploadFence);

  // Create render fence (signaled initially so first frame doesn't wait)
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &m_renderFence);
}

void RocketRenderDXVK::DestroyCommandPool() {
  if (m_renderFence)
    vkDestroyFence(m_vkDevice, m_renderFence, nullptr);
  if (m_uploadFence)
    vkDestroyFence(m_vkDevice, m_uploadFence, nullptr);
  if (m_commandPool)
    vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
}

void RocketRenderDXVK::CreateSamplers() {
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

  vkCreateSampler(m_vkDevice, &samplerInfo, nullptr, &m_samplerLinear);
}

void RocketRenderDXVK::DestroySamplers() {
  if (m_samplerLinear)
    vkDestroySampler(m_vkDevice, m_samplerLinear, nullptr);
}

void RocketRenderDXVK::CreateDescriptorSetLayout() {
  // Layout for transform uniform buffer (binding 1, dynamic)
  VkDescriptorSetLayoutBinding transformBinding = {};
  transformBinding.binding = 1; // Matches shader
  transformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  transformBinding.descriptorCount = 1;
  transformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &transformBinding;

  vkCreateDescriptorSetLayout(m_vkDevice, &layoutInfo, nullptr,
                              &m_descriptorSetLayoutTransform);

  // Layout for texture sampler (binding 2)
  VkDescriptorSetLayoutBinding textureBinding = {};
  textureBinding.binding = 2;
  textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  textureBinding.descriptorCount = 1;
  textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  layoutInfo.pBindings = &textureBinding;
  vkCreateDescriptorSetLayout(m_vkDevice, &layoutInfo, nullptr,
                              &m_descriptorSetLayoutTexture);
}

void RocketRenderDXVK::DestroyDescriptorSetLayout() {
  if (m_descriptorSetLayoutTransform)
    vkDestroyDescriptorSetLayout(m_vkDevice, m_descriptorSetLayoutTransform,
                                 nullptr);
  if (m_descriptorSetLayoutTexture)
    vkDestroyDescriptorSetLayout(m_vkDevice, m_descriptorSetLayoutTexture,
                                 nullptr);
}

void RocketRenderDXVK::CreatePipelineLayout() {
  VkDescriptorSetLayout layouts[] = {m_descriptorSetLayoutTransform,
                                     m_descriptorSetLayoutTexture};

  VkPipelineLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 2;
  layoutInfo.pSetLayouts = layouts;

  vkCreatePipelineLayout(m_vkDevice, &layoutInfo, nullptr, &m_pipelineLayout);
}

void RocketRenderDXVK::DestroyPipelineLayout() {
  if (m_pipelineLayout)
    vkDestroyPipelineLayout(m_vkDevice, m_pipelineLayout, nullptr);
}

void RocketRenderDXVK::CreateDescriptorPool() {
  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500},
  };

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = 1000;
  poolInfo.poolSizeCount = 3;
  poolInfo.pPoolSizes = poolSizes;

  vkCreateDescriptorPool(m_vkDevice, &poolInfo, nullptr, &m_descriptorPool);
}

void RocketRenderDXVK::DestroyDescriptorPool() {
  if (m_descriptorPool)
    vkDestroyDescriptorPool(m_vkDevice, m_descriptorPool, nullptr);
}

void RocketRenderDXVK::CreateRenderPass() {
  VkFormat colorFormat = (m_backBufferFormat != VK_FORMAT_UNDEFINED)
                             ? m_backBufferFormat
                             : VK_FORMAT_B8G8R8A8_UNORM;
  m_renderPassFormat = colorFormat;

  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = colorFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp =
      VK_ATTACHMENT_LOAD_OP_LOAD; // Preserve existing content
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment = {};
  depthAttachment.format = m_depthStencilFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorRef = {0,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef = {
      1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 2;
  renderPassInfo.pAttachments = attachments;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  vkCreateRenderPass(m_vkDevice, &renderPassInfo, nullptr, &m_renderPass);
}

void RocketRenderDXVK::DestroyRenderPass() {
  if (m_renderPass) {
    vkDestroyRenderPass(m_vkDevice, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;
  }
}

void RocketRenderDXVK::CreatePipelines() {
  // Create shader modules from pre-compiled SPIR-V
  VkShaderModuleCreateInfo shaderInfo = {};
  shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

  VkShaderModule vertModule, fragColorModule, fragTextureModule;

  shaderInfo.codeSize = sizeof(shader_vert);
  shaderInfo.pCode = (uint32_t *)shader_vert;
  vkCreateShaderModule(m_vkDevice, &shaderInfo, nullptr, &vertModule);

  shaderInfo.codeSize = sizeof(shader_frag_color);
  shaderInfo.pCode = (uint32_t *)shader_frag_color;
  vkCreateShaderModule(m_vkDevice, &shaderInfo, nullptr, &fragColorModule);

  shaderInfo.codeSize = sizeof(shader_frag_texture);
  shaderInfo.pCode = (uint32_t *)shader_frag_texture;
  vkCreateShaderModule(m_vkDevice, &shaderInfo, nullptr, &fragTextureModule);

  // Vertex input - matches Rml::Vertex
  VkVertexInputBindingDescription vertexBinding = {};
  vertexBinding.binding = 0;
  vertexBinding.stride = sizeof(Rml::Vertex);
  vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexAttribs[3] = {};
  // Position
  vertexAttribs[0].location = 0;
  vertexAttribs[0].binding = 0;
  vertexAttribs[0].format = VK_FORMAT_R32G32_SFLOAT;
  vertexAttribs[0].offset = offsetof(Rml::Vertex, position);
  // Color
  vertexAttribs[1].location = 1;
  vertexAttribs[1].binding = 0;
  vertexAttribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
  vertexAttribs[1].offset = offsetof(Rml::Vertex, colour);
  // TexCoord
  vertexAttribs[2].location = 2;
  vertexAttribs[2].binding = 0;
  vertexAttribs[2].format = VK_FORMAT_R32G32_SFLOAT;
  vertexAttribs[2].offset = offsetof(Rml::Vertex, tex_coord);

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
  vertexInputInfo.vertexAttributeDescriptionCount = 3;
  vertexInputInfo.pVertexAttributeDescriptions = vertexAttribs;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor =
      VK_BLEND_FACTOR_ONE; // Premultiplied alpha
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.alphaBlendOp =
      VK_BLEND_OP_SUBTRACT;                  // Match RmlUi Vulkan backend
  colorBlendAttachment.colorWriteMask = 0xf; // RGBA

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  // Shader stages
  VkPipelineShaderStageCreateInfo vertStage = {};
  vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragColorStage = {};
  fragColorStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragColorStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragColorStage.module = fragColorModule;
  fragColorStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragTextureStage = {};
  fragTextureStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragTextureStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragTextureStage.module = fragTextureModule;
  fragTextureStage.pName = "main";

  // Pipeline without textures
  VkPipelineShaderStageCreateInfo stagesNoTex[] = {vertStage, fragColorStage};

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stagesNoTex;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.renderPass = m_renderPass;
  pipelineInfo.subpass = 0;

  vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
                            nullptr, &m_pipelineWithoutTextures);

  // Pipeline with textures
  VkPipelineShaderStageCreateInfo stagesWithTex[] = {vertStage,
                                                     fragTextureStage};
  pipelineInfo.pStages = stagesWithTex;

  vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo,
                            nullptr, &m_pipelineWithTextures);

  // TODO: Create stencil pipelines for transformed scissor regions

  // Cleanup shader modules
  vkDestroyShaderModule(m_vkDevice, vertModule, nullptr);
  vkDestroyShaderModule(m_vkDevice, fragColorModule, nullptr);
  vkDestroyShaderModule(m_vkDevice, fragTextureModule, nullptr);
}

void RocketRenderDXVK::DestroyPipelines() {
  if (m_pipelineWithTextures) {
    vkDestroyPipeline(m_vkDevice, m_pipelineWithTextures, nullptr);
    m_pipelineWithTextures = VK_NULL_HANDLE;
  }
  if (m_pipelineWithoutTextures) {
    vkDestroyPipeline(m_vkDevice, m_pipelineWithoutTextures, nullptr);
    m_pipelineWithoutTextures = VK_NULL_HANDLE;
  }
  if (m_pipelineStencilRegion) {
    vkDestroyPipeline(m_vkDevice, m_pipelineStencilRegion, nullptr);
    m_pipelineStencilRegion = VK_NULL_HANDLE;
  }
  if (m_pipelineStencilWithTextures) {
    vkDestroyPipeline(m_vkDevice, m_pipelineStencilWithTextures, nullptr);
    m_pipelineStencilWithTextures = VK_NULL_HANDLE;
  }
  if (m_pipelineStencilWithoutTextures) {
    vkDestroyPipeline(m_vkDevice, m_pipelineStencilWithoutTextures, nullptr);
    m_pipelineStencilWithoutTextures = VK_NULL_HANDLE;
  }
}

void RocketRenderDXVK::CreateDepthStencilImage() {
  if (m_backBufferWidth == 0 || m_backBufferHeight == 0)
    return;

  if (m_depthStencilFormat == VK_FORMAT_UNDEFINED) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "No depth/stencil format selected");
    return;
  }

  // Create depth/stencil image
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = m_depthStencilFormat;
  imageInfo.extent.width = m_backBufferWidth;
  imageInfo.extent.height = m_backBufferHeight;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo,
                     &m_depthStencilImage.m_p_vk_image,
                     &m_depthStencilImage.m_p_vma_allocation,
                     nullptr) != VK_SUCCESS) {
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "Failed to create depth/stencil image");
    return;
  }

  // Create image view
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_depthStencilImage.m_p_vk_image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = m_depthStencilFormat;
  // Set aspect mask based on format (some formats don't have stencil)
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (m_depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
      m_depthStencilFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
      m_depthStencilFormat == VK_FORMAT_D16_UNORM_S8_UINT) {
    viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_vkDevice, &viewInfo, nullptr,
                        &m_depthStencilImage.m_p_vk_image_view) != VK_SUCCESS) {
    vmaDestroyImage(m_allocator, m_depthStencilImage.m_p_vk_image,
                    m_depthStencilImage.m_p_vma_allocation);
    m_depthStencilImage = {};
    Rml::Log::Message(Rml::Log::LT_ERROR,
                      "Failed to create depth/stencil image view");
  }
}

void RocketRenderDXVK::DestroyDepthStencilImage() {
  if (m_depthStencilImage.m_p_vk_image_view)
    vkDestroyImageView(m_vkDevice, m_depthStencilImage.m_p_vk_image_view,
                       nullptr);
  if (m_depthStencilImage.m_p_vk_image)
    vmaDestroyImage(m_allocator, m_depthStencilImage.m_p_vk_image,
                    m_depthStencilImage.m_p_vma_allocation);
  m_depthStencilImage = {};
}

void RocketRenderDXVK::CreateFramebuffer() {
  if (!m_backBufferImageView || !m_depthStencilImage.m_p_vk_image_view)
    return;

  VkImageView attachments[] = {m_backBufferImageView,
                               m_depthStencilImage.m_p_vk_image_view};

  VkFramebufferCreateInfo framebufferInfo = {};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = m_renderPass;
  framebufferInfo.attachmentCount = 2;
  framebufferInfo.pAttachments = attachments;
  framebufferInfo.width = m_backBufferWidth;
  framebufferInfo.height = m_backBufferHeight;
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(m_vkDevice, &framebufferInfo, nullptr,
                          &m_framebuffer) != VK_SUCCESS) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to create framebuffer");
  }
}

void RocketRenderDXVK::DestroyFramebuffer() {
  if (m_framebuffer)
    vkDestroyFramebuffer(m_vkDevice, m_framebuffer, nullptr);
  m_framebuffer = VK_NULL_HANDLE;
}

void RocketRenderDXVK::RecreateRenderPassAndPipelines(VkFormat newFormat) {
  vkDeviceWaitIdle(m_vkDevice);
  m_backBufferFormat = newFormat;
  DestroyPipelines();
  DestroyRenderPass();
  CreateRenderPass();
  CreatePipelines();
}
