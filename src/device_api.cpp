
#include "device_api.hpp"

#include <limits>

vk::UniqueSwapchainKHR DeviceApi::CreateSwapchain(
    vk::UniqueSurfaceKHR const& surface, vk::Extent2D& extent,
    std::vector<uint32_t> const& queueFamilyIndices,
    bool const uniqueQueueFamilies,
    vk::UniqueSwapchainKHR const& oldSwapchain) const {
  auto capabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface.get());
  extent = SetExtent(extent, capabilities);
  auto surfaceFormat = SelectSurfaceFormat(physicalDevice_, surface);
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment |
                              vk::ImageUsageFlagBits::eTransferSrc;

  vk::SwapchainCreateInfoKHR swapChainCreateInfo(
      {}, surface.get(), SelectImageCount(capabilities), surfaceFormat.format,
      surfaceFormat.colorSpace, extent, 1, usage, vk::SharingMode::eExclusive,
      {}, SelectPreTransform(capabilities), SelectCompositeAlpha(capabilities),
      SelectPresentMode(physicalDevice_, surface), true, oldSwapchain.get());

  if (!uniqueQueueFamilies) {
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    swapChainCreateInfo.setQueueFamilyIndices(queueFamilyIndices);
  }

  return device_->createSwapchainKHRUnique(swapChainCreateInfo);
}

uint32_t DeviceApi::GetNumSwapchainImages(
    vk::UniqueSwapchainKHR const& swapchain) const {
  return device_->getSwapchainImagesKHR(swapchain.get()).size();
}

uint32_t DeviceApi::GetNextImageIndex(vk::UniqueSwapchainKHR const& swapchain,
                                      vk::Semaphore const& semaphore) {
  uint32_t imageIndex;
  auto result = device_->acquireNextImageKHR(swapchain.get(), UINT64_MAX,
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
  return device_->createCommandPoolUnique({{}, graphicsFamilyIndex});
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
  return device_->createDescriptorSetLayoutUnique(settings);
}

vk::UniquePipelineLayout DeviceApi::CreatePipelineLayout(
    vk::PipelineLayoutCreateInfo const& settings) const {
  return device_->createPipelineLayoutUnique(settings);
}

vk::UniquePipelineCache DeviceApi::CreatePipelineCache(
    vk::PipelineCacheCreateInfo const& settings) const {
  return device_->createPipelineCacheUnique(settings);
}

vk::UniqueRenderPass DeviceApi::CreateRenderpass(
    vk::RenderPassCreateInfo const& settings,
    vk::Format const surfaceFormat) const {
  auto attachments = *settings.pAttachments;
  attachments.format = surfaceFormat;
  auto updateSetting{settings};
  updateSetting.setAttachments(attachments);
  return device_->createRenderPassUnique(updateSetting);
}

vk::UniquePipeline DeviceApi::CreatePipeline(
    vk::UniquePipelineCache const& cache,
    vk::GraphicsPipelineCreateInfo const& settings) const {
  auto result = device_->createGraphicsPipelineUnique(cache.get(), settings);
  // TODO: check result is ok
  return std::move(result.value);
}

std::vector<vk::UniqueImageView> DeviceApi::CreateImageViews(
    vk::UniqueSwapchainKHR const& swapchain, vk::Format const surfaceFormat,
    vk::ComponentMapping const& componentMapping,
    vk::ImageSubresourceRange const& subResourceRange) {
  std::vector<vk::UniqueImageView> imageViews;
  for (auto& image : device_->getSwapchainImagesKHR(swapchain.get())) {
    imageViews.push_back(device_->createImageViewUnique({{},
                                                         image,
                                                         vk::ImageViewType::e2D,
                                                         surfaceFormat,
                                                         componentMapping,
                                                         subResourceRange}));
  }
  return imageViews;
}

Framebuffer DeviceApi::CreateFramebuffer(vk::UniqueImageView&& imageView,
                                         vk::UniqueRenderPass const& renderPass,
                                         vk::Extent2D const& extent) const {
  auto framebuffer = device_->createFramebufferUnique(
      {{}, renderPass.get(), imageView.get(), extent.width, extent.height, 1});
  return {std::move(imageView), std::move(framebuffer)};
}

std::vector<Framebuffer> DeviceApi::CreateFramebuffers(
    std::vector<vk::UniqueImageView>&& imageViews,
    vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent) const {
  std::vector<Framebuffer> framebuffers;
  for (auto& imageView : imageViews) {
    framebuffers.push_back(
        CreateFramebuffer(std::move(imageView), renderPass, extent));
  }

  return framebuffers;
}

std::vector<vk::UniqueCommandBuffer> DeviceApi::AllocateCommandBuffer(
    vk::CommandBufferLevel bufferLevel, uint32_t numBuffers,
    vk::UniqueCommandPool const& commandPool) const {
  return device_->allocateCommandBuffersUnique(
      {commandPool.get(), bufferLevel, numBuffers});
}

vk::UniqueBuffer DeviceApi::CreateBuffer(
    uint32_t const size, vk::BufferUsageFlags const usage) const {
  return device_->createBufferUnique({{}, size, usage});
}

vk::MemoryRequirements DeviceApi::GetBufferMemoryRequirements(
    vk::UniqueBuffer const& buffer) const {
  return device_->getBufferMemoryRequirements(buffer.get());
}

vk::UniqueDeviceMemory DeviceApi::AllocateMemory(vk::DeviceSize size,
                                                 uint32_t typeIndex) const {
  return device_->allocateMemoryUnique({size, typeIndex});
}

void DeviceApi::BindBufferMemory(vk::UniqueBuffer const& buffer,
                                 vk::UniqueDeviceMemory const& memory,
                                 vk::DeviceSize const offset) const {
  device_->bindBufferMemory(buffer.get(), memory.get(), offset);
}

void* DeviceApi::MapMemory(vk::UniqueDeviceMemory const& memory,
                           vk::DeviceSize const offset,
                           vk::DeviceSize const size) const {
  return device_->mapMemory(memory.get(), offset, size);
}

void DeviceApi::UnmapMemory(vk::UniqueDeviceMemory const& memory) const {
  return device_->unmapMemory(memory.get());
}

////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////

vk::UniqueDevice CreateVulkanDevice(
    vk::PhysicalDevice const& physicalDevice,
    std::vector<uint32_t> const& queueFamilyIndices,
    std::vector<char const*> const& extensions,
    vk::PhysicalDeviceFeatures const* physicalDeviceFeatures) {
  std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
  float queuePriority = 1.0f;
  for (auto const& queueFamilyIndex : queueFamilyIndices) {
    deviceQueueCreateInfos.push_back({{}, queueFamilyIndex, 1, &queuePriority});
  }

  return physicalDevice.createDeviceUnique(
      {{}, deviceQueueCreateInfos, {}, extensions, physicalDeviceFeatures});
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

vk::SurfaceFormatKHR SelectSurfaceFormat(vk::PhysicalDevice const& device,
                                         vk::UniqueSurfaceKHR const& surface) {
  auto formats = device.getSurfaceFormatsKHR(surface.get());
  // Default is undefined
  vk::SurfaceFormatKHR pickedFormat = {};

  std::vector<vk::Format> requestedFormats = {
      vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm,
      vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm};
  vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

  for (size_t i = 0; i < requestedFormats.size(); i++) {
    auto requestedFormat = requestedFormats[i];
    auto it = std::find_if(
        formats.begin(), formats.end(),
        [requestedFormat, requestedColorSpace](vk::SurfaceFormatKHR const& f) {
          return (f.format == requestedFormat) &&
                 (f.colorSpace == requestedColorSpace);
        });
    if (it != formats.end()) {
      pickedFormat = *it;
      break;
    }
  }

  return pickedFormat;
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