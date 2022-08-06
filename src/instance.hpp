#ifndef VULKAN_RENDERER_INSTANCE_HPP
#define VULKAN_RENDERER_INSTANCE_HPP

#include <iostream>

#include "debug.hpp"
#include "device.hpp"
#include "queues.hpp"
#include "vulkan/vulkan.hpp"

#define APPLICATION_NAME "Test"
#define APPLICATION_VERSION 1
#define ENGINE_NAME "Crystalline"
#define ENGINE_VERSION 1
#define VULKAN_API_VERSION VK_API_VERSION_1_1

namespace vulkan_renderer {

enum class DeviceFeatures : int {
  NoFeatures = 0,
  GeometryShader = 1 << 0,
  PresentQueue = 1 << 1,
  Anisotropy = 1 << 2,
  SampleShading = 1 << 3,
};

DeviceFeatures operator|(DeviceFeatures lhs, DeviceFeatures rhs);
DeviceFeatures operator|=(DeviceFeatures& lhs, DeviceFeatures rhs);
DeviceFeatures operator&(DeviceFeatures lhs, DeviceFeatures rhs);

class DeviceSpec {
 public:
  DeviceSpec(vk::PhysicalDevice const& device, vk::SurfaceKHR const& surface,
             DeviceFeatures const requiredFeatures);

  bool HasRequiredFeatures() const {
    return requiredFeatures_ == (requiredFeatures_ & GetAvailableFeatures());
  }

  DeviceFeatures GetAvailableFeatures() const;
  int GetScore() const { return score_; }

  std::shared_ptr<Device> CreateDevice(
      vk::SurfaceKHR const& surface, vk::Extent2D& extent,
      std::function<void()> const swapchainRecreateCallback) const;

  // This and has needed to be key of an unordered map
  bool operator==(DeviceSpec const& other) const {
    return device_ == other.device_;
  }

  std::size_t Hash() const { return std::hash<vk::PhysicalDevice>()(device_); }

 protected:
  vk::PhysicalDeviceFeatures GetFeatures() const;

 private:
  vk::PhysicalDevice device_;
  vk::SurfaceFormatKHR surfaceFormat_;
  class QueueFamilies queueFamilies_;
  DeviceFeatures requiredFeatures_;
  int score_;
};

}  // namespace vulkan_renderer

// Needed for DeviceSpec to be key of an unordered map
template <>
struct std::hash<vulkan_renderer::DeviceSpec> {
  std::size_t operator()(vulkan_renderer::DeviceSpec const& spec) const {
    return spec.Hash();
  }
};

namespace vulkan_renderer {

vk::UniqueInstance CreateInstance(std::vector<char const*>& layers,
                                  std::vector<char const*>& extensions,
                                  DebugMessenger const& debugMessenger);

class Instance {
 public:
  template <class Window>
  explicit Instance(Window& window, DebugMessenger&& debugMessenger = {},
                    std::vector<char const*> layers = {},
                    std::vector<char const*> extensions = {})
      : debugMessenger_(std::move(debugMessenger)) {
    auto [width, height] = window.Size();
    extent_ = vk::Extent2D{width, height};
    window.BindResizeCallback([&](uint32_t const width, uint32_t const height) {
      extent_ = vk::Extent2D{width, height};
      RecreateSwapchain();
    });

    auto windowExtensions = window.GetVulkanExtensions();
    extensions.insert(extensions.end(), windowExtensions.begin(),
                      windowExtensions.end());

    instance_ = CreateInstance(layers, extensions, debugMessenger);
    surface_ = window.GetVulkanSurface(instance_);

    debugMessenger_.Initialise(instance_.get());
  }

  std::vector<DeviceSpec> GetSuitableDevices(
      DeviceFeatures const requiredFeatures = DeviceFeatures::NoFeatures) const;

  // Will create the best device that meets the required features
  std::shared_ptr<Device> GetDevice(
      DeviceFeatures const requiredFeatures = DeviceFeatures::NoFeatures);

  std::shared_ptr<Device> GetDevice(DeviceSpec const&);

  void RecreateSwapchain();

 protected:
  vk::UniqueInstance instance_;
  vk::UniqueSurfaceKHR surface_;
  DebugMessenger debugMessenger_;
  vk::Extent2D extent_;
  std::unordered_map<DeviceSpec, std::shared_ptr<Device>> devices_;
};

}  // namespace vulkan_renderer

#endif