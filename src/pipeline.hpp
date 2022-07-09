#pragma once

#include "command.hpp"
#include "device_api.hpp"
#include "framebuffer.hpp"
#include "pipeline_settings.hpp"
#include "shader.hpp"

using CommandId = uint32_t;

class Pipeline {
 public:
  // TODO: should probably have the commandPool and descriptor pool here
  Pipeline(PipelineSettings const& settings, vk::Extent2D const& extent,
           std::vector<vk::UniqueImageView>&& imageViews, DeviceApi& device)
      : settings_(settings.GetVulkanSettings()),
        shaders_(settings_.CreateShaders(device)),
        descriptorSetLayouts_(shaders_, device),
        layout_(device.CreatePipelineLayout(
            settings_.LayoutSettings, descriptorSetLayouts_.GetLayouts())),
        renderPass_(
            device.CreateRenderpass(settings_.GetRenderPassCreateInfo())),
        cache_(device.CreatePipelineCache({})),
        pipeline_(device.CreatePipeline(
            cache_, settings_.GetPipelineCreateInfo(GetShaderStages(), layout_,
                                                    renderPass_))),
        framebuffers_(device.CreateFramebuffers(
            std::forward<std::vector<vk::UniqueImageView>>(imageViews),
            renderPass_, extent)) {}

  CommandId AddCommand(Command&& command) {
    // TODO: do this properly
    commands_.emplace(0, std::move(command));
    return 0;
  }

  void RecordCommands(ImageIndex const imageIndex, vk::Extent2D const& extent,
                      Queues const& queues, DeviceApi& device) {
    assert(imageIndex < framebuffers_.size());

    for (auto& element : commands_) {
      auto& command = element.second;
      command.Initialise(descriptorSetLayouts_, queues, device);

      command.Record(imageIndex, pipeline_, layout_, renderPass_,
                     framebuffers_[imageIndex], extent, queues, device);
    }
  }

  void Draw(CommandId commandId, uint32_t const imageIndex,
            Queues const& queues) const {
    assert(commands_.contains(commandId));
    commands_.at(commandId).Draw(imageIndex, queues);
  }

  void Recreate(std::vector<vk::UniqueImageView>&& imageViews,
                vk::Extent2D const& extent, DeviceApi const& device) {
    renderPass_ = device.CreateRenderpass(settings_.GetRenderPassCreateInfo());
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
  VulkanPipelineSettings settings_;
  std::vector<Shader> shaders_;
  DescriptorSetLayouts descriptorSetLayouts_;
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
};