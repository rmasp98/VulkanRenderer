#pragma once

#include "command.hpp"
#include "device_api.hpp"
#include "framebuffer.hpp"
#include "pipeline_settings.hpp"
#include "shader.hpp"

using CommandId = uint32_t;

class Pipeline {
 public:
  Pipeline(PipelineSettings const& settings, vk::Format const surfaceFormat,
           vk::Extent2D const& extent,
           std::vector<vk::UniqueImageView>&& imageViews,
           DeviceApi const& device)
      : settings_(settings.GetVulkanSettings()),
        shaders_(settings_.CreateShaders(device)),
        layout_(device.CreatePipelineLayout(settings_.LayoutSettings)),
        renderPass_(device.CreateRenderpass(settings_.GetRenderPassCreateInfo(),
                                            surfaceFormat)),
        cache_(device.CreatePipelineCache({})),
        pipeline_(device.CreatePipeline(
            cache_, settings_.GetPipelineCreateInfo(GetShaderStages(), layout_,
                                                    renderPass_))),
        framebuffers_(device.CreateFramebuffers(
            std::forward<std::vector<vk::UniqueImageView>>(imageViews),
            renderPass_, extent)) {}

  // TODO: need to recreate pipeline/framebuffers whatever else on swapchain
  // reset

  CommandId AddCommand(Command&& command) {
    commands_.emplace(0, std::move(command));
    return 0;
  }

  void RegisterCommands(DeviceApi& device,
                        std::shared_ptr<MemoryAllocator>& allocator,
                        vk::UniqueCommandPool const& commandPool,
                        vk::Extent2D const& extent) {
    for (auto& element : commands_) {
      auto& command = element.second;
      if (!command.IsRegistered()) {
        // TODO: need to get/pass in command pool
        command.Allocate(device, framebuffers_.size(), commandPool);

        auto descriptorSetLayouts = GetDescriptorSetLayouts();
        command.Record(device, pipeline_, renderPass_, framebuffers_,
                       descriptorSetLayouts, extent, allocator);
      }
    }
  }

  void Draw(CommandId commandId, uint32_t const imageIndex,
            Queues const& queues) const {
    assert(commands_.contains(commandId));
    commands_.at(commandId).Draw(imageIndex, queues);
  }

  void Recreate(vk::Format const surfaceFormat,
                std::vector<vk::UniqueImageView>&& imageViews,
                vk::Extent2D const& extent, DeviceApi const& device) {
    renderPass_ = device.CreateRenderpass(settings_.GetRenderPassCreateInfo(),
                                          surfaceFormat);
    pipeline_ = device.CreatePipeline(
        cache_, settings_.GetPipelineCreateInfo(GetShaderStages(), layout_,
                                                renderPass_));
    framebuffers_ = device.CreateFramebuffers(
        std::forward<std::vector<vk::UniqueImageView>>(imageViews), renderPass_,
        extent);

    for (auto& command : commands_) {
      command.second.Unregister();
    }
  }

 private:
  // TODO: do we need settings?
  VulkanPipelineSettings settings_;
  std::vector<Shader> shaders_;
  vk::UniquePipelineLayout layout_;
  vk::UniqueRenderPass renderPass_;
  vk::UniquePipelineCache cache_;
  vk::UniquePipeline pipeline_;
  std::vector<Framebuffer> framebuffers_;
  std::unordered_map<CommandId, Command> commands_;

  std::vector<vk::PipelineShaderStageCreateInfo> GetShaderStages() {
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (auto& shader : shaders_) {
      shaderStages.push_back(shader.GetStage());
    }
    return shaderStages;
  }

  std::unordered_map<vk::ShaderStageFlagBits, vk::DescriptorSetLayout>
  GetDescriptorSetLayouts() {
    std::unordered_map<vk::ShaderStageFlagBits, vk::DescriptorSetLayout>
        descriptorSetLayouts;
    for (auto& shader : shaders_) {
      descriptorSetLayouts.insert({shader.GetType(), shader.GetLayout()});
    }
    return descriptorSetLayouts;
  }
};

// std::vector<Framebuffer> RegisterCommands(std::shared_ptr<Device>& device,
//                                           vk::Extent2D const& extent) {
//   // Might need to free buffers?
//   auto descriptorSetLayouts = GetDescriptorSetLayouts();
//   for (auto& command : commands_) {
//     command.second.Allocate(device, NumBuffers());
//     command.second.Record(device, pipeline_, renderPass_, framebuffers_,
//                           descriptorSetLayouts, extent);
//   }
//   RegisterCommands(device, extent);

//   return framebuffers;
// }