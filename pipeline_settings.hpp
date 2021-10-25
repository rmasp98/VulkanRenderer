#pragma once

#include <unordered_map>

#include "vulcan_defaults.hpp"
#include "vulkan/vulkan.hpp"

struct PipelineSettings {
  PipelineSettings(std::unordered_map<vk::ShaderStageFlagBits,
                                      std::vector<char>> const& shaderFiles)
      : ShaderFiles(shaderFiles) {}

  ~PipelineSettings() = default;

  // TODO: create setter functions for pipeline functionality

  vk::PipelineVertexInputStateCreateInfo VertexInput =
      defaults::pipeline::VertexInput;

  vk::PipelineInputAssemblyStateCreateInfo InputAssembly =
      defaults::pipeline::InputAssembly;

  vk::PipelineViewportStateCreateInfo ViewportState =
      defaults::pipeline::ViewportState;

  vk::PipelineRasterizationStateCreateInfo Rasterizer =
      defaults::pipeline::Rasterizer;

  vk::PipelineMultisampleStateCreateInfo Multisampling_ =
      defaults::pipeline::Multisampling;

  vk::PipelineDepthStencilStateCreateInfo DepthStencil =
      defaults::pipeline::DepthStencil;

  vk::PipelineColorBlendAttachmentState ColourBlendAttachment =
      defaults::pipeline::ColourBlendAttachment;

  vk::PipelineColorBlendStateCreateInfo ColourBlending =
      defaults::pipeline::ColourBlending;

  std::array<vk::DynamicState, 2> DynamicStates =
      defaults::pipeline::DynamicStates;

  vk::PipelineDynamicStateCreateInfo DynamicState =
      defaults::pipeline::DynamicState;

  // Layout
  vk::PipelineLayoutCreateInfo LayoutSettings =
      defaults::pipeline::LayoutCreateInfo;

  // RenderPass
  vk::AttachmentDescription ColourAttachment =
      defaults::pipeline::ColourAttachment;
  vk::SubpassDescription Subpass = defaults::pipeline::Subpass;
  vk::SubpassDependency Dependency = defaults::pipeline::Dependency;

  vk::AttachmentReference ColourAttachmentRef =
      defaults::pipeline::ColourAttachmentRef;

  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> ShaderFiles;
};
