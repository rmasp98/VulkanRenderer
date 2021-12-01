#pragma once

#include <memory>

#include "command.hpp"
#include "pipeline_settings.hpp"
#include "queues.hpp"
#include "utils.hpp"
#include "vertex_buffer.hpp"
#include "vulkan/vulkan.hpp"

class Pipeline {
 public:
  Pipeline(PipelineSettings const& settings) : settings_(settings) {}
  ~Pipeline() = default;

  Pipeline(Pipeline const& other) = delete;
  Pipeline& operator=(Pipeline const& other) = delete;
  Pipeline(Pipeline&& other) = default;
  Pipeline& operator=(Pipeline&& other) = default;

  void Register(std::shared_ptr<Device>&, vk::Extent2D const&);

  void CreateFramebuffers(std::shared_ptr<Device>&, vk::Extent2D const&);

  uint32_t NumBuffers() const { return framebuffers_.size(); }

  // TODO: needs to check if command already exists?
  uint32_t RegisterCommand(std::shared_ptr<Device>& device, Command&& command,
                           vk::Extent2D const& extent) {
    auto descriptorSetLayouts = GetDescriptorSetLayouts();
    command.Allocate(device, NumBuffers());
    command.Record(device, pipeline_, renderPass_, framebuffers_,
                   descriptorSetLayouts, extent);
    commands_.emplace(0, std::move(command));
    return 0;
  }

  void Draw(uint32_t commandId, uint32_t const imageIndex,
            std::shared_ptr<Device>& device) {
    assert(commands_.count(commandId) > 0);
    commands_[commandId].Draw(imageIndex, device);
  }

 private:
  PipelineSettings settings_;
  std::vector<Shader> shaders_;
  vk::UniquePipelineLayout layout_;
  // TODO: RenderPass may be independent of pipeline
  vk::UniqueRenderPass renderPass_;
  vk::UniquePipelineCache cache_;
  vk::UniquePipeline pipeline_;
  std::vector<Framebuffer> framebuffers_;
  std::unordered_map<uint32_t, Command> commands_;

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

std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStages(
    std::unordered_map<vk::ShaderStageFlagBits, vk::UniqueShaderModule> const&
        shaderModules);