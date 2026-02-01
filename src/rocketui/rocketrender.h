#ifndef ROCKETRENDER_H
#define ROCKETRENDER_H

#include <RmlUi/Core/RenderInterface.h>

// Forward declarations for DXVK interop
struct IDirect3DDevice9;
struct ID3D9VkInteropDevice;

// Vulkan types - use DXVK's bundled headers
#include "../thirdparty/dxvk-native-2.7.1-4bbe487/include/vulkan/include/vulkan/vulkan.h"

// VMA forward declaration
struct VmaAllocator_T;
typedef VmaAllocator_T *VmaAllocator;
struct VmaAllocation_T;
typedef VmaAllocation_T *VmaAllocation;
struct VmaVirtualBlock_T;
typedef VmaVirtualBlock_T *VmaVirtualBlock;
struct VmaVirtualAllocation_T;
typedef VmaVirtualAllocation_T *VmaVirtualAllocation;

class RocketRenderDXVK : public Rml::RenderInterface {
public:
  static RocketRenderDXVK m_Instance;

  RocketRenderDXVK();
  ~RocketRenderDXVK();

  // Initialize with DXVK's D3D9 device - we'll extract Vulkan handles from it
  bool Initialize(IDirect3DDevice9 *pD3D9Device);
  void Shutdown();

  // Called each frame before/after rendering
  void BeginFrame();
  void EndFrame();

  // Called when D3D9 device is lost
  void ReleaseBackBuffer();

  // Force full reinitialization (e.g., after map change)
  void Reinitialize(IDirect3DDevice9 *pDevice);

  // Update D3D9 device pointer after device reset
  void SetD3D9Device(IDirect3DDevice9 *pDevice) { m_pD3D9Device = pDevice; }

  void SetScreenSize(int width, int height);

  // RmlUi RenderInterface implementation
  Rml::CompiledGeometryHandle
  CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                  Rml::Span<const int> indices) override;
  void RenderGeometry(Rml::CompiledGeometryHandle handle,
                      Rml::Vector2f translation,
                      Rml::TextureHandle texture) override;
  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

  Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions,
                                 const Rml::String &source) override;
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                     Rml::Vector2i source_dimensions) override;
  void ReleaseTexture(Rml::TextureHandle texture_handle) override;

  void EnableScissorRegion(bool enable) override;
  void SetScissorRegion(Rml::Rectanglei region) override;

  void SetTransform(const Rml::Matrix4f *transform) override;

private:
  // Texture handling
  struct texture_data_t {
    VkImage m_p_vk_image;
    VkImageView m_p_vk_image_view;
    VkSampler m_p_vk_sampler;
    VkDescriptorSet m_p_vk_descriptor_set;
    VmaAllocation m_p_vma_allocation;
    uint32_t m_generation;
  };

  // Geometry handle
  struct geometry_handle_t {
    int m_num_indices;
    uint32_t m_generation;
    VkDescriptorBufferInfo m_p_vertex;
    VkDescriptorBufferInfo m_p_index;
    VkDescriptorBufferInfo m_p_shader;
    VmaVirtualAllocation m_p_vertex_allocation;
    VmaVirtualAllocation m_p_index_allocation;
    VmaVirtualAllocation m_p_shader_allocation;
  };

  // Staging buffer for uploads
  struct buffer_data_t {
    VkBuffer m_p_vk_buffer;
    VmaAllocation m_p_vma_allocation;
  };

  // Shader uniform data
  struct shader_vertex_user_data_t {
    Rml::Matrix4f m_transform;
    Rml::Vector2f m_translate;
  };

  // Internal methods
  Rml::TextureHandle CreateTexture(Rml::Span<const Rml::byte> source,
                                   Rml::Vector2i dimensions,
                                   const Rml::String &name);
  buffer_data_t CreateStagingBuffer(VkDeviceSize size,
                                    VkBufferUsageFlags flags);
  void DestroyStagingBuffer(const buffer_data_t &data);

  void CreatePipelines();
  void CreateRenderPass();
  void CreateDescriptorSetLayout();
  void CreatePipelineLayout();
  void CreateDescriptorPool();
  void CreateSamplers();
  void CreateDepthStencilImage();
  void CreateFramebuffer();
  void CreateCommandPool();

  void DestroyPipelines();
  void DestroyRenderPass();
  void DestroyDescriptorSetLayout();
  void DestroyPipelineLayout();
  void DestroyDescriptorPool();
  void DestroySamplers();
  void DestroyDepthStencilImage();
  void DestroyFramebuffer();
  void DestroyCommandPool();

  void UploadToGPU(std::function<void(VkCommandBuffer)> commands);

  // Back buffer acquisition from DXVK
  bool AcquireBackBuffer();

  // DXVK interop handles
  IDirect3DDevice9 *m_pD3D9Device;
  ID3D9VkInteropDevice *m_pDxvkInterop;

  // Vulkan handles from DXVK
  VkInstance m_vkInstance;
  VkPhysicalDevice m_vkPhysicalDevice;
  VkDevice m_vkDevice;
  VkQueue m_vkQueue;
  uint32_t m_queueFamilyIndex;

  // VMA allocator
  VmaAllocator m_allocator;

  // Rendering resources
  VkRenderPass m_renderPass;
  VkPipelineLayout m_pipelineLayout;
  VkPipeline m_pipelineWithTextures;
  VkPipeline m_pipelineWithoutTextures;
  VkPipeline m_pipelineStencilRegion;
  VkPipeline m_pipelineStencilWithTextures;
  VkPipeline m_pipelineStencilWithoutTextures;

  VkDescriptorSetLayout m_descriptorSetLayoutTransform;
  VkDescriptorSetLayout m_descriptorSetLayoutTexture;
  VkDescriptorPool m_descriptorPool;
  VkDescriptorSet m_descriptorSet;

  VkSampler m_samplerLinear;
  VkCommandPool m_commandPool;
  VkCommandBuffer m_commandBuffer;
  VkFence m_uploadFence;
  VkFence m_renderFence;

  // Depth/stencil for scissor with transforms
  texture_data_t m_depthStencilImage;
  VkFramebuffer m_framebuffer;

  // Memory pool for geometry (simplified from RmlUi's version)
  VkBuffer m_geometryBuffer;
  VmaAllocation m_geometryBufferAlloc;
  VmaVirtualBlock m_geometryVirtualBlock;
  char *m_geometryBufferMapped;
  VkDeviceSize m_geometryBufferSize;
  VkDeviceSize m_minUniformAlignment;
  VkDeviceSize m_frameUniformOffset;
  VkDeviceSize m_uniformBufferStart;

  // State
  int m_width;
  int m_height;
  bool m_transformEnabled;
  bool m_scissorEnabled;
  bool m_useStencilPipeline;
  bool m_applyStencilToGeometry;
  bool m_initialized;
  bool m_frameActive;
  uint32_t m_generation;

  VkRect2D m_scissor;
  VkRect2D m_scissorOriginal;
  VkViewport m_viewport;
  Rml::Matrix4f m_projection;
  shader_vertex_user_data_t m_shaderUserData;

  // Pending deletions
  Rml::Vector<texture_data_t *> m_pendingTextureDeletes;
  Rml::Vector<geometry_handle_t *> m_pendingGeometryDeletes;

  // Back buffer tracking
  VkImage m_backBufferImage;
  VkImageView m_backBufferImageView;
  VkFormat m_backBufferFormat;
  VkFormat m_renderPassFormat;
  VkFormat m_depthStencilFormat;
  VkImageLayout m_backBufferCurrentLayout;
  struct ID3D9VkInteropTexture *m_pBackBufferInterop;
  uint32_t m_backBufferWidth;
  uint32_t m_backBufferHeight;
  void RecreateRenderPassAndPipelines(VkFormat newFormat);
};

#endif // ROCKETRENDER_H
