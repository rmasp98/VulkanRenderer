#ifndef VULKAN_RENDERER_SEMAPHORES_HPP
#define VULKAN_RENDERER_SEMAPHORES_HPP

#include "defaults.hpp"
#include "device_api.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

struct Semaphores {
  vk::UniqueSemaphore WaitSemaphore;
  vk::UniqueSemaphore CompleteSemaphore;
  vk::UniqueFence CompleteFence;
};

class RenderSemaphores {
 public:
  RenderSemaphores(uint32_t const numSwapchainImages, DeviceApi const& device) {
    for (uint32_t i = 0; i < defaults::MaxFramesInFlight; ++i) {
      semaphores_.push_back(
          {device.CreateSemaphore(), device.CreateSemaphore(),
           device.CreateFence({vk::FenceCreateFlagBits::eSignaled})});
    }

    imagesInFlight_.resize(numSwapchainImages);
  }

  Semaphores const& GetSemphores() const {
    return semaphores_[semaphoreIndex_];
  }

  vk::Result WaitForImageInFlight(DeviceApi const& device,
                                  uint32_t imageIndex) {
    assert(imageIndex < imagesInFlight_.size());
    if (imagesInFlight_[imageIndex]) {
      auto imageWaitResult =
          device.WaitForFences({imagesInFlight_[imageIndex]});
      if (imageWaitResult != vk::Result::eSuccess) {
        return imageWaitResult;
      }
    }

    auto const& completeFence = GetSemphores().CompleteFence.get();
    device.ResetFences({completeFence});
    imagesInFlight_[imageIndex] = completeFence;
    return vk::Result::eSuccess;
  }

  vk::Result WaitForRenderComplete(DeviceApi const& device) const {
    return device.WaitForFences({GetSemphores().CompleteFence.get()});
  }

  void ResizeImagesInFlightFences(uint32_t const numSwapchainImages) {
    imagesInFlight_.resize(numSwapchainImages);
  }

  void IterateSemaphores() {
    semaphoreIndex_ = (semaphoreIndex_ + 1) % defaults::MaxFramesInFlight;
  }

 private:
  std::vector<Semaphores> semaphores_;
  std::vector<vk::Fence> imagesInFlight_;
  uint32_t semaphoreIndex_ = 0;
};

}  // namespace vulkan_renderer

#endif