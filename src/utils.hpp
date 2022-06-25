#pragma once

#include <set>
#include <vector>

#include "vulkan/vulkan.hpp"

class QueueFamilies {
 public:
  QueueFamilies(vk::PhysicalDevice const& device,
                vk::UniqueSurfaceKHR const& surface) {
    uint32_t familyIndex = 0;
    for (auto const& family : device.getQueueFamilyProperties()) {
      if (graphics_ == -1 &&
          (family.queueFlags & vk::QueueFlagBits::eGraphics)) {
        graphics_ = familyIndex;
      }

      if (present_ == -1 &&
          device.getSurfaceSupportKHR(familyIndex, surface.get())) {
        present_ = familyIndex;
      }

      ++familyIndex;
      if (graphics_ != -1 && present_ != -1) return;
    }
  }

  uint32_t Graphics() const { return graphics_; }
  uint32_t Present() const { return present_; }

  bool Complete() const { return graphics_ != -1 && present_ != -1; }
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