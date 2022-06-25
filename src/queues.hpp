#pragma once

#include <set>

#include "device_api.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan_defaults.hpp"

class Queues {
 public:
  Queues(DeviceApi const& device, QueueFamilies const& families,
         uint32_t const numSwapchainImages)
      : families_(families),
        graphicsQueue_(device.GetQueue(families.Graphics(), 0)),
        presentQueue_(device.GetQueue(families.Present(), 0)) {
    for (uint32_t i = 0; i < defaults::MaxFramesInFlight; ++i) {
      imageAcquired_.push_back(device.CreateSemaphore({}));
      renderComplete_.push_back(device.CreateSemaphore({}));
      renderCompleteFences_.push_back(
          device.CreateFence({vk::FenceCreateFlagBits::eSignaled}));
    }

    imagesInFlight_.resize(numSwapchainImages);
  }

  void SubmitToGraphics(vk::UniqueCommandBuffer const& buffer) const {
    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submitInfo(GetImageAquiredSemaphore(),
                              waitDestinationStageMask, buffer.get(),
                              GetRenderCompleteSemaphore());
    graphicsQueue_.submit(submitInfo, GetRenderCompleteFence());
  }

  // TODO: might need to rename to nowait
  void SubmitToGraphics(vk::UniqueCommandBuffer& buffer) const {
    vk::SubmitInfo submitInfo({}, {}, buffer.get(), {});
    graphicsQueue_.submit(submitInfo);
  }

  void SubmitToPresent(uint32_t imageIndex,
                       vk::UniqueSwapchainKHR const& swapchain) {
    // TODO: Find out why we can't pass const
    auto result = presentQueue_.presentKHR(
        {GetRenderCompleteSemaphore(), swapchain.get(), imageIndex});
    // TODO: Check it is ok to put this here
    semaphoreIndex_ = (semaphoreIndex_ + 1) % defaults::MaxFramesInFlight;
    if (result == vk::Result::eSuboptimalKHR) {
      throw vk::OutOfDateKHRError("Suboptimal swapchain");
    } else if (result != vk::Result::eSuccess) {
      // TODO: check other "successful" respsonses such as timeout
      throw std::exception();
    }
  }

  vk::Semaphore const& GetImageAquiredSemaphore() const {
    return imageAcquired_[semaphoreIndex_].get();
  }

  vk::Result WaitForRenderComplete(DeviceApi const& device) const {
    return device.WaitForFences({GetRenderCompleteFence()});
  }

  vk::Result WaitForImageInFlight(DeviceApi const& device,
                                  uint32_t imageIndex) {
    if (imagesInFlight_[imageIndex]) {
      auto imageWaitResult =
          device.WaitForFences({imagesInFlight_[imageIndex]});
      if (imageWaitResult != vk::Result::eSuccess) {
        return imageWaitResult;
      }
    }
    imagesInFlight_[imageIndex] = renderCompleteFences_[semaphoreIndex_].get();
    return vk::Result::eSuccess;
  }

  void ResetRenderCompleteFence(DeviceApi const& device) {
    device.ResetFences({GetRenderCompleteFence()});
  }

  void PresentWaitIdle() const { presentQueue_.waitIdle(); }
  void GraphicsWaitIdle() const { graphicsQueue_.waitIdle(); }

  QueueFamilies GetQueueFamilies() const { return families_; }

  void ResizeImagesInFlightFences(uint32_t const numSwapchainImages) {
    imagesInFlight_.resize(numSwapchainImages);
  }

 private:
  QueueFamilies families_;
  vk::Queue graphicsQueue_;
  vk::Queue presentQueue_;

  std::vector<vk::UniqueSemaphore> imageAcquired_;
  std::vector<vk::UniqueSemaphore> renderComplete_;
  std::vector<vk::UniqueFence> renderCompleteFences_;
  std::vector<vk::Fence> imagesInFlight_;
  uint32_t semaphoreIndex_ = 0;

  vk::Semaphore const& GetRenderCompleteSemaphore() const {
    return renderComplete_[semaphoreIndex_].get();
  }

  vk::Fence const& GetRenderCompleteFence() const {
    return renderCompleteFences_[semaphoreIndex_].get();
  }
};
