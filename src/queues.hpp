#ifndef VULKAN_RENDERER_QUEUES_HPP
#define VULKAN_RENDERER_QUEUES_HPP

#include <set>

#include "device_api.hpp"
#include "semaphores.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

// TODO: do we want to add a transfer queue?
class Queues {
 public:
  Queues(DeviceApi const& device, QueueFamilies const& families)
      : families_(families),
        graphicsQueue_(device.GetQueue(families.Graphics(), 0)),
        presentQueue_(device.GetQueue(families.Present(), 0)) {}

  void SubmitToGraphics(vk::SubmitInfo const& submitInfo,
                        vk::Fence const& completeFence) const {
    graphicsQueue_.submit(submitInfo, completeFence);
  }

  void SubmitToGraphics(vk::CommandBuffer const& buffer) const {
    vk::SubmitInfo submitInfo({}, {}, buffer, {});

    graphicsQueue_.submit(submitInfo, {});
  }

  void SubmitToPresent(uint32_t const imageIndex,
                       vk::SwapchainKHR const& swapchain,
                       vk::Semaphore const& renderCompleteSemaphore) {
    auto result = presentQueue_.presentKHR(
        {renderCompleteSemaphore, swapchain, imageIndex});

    if (result == vk::Result::eSuboptimalKHR) {
      throw vk::OutOfDateKHRError("Suboptimal swapchain");
    } else if (result != vk::Result::eSuccess) {
      // TODO: check other "successful" respsonses such as timeout
      throw std::exception();
    }
  }

  void PresentWaitIdle() const { presentQueue_.waitIdle(); }
  void GraphicsWaitIdle() const { graphicsQueue_.waitIdle(); }

  QueueFamilies GetQueueFamilies() const { return families_; }

 private:
  QueueFamilies families_;
  vk::Queue graphicsQueue_;
  vk::Queue presentQueue_;
};

}  // namespace vulkan_renderer

#endif