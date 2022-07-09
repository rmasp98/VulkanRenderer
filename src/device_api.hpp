#pragma once

#include <vector>

#include "framebuffer.hpp"
#include "memory.hpp"
#include "utils.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan_defaults.hpp"

vk::UniqueDevice CreateVulkanDevice(
    vk::PhysicalDevice const& physicalDevice,
    std::vector<uint32_t> const& queueFamilyIndices,
    std::vector<char const*> const& extensions,
    vk::PhysicalDeviceFeatures const* physicalDeviceFeatures);

inline std::vector<char const*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

using ImageIndex = uint32_t;

class DeviceApi {
 public:
  DeviceApi(vk::PhysicalDevice const& physicalDevice,
            QueueFamilies const& queueFamilies,
            vk::UniqueSurfaceKHR const& surface,
            vk::SurfaceFormatKHR const& surfaceFormat, vk::Extent2D& extent)
      : physicalDevice_(physicalDevice),
        device_(CreateVulkanDevice(
            physicalDevice_, queueFamilies.UniqueIndices(), extensions, {})),
        surfaceFormat_(surfaceFormat),
        swapchain_(CreateSwapchain(surface, extent, queueFamilies, {})),
        commandPool_(CreateCommandPool(queueFamilies.Graphics())),
        descriptorPool_(CreateDescriptorPool()),
        allocator_(physicalDevice_) {}

  void RecreateSwapchain(vk::UniqueSurfaceKHR const& surface,
                         vk::Extent2D& extent,
                         QueueFamilies const& queueFamilies);

  vk::UniqueSwapchainKHR const& GetSwapchain() { return swapchain_; }

  uint32_t GetNumSwapchainImages() const;

  ImageIndex GetNextImageIndex(vk::Semaphore const& semaphore);

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
      vk::PipelineLayoutCreateInfo const&,
      std::vector<vk::DescriptorSetLayout> const&) const;

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

  Framebuffer CreateFramebuffer(vk::UniqueImageView&& imageView,
                                vk::UniqueRenderPass const& renderPass,
                                vk::Extent2D const& extent) const;

  std::vector<Framebuffer> CreateFramebuffers(
      std::vector<vk::UniqueImageView>&& imageViews,
      vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const;

  //////////////////////////////////////////////////////////////////////////////
  // Command Creation
  //////////////////////////////////////////////////////////////////////////////

  std::vector<vk::UniqueCommandBuffer> AllocateCommandBuffers(
      vk::CommandBufferLevel bufferLevel, uint32_t numBuffers) const;

  //////////////////////////////////////////////////////////////////////////////
  // Buffer Creation
  //////////////////////////////////////////////////////////////////////////////

  vk::UniqueBuffer CreateBuffer(uint32_t const size,
                                vk::BufferUsageFlags const usage) const;

  vk::UniqueImage CreateImage(vk::ImageCreateInfo const& createInfo) const {
    return device_->createImageUnique(createInfo);
  }

  vk::MemoryRequirements GetBufferMemoryRequirements(
      vk::UniqueBuffer const& buffer) const;

  AllocationId AllocateMemory(vk::UniqueBuffer const& buffer,
                              vk::MemoryPropertyFlags const flags);

  AllocationId AllocateMemory(vk::UniqueImage const& image,
                              vk::MemoryPropertyFlags const flags) {
    return allocator_.Allocate(image, flags, device_);
  }

  void DeallocateMemory(AllocationId const id);

  void BindBufferMemory(vk::UniqueBuffer const& buffer,
                        vk::UniqueDeviceMemory const& memory,
                        vk::DeviceSize const offset) const;

  void* MapMemory(AllocationId const id) const;

  void UnmapMemory(AllocationId const id) const;

  uint64_t GetMemoryOffset(uint32_t const id) const;

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
      vk::UniqueSurfaceKHR const& surface, vk::Extent2D& extent,
      QueueFamilies const& queueFamilies,
      vk::UniqueSwapchainKHR const& oldSwapchain) const;
};

vk::Extent2D SetExtent(vk::Extent2D const& windowExtent,
                       vk::SurfaceCapabilitiesKHR const& capabilities);

uint32_t SelectImageCount(vk::SurfaceCapabilitiesKHR const& capabilities);

vk::SurfaceTransformFlagBitsKHR SelectPreTransform(
    vk::SurfaceCapabilitiesKHR const& capabilities);

vk::CompositeAlphaFlagBitsKHR SelectCompositeAlpha(
    vk::SurfaceCapabilitiesKHR const& capabilities);

vk::PresentModeKHR SelectPresentMode(vk::PhysicalDevice const& device,
                                     vk::UniqueSurfaceKHR const& surface);