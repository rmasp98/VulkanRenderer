#ifndef VULKAN_RENDERER_DEVICE_HPP
#define VULKAN_RENDERER_DEVICE_HPP

#include <memory>
#include <vector>

#include "command.hpp"
#include "device_api.hpp"
#include "handle.hpp"
#include "pipeline.hpp"
#include "queues.hpp"
#include "render_pass.hpp"
#include "semaphores.hpp"

namespace vulkan_renderer {

using RenderPassId = uint32_t;
using RenderPassHandle = Handle<RenderPassId>;
using CommandId = uint32_t;
using CommandHandle = Handle<CommandId>;

class Device {
 public:
  Device(vk::PhysicalDevice const& physicalDevice,
         vk::PhysicalDeviceFeatures const* features,
         QueueFamilies const& queueFamilies, vk::SurfaceKHR const& surface,
         vk::Extent2D extent, vk::SurfaceFormatKHR const& surfaceFormat,
         std::function<void()> const swapchainRecreateCallback)
      : api_(physicalDevice, features, queueFamilies, surface, surfaceFormat,
             extent),
        extent_(extent),
        queues_(api_, queueFamilies),
        renderSemaphores_(api_.GetNumSwapchainImages(), api_),
        commandPool_(api_.CreateCommandPool(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueFamilies.Graphics())),
        swapchainRecreateCallback_(swapchainRecreateCallback) {}

  RenderPassHandle CreateRenderPass(RenderPassSettings settings) {
    settings.Initialise(api_);

    static std::atomic<uint32_t> currentRenderPassId = 0;
    auto renderPassId = currentRenderPassId++;

    renderPasses_.emplace(renderPassId,
                          RenderPass{settings, extent_, queues_, api_});

    // TODO: probably need locks around the erase and emplace above
    return {renderPassId, [&](RenderPassId const id) { RemoveRenderPass(id); }};
  }

  PipelineHandle CreatePipeline(PipelineSettings const& settings,
                                RenderPassHandle const& renderPass) {
    assert(renderPasses_.contains(renderPass.Get()));
    auto pipeline =
        renderPasses_.at(renderPass.Get()).CreatePipeline(settings, api_);

    // We need to initialise all the commands with the new pipeline
    for (auto& [_, command] : commands_) {
      command.UpdateDescriptorSets(
          renderPasses_.at(renderPass.Get()).GetPipeline(pipeline.Get()), api_);
    }

    return pipeline;
  }

  CommandHandle AddCommand(Command&& command) {
    static std::atomic<uint32_t> currentCommandId = 0;
    auto commandId = currentCommandId++;

    command.Allocate(commandPool_.get(), queues_, api_);

    for (auto& [_, renderPass] : renderPasses_) {
      for (auto pipelineId : renderPass.GetPipelineIds()) {
        command.UpdateDescriptorSets(renderPass.GetPipeline(pipelineId), api_);
      }
    }

    commands_.emplace(commandId, std::move(command));
    return {commandId, [&](CommandId const id) { RemoveCommand(id); }};
  }

  void StartRender(RenderPassHandle const& renderPass) {
    assert(renderPasses_.contains(renderPass.Get()));
    currentRenderPass_ = renderPass.Get();

    renderSemaphores_.IterateSemaphores();
    if (renderSemaphores_.WaitForRenderComplete(api_) == vk::Result::eTimeout) {
      // TODO: figure out how to handle a timeout
    }

    // TODO: might be mixing imageIndex with current frame
    auto const& semaphores = renderSemaphores_.GetSemphores();
    try {
      currentImageIndex_ =
          api_.GetNextImageIndex(semaphores.WaitSemaphore.get());

      renderSemaphores_.WaitForImageInFlight(api_, currentImageIndex_);
      renderPassInitialised_ = true;
    } catch (vk::OutOfDateKHRError const&) {
      if (swapchainRecreateCallback_) {
        swapchainRecreateCallback_();
      }
    }
  }

  void Draw(CommandHandle const& command, PipelineHandle const& pipeline) {
    assert(commands_.contains(command.Get()));
    if (!renderPassInitialised_) return;

    auto& currentCommand = commands_.at(command.Get());
    if (currentCommand.IsOutdated(currentImageIndex_)) {
      currentCommand.Record(currentImageIndex_,
                            renderPasses_.at(currentRenderPass_),
                            pipeline.Get(), extent_);
    }

    currentCommand.UploadUniforms(currentImageIndex_, queues_, api_);
    currentCommand.Draw(currentImageIndex_, renderSemaphores_.GetSemphores(),
                        queues_);
  }

  void PresentRender() {
    if (!renderPassInitialised_) return;

    try {
      queues_.SubmitToPresent(
          currentImageIndex_, api_.GetSwapchain(),
          renderSemaphores_.GetSemphores().CompleteSemaphore.get());
    } catch (vk::OutOfDateKHRError const&) {
      if (swapchainRecreateCallback_) {
        swapchainRecreateCallback_();
      }
    }

    renderPassInitialised_ = false;
  }

  void RecreateSwapchain(vk::SurfaceKHR const& surface, vk::Extent2D& extent) {
    WaitIdle();
    extent_ = extent;
    api_.RecreateSwapchain(surface, extent_, queues_.GetQueueFamilies());
    renderSemaphores_.ResizeImagesInFlightFences(api_.GetNumSwapchainImages());

    for (auto& [_, renderPass] : renderPasses_) {
      renderPass.Recreate(extent_, queues_, api_);
    }

    ReinitialiseCommands();
  }

  void WaitIdle() const { api_.WaitIdle(); }

 protected:
  void ReinitialiseCommands() {
    api_.ResetCommandPool(commandPool_);
    for (auto& [_, command] : commands_) {
      command.Allocate(commandPool_.get(), queues_, api_);
      for (auto& [_, renderPass] : renderPasses_) {
        for (auto pipelineId : renderPass.GetPipelineIds()) {
          command.UpdateDescriptorSets(renderPass.GetPipeline(pipelineId),
                                       api_);
        }
      }
    }
  }

  void RemoveCommand(CommandId const id) {
    WaitIdle();
    commands_.erase(id);
  }

  void RemoveRenderPass(RenderPassId const id) {
    WaitIdle();
    ReinitialiseCommands();
    renderPasses_.erase(id);
  }

 private:
  DeviceApi api_;
  vk::Extent2D extent_;
  Queues queues_;
  RenderSemaphores renderSemaphores_;
  std::set<std::shared_ptr<class Pipeline>> pipelines_;
  std::unordered_map<RenderPassId, RenderPass> renderPasses_;
  vk::UniqueCommandPool commandPool_;
  std::unordered_map<CommandId, Command> commands_;
  std::function<void()> swapchainRecreateCallback_;

  bool renderPassInitialised_ = false;
  RenderPassId currentRenderPass_;
  ImageIndex currentImageIndex_;
};

}  // namespace vulkan_renderer

#endif