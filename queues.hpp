#pragma once

#include <set>

#include "vulkan/vulkan.hpp"

class QueueFamilies {
 public:
  QueueFamilies(vk::PhysicalDevice const& device,
                vk::UniqueSurfaceKHR const& surface) {
    uint32_t familyIndex = 0;
    for (auto const& family : device.getQueueFamilyProperties()) {
      if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
        graphics_ = familyIndex;
        if (present_ >= 0) {
          return;
        }
      }

      if (device.getSurfaceSupportKHR(familyIndex, surface.get())) {
        present_ = familyIndex;
        if (graphics_ >= 0) {
          return;
        }
      }

      ++familyIndex;
    }
  }

  uint32_t Graphics() const { return graphics_; }
  uint32_t Present() const { return present_; }

  bool Complete() const { return graphics_ >= 0 && present_ >= 0; }
  bool IsUniqueFamilies() const { return graphics_ == present_; }

  std::vector<uint32_t> UniqueIndices() const {
    // Removes duplicate indices
    std::set<uint32_t> uniqueSet{static_cast<uint32_t>(graphics_),
                                 static_cast<uint32_t>(present_)};
    return {uniqueSet.begin(), uniqueSet.end()};
  }

 private:
  int graphics_ = -1;
  int present_ = -1;
};

class Queues {
 public:
  Queues(vk::UniqueDevice& device, QueueFamilies const& families)
      : families_(families),
        graphicsQueue_(device->getQueue(families.Graphics(), 0)),
        presentQueue_(device->getQueue(families.Present(), 0)) {}

  void SubmitToGraphics(vk::UniqueCommandBuffer& buffer,
                        vk::Semaphore waitSemaphore,
                        vk::Semaphore signalSemaphore, vk::Fence fence) const {
    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submitInfo(waitSemaphore, waitDestinationStageMask,
                              buffer.get(), signalSemaphore);
    graphicsQueue_.submit(submitInfo, fence);
  }

  void SubmitToGraphics(vk::UniqueCommandBuffer& buffer) const {
    vk::SubmitInfo submitInfo({}, {}, buffer.get(), {});
    graphicsQueue_.submit(submitInfo);
  }

  void SubmitToPresent(uint32_t imageIndex, vk::UniqueSwapchainKHR& swapchain,
                       vk::Semaphore waitSemaphore) const {
    // TODO: Find out why we can't pass const
    auto result =
        presentQueue_.presentKHR({waitSemaphore, swapchain.get(), imageIndex});
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
