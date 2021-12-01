
#include "pipeline.hpp"

#include <memory>

#include "utils.hpp"

void Pipeline::Register(std::shared_ptr<Device>& device,
                        vk::Extent2D const& extent) {
  auto settings = settings_.GetVulkanSettings();

  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
  for (auto& shaderDetails : settings.Shaders) {
    Shader shader{shaderDetails.first, shaderDetails.second, device};
    shaderStages.push_back(shader.GetStage());
    descriptorSetLayouts.push_back(shader.GetLayout());
    shaders_.push_back(std::move(shader));
  }

  settings.LayoutSettings.setSetLayouts(descriptorSetLayouts);
  layout_ = device->CreatePipelineLayout(settings.LayoutSettings);
  cache_ = device->CreatePipelineCache({});
  settings.Subpass.pColorAttachments = &settings.ColourAttachmentRef;

  renderPass_ = device->CreateRenderpass(
      {{}, settings.ColourAttachment, settings.Subpass, settings.Dependency});

  auto vertexInput = GetPipelineVertexInput();
  pipeline_ = device->CreatePipeline(cache_, {{},
                                              shaderStages,
                                              &vertexInput,
                                              &settings.InputAssembly,
                                              nullptr,
                                              &settings.ViewportState,
                                              &settings.Rasterizer,
                                              &settings.Multisampling,
                                              &settings.DepthStencil,
                                              &settings.ColourBlending,
                                              &settings.DynamicState,
                                              layout_.get(),
                                              renderPass_.get()});
  CreateFramebuffers(device, extent);
}

void Pipeline::CreateFramebuffers(std::shared_ptr<Device>& device,
                                  vk::Extent2D const& extent) {
  framebuffers_ = device->CreateFramebuffers(renderPass_, extent);

  // Might need to free buffers?
  auto descriptorSetLayouts = GetDescriptorSetLayouts();
  for (auto& command : commands_) {
    command.second.Allocate(device, NumBuffers());
    command.second.Record(device, pipeline_, renderPass_, framebuffers_,
                          descriptorSetLayouts, extent);
  }
}

