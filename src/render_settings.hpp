#ifndef VULKAN_RENDERER_PIPELINE_SETTINGS_HPP
#define VULKAN_RENDERER_PIPELINE_SETTINGS_HPP

#include <unordered_map>

#include "defaults.hpp"
#include "shader.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class RenderPassSettings {
 public:
  RenderPassSettings(
      bool const enableDepth = false,
      vk::SampleCountFlagBits const samples = vk::SampleCountFlagBits::e1)
      : attachments_{defaults::render_pass::ColourAttachment},
        clearValues_{
            vk::ClearColorValue(std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f})} {
    if (enableDepth) {
      EnableDepth();
    }
    SetMultisampleCount(samples);
  }

  void EnableDepth() {
    enableDepth_ = true;
    if (!subpass_.pDepthStencilAttachment) {
      SetDepthAttachment();
    }
  }

  void SetMultisampleCount(vk::SampleCountFlagBits const samples) {
    samples_ = samples;
    for (auto& attachment : attachments_) {
      // The present attachment should only ever have one sample
      if (attachment.finalLayout != vk::ImageLayout::ePresentSrcKHR) {
        attachment.samples = samples_;
      }
    }

    if (samples_ == vk::SampleCountFlagBits::e1) {
      RemoveMultisampleAttachment();
    } else if (subpass_.preserveAttachmentCount == 0) {
      SetMultisampleAttachment();
    }
  }

  void Initialise(DeviceApi const& device) {
    auto maxMultiSampleCount = device.GetMaxMultiSamplingCount();
    if (samples_ > maxMultiSampleCount) {
      samples_ = maxMultiSampleCount;
    }

    for (auto& attachment : attachments_) {
      if (attachment.finalLayout ==
          vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        attachment.format = device.GetDepthBufferFormat();
      } else {
        attachment.format = device.GetSurfaceFormat();
      }

      if (attachment.samples > vk::SampleCountFlagBits::e1) {
        attachment.samples = samples_;
      }
    }
  }

  std::vector<ImageBuffer> CreateAttachmentBuffers(vk::Extent2D const& extent,
                                                   Queues const& queues,
                                                   DeviceApi& device) const {
    std::vector<ImageBuffer> attachmentBuffers;
    if (enableDepth_) {
      attachmentBuffers.push_back(
          DepthBuffer{extent, samples_, queues, device});
    }

    if (samples_ > vk::SampleCountFlagBits::e1) {
      auto surfaceFormat = device.GetSurfaceFormat();
      attachmentBuffers.emplace_back(
          ImageProperties{
              .Extent = {extent.width, extent.height, 1},
              .Usage = vk::ImageUsageFlagBits::eTransientAttachment |
                       vk::ImageUsageFlagBits::eColorAttachment,
              .Format = surfaceFormat,
              .SampleCount = samples_},
          queues, device);
    }

    return attachmentBuffers;
  }

  vk::RenderPassCreateInfo GetRenderPassCreateInfo() {
    // This is needed because copy construction screws pointers up and it was
    // simpler this way
    subpass_ = vk::SubpassDescription{};
    subpass_.setColorAttachments(colourAttachmentReference_);
    if (enableDepth_) {
      subpass_.setPDepthStencilAttachment(&depthAttachmentReference_);
    }
    if (samples_ > vk::SampleCountFlagBits::e1) {
      subpass_.setResolveAttachments(resolveAttachmentReference_);
    }

    return {{}, attachments_, subpass_, dependency_};
  }

  std::vector<vk::ClearValue> const& GetClearValues() const {
    return clearValues_;
  }

  vk::SampleCountFlagBits GetMultisampleCount() const { return samples_; }

 protected:
  void SetDepthAttachment() {
    vk::AttachmentDescription depthAttachment{
        defaults::render_pass::DepthAttachment};
    depthAttachment.samples = samples_;
    attachments_.push_back(depthAttachment);
    depthAttachmentReference_.attachment = attachments_.size() - 1;
    clearValues_.push_back(vk::ClearDepthStencilValue(1.0f, 0));
  }

  void SetMultisampleAttachment() {
    auto colourAttachment{defaults::render_pass::ColourAttachment};
    colourAttachment.samples = samples_;
    colourAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachments_.push_back(colourAttachment);
    colourAttachmentReference_.attachment = attachments_.size() - 1;
    clearValues_.push_back(
        vk::ClearColorValue(std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f}));
  }

  void RemoveMultisampleAttachment() {
    for (auto it = attachments_.begin() + 1; it < attachments_.end(); ++it) {
      if (it->finalLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        it = attachments_.erase(it);
      }
    }

    colourAttachmentReference_.attachment = 0;
  }

 private:
  std::vector<vk::AttachmentDescription> attachments_;
  vk::AttachmentReference colourAttachmentReference_{
      0, vk::ImageLayout::eColorAttachmentOptimal};
  vk::AttachmentReference depthAttachmentReference_{
      0, vk::ImageLayout::eDepthStencilAttachmentOptimal};
  vk::AttachmentReference resolveAttachmentReference_{
      0, vk::ImageLayout::eColorAttachmentOptimal};
  vk::SubpassDescription subpass_;
  vk::SubpassDependency dependency_ = defaults::render_pass::Dependency;
  std::vector<vk::ClearValue> clearValues_;

  bool enableDepth_ = false;
  vk::SampleCountFlagBits samples_;
};

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

  // These are set in the following functions
  vk::PipelineVertexInputStateCreateInfo vertexInput_ = {};
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions_ = {};
  std::vector<vk::VertexInputBindingDescription> bindingDescriptons_ = {};

  template <class VertexStruct>
  void AddVertexLayout() {
    uint32_t binding = attributeDescriptions_.size();
    auto attrDesc = VertexStruct::GetAttributeDetails(binding);
    attributeDescriptions_.insert(attributeDescriptions_.end(),
                                  attrDesc.begin(), attrDesc.end());
    bindingDescriptons_.push_back({binding, sizeof(VertexStruct)});
  }

  vk::PipelineLayoutCreateInfo GetPipelineLayoutCreateInfo(
      std::vector<vk::DescriptorSetLayout> const& layouts,
      std::vector<vk::PushConstantRange> pushConstants) {
    LayoutSettings.setSetLayouts(layouts);
    LayoutSettings.setPushConstantRanges(pushConstants);
    return LayoutSettings;
  }

  vk::GraphicsPipelineCreateInfo GetPipelineCreateInfo(
      std::vector<vk::PipelineShaderStageCreateInfo> const& shaderStages,
      vk::PipelineLayout const& layout, vk::RenderPass const& renderPass) {
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
            layout,
            renderPass};
  }

  std::vector<Shader> CreateShaders(DeviceApi& device) const {
    std::vector<Shader> shaders;
    for (auto& shaderDetails : Shaders) {
      shaders.push_back({shaderDetails.first, shaderDetails.second, device});
    }
    return shaders;
  }
};

}  // namespace vulkan_renderer

#endif