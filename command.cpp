
#include "command.hpp"

#include <cassert>
#include <iostream>
#include <memory>

void Command::Allocate(std::shared_ptr<Device> const& device,
                       uint32_t const numFramebuffers) {
  cmdBuffers_ = device->AllocateCommandBuffer(vk::CommandBufferLevel::ePrimary,
                                              numFramebuffers);
}

void Command::Record(
    std::shared_ptr<Device>& device, vk::UniquePipeline const& pipeline,
    vk::UniqueRenderPass const& renderPass,
    std::vector<Framebuffer> const& framebuffers,
    std::unordered_map<vk::ShaderStageFlagBits, vk::DescriptorSetLayout> const&
        descriptorSetLayouts,
    vk::Extent2D const& extent) const {
  assert(cmdBuffers_.size() == framebuffers.size());
  for (uint32_t i = 0; i < cmdBuffers_.size(); ++i) {
    auto& cmdBuffer = cmdBuffers_[i];
    cmdBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

    std::array<vk::ClearValue, 1> clearValues;
    clearValues[0].color =
        vk::ClearColorValue(std::array<float, 4>({{0.2f, 0.2f, 0.2f, 0.2f}}));

    cmdBuffer->beginRenderPass(
        {renderPass.get(), framebuffers[i].GetFramebuffer(),
         vk::Rect2D(vk::Offset2D(0, 0), extent), clearValues},
        vk::SubpassContents::eInline);

    cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());

    cmdBuffer->setViewport(
        0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0.0f, 1.0f));
    cmdBuffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

    for (auto& vertBuffer : vertBuffers_) {
      if (!vertBuffer->IsUploaded()) {
        vertBuffer->Upload(device, descriptorSetLayouts);
      }
      vertBuffer->Record(cmdBuffer);
    }

    cmdBuffer->endRenderPass();
    cmdBuffer->end();
  }
}
