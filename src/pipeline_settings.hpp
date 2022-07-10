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
  std::vector<vk::AttachmentDescription> RenderPassAttachments;
  vk::AttachmentReference ColourAttachmentRef;
  vk::AttachmentReference DepthAttachmentRef;
  vk::SubpassDescription Subpass;
  vk::SubpassDependency Dependency;
  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> Shaders;
  std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts;
  std::vector<vk::VertexInputAttributeDescription> AttributeDescriptions;
  std::vector<vk::VertexInputBindingDescription> BindingDescriptons;

  vk::RenderPassCreateInfo GetRenderPassCreateInfo(
      std::vector<vk::Format> imageFormats) {
    assert(imageFormats.size() == RenderPassAttachments.size());
    for (uint32_t i = 0; i < imageFormats.size(); ++i) {
      RenderPassAttachments[i].format = imageFormats[i];
    }
    return {{}, RenderPassAttachments, Subpass, Dependency};
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

  std::vector<Shader> CreateShaders(DeviceApi& device) const {
    std::vector<Shader> shaders;
    for (auto& shaderDetails : Shaders) {
      shaders.push_back({shaderDetails.first, shaderDetails.second, device});
    }
    return shaders;
  }
};

class PipelineSettings {
 public:
  explicit PipelineSettings(
      std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> const&
          shaders)
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
        {defaults::pipeline::ColourAttachment,
         defaults::pipeline::DepthAttachment},
        defaults::pipeline::ColourAttachmentRef,
        defaults::pipeline::DepthAttachmentRef,
        defaults::pipeline::Subpass,
        defaults::pipeline::Dependency,
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
    settings.Subpass.setPDepthStencilAttachment(&settings.DepthAttachmentRef);

    return settings;
  }

 private:
  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> shaders_;
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions_;
  std::vector<vk::VertexInputBindingDescription> bindingDescriptons_;
};
