#ifndef VULKAN_RENDERER_PIPELINE_HPP
#define VULKAN_RENDERER_PIPELINE_HPP

#include "buffers/uniform_buffer.hpp"
#include "descriptor_sets.hpp"
#include "device_api.hpp"
#include "handle.hpp"
#include "render_settings.hpp"
#include "shader.hpp"

namespace vulkan_renderer {

using PipelineId = uint32_t;
using PipelineHandle = Handle<PipelineId>;

class Pipeline {
 public:
  // TODO: should probably have the descriptor pool here
  Pipeline(PipelineId const id, PipelineSettings const& settings,
           vk::RenderPass const& renderPass, DeviceApi& device)
      : id_(id),
        settings_(settings),
        shaders_(settings_.CreateShaders(device)),
        descriptorSetLayouts_(CreateDescriptorSetLayouts(shaders_, device)),
        layout_(
            device.CreatePipelineLayout(settings_.GetPipelineLayoutCreateInfo(
                GetLayouts(), GetPushConstants()))),
        cache_(device.CreatePipelineCache({})),
        pipeline_(device.CreatePipeline(
            cache_, settings_.GetPipelineCreateInfo(
                        GetShaderStages(), layout_.get(), renderPass))) {}

  PipelineId GetId() const { return id_; }

  DescriptorSets CreateDescriptorSets(DeviceApi const& device) const {
    return {descriptorSetLayouts_, device};
  }

  void UploadPushConstants(std::shared_ptr<PushConstant> const& pushConstant,
                           vk::CommandBuffer const& cmdBuffer) const {
    pushConstant->Upload(layout_.get(), cmdBuffer);
  }

  void Bind(vk::CommandBuffer const& cmdBuffer) const {
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.get());
  }

  void BindDescriptorSet(ImageIndex const imageIndex,
                         DescriptorSets const& descriptorSets,
                         vk::CommandBuffer const& cmdBuffer) const {
    descriptorSets.Bind(imageIndex, cmdBuffer, layout_.get());
  }

  void Recreate(vk::RenderPass const& renderPass, DeviceApi const& device) {
    pipeline_ = device.CreatePipeline(
        cache_, settings_.GetPipelineCreateInfo(GetShaderStages(),
                                                layout_.get(), renderPass));
  }

 protected:
  // TODO: move outside class
  std::vector<vk::DescriptorSetLayout> GetLayouts() const {
    std::vector<vk::DescriptorSetLayout> layouts;
    for (auto& [_, set] : descriptorSetLayouts_) {
      layouts.push_back(set.Layout.get());
    }
    return layouts;
  }

  // TODO: move outside class
  std::unordered_map<uint32_t, DescriptorSetLayout> CreateDescriptorSetLayouts(
      std::vector<Shader> const& shaders, DeviceApi const& device) {
    auto combinedSetBindings = GetCombinedBindingsFromShaders(shaders);

    std::unordered_map<uint32_t, DescriptorSetLayout> setLayouts;
    for (auto& [set, bindings] : combinedSetBindings) {
      setLayouts.emplace(set, DescriptorSetLayout{bindings, device});
    }
    return setLayouts;
  }

  // TODO: move outside class
  std::vector<vk::PipelineShaderStageCreateInfo> GetShaderStages() {
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (auto& shader : shaders_) {
      shaderStages.push_back(shader.GetStage());
    }
    return shaderStages;
  }

  // TODO: move outside class
  std::vector<vk::PushConstantRange> GetPushConstants() const {
    std::vector<vk::PushConstantRange> pushConstants;
    for (auto const& shader : shaders_) {
      auto shaderPushConstants = shader.GetPushConstants();
      pushConstants.insert(pushConstants.end(), shaderPushConstants.begin(),
                           shaderPushConstants.end());
    }
    return pushConstants;
  }

 private:
  PipelineId id_;
  PipelineSettings settings_;
  std::vector<Shader> shaders_;
  std::unordered_map<uint32_t, DescriptorSetLayout> descriptorSetLayouts_;
  vk::UniquePipelineLayout layout_;
  vk::UniquePipelineCache cache_;
  vk::UniquePipeline pipeline_;
};

}  // namespace vulkan_renderer

#endif