#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "vulkan/vulkan.hpp"
#include "vulkan_defaults.hpp"

class Framebuffer {
 public:
  Framebuffer(vk::UniqueDevice const& device,
              vk::UniqueRenderPass const& renderPass,
              vk::Extent2D const& extent, vk::UniqueImageView&& imageView)
      : imageView_(std::move(imageView)),
        framebuffer_(device->createFramebufferUnique({{},
                                                      renderPass.get(),
                                                      imageView_.get(),
                                                      extent.width,
                                                      extent.height,
                                                      1})) {}
  ~Framebuffer() = default;

  Framebuffer(Framebuffer const& other) = delete;
  Framebuffer& operator=(Framebuffer const& other) = delete;
  Framebuffer(Framebuffer&& other) = default;
  Framebuffer& operator=(Framebuffer&& other) = default;

  vk::Framebuffer GetFramebuffer() const { return framebuffer_.get(); }

 private:
  vk::UniqueImageView imageView_;
  vk::UniqueFramebuffer framebuffer_;
};

enum class DeviceFeatures : int {
  NoFeatures = 0,
  GeometryShader = 1 << 0,
  PresentQueue = 1 << 1
};

inline DeviceFeatures operator|(DeviceFeatures lhs, DeviceFeatures rhs) {
  return static_cast<DeviceFeatures>(
      static_cast<std::underlying_type<DeviceFeatures>::type>(lhs) |
      static_cast<std::underlying_type<DeviceFeatures>::type>(rhs));
}

inline DeviceFeatures operator|=(DeviceFeatures& lhs, DeviceFeatures rhs) {
  lhs = static_cast<DeviceFeatures>(
      static_cast<std::underlying_type<DeviceFeatures>::type>(lhs) |
      static_cast<std::underlying_type<DeviceFeatures>::type>(rhs));
  return lhs;
}

inline DeviceFeatures operator&(DeviceFeatures lhs, DeviceFeatures rhs) {
  return static_cast<DeviceFeatures>(
      static_cast<std::underlying_type<DeviceFeatures>::type>(lhs) &
      static_cast<std::underlying_type<DeviceFeatures>::type>(rhs));
}

struct AvailableDevice {
  AvailableDevice(vk::PhysicalDevice const& device,
                  DeviceFeatures const features, int score)
      : Device(device), Features(features), Score(score) {}

  vk::PhysicalDevice Device;
  DeviceFeatures Features;
  int Score;
};

class Semaphores {
 public:
  Semaphores(vk::UniqueDevice const& device, uint32_t numImages) {
    for (uint32_t i = 0; i < defaults::MaxFramesInFlight; ++i) {
      imageAcquired_.push_back(device->createSemaphoreUnique({}));
      renderComplete_.push_back(device->createSemaphoreUnique({}));
      renderCompleteFences_.push_back(
          device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled}));
    }

    imagesInFlight_.resize(numImages);
  }

  vk::Semaphore GetImageAquiredSemaphore() const {
    return imageAcquired_[semaphoreIndex_].get();
  }

  vk::Semaphore GetRenderCompleteSemaphore() const {
    return renderComplete_[semaphoreIndex_].get();
  }

  vk::Fence GetRenderCompleteFence() const {
    return renderCompleteFences_[semaphoreIndex_].get();
  }

  void ResetRenderCompleteFence(vk::UniqueDevice const& device) {
    device->resetFences(renderCompleteFences_[semaphoreIndex_].get());
  }

  vk::Result WaitForImageInFlight(vk::UniqueDevice const& device,
                                  uint32_t imageIndex) {
    if (imagesInFlight_[imageIndex]) {
      auto imageWaitResult =
          device->waitForFences(imagesInFlight_[imageIndex], true, UINT64_MAX);
      if (imageWaitResult != vk::Result::eSuccess) {
        return imageWaitResult;
      }
    }
    imagesInFlight_[imageIndex] = renderCompleteFences_[semaphoreIndex_].get();
    return vk::Result::eSuccess;
  }

  void IterateSemaphoreIndex() {
    semaphoreIndex_ = (semaphoreIndex_ + 1) % defaults::MaxFramesInFlight;
  }

 private:
  std::vector<vk::UniqueSemaphore> imageAcquired_;
  std::vector<vk::UniqueSemaphore> renderComplete_;
  std::vector<vk::UniqueFence> renderCompleteFences_;
  std::vector<vk::Fence> imagesInFlight_;
  uint32_t semaphoreIndex_ = 0;
};

vk::UniqueInstance CreateInstance(std::string const& appName,
                                  std::vector<char const*> const& layers,
                                  std::vector<char const*>& extensions,
                                  uint32_t const apiVersion);

vk::UniqueDebugUtilsMessengerEXT CreateDebugUtilsMessenger(
    vk::UniqueInstance const&);

template <class T>
inline constexpr const T& clamp(const T& value, const T& low, const T& high) {
  return value < low ? low : high < value ? high : value;
}

std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStages(
    vk::UniqueDevice const& device,
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> const&
        files);

std::vector<char> LoadShader(char const* filename);
std::vector<uint32_t> LoadShader2(char const* filename);
