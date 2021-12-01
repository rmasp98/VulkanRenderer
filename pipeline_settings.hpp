#pragma once

#include <set>
#include <unordered_map>

#include "shader.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan_defaults.hpp"

struct VulkanPipelineSettings {
  vk::PipelineVertexInputStateCreateInfo VertexInput;
  vk::PipelineInputAssemblyStateCreateInfo InputAssembly;
  vk::PipelineViewportStateCreateInfo ViewportState;
  vk::PipelineRasterizationStateCreateInfo Rasterizer;
  vk::PipelineMultisampleStateCreateInfo Multisampling;
  vk::PipelineDepthStencilStateCreateInfo DepthStencil;
  vk::PipelineColorBlendAttachmentState ColourBlendAttachment;
  vk::PipelineColorBlendStateCreateInfo ColourBlending;
  std::array<vk::DynamicState, 2> DynamicStates;
  vk::PipelineDynamicStateCreateInfo DynamicState;
  // Layout
  vk::PipelineLayoutCreateInfo LayoutSettings;
  // RenderPass
  vk::AttachmentDescription ColourAttachment;
  vk::SubpassDescription Subpass;
  vk::SubpassDependency Dependency;
  vk::AttachmentReference ColourAttachmentRef;
  std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> Shaders;
};

class PipelineSettings {
 public:
  PipelineSettings(
      std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> const& shaders)
      : shaders_(shaders) {
    // TODO: assert that at least vertex and fragment shader are here?
  }

  ~PipelineSettings() = default;

  VulkanPipelineSettings GetVulkanSettings() {
    return {
        defaults::pipeline::VertexInput,
        defaults::pipeline::InputAssembly,
        defaults::pipeline::ViewportState,
        defaults::pipeline::Rasterizer,
        defaults::pipeline::Multisampling,
        defaults::pipeline::DepthStencil,
        defaults::pipeline::ColourBlendAttachment,
        defaults::pipeline::ColourBlending,
        defaults::pipeline::DynamicStates,
        defaults::pipeline::DynamicState,
        defaults::pipeline::LayoutCreateInfo,
        defaults::pipeline::ColourAttachment,
        defaults::pipeline::Subpass,
        defaults::pipeline::Dependency,
        defaults::pipeline::ColourAttachmentRef,
        shaders_,
    };
  }

 private:
  std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> shaders_;
};
