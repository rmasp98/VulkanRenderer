#pragma once

#include <vector>

#include "framebuffer.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan_defaults.hpp"

vk::UniqueDevice CreateVulkanDevice(
    vk::PhysicalDevice const& physicalDevice,
    std::vector<uint32_t> const& queueFamilyIndices,
    std::vector<char const*> const& extensions,
    vk::PhysicalDeviceFeatures const* physicalDeviceFeatures);

inline std::vector<char const*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

class DeviceApi {
 public:
  DeviceApi(vk::PhysicalDevice const& physicalDevice,
            std::vector<uint32_t> const& queueFamilyIndices)
      : physicalDevice_(physicalDevice),
        device_(CreateVulkanDevice(physicalDevice_, queueFamilyIndices,
                                   extensions, {})) {}

  vk::UniqueSwapchainKHR CreateSwapchain(
      vk::UniqueSurfaceKHR const& surface, vk::Extent2D& extent,
      std::vector<uint32_t> const& queueFamilyIndices,
      bool const uniqueQueueFamilies,
      vk::UniqueSwapchainKHR const& oldSwapchain) const;

  uint32_t GetNumSwapchainImages(vk::UniqueSwapchainKHR const& swapchain) const;

  uint32_t GetNextImageIndex(vk::UniqueSwapchainKHR const& swapchain,
                             vk::Semaphore const& semaphore);

  vk::Queue GetQueue(uint32_t const queueFamily,
                     uint32_t const queueIndex) const;

  vk::UniqueSemaphore CreateSemaphore(vk::SemaphoreCreateInfo const&) const;
  vk::UniqueFence CreateFence(vk::FenceCreateInfo const&) const;

  vk::Result WaitForFences(std::vector<vk::Fence> const& fences,
                           bool waitForAll = true,
                           uint64_t timeout = UINT64_MAX) const;

  void ResetFences(std::vector<vk::Fence> const& fences) const;

  ////////////////////////////////////////////////////////////////////////
  // Misc
  ////////////////////////////////////////////////////////////////////////

  vk::UniqueCommandPool CreateCommandPool(
      uint32_t const graphicsFamilyIndex) const;

  void WaitIdle() const { device_->waitIdle(); }

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

  vk::UniqueRenderPass CreateRenderpass(vk::RenderPassCreateInfo const&,
                                        vk::Format const surfaceFormat) const;

  vk::UniquePipeline CreatePipeline(
      vk::UniquePipelineCache const& cache,
      vk::GraphicsPipelineCreateInfo const& settings) const;

  std::vector<vk::UniqueImageView> CreateImageViews(
      vk::UniqueSwapchainKHR const& swapchain, vk::Format const surfaceFormat,
      vk::ComponentMapping const& componentMapping =
          defaults::framebuffer::ComponentMapping,
      vk::ImageSubresourceRange const& subResourceRange =
          defaults::framebuffer::SubResourceRange);

  Framebuffer CreateFramebuffer(vk::UniqueImageView&& imageView,
                                vk::UniqueRenderPass const& renderPass,
                                vk::Extent2D const& extent) const;

  std::vector<Framebuffer> CreateFramebuffers(
      std::vector<vk::UniqueImageView>&& imageViews,
      vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const;

  //////////////////////////////////////////////////////////////////////////////
  // Command Creation
  //////////////////////////////////////////////////////////////////////////////

  std::vector<vk::UniqueCommandBuffer> AllocateCommandBuffer(
      vk::CommandBufferLevel bufferLevel, uint32_t numBuffers,
      vk::UniqueCommandPool const& commandPool) const;

  //////////////////////////////////////////////////////////////////////////////
  // Buffer Creation
  //////////////////////////////////////////////////////////////////////////////

  vk::UniqueBuffer CreateBuffer(uint32_t const size,
                                vk::BufferUsageFlags const usage) const;

  vk::MemoryRequirements GetBufferMemoryRequirements(
      vk::UniqueBuffer const& buffer) const;

  vk::UniqueDeviceMemory AllocateMemory(vk::DeviceSize const size,
                                        uint32_t const typeIndex) const;

  void BindBufferMemory(vk::UniqueBuffer const& buffer,
                        vk::UniqueDeviceMemory const& memory,
                        vk::DeviceSize const offset) const;

  void* MapMemory(vk::UniqueDeviceMemory const& memory,
                  vk::DeviceSize const offset, vk::DeviceSize const size) const;

  void UnmapMemory(vk::UniqueDeviceMemory const& memory) const;

 private:
  vk::PhysicalDevice physicalDevice_;
  vk::UniqueDevice device_;
};

vk::Extent2D SetExtent(vk::Extent2D const& windowExtent,
                       vk::SurfaceCapabilitiesKHR const& capabilities);

vk::SurfaceFormatKHR SelectSurfaceFormat(vk::PhysicalDevice const& device,
                                         vk::UniqueSurfaceKHR const& surface);

uint32_t SelectImageCount(vk::SurfaceCapabilitiesKHR const& capabilities);

vk::SurfaceTransformFlagBitsKHR SelectPreTransform(
    vk::SurfaceCapabilitiesKHR const& capabilities);

vk::CompositeAlphaFlagBitsKHR SelectCompositeAlpha(
    vk::SurfaceCapabilitiesKHR const& capabilities);

vk::PresentModeKHR SelectPresentMode(vk::PhysicalDevice const& device,
                                     vk::UniqueSurfaceKHR const& surface);