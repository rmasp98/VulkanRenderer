#ifndef VULKAN_RENDERER_DEVICE_API_HPP
#define VULKAN_RENDERER_DEVICE_API_HPP

#include <vector>

#include "defaults.hpp"
#include "framebuffer.hpp"
#include "memory.hpp"
#include "utils.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

vk::UniqueDevice CreateVulkanDevice(
    vk::PhysicalDevice const& physicalDevice,
    std::vector<uint32_t> const& queueFamilyIndices,
    std::vector<char const*> const& extensions,
    vk::PhysicalDeviceFeatures const* features);

inline std::vector<char const*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

using ImageIndex = uint32_t;

class DeviceApi {
 public:
  DeviceApi(vk::PhysicalDevice const& physicalDevice,
            vk::PhysicalDeviceFeatures const* features,
            QueueFamilies const& queueFamilies, vk::SurfaceKHR const& surface,
            vk::SurfaceFormatKHR const& surfaceFormat, vk::Extent2D& extent)
      : physicalDevice_(physicalDevice),
        device_(CreateVulkanDevice(physicalDevice_,
                                   queueFamilies.UniqueIndices(), extensions,
                                   features)),
        surfaceFormat_(surfaceFormat),
        swapchain_(CreateSwapchain(surface, extent, queueFamilies, {})),
        commandPool_(CreateCommandPool(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueFamilies.Graphics())),
        descriptorPool_(CreateDescriptorPool()),
        allocator_(physicalDevice_) {}

  void RecreateSwapchain(vk::SurfaceKHR const& surface, vk::Extent2D& extent,
                         QueueFamilies const& queueFamilies);

  vk::SwapchainKHR const& GetSwapchain() { return swapchain_.get(); }

  uint32_t GetNumSwapchainImages() const;
  vk::Format GetSurfaceFormat() const { return surfaceFormat_.format; }

  vk::Format GetDepthBufferFormat(
      vk::ImageTiling tiling = vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlags features =
          vk::FormatFeatureFlagBits::eDepthStencilAttachment) const;

  ImageIndex GetNextImageIndex(vk::Semaphore const& semaphore);

  vk::Queue GetQueue(uint32_t const queueFamily,
                     uint32_t const queueIndex) const;

  bool IsLinearFilteringSupported(vk::Format const format) const {
    auto formatProperties = physicalDevice_.getFormatProperties(format);
    return (formatProperties.optimalTilingFeatures &
            vk::FormatFeatureFlagBits::eSampledImageFilterLinear) ==
           vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
  }

  vk::SampleCountFlagBits GetMaxMultiSamplingCount() const;

  //////////////////////////////////////////////////////////////////////////
  // Semaphores and fences
  //////////////////////////////////////////////////////////////////////////

  vk::UniqueSemaphore CreateSemaphore(
      vk::SemaphoreCreateInfo const& = {}) const;
  vk::UniqueFence CreateFence(vk::FenceCreateInfo const&) const;

  vk::Result WaitForFences(std::vector<vk::Fence> const& fences,
                           bool waitForAll = true,
                           uint64_t timeout = UINT64_MAX) const;

  void ResetFences(std::vector<vk::Fence> const& fences) const;

  void WaitIdle() const { device_->waitIdle(); }

  ////////////////////////////////////////////////////////////////////////
  // Command Creation
  ////////////////////////////////////////////////////////////////////////

  vk::UniqueCommandPool CreateCommandPool(
      vk::CommandPoolCreateFlags const,
      uint32_t const graphicsFamilyIndex) const;

  void ResetCommandPool(vk::UniqueCommandPool const& pool,
                        vk::CommandPoolResetFlags const flags = {}) const {
    device_->resetCommandPool(pool.get(), flags);
  }

  vk::UniqueCommandBuffer AllocateCommandBuffer(
      vk::CommandBufferLevel const bufferLevel =
          vk::CommandBufferLevel::ePrimary) const;

  std::vector<vk::CommandBuffer> AllocateCommandBuffers(
      vk::CommandBufferLevel const bufferLevel, uint32_t const numBuffers,
      vk::CommandPool const&) const;

  ////////////////////////////////////////////////////////////////////////
  // Shader Creation
  ////////////////////////////////////////////////////////////////////////
  vk::UniqueShaderModule CreateShaderModule(
      std::vector<char> const& shaderFile) const;

  vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(
      vk::DescriptorSetLayoutCreateInfo const& settings) const;

  ////////////////////////////////////////////////////////////////////////
  // Pipeline Creation
  ////////////////////////////////////////////////////////////////////////
  vk::UniquePipelineLayout CreatePipelineLayout(
      vk::PipelineLayoutCreateInfo const&) const;

  vk::UniquePipelineCache CreatePipelineCache(
      vk::PipelineCacheCreateInfo const&) const;

  vk::UniqueRenderPass CreateRenderpass(vk::RenderPassCreateInfo const&) const;

  vk::UniquePipeline CreatePipeline(
      vk::UniquePipelineCache const& cache,
      vk::GraphicsPipelineCreateInfo const& settings) const;

  vk::UniqueDescriptorPool CreateDescriptorPool() const;

  std::vector<vk::UniqueImageView> CreateSwapchainImageViews(
      vk::ComponentMapping const& componentMapping =
          defaults::framebuffer::ComponentMapping,
      vk::ImageSubresourceRange const& subResourceRange =
          defaults::framebuffer::SubResourceRange);

  vk::UniqueImageView CreateImageView(
      vk::Image const& image, vk::ImageViewType const viewType,
      vk::Format const format, vk::ComponentMapping const& componentMapping,
      vk::ImageSubresourceRange const& subResourceRange);

  Framebuffer CreateFramebuffer(
      vk::UniqueImageView&& imageView,
      std::vector<vk::ImageView> const& attachmentViews,
      vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const;

  std::vector<Framebuffer> CreateFramebuffers(
      std::vector<vk::UniqueImageView>&& imageViews,
      std::vector<vk::ImageView> const& attachmentViews,
      vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const;

  //////////////////////////////////////////////////////////////////////////////
  // Buffer Creation
  //////////////////////////////////////////////////////////////////////////////

  vk::UniqueBuffer CreateBuffer(uint32_t const size,
                                vk::BufferUsageFlags const usage) const;

  vk::UniqueImage CreateImage(
      vk::ImageCreateInfo&& createInfo,
      std::vector<uint32_t> const& queueFamilyIndices) const {
    createInfo.setQueueFamilyIndices(queueFamilyIndices);
    return device_->createImageUnique(createInfo);
  }

  vk::MemoryRequirements GetBufferMemoryRequirements(
      vk::Buffer const& buffer) const;

  Allocation AllocateMemory(vk::Buffer const& buffer,
                            vk::MemoryPropertyFlags const flags);

  Allocation AllocateMemory(vk::Image const& image,
                            vk::MemoryPropertyFlags const flags) {
    return allocator_.Allocate(image, flags, device_.get());
  }

  void DeallocateMemory(Allocation const&);

  void BindBufferMemory(vk::Buffer const& buffer,
                        vk::DeviceMemory const& memory,
                        vk::DeviceSize const offset) const;

  void* MapMemory(Allocation const&) const;

  void UnmapMemory(Allocation const&) const;

  uint64_t GetMemoryOffset(Allocation const&) const;

  //////////////////////////////////////////////////////////////////////////////
  // Shader Data
  //////////////////////////////////////////////////////////////////////////////

  std::vector<vk::UniqueDescriptorSet> AllocateDescriptorSet(
      vk::DescriptorSetLayout const& layout) const;

  void UpdateDescriptorSet(
      std::vector<vk::WriteDescriptorSet> const& writeSet) const;

  vk::UniqueSampler CreateSampler(vk::SamplerCreateInfo const& createInfo) {
    // TODO: tidy this up
    auto createInfo2 = createInfo;
    auto maxAnisotropy =
        physicalDevice_.getProperties().limits.maxSamplerAnisotropy;
    if (createInfo2.maxAnisotropy > maxAnisotropy) {
      createInfo2.maxAnisotropy = maxAnisotropy;
    }
    return device_->createSamplerUnique(createInfo2);
  }

 private:
  vk::PhysicalDevice physicalDevice_;
  vk::UniqueDevice device_;
  vk::SurfaceFormatKHR surfaceFormat_;
  vk::UniqueSwapchainKHR swapchain_;
  vk::UniqueCommandPool commandPool_;
  vk::UniqueDescriptorPool descriptorPool_;
  MemoryAllocator allocator_;

  vk::UniqueSwapchainKHR CreateSwapchain(
      vk::SurfaceKHR const& surface, vk::Extent2D& extent,
      QueueFamilies const& queueFamilies,
      vk::SwapchainKHR const& oldSwapchain) const;
};

vk::Extent2D SetExtent(vk::Extent2D const& windowExtent,
                       vk::SurfaceCapabilitiesKHR const& capabilities);

uint32_t SelectImageCount(vk::SurfaceCapabilitiesKHR const& capabilities);

vk::SurfaceTransformFlagBitsKHR SelectPreTransform(
    vk::SurfaceCapabilitiesKHR const& capabilities);

vk::CompositeAlphaFlagBitsKHR SelectCompositeAlpha(
    vk::SurfaceCapabilitiesKHR const& capabilities);

vk::PresentModeKHR SelectPresentMode(vk::PhysicalDevice const& device,
                                     vk::SurfaceKHR const& surface);

}  // namespace vulkan_renderer

#endif