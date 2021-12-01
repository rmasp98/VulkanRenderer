#pragma once

#include <utility>

#include "device.hpp"
#include "pipeline.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

// TODO: need to check through all vulkan calls for exception functions

class RenderApi {
 public:
  RenderApi(Window&, std::vector<char const*> layers = {},
            std::vector<char const*> extensions = {},
            DeviceFeatures const requiredFeatures = DeviceFeatures::NoFeatures);

  ~RenderApi() = default;

  std::vector<DeviceDetails> GetDevices(
      DeviceFeatures const requiredFeatures) const {
    return GetSuitableDevices(instance_, surface_, requiredFeatures);
  }

  void SetDevice(DeviceDetails const& deviceDetails) {
    device_ = deviceDetails.CreateDevice(surface_, extent_);
    for (auto& pipeline : pipelines_) {
      pipeline.second.Register(device_, extent_);
    }
  }

  uint32_t RegisterPipeline(PipelineSettings const& settings) {
    Pipeline pipeline{settings};
    if (device_) {
      pipeline.Register(device_, extent_);
    }

    static std::atomic<uint32_t> pipelineTracker{0};
    auto pipelineId = pipelineTracker++;
    pipelines_.emplace(pipelineId, std::move(pipeline));
    return pipelineId;
  }

  uint32_t RegisterCommand(uint32_t pipelineId, Command&& command) {
    assert(pipelines_.count(pipelineId));
    return pipelines_.at(pipelineId)
        .RegisterCommand(device_, std::forward<Command>(command), extent_);
  }

  void Draw(uint32_t pipelineId, uint32_t commandId);

  void WaitIdle() { device_->WaitIdle(); }

 protected:
  vk::UniqueInstance instance_;
  vk::UniqueSurfaceKHR surface_;
  vk::Extent2D extent_;
  std::shared_ptr<Device> device_;
  vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger_;
  // TODO: Change to map with pipeline names
  std::unordered_map<uint32_t, Pipeline> pipelines_;

  void RecreateSwapchain() {
    if (extent_.width == 0 || extent_.height == 0) {
      // TODO: could use conditional mutex in renderAPI to stop renderloop
      return;
    }
    WaitIdle();
    device_->RecreateSwapchain(surface_, extent_);
    for (auto& pipeline : pipelines_) {
      pipeline.second.CreateFramebuffers(device_, extent_);
    }
  }
};

vk::UniqueInstance CreateRenderInstance(Window const& window,
                                        std::vector<char const*> layers,
                                        std::vector<char const*> extensions);

std::shared_ptr<Device> GetPrimaryDevice(vk::UniqueInstance const& instance,
                                         vk::UniqueSurfaceKHR const& surface,
                                         vk::Extent2D& extent,
                                         DeviceFeatures const requiredFeatures);

vk::Extent2D GetExtent(Window const& window);
