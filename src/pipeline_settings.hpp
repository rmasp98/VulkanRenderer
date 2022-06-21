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
  std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
  std::vector<vk::VertexInputAttributeDescription> AttributeDescriptions;
  std::vector<vk::VertexInputBindingDescription> BindingDescriptons;

  vk::RenderPassCreateInfo GetRenderPassCreateInfo() const {
    return {{}, ColourAttachment, Subpass, Dependency};
  }

  vk::GraphicsPipelineCreateInfo GetPipelineCreateInfo(
      std::vector<vk::PipelineShaderStageCreateInfo> const& shaderStages,
      vk::UniquePipelineLayout const& layout,
      vk::UniqueRenderPass const& renderPass) const {
    return {{},
            shaderStages,
            &VertexInput,
            &InputAssembly,
            nullptr,
            &ViewportState,
            &Rasterizer,
            &Multisampling,
            &DepthStencil,
            &ColourBlending,
            &DynamicState,
            layout.get(),
            renderPass.get()};
  }

  std::vector<Shader> CreateShaders(DeviceApi const& device) {
    std::vector<Shader> shaders;
    for (auto& shaderDetails : Shaders) {
      shaders.push_back({shaderDetails.first, shaderDetails.second, device});
    }

    for (auto& shader : shaders) {
      DescriptorSetLayouts.push_back(shader.GetLayout());
    }
    LayoutSettings.setSetLayouts(DescriptorSetLayouts);

    return shaders;
  }
};

class PipelineSettings {
 public:
  explicit PipelineSettings(
      std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> const& shaders)
      : shaders_(shaders) {
    // TODO: assert that at least vertex and fragment shader are here?
  }

  template <class VertexStruct>
  void AddVertexLayout() {
    uint32_t binding = attributeDescriptions_.size();
    auto attrDesc = VertexStruct::GetAttributeDetails(binding);
    attributeDescriptions_.insert(attributeDescriptions_.end(),
                                  attrDesc.begin(), attrDesc.end());
    bindingDescriptons_.push_back({binding, sizeof(VertexStruct)});
  }

  ~PipelineSettings() = default;

  VulkanPipelineSettings GetVulkanSettings() const {
    VulkanPipelineSettings settings{
        {},
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
        {},
        attributeDescriptions_,
        bindingDescriptons_,
    };

    settings.VertexInput = {
        {},
        settings.BindingDescriptons,
        settings.AttributeDescriptions,
    };

    settings.Subpass.setColorAttachments(settings.ColourAttachmentRef);

    return settings;
  }

 private:
  std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> shaders_;
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions_;
  std::vector<vk::VertexInputBindingDescription> bindingDescriptons_;
};
