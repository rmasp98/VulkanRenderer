
#include "pipeline.hpp"

#include <memory>

#include "utils.hpp"

// TODO: Check validity of settings
Pipeline::Pipeline(PipelineSettings const& settings) : settings_(settings) {}

void Pipeline::Register(std::shared_ptr<Device>& device,
                        vk::Extent2D const& extent) {
  if (!layout_ && !cache_) {
    auto descriptorLayouts = GetDescriptorLayouts(device);
    settings_.LayoutSettings.setSetLayouts(descriptorLayouts);
    layout_ = device->CreatePipelineLayout(settings_.LayoutSettings);
    cache_ = device->CreatePipelineCache({});
  }
  settings_.Subpass.pColorAttachments = &settings_.ColourAttachmentRef;

  renderPass_ = device->CreateRenderpass({{},
                                          settings_.ColourAttachment,
                                          settings_.Subpass,
                                          settings_.Dependency});

  auto shadersModules = device->CreateShaderModules(settings_.ShaderFiles);
  auto shaderStages = CreateShaderStages(shadersModules);
  auto vertexInput = GetPipelineVertexInput();
  pipeline_ = device->CreatePipeline(cache_, {{},
                                              shaderStages,
                                              &vertexInput,
                                              &settings_.InputAssembly,
                                              nullptr,
                                              &settings_.ViewportState,
                                              &settings_.Rasterizer,
                                              &settings_.Multisampling_,
                                              &settings_.DepthStencil,
                                              &settings_.ColourBlending,
                                              &settings_.DynamicState,
                                              layout_.get(),
                                              renderPass_.get()});

  framebuffers_ = device->CreateFramebuffers(renderPass_, extent);

  // Might need to free buffers?
  for (auto& command : commands_) {
    command.second.Allocate(device, framebuffers_.size());
    command.second.Record(device, pipeline_, renderPass_, framebuffers_,
                          extent);
  }
}

std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStages(
    std::unordered_map<vk::ShaderStageFlagBits, vk::UniqueShaderModule> const&
        shaderModules) {
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
  for (auto& module : shaderModules) {
    shaderStages.push_back({{}, module.first, module.second.get(), "main"});
  }
  return shaderStages;
}
