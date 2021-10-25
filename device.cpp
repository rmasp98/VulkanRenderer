
#include "device.hpp"

#include <atomic>
#include <iostream>
#include <limits>
#include <memory>

#include "pipeline.hpp"
#include "vulkan/vulkan.hpp"

std::vector<char const*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

Device::Device(vk::PhysicalDevice const& physicalDevice,
               vk::SurfaceFormatKHR const& surfaceFormat,
               QueueFamilies const& queueFamilies,
               vk::UniqueSurfaceKHR const& surface, vk::Extent2D& extent)
    : physicalDevice_(physicalDevice),
      surfaceFormat_(surfaceFormat),
      device_(CreateDevice(physicalDevice_, queueFamilies.UniqueIndices(),
                           extensions, {})),
      queues_(device_, queueFamilies),
      commandPool_(
          device_->createCommandPoolUnique({{}, queueFamilies.Graphics()})),
      swapchain_(CreateSwapchain(surface, extent)),
      numSwapchainImages_(
          device_->getSwapchainImagesKHR(swapchain_.get()).size()),
      descriptorPool_(CreateDescriptorPool()),
      semaphores_(device_, numSwapchainImages_),
      allocator_(std::make_shared<MemoryAllocator>(
          physicalDevice.getMemoryProperties())) {}

void Device::RecreateSwapchain(vk::UniqueSurfaceKHR const& surface,
                               vk::Extent2D& extent) {
  swapchain_ = CreateSwapchain(surface, extent, swapchain_);
  numSwapchainImages_ = device_->getSwapchainImagesKHR(swapchain_.get()).size();
}

vk::UniquePipelineLayout Device::CreatePipelineLayout(
    vk::PipelineLayoutCreateInfo const& settings) const {
  return device_->createPipelineLayoutUnique(settings);
}

vk::UniqueRenderPass Device::CreateRenderpass(
    vk::RenderPassCreateInfo const& settings) const {
  auto attachments = *settings.pAttachments;
  attachments.format = surfaceFormat_.format;
  auto updateSetting{settings};
  updateSetting.setAttachments(attachments);
  return device_->createRenderPassUnique(updateSetting);
}

vk::UniquePipelineCache Device::CreatePipelineCache(
    vk::PipelineCacheCreateInfo const& settings) const {
  return device_->createPipelineCacheUnique(settings);
}

vk::UniqueDescriptorSetLayout Device::CreateDescriptorSetLayout(
    vk::DescriptorSetLayoutCreateInfo const& settings) const {
  return device_->createDescriptorSetLayoutUnique(settings);
}

vk::UniquePipeline Device::CreatePipeline(
    vk::UniquePipelineCache const& cache,
    vk::GraphicsPipelineCreateInfo const& settings) const {
  auto result = device_->createGraphicsPipelineUnique(cache.get(), settings);
  // TODO: check result is ok
  return std::move(result.value);
}

std::vector<Framebuffer> Device::CreateFramebuffers(
    vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent,
    vk::ComponentMapping const& componentMapping,
    vk::ImageSubresourceRange const& subResourceRange) const {
  std::vector<Framebuffer> framebuffers;
  for (auto& image : device_->getSwapchainImagesKHR(swapchain_.get())) {
    auto imageView = device_->createImageViewUnique({{},
                                                     image,
                                                     vk::ImageViewType::e2D,
                                                     surfaceFormat_.format,
                                                     componentMapping,
                                                     subResourceRange});
    framebuffers.push_back({device_, renderPass, extent, std::move(imageView)});
  }
  return framebuffers;
}

std::unordered_map<vk::ShaderStageFlagBits, vk::UniqueShaderModule>
Device::CreateShaderModules(
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> const&
        shaderFiles) const {
  std::unordered_map<vk::ShaderStageFlagBits, vk::UniqueShaderModule>
      shaderModules;
  for (auto& file : shaderFiles) {
    shaderModules.insert(
        {file.first,
         device_->createShaderModuleUnique(
             {{},
              file.second.size(),
              reinterpret_cast<const uint32_t*>(file.second.data())})});
  }
  return shaderModules;
}

uint32_t Device::GetNextImage() {
  auto renderCompleteWaitResult = device_->waitForFences(
      semaphores_.GetRenderCompleteFence(), true, UINT64_MAX);
  if (renderCompleteWaitResult == vk::Result::eTimeout) {
    // TODO: figure out how to handle a timeout
  }

  uint32_t imageIndex;
  auto result = device_->acquireNextImageKHR(
      swapchain_.get(), UINT64_MAX, semaphores_.GetImageAquiredSemaphore(),
      nullptr, &imageIndex);
  if (result != vk::Result::eSuccess) {
    // TODO: check for the other possible "successes" like timeout
    throw std::exception();
  }

  semaphores_.WaitForImageInFlight(device_, imageIndex);
  semaphores_.ResetRenderCompleteFence(device_);

  return imageIndex;
}

void Device::SubmitToGraphics(vk::UniqueCommandBuffer& cmdBuffer) const {
  queues_.SubmitToGraphics(cmdBuffer, semaphores_.GetImageAquiredSemaphore(),
                           semaphores_.GetRenderCompleteSemaphore(),
                           semaphores_.GetRenderCompleteFence());
}

void Device::SubmitToPresent(uint32_t const imageIndex) {
  queues_.SubmitToPresent(imageIndex, swapchain_,
                          semaphores_.GetRenderCompleteSemaphore());
  // TODO: Check it is ok to put this here
  semaphores_.IterateSemaphoreIndex();
}

std::vector<vk::UniqueCommandBuffer> Device::AllocateCommandBuffer(
    vk::CommandBufferLevel bufferLevel, uint32_t numBuffers) const {
  return device_->allocateCommandBuffersUnique(
      {commandPool_.get(), bufferLevel, numBuffers});
}

std::vector<vk::UniqueDescriptorSet> Device::AllocateDescriptorSet(
    vk::DescriptorSetLayout const& layout) const {
  std::vector<vk::DescriptorSetLayout> layouts(numSwapchainImages_, layout);
  return device_->allocateDescriptorSetsUnique(
      {descriptorPool_.get(), layouts});
}

void Device::TransferBuffer(vk::UniqueBuffer const& sourceBuffer,
                            vk::UniqueBuffer const& destBuffer,
                            uint32_t const size) {
  // transfer to memory
  auto cmdBuffer = AllocateCommandBuffer(vk::CommandBufferLevel::ePrimary, 1);
  cmdBuffer[0]->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  vk::BufferCopy copyRegion{0, 0, size};
  cmdBuffer[0]->copyBuffer(sourceBuffer.get(), destBuffer.get(), copyRegion);
  cmdBuffer[0]->end();

  queues_.SubmitToGraphics(cmdBuffer[0]);

  // TODO: remove if we can deallocate in response to completion in callback
  queues_.GraphicsWaitIdle();
}

// TODO: might need to provide some of the flags
std::unique_ptr<BufferRef> Device::UploadBuffer(
    void const* data, uint32_t const size,
    vk::BufferUsageFlags const bufferUsage, bool const optimise) {
  auto buffer = std::make_unique<BufferRef>(device_, allocator_, size,
                                            bufferUsage, optimise);
  UploadBuffer(buffer, data);
  return buffer;
}

void Device::UploadBuffer(std::unique_ptr<BufferRef>& buffer,
                          void const* data) {
  buffer->Upload(
      device_, data,
      [&](vk::UniqueBuffer const& srcBuffer, vk::UniqueBuffer const& dstBuffer,
          uint32_t const size) { TransferBuffer(srcBuffer, dstBuffer, size); });
}

vk::UniqueSwapchainKHR Device::CreateSwapchain(
    vk::UniqueSurfaceKHR const& surface, vk::Extent2D& extent,
    vk::UniqueSwapchainKHR const& oldSwapchain) {
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

  std::vector<uint32_t> queueFamilyIndices;
  if (!queues_.GetQueueFamilies().IsUniqueFamilies()) {
    queueFamilyIndices = queues_.GetQueueFamilies().UniqueIndices();
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    swapChainCreateInfo.queueFamilyIndexCount = 2;
    swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  }

  return device_->createSwapchainKHRUnique(swapChainCreateInfo);
}

// TODO: switch this to a memory allocation later
vk::UniqueDeviceMemory Device::AllocateMemory(
    vk::UniqueBuffer const& buffer, vk::MemoryPropertyFlags const flags) const {
  uint32_t typeIndex = uint32_t(~0);
  auto memoryProperties = physicalDevice_.getMemoryProperties();
  auto memoryRequirements = device_->getBufferMemoryRequirements(buffer.get());
  auto typeBits = memoryRequirements.memoryTypeBits;
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    if ((typeBits & 1) &&
        ((memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)) {
      typeIndex = i;
      break;
    }
    typeBits >>= 1;
  }
  assert(typeIndex != uint32_t(~0));
  return device_->allocateMemoryUnique({memoryRequirements.size, typeIndex});
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

// TODO: do more with features
DeviceFeatures GetDeviceFeatures(vk::PhysicalDevice const& device) {
  DeviceFeatures features = DeviceFeatures::NoFeatures;

  auto deviceFeatures = device.getFeatures();
  if (deviceFeatures.geometryShader) {
    features |= DeviceFeatures::GeometryShader;
  }

  return features;
}

// TODO: do more with score
int CalculateScore(vk::PhysicalDevice const& device,
                   vk::SurfaceFormatKHR const& surfaceFormat,
                   QueueFamilies const& queueFamilies) {
  int score = 0;

  if (!queueFamilies.Complete()) {
    return -1;
  }

  if (surfaceFormat.format == vk::Format::eUndefined) {
    return -1;
  }

  auto properties = device.getProperties();
  if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
    score += 1000;
  }

  return score;
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

std::vector<DeviceDetails> GetSuitableDevices(
    vk::UniqueInstance const& instance, vk::UniqueSurfaceKHR const& surface,
    DeviceFeatures const requiredFeatures) {
  std::vector<DeviceDetails> devices;
  for (auto const& physicalDevice : instance->enumeratePhysicalDevices()) {
    DeviceDetails device{physicalDevice, surface};
    if (requiredFeatures == (requiredFeatures & device.GetFeatures()) &&
        device.GetScore() >= 0) {
      devices.push_back(std::move(device));
    }
  }

  // TODO: Test this
  std::sort(devices.begin(), devices.end(),
            [&](DeviceDetails const& i, DeviceDetails const& j) {
              return i.GetScore() > j.GetScore();
            });

  return devices;
}

vk::UniqueDevice CreateDevice(
    vk::PhysicalDevice const& physicalDevice,
    std::vector<uint32_t> const& queueFamilyIndices,
    std::vector<char const*> const& extensions,
    vk::PhysicalDeviceFeatures const* physicalDeviceFeatures) {
  std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
  float queuePriority = 1.0f;
  for (auto const& queueFamilyIndex : queueFamilyIndices) {
    deviceQueueCreateInfos.push_back({{}, queueFamilyIndex, 1, &queuePriority});
  }
  vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfos, {},
                                        extensions, physicalDeviceFeatures);
  return physicalDevice.createDeviceUnique(deviceCreateInfo);
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

uint32_t SelectImageCount(vk::SurfaceCapabilitiesKHR const& capabilities) {
  return capabilities.maxImageCount != 0 &&
                 capabilities.minImageCount > capabilities.maxImageCount
             ? capabilities.maxImageCount
             : capabilities.minImageCount + 1;
}

vk::Extent2D SetExtent(vk::Extent2D const& windowExtent,
                       vk::SurfaceCapabilitiesKHR const& capabilities) {
  auto extent{windowExtent};
  if (capabilities.currentExtent.width ==
      std::numeric_limits<uint32_t>::max()) {
    extent.width = clamp(extent.width, capabilities.minImageExtent.width,
                         capabilities.maxImageExtent.width);
    extent.height = clamp(extent.height, capabilities.minImageExtent.height,
                          capabilities.maxImageExtent.height);
  }
  return extent;
}
