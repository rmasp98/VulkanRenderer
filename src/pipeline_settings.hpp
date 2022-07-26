#pragma once

#include <unordered_map>

#include "defaults.hpp"
#include "shader.hpp"
#include "vulkan/vulkan.hpp"

struct PipelineSettings {
  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> Shaders;
  vk::PipelineInputAssemblyStateCreateInfo InputAssembly =
      defaults::pipeline::InputAssembly;
  vk::PipelineViewportStateCreateInfo ViewportState =
      defaults::pipeline::ViewportState;
  vk::PipelineRasterizationStateCreateInfo Rasterizer =
      defaults::pipeline::Rasterizer;
  vk::PipelineMultisampleStateCreateInfo Multisampling =
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
  bool EnableDepthBuffer = true;
  vk::AttachmentDescription DepthAttachment =
      defaults::pipeline::DepthAttachment;
  vk::AttachmentDescription ColourResolveAttachment =
      defaults::pipeline::ColourResolveAttachment;
  vk::AttachmentReference ColourAttachmentRef =
      defaults::pipeline::ColourAttachmentRef;
  vk::AttachmentReference ColourResolveAttachmentRef =
      defaults::pipeline::ColourResolveAttachmentRef;
  vk::AttachmentReference DepthAttachmentRef =
      defaults::pipeline::DepthAttachmentRef;
  vk::SubpassDescription Subpass = defaults::pipeline::Subpass;
  vk::SubpassDependency Dependency = defaults::pipeline::Dependency;

  // These are set in the following functions
  vk::PipelineVertexInputStateCreateInfo vertexInput_ = {};
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions_ = {};
  std::vector<vk::VertexInputBindingDescription> bindingDescriptons_ = {};
  std::vector<vk::AttachmentDescription> renderPassAttachments_ = {};
  std::vector<vk::ClearValue> clearValues_;

  template <class VertexStruct>
  void AddVertexLayout() {
    uint32_t binding = attributeDescriptions_.size();
    auto attrDesc = VertexStruct::GetAttributeDetails(binding);
    attributeDescriptions_.insert(attributeDescriptions_.end(),
                                  attrDesc.begin(), attrDesc.end());
    bindingDescriptons_.push_back({binding, sizeof(VertexStruct)});
  }

  std::vector<ImageBuffer> CreateAttachmentBuffers(vk::Extent2D const& extent,
                                                   Queues const& queues,
                                                   DeviceApi& device) {
    auto maxMultiSampleCount = device.GetMaxMultiSamplingCount();
    if (Multisampling.rasterizationSamples > maxMultiSampleCount) {
      Multisampling.rasterizationSamples = maxMultiSampleCount;
      // TODO: log that requested multisampling not available to using x
    }

    std::vector<ImageBuffer> attachmentBuffers;
    if (Multisampling.rasterizationSamples > vk::SampleCountFlagBits::e1) {
      auto surfaceFormat = device.GetSurfaceFormat();
      attachmentBuffers.emplace_back(
          ImageProperties{
              .Extent = {extent.width, extent.height, 1},
              .Usage = vk::ImageUsageFlagBits::eTransientAttachment |
                       vk::ImageUsageFlagBits::eColorAttachment,
              .Format = surfaceFormat,
              .SampleCount = Multisampling.rasterizationSamples},
          queues, device);
      ColourResolveAttachment.format = surfaceFormat;
    }

    if (EnableDepthBuffer) {
      DepthBuffer depthBuffer(extent, Multisampling.rasterizationSamples,
                              queues, device);
      DepthAttachment.format = depthBuffer.GetFormat();
      attachmentBuffers.emplace_back(std::move(depthBuffer));
    }

    return attachmentBuffers;
  }

  vk::RenderPassCreateInfo GetRenderPassCreateInfo(
      vk::Format const surfaceFormat) {
    // Clear because this is called multiple times
    renderPassAttachments_.clear();
    uint32_t attachmentIndex = 0;

    if (Multisampling.rasterizationSamples > vk::SampleCountFlagBits::e1) {
      ColourAttachment.samples = Multisampling.rasterizationSamples;
      DepthAttachment.samples = Multisampling.rasterizationSamples;

      ColourAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

      clearValues_.push_back(
          vk::ClearColorValue(std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f}));
      renderPassAttachments_.push_back(ColourResolveAttachment);
      ColourResolveAttachmentRef.attachment = attachmentIndex++;
      Subpass.setResolveAttachments(ColourResolveAttachmentRef);
    }

    clearValues_.push_back(
        vk::ClearColorValue(std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f}));
    ColourAttachment.format = surfaceFormat;
    renderPassAttachments_.push_back(ColourAttachment);
    ColourAttachmentRef.attachment = attachmentIndex++;
    Subpass.setColorAttachments(ColourAttachmentRef);

    if (EnableDepthBuffer) {
      clearValues_.push_back(vk::ClearDepthStencilValue(1.0f, 0));
      DepthAttachmentRef.attachment = attachmentIndex++;
      renderPassAttachments_.push_back(DepthAttachment);
      Subpass.setPDepthStencilAttachment(&DepthAttachmentRef);
    }

    return {{}, renderPassAttachments_, Subpass, Dependency};
  }

  std::vector<vk::ClearValue> GetClearValues() const { return clearValues_; }

  vk::GraphicsPipelineCreateInfo GetPipelineCreateInfo(
      std::vector<vk::PipelineShaderStageCreateInfo> const& shaderStages,
      vk::UniquePipelineLayout const& layout,
      vk::UniqueRenderPass const& renderPass) {
    vertexInput_ = {
        {},
        bindingDescriptons_,
        attributeDescriptions_,
    };
    return {{},
            shaderStages,
            &vertexInput_,
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
