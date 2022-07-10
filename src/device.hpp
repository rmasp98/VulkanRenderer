#pragma once

#include <memory>
#include <vector>

#include "device_api.hpp"
#include "pipeline.hpp"
#include "queues.hpp"

class Device {
 public:
  Device(vk::PhysicalDevice const& physicalDevice,
         QueueFamilies const& queueFamilies,
         vk::UniqueSurfaceKHR const& surface, vk::Extent2D extent,
         vk::SurfaceFormatKHR const& surfaceFormat,
         std::function<void()> const swapchainRecreateCallback)
      : api_(physicalDevice, queueFamilies, surface, surfaceFormat, extent),
        extent_(extent),
        queues_(api_, queueFamilies, api_.GetNumSwapchainImages()),
        swapchainRecreateCallback_(swapchainRecreateCallback) {}

  std::shared_ptr<Pipeline> CreatePipeline(PipelineSettings const& settings) {
    auto imageViews = api_.CreateSwapchainImageViews();
    auto pipeline = std::make_shared<Pipeline>(
        settings, extent_, std::move(imageViews), queues_, api_);
    pipelines_.push_back(pipeline);
    return pipeline;
  }

  void Draw(std::shared_ptr<Pipeline>& pipeline, CommandId const commandId) {
    try {
      auto renderCompleteResult = queues_.WaitForRenderComplete(api_);
      if (renderCompleteResult == vk::Result::eTimeout) {
        // TODO: figure out how to handle a timeout
      }

      // TODO: might be mixing imageIndex with current frame
      auto imageIndex =
          api_.GetNextImageIndex(queues_.GetImageAquiredSemaphore());

      queues_.WaitForImageInFlight(api_, imageIndex);
      queues_.ResetRenderCompleteFence(api_);

      pipeline->RecordCommands(imageIndex, extent_, queues_, api_);
      pipeline->Draw(commandId, imageIndex, queues_);

      queues_.SubmitToPresent(imageIndex, api_.GetSwapchain());
    } catch (vk::OutOfDateKHRError const&) {
      if (swapchainRecreateCallback_) {
        swapchainRecreateCallback_();
      }
    }
  }

  void RecreateSwapchain(vk::UniqueSurfaceKHR const& surface,
                         vk::Extent2D& extent) {
    WaitIdle();
    extent_ = extent;
    api_.RecreateSwapchain(surface, extent_, queues_.GetQueueFamilies());
    queues_.ResizeImagesInFlightFences(api_.GetNumSwapchainImages());

    for (auto& pipeline : pipelines_) {
      auto imageViews = api_.CreateSwapchainImageViews();
      pipeline->Recreate(std::move(imageViews), extent_, api_);
    }
  }

  void WaitIdle() const { api_.WaitIdle(); }

 private:
  DeviceApi api_;
  vk::Extent2D extent_;
  Queues queues_;
  std::vector<std::shared_ptr<class Pipeline>> pipelines_;
  std::function<void()> swapchainRecreateCallback_;
};