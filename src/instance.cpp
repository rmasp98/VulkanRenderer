
#include "instance.hpp"

#include <iostream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace vulkan_renderer {

std::vector<DeviceSpec> Instance::GetSuitableDevices(
    DeviceFeatures const requiredFeatures) const {
  std::vector<DeviceSpec> devices;
  for (auto const& physicalDevice : instance_->enumeratePhysicalDevices()) {
    DeviceSpec const device{physicalDevice, surface_.get(), requiredFeatures};
    if (device.HasRequiredFeatures() && device.GetScore() >= 0) {
      devices.push_back(std::move(device));
    }
  }

  std::sort(devices.begin(), devices.end(),
            [&](DeviceSpec const& i, DeviceSpec const& j) {
              return i.GetScore() > j.GetScore();
            });

  return devices;
}

std::shared_ptr<Device> Instance::GetDevice(
    DeviceFeatures const requiredFeatures) {
  auto devices = GetSuitableDevices(requiredFeatures);
  if (devices.size() > 0) {
    return GetDevice(devices[0]);
  }
  return nullptr;
}

std::shared_ptr<Device> Instance::GetDevice(DeviceSpec const& deviceSpec) {
  if (devices_.contains(deviceSpec)) {
    return devices_.at(deviceSpec);
  }

  auto device = deviceSpec.CreateDevice(surface_.get(), extent_,
                                        [&]() { RecreateSwapchain(); });
  devices_.insert({deviceSpec, device});

  return device;
}

void Instance::RecreateSwapchain() {
  if (extent_.width == 0 || extent_.height == 0) {
    // TODO: could use conditional mutex in renderAPI to stop renderloop
    return;
  }

  for (auto& device : devices_) {
    device.second->RecreateSwapchain(surface_.get(), extent_);
  }
}

vk::UniqueInstance CreateInstance(std::vector<char const*>& layers,
                                  std::vector<char const*>& extensions,
                                  DebugMessenger const& debugMessenger) {
  vk::DynamicLoader loader;
  VULKAN_HPP_DEFAULT_DISPATCHER.init(
      loader.getProcAddress<PFN_vkGetInstanceProcAddr>(
          "vkGetInstanceProcAddr"));

  debugMessenger.AddDebugConfig(layers, extensions);

  vk::ApplicationInfo appInfo(APPLICATION_NAME, APPLICATION_VERSION,
                              ENGINE_NAME, ENGINE_VERSION, VULKAN_API_VERSION);
  vk::InstanceCreateInfo createInfo({}, &appInfo, layers, extensions);
  createInfo.pNext = debugMessenger.GetCreateInfo();

  auto instance = vk::createInstanceUnique(createInfo, nullptr,
                                           VULKAN_HPP_DEFAULT_DISPATCHER);
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
  return instance;
}

vk::SurfaceFormatKHR GetSurfaceFormat(vk::PhysicalDevice const& device,
                                      vk::SurfaceKHR const& surface) {
  auto formats = device.getSurfaceFormatsKHR(surface);

  std::vector<vk::Format> requestedFormats = {
      vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm,
      vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm};
  vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

  for (auto const& requestedFormat : requestedFormats) {
    auto it = std::find_if(formats.begin(), formats.end(),
                           [&](vk::SurfaceFormatKHR const& f) {
                             return (f.format == requestedFormat) &&
                                    (f.colorSpace == requestedColorSpace);
                           });
    if (it != formats.end()) {
      return *it;
    }
  }

  throw std::runtime_error("Could not find a suitable surface format");
}

int GetDeviceScore(vk::PhysicalDevice const& device,
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

  auto sampleCount = properties.limits.framebufferColorSampleCounts &
                     properties.limits.framebufferDepthSampleCounts;
  // TODO: decide multiplier for this property
  if (sampleCount & vk::SampleCountFlagBits::e64)
    score += 64 * 10;
  else if (sampleCount & vk::SampleCountFlagBits::e32)
    score += 32 * 10;
  else if (sampleCount & vk::SampleCountFlagBits::e16)
    score += 16 * 10;
  else if (sampleCount & vk::SampleCountFlagBits::e8)
    score += 8 * 10;
  else if (sampleCount & vk::SampleCountFlagBits::e4)
    score += 4 * 10;
  else if (sampleCount & vk::SampleCountFlagBits::e2)
    score += 2 * 10;

  return score;
}

DeviceSpec::DeviceSpec(vk::PhysicalDevice const& device,
                       vk::SurfaceKHR const& surface,
                       DeviceFeatures const requiredFeatures)
    : device_(device),
      surfaceFormat_(GetSurfaceFormat(device, surface)),
      queueFamilies_(device, surface),
      requiredFeatures_(requiredFeatures),
      score_(GetDeviceScore(device_, surfaceFormat_, queueFamilies_)) {}

// TODO: add all other intesting features
DeviceFeatures DeviceSpec::GetAvailableFeatures() const {
  DeviceFeatures features = DeviceFeatures::NoFeatures;

  auto deviceFeatures = device_.getFeatures();
  if (deviceFeatures.geometryShader) {
    features |= DeviceFeatures::GeometryShader;
  }

  if (deviceFeatures.samplerAnisotropy) {
    features |= DeviceFeatures::Anisotropy;
  }

  if (deviceFeatures.sampleRateShading) {
    features |= DeviceFeatures::SampleShading;
  }

  return features;
}

vk::PhysicalDeviceFeatures DeviceSpec::GetFeatures() const {
  vk::PhysicalDeviceFeatures features;
  if (DeviceFeatures::SampleShading ==
      (requiredFeatures_ & DeviceFeatures::SampleShading)) {
    features.sampleRateShading = true;
  }

  return features;
}

std::shared_ptr<Device> DeviceSpec::CreateDevice(
    vk::SurfaceKHR const& surface, vk::Extent2D& extent,
    std::function<void()> const swapchainRecreateCallback) const {
  auto features = GetFeatures();
  return std::make_shared<Device>(device_, &features, queueFamilies_, surface,
                                  extent, surfaceFormat_,
                                  swapchainRecreateCallback);
}

DeviceFeatures operator|(DeviceFeatures lhs, DeviceFeatures rhs) {
  return static_cast<DeviceFeatures>(
      static_cast<std::underlying_type<DeviceFeatures>::type>(lhs) |
      static_cast<std::underlying_type<DeviceFeatures>::type>(rhs));
}

DeviceFeatures operator|=(DeviceFeatures& lhs, DeviceFeatures rhs) {
  lhs = static_cast<DeviceFeatures>(
      static_cast<std::underlying_type<DeviceFeatures>::type>(lhs) |
      static_cast<std::underlying_type<DeviceFeatures>::type>(rhs));
  return lhs;
}

DeviceFeatures operator&(DeviceFeatures lhs, DeviceFeatures rhs) {
  return static_cast<DeviceFeatures>(
      static_cast<std::underlying_type<DeviceFeatures>::type>(lhs) &
      static_cast<std::underlying_type<DeviceFeatures>::type>(rhs));
}

}