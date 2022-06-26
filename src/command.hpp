#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "buffers/vertex_buffer.hpp"
#include "device_api.hpp"
#include "queues.hpp"
#include "vulkan/vulkan.hpp"

class Command {
 public:
  void AddVertexBuffer(std::unique_ptr<Buffer>&& vertBuffer) {
    vertBuffers_.emplace_back(std::move(vertBuffer));
  }

  // TODO: Give ability to set a relative viewport and scissor

  void Allocate(DeviceApi const& device, uint32_t const numFramebuffers) {
    cmdBuffers_ = device.AllocateCommandBuffers(
        vk::CommandBufferLevel::ePrimary, numFramebuffers);
  }

  void Record(
      ImageIndex const imageIndex, vk::UniquePipeline const& pipeline,
      vk::UniquePipelineLayout const& pipelineLayout,
      vk::UniqueRenderPass const& renderPass, Framebuffer const& framebuffer,
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>& uniformBuffers,
      vk::Extent2D const& extent, Queues const& queues, DeviceApi& device) {
    assert(imageIndex < cmdBuffers_.size());

    auto& cmdBuffer = cmdBuffers_[imageIndex];
    cmdBuffer->reset();
    cmdBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

    std::array<vk::ClearValue, 1> clearValues;
    clearValues[0].color =
        vk::ClearColorValue(std::array<float, 4>({{0.2f, 0.2f, 0.2f, 0.2f}}));

    cmdBuffer->beginRenderPass(
        {renderPass.get(), framebuffer.GetFramebuffer(),
         vk::Rect2D(vk::Offset2D(0, 0), extent), clearValues},
        vk::SubpassContents::eInline);

    cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());

    cmdBuffer->setViewport(
        0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0.0f, 1.0f));
    cmdBuffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

    for (auto& uniformBuffer : uniformBuffers) {
      uniformBuffer.second->Bind(imageIndex, cmdBuffer, pipelineLayout);
    }

    for (auto& vertBuffer : vertBuffers_) {
      if (!vertBuffer->IsUploaded()) {
        vertBuffer->Upload(queues, device);
      }
      vertBuffer->UpdateUniformBuffer(uniformBuffers, imageIndex, queues,
                                      device);
      vertBuffer->Record(cmdBuffer);
    }

    cmdBuffer->endRenderPass();
    cmdBuffer->end();
  }

  bool IsAllocated() { return !cmdBuffers_.empty(); }

  void Unregister() { cmdBuffers_.clear(); }

  void Draw(uint32_t const imageIndex, Queues const& queues) const {
    queues.SubmitToGraphics(cmdBuffers_[imageIndex]);
  }

 private:
  std::vector<vk::UniqueCommandBuffer> cmdBuffers_;
  std::vector<std::unique_ptr<Buffer>> vertBuffers_;
};
