
#include "device_api.hpp"

#include <limits>

void DeviceApi::RecreateSwapchain(vk::UniqueSurfaceKHR const& surface,
                                  vk::Extent2D& extent,
                                  QueueFamilies const& queueFamilies) {
  swapchain_ = CreateSwapchain(surface, extent, queueFamilies, swapchain_);
}

uint32_t DeviceApi::GetNumSwapchainImages() const {
  return device_->getSwapchainImagesKHR(swapchain_.get()).size();
}

vk::Format DeviceApi::GetDepthBufferFormat(
    vk::ImageTiling tiling, vk::FormatFeatureFlags features) const {
  std::vector<vk::Format> candidateFormats{vk::Format::eD32Sfloat,
                                           vk::Format::eD32SfloatS8Uint,
                                           vk::Format::eD24UnormS8Uint};
  for (auto format : candidateFormats) {
    vk::FormatProperties props = physicalDevice_.getFormatProperties(format);

    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == vk::ImageTiling::eOptimal &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  throw std::runtime_error("failed to find supported format!");
}

ImageIndex DeviceApi::GetNextImageIndex(vk::Semaphore const& semaphore) {
  uint32_t imageIndex;
  auto result = device_->acquireNextImageKHR(swapchain_.get(), UINT64_MAX,
                                             semaphore, nullptr, &imageIndex);
  if (result != vk::Result::eSuccess) {
    // TODO: check for the other possible "successes" like timeout
    // throw std::exception();
  }
  return imageIndex;
}

vk::Queue DeviceApi::GetQueue(uint32_t const queueFamily,
                              uint32_t const queueIndex) const {
  return device_->getQueue(queueFamily, queueIndex);
}

vk::SampleCountFlagBits DeviceApi::GetMaxMultiSamplingCount() const {
  auto properties = physicalDevice_.getProperties();
  auto sampleCount = properties.limits.framebufferColorSampleCounts &
                     properties.limits.framebufferDepthSampleCounts;
  if (sampleCount & vk::SampleCountFlagBits::e64)
    return vk::SampleCountFlagBits::e64;
  else if (sampleCount & vk::SampleCountFlagBits::e32)
    return vk::SampleCountFlagBits::e32;
  else if (sampleCount & vk::SampleCountFlagBits::e16)
    return vk::SampleCountFlagBits::e16;
  else if (sampleCount & vk::SampleCountFlagBits::e8)
    return vk::SampleCountFlagBits::e8;
  else if (sampleCount & vk::SampleCountFlagBits::e4)
    return vk::SampleCountFlagBits::e4;
  else if (sampleCount & vk::SampleCountFlagBits::e2)
    return vk::SampleCountFlagBits::e2;

  return vk::SampleCountFlagBits::e1;
}

vk::UniqueSemaphore DeviceApi::CreateSemaphore(
    vk::SemaphoreCreateInfo const& createInfo) const {
  return device_->createSemaphoreUnique(createInfo);
}

vk::UniqueFence DeviceApi::CreateFence(
    vk::FenceCreateInfo const& createInfo) const {
  return device_->createFenceUnique(createInfo);
}

vk::Result DeviceApi::WaitForFences(std::vector<vk::Fence> const& fences,
                                    bool waitForAll, uint64_t timeout) const {
  return device_->waitForFences(fences, waitForAll, timeout);
}

void DeviceApi::ResetFences(std::vector<vk::Fence> const& fences) const {
  device_->resetFences(fences);
}

vk::UniqueCommandPool DeviceApi::CreateCommandPool(
    uint32_t const graphicsFamilyIndex) const {
  return device_->createCommandPoolUnique(
      {vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
       graphicsFamilyIndex});
}

vk::UniqueShaderModule DeviceApi::CreateShaderModule(
    std::vector<char> const& shaderFile) const {
  return device_->createShaderModuleUnique(
      {{},
       shaderFile.size(),
       reinterpret_cast<const uint32_t*>(shaderFile.data())});
}

vk::UniqueDescriptorSetLayout DeviceApi::CreateDescriptorSetLayout(
    vk::DescriptorSetLayoutCreateInfo const& settings) const {
  if (settings.bindingCount > 0) {
    return device_->createDescriptorSetLayoutUnique(settings);
  }
  return {};
}

vk::UniquePipelineLayout DeviceApi::CreatePipelineLayout(
    vk::PipelineLayoutCreateInfo const& settings,
    std::vector<vk::DescriptorSetLayout> const& layouts) const {
  auto newSettings = settings;
  newSettings.setSetLayouts(layouts);
  return device_->createPipelineLayoutUnique(newSettings);
}

vk::UniquePipelineCache DeviceApi::CreatePipelineCache(
    vk::PipelineCacheCreateInfo const& settings) const {
  return device_->createPipelineCacheUnique(settings);
}

vk::UniqueRenderPass DeviceApi::CreateRenderpass(
    vk::RenderPassCreateInfo const& settings) const {
  return device_->createRenderPassUnique(settings);
}

vk::UniquePipeline DeviceApi::CreatePipeline(
    vk::UniquePipelineCache const& cache,
    vk::GraphicsPipelineCreateInfo const& settings) const {
  auto result = device_->createGraphicsPipelineUnique(cache.get(), settings);
  // TODO: check result is ok
  return std::move(result.value);
}

vk::UniqueDescriptorPool DeviceApi::CreateDescriptorPool() const {
  // TODO: move descriptor pools to pipeline. They should be created once the
  // shader has been created
  auto descriptorSizes = std::vector<vk::DescriptorPoolSize>(
      {{vk::DescriptorType::eUniformBuffer, GetNumSwapchainImages()},
       {vk::DescriptorType::eCombinedImageSampler, GetNumSwapchainImages()}});
  // TODO: figure out a how to decide the number
  return device_->createDescriptorPoolUnique(
      {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000,
       descriptorSizes});
}

std::vector<vk::UniqueImageView> DeviceApi::CreateSwapchainImageViews(
    vk::ComponentMapping const& componentMapping,
    vk::ImageSubresourceRange const& subResourceRange) {
  std::vector<vk::UniqueImageView> imageViews;
  for (auto& image : device_->getSwapchainImagesKHR(swapchain_.get())) {
    imageViews.push_back(CreateImageView(image, vk::ImageViewType::e2D,
                                         surfaceFormat_.format,
                                         componentMapping, subResourceRange));
  }
  return imageViews;
}

vk::UniqueImageView DeviceApi::CreateImageView(
    vk::Image const& image, vk::ImageViewType const viewType,
    vk::Format const format, vk::ComponentMapping const& componentMapping,
    vk::ImageSubresourceRange const& subResourceRange) {
  return device_->createImageViewUnique(
      {{}, image, viewType, format, componentMapping, subResourceRange});
}

Framebuffer DeviceApi::CreateFramebuffer(
    vk::UniqueImageView&& imageView,
    std::vector<vk::ImageView> const& attachmentViews,
    vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const {
  // Order of attachments must follow attachmentIndex in CreateRenderPass
  std::vector<vk::ImageView> attachments;
  attachments.push_back(imageView.get());
  attachments.insert(attachments.end(), attachmentViews.begin(),
                     attachmentViews.end());

  auto framebuffer = device_->createFramebufferUnique(
      {{}, renderPass.get(), attachments, extent.width, extent.height, 1});
  return {std::move(imageView), std::move(framebuffer)};
}

std::vector<Framebuffer> DeviceApi::CreateFramebuffers(
    std::vector<vk::UniqueImageView>&& imageViews,
    std::vector<vk::ImageView> const& attachmentViews,
    vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const {
  std::vector<Framebuffer> framebuffers;
  for (auto& imageView : imageViews) {
    framebuffers.push_back(CreateFramebuffer(
        std::move(imageView), attachmentViews, renderPass, extent));
  }

  return framebuffers;
}

std::vector<vk::UniqueCommandBuffer> DeviceApi::AllocateCommandBuffers(
    vk::CommandBufferLevel bufferLevel, uint32_t numBuffers) const {
  return device_->allocateCommandBuffersUnique(
      {commandPool_.get(), bufferLevel, numBuffers});
}

vk::UniqueBuffer DeviceApi::CreateBuffer(
    uint32_t const size, vk::BufferUsageFlags const usage) const {
  return device_->createBufferUnique({{}, size, usage});
}

Allocation DeviceApi::AllocateMemory(vk::UniqueBuffer const& buffer,
                                     vk::MemoryPropertyFlags const flags) {
  return allocator_.Allocate(buffer, flags, device_);
}

void DeviceApi::DeallocateMemory(Allocation const& allocation) {
  allocator_.Deallocate(allocation);
}

void DeviceApi::BindBufferMemory(vk::UniqueBuffer const& buffer,
                                 vk::UniqueDeviceMemory const& memory,
                                 vk::DeviceSize const offset) const {
  device_->bindBufferMemory(buffer.get(), memory.get(), offset);
}

void* DeviceApi::MapMemory(Allocation const& allocation) const {
  return allocator_.MapMemory(allocation, device_);
}

void DeviceApi::UnmapMemory(Allocation const& allocation) const {
  allocator_.UnmapMemory(allocation, device_);
}

uint64_t DeviceApi::GetMemoryOffset(Allocation const& allocation) const {
  return allocator_.GetOffset(allocation);
}

std::vector<vk::UniqueDescriptorSet> DeviceApi::AllocateDescriptorSet(
    vk::DescriptorSetLayout const& layout) const {
  std::vector<vk::DescriptorSetLayout> layouts(GetNumSwapchainImages(), layout);
  return device_->allocateDescriptorSetsUnique(
      {descriptorPool_.get(), layouts});
}

void DeviceApi::UpdateDescriptorSet(
    std::vector<vk::WriteDescriptorSet> const& writeSet) const {
  device_->updateDescriptorSets(writeSet, nullptr);
}

vk::UniqueSwapchainKHR DeviceApi::CreateSwapchain(
    vk::UniqueSurfaceKHR const& surface, vk::Extent2D& extent,
    QueueFamilies const& queueFamilies,
    vk::UniqueSwapchainKHR const& oldSwapchain) const {
  auto capabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface.get());
  extent = SetExtent(extent, capabilities);
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment |
                              vk::ImageUsageFlagBits::eTransferSrc;

  vk::SwapchainCreateInfoKHR swapChainCreateInfo(
      {}, surface.get(), SelectImageCount(capabilities), surfaceFormat_.format,
      surfaceFormat_.colorSpace, extent, 1, usage, vk::SharingMode::eExclusive,
      {}, SelectPreTransform(capabilities), SelectCompositeAlpha(capabilities),
      SelectPresentMode(physicalDevice_, surface), true, oldSwapchain.get());

  if (!queueFamilies.IsUniqueFamilies()) {
    auto queueIndices = queueFamilies.UniqueIndices();
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    swapChainCreateInfo.setQueueFamilyIndices(queueIndices);
  }

  return device_->createSwapchainKHRUnique(swapChainCreateInfo);
}

////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////

vk::UniqueDevice CreateVulkanDevice(
    vk::PhysicalDevice const& physicalDevice,
    std::vector<uint32_t> const& queueFamilyIndices,
    std::vector<char const*> const& extensions,
    vk::PhysicalDeviceFeatures const* features) {
  std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
  float queuePriority = 1.0f;
  for (auto const& queueFamilyIndex : queueFamilyIndices) {
    deviceQueueCreateInfos.push_back({{}, queueFamilyIndex, 1, &queuePriority});
  }

  return physicalDevice.createDeviceUnique(
      {{}, deviceQueueCreateInfos, {}, extensions, features});
}

// TODO: Move to a utils file if needed elsewhere
template <class T>
inline constexpr const T& Clamp(const T& value, const T& low, const T& high) {
  return value < low ? low : high < value ? high : value;
}

///////////////////////////////////////////////////////////////////////////
// These functions are used to create the swapchain
// TODO: check these over to make sure they make sense
///////////////////////////////////////////////////////////////////////////

vk::Extent2D SetExtent(vk::Extent2D const& windowExtent,
                       vk::SurfaceCapabilitiesKHR const& capabilities) {
  auto extent{capabilities.currentExtent};
  if (capabilities.currentExtent.width ==
      std::numeric_limits<uint32_t>::max()) {
    extent.width = Clamp(windowExtent.width, capabilities.minImageExtent.width,
                         capabilities.maxImageExtent.width);
    extent.height =
        Clamp(windowExtent.height, capabilities.minImageExtent.height,
              capabilities.maxImageExtent.height);
  }
  return extent;
}

uint32_t SelectImageCount(vk::SurfaceCapabilitiesKHR const& capabilities) {
  return capabilities.maxImageCount != 0 &&
                 capabilities.minImageCount > capabilities.maxImageCount
             ? capabilities.maxImageCount
             : capabilities.minImageCount + 1;
}

vk::SurfaceTransformFlagBitsKHR SelectPreTransform(
    vk::SurfaceCapabilitiesKHR const& capabilities) {
  return (capabilities.supportedTransforms &
          vk::SurfaceTransformFlagBitsKHR::eIdentity)
             ? vk::SurfaceTransformFlagBitsKHR::eIdentity
             : capabilities.currentTransform;
}

vk::CompositeAlphaFlagBitsKHR SelectCompositeAlpha(
    vk::SurfaceCapabilitiesKHR const& capabilities) {
  return (capabilities.supportedCompositeAlpha &
          vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
             ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
         : (capabilities.supportedCompositeAlpha &
            vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
             ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
         : (capabilities.supportedCompositeAlpha &
            vk::CompositeAlphaFlagBitsKHR::eInherit)
             ? vk::CompositeAlphaFlagBitsKHR::eInherit
             : vk::CompositeAlphaFlagBitsKHR::eOpaque;
}

vk::PresentModeKHR SelectPresentMode(vk::PhysicalDevice const& device,
                                     vk::UniqueSurfaceKHR const& surface) {
  auto presentModes = device.getSurfacePresentModesKHR(surface.get());
  for (const auto& presentMode : presentModes) {
    if (presentMode == vk::PresentModeKHR::eMailbox) {
      return vk::PresentModeKHR::eMailbox;
    }
  }
  return vk::PresentModeKHR::eFifo;
}

///////////////////////////////////////////////////////////////////////////