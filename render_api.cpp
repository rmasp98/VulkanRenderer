#include "render_api.hpp"

#include <iostream>
#include <memory>

#include "utils.hpp"

RenderApi::RenderApi(Window& window, std::vector<char const*> layers,
                     std::vector<char const*> extensions,
                     DeviceFeatures const requiredFeatures)
    : instance_(CreateRenderInstance(window, layers, extensions)),
      surface_(window.GetVulkanSurface(instance_)),
      extent_(GetExtent(window)),
      device_(
          GetPrimaryDevice(instance_, surface_, extent_, requiredFeatures)) {
  if (layers.empty()) {
    debugUtilsMessenger_ = CreateDebugUtilsMessenger(instance_);
  }

  window.BindResizeCallback([&](uint32_t width, uint32_t height) {
    extent_ = vk::Extent2D(width, height);
    RecreateSwapchain();
  });
}

vk::UniqueInstance CreateRenderInstance(Window const& window,
                                        std::vector<char const*> layers,
                                        std::vector<char const*> extensions) {
  // TODO: validate optional layers and extensions?
  auto windowExtensions = window.GetVulkanExtensions();
  extensions.insert(extensions.end(), windowExtensions.begin(),
                    windowExtensions.end());

#if !defined(NDEBUG)
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  return CreateInstance("Test", layers, extensions, VK_API_VERSION_1_1);
}

void RenderApi::Draw(uint32_t pipelineId, uint32_t commandId) {
  // Assert pipelines greater than 0
  assert(pipelines_.count(pipelineId) > 0);
  try {
    auto imageIndex = device_->GetNextImage();
    pipelines_.at(pipelineId).Draw(commandId, imageIndex, device_);
    device_->SubmitToPresent(imageIndex);
  } catch (vk::OutOfDateKHRError const& e) {
    RecreateSwapchain();
  } catch (std::exception const& e) {
    // TODO: handle errors I don't know about
  }
}

std::shared_ptr<Device> GetPrimaryDevice(
    vk::UniqueInstance const& instance, vk::UniqueSurfaceKHR const& surface,
    vk::Extent2D& extent, DeviceFeatures const requiredFeatures) {
  auto devices = GetSuitableDevices(instance, surface, requiredFeatures);
  // TODO: Assert devices greater than 0
  return devices[0].CreateDevice(surface, extent);
}

vk::Extent2D GetExtent(Window const& window) {
  auto [width, height] = window.Size();
  return vk::Extent2D(width, height);
}
