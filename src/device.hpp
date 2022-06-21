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
      : api_(physicalDevice, queueFamilies.UniqueIndices()),
        extent_(extent),
        surfaceFormat_(surfaceFormat),
        swapchain_(api_.CreateSwapchain(surface, extent,
                                        queueFamilies.UniqueIndices(),
                                        queueFamilies.IsUniqueFamilies(), {})),
        queues_(api_, queueFamilies, api_.GetNumSwapchainImages(swapchain_)),
        allocator_(std::make_shared<MemoryAllocator>(
            physicalDevice.getMemoryProperties())),
        commandPool_(api_.CreateCommandPool(queueFamilies.Graphics())),
        swapchainRecreateCallback_(swapchainRecreateCallback) {}

  std::shared_ptr<Pipeline> CreatePipeline(PipelineSettings const& settings) {
    auto imageViews = api_.CreateImageViews(swapchain_, surfaceFormat_.format);
    auto pipeline = std::make_shared<Pipeline>(
        settings, surfaceFormat_.format, extent_, std::move(imageViews), api_);
    pipelines_.push_back(pipeline);
    return pipeline;
  }

  void Draw(std::shared_ptr<Pipeline>& pipeline, CommandId const commandId) {
    try {
      auto renderCompleteResult = queues_.WaitForRenderComplete(api_);
      if (renderCompleteResult == vk::Result::eTimeout) {
        // TODO: figure out how to handle a timeout
      }

      auto imageIndex = api_.GetNextImageIndex(
          swapchain_, queues_.GetImageAquiredSemaphore());

      queues_.WaitForImageInFlight(api_, imageIndex);
      queues_.ResetRenderCompleteFence(api_);

      pipeline->RegisterCommands(api_, allocator_, commandPool_, extent_);
      pipeline->Draw(commandId, imageIndex, queues_);

      queues_.SubmitToPresent(imageIndex, swapchain_);
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
    swapchain_ = api_.CreateSwapchain(
        surface, extent_, queues_.GetQueueFamilies().UniqueIndices(),
        queues_.GetQueueFamilies().IsUniqueFamilies(), swapchain_);
    queues_.ResizeImagesInFlightFences(api_.GetNumSwapchainImages(swapchain_));

    for (auto& pipeline : pipelines_) {
      auto imageViews =
          api_.CreateImageViews(swapchain_, surfaceFormat_.format);
      pipeline->Recreate(surfaceFormat_.format, std::move(imageViews), extent_,
                         api_);
    }
  }

  void WaitIdle() const { api_.WaitIdle(); }

 private:
  DeviceApi api_;
  vk::Extent2D extent_;
  vk::SurfaceFormatKHR surfaceFormat_;
  vk::UniqueSwapchainKHR swapchain_;
  Queues queues_;
  std::shared_ptr<MemoryAllocator> allocator_;
  vk::UniqueCommandPool commandPool_;
  std::vector<std::shared_ptr<class Pipeline>> pipelines_;
  std::function<void()> swapchainRecreateCallback_;
};